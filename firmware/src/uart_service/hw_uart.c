#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/init.h>
#include <string.h>
#include "types.h"
#include "hardware.h"

LOG_MODULE_REGISTER(hw_uart, LOG_LEVEL_INF);

#define UART_RX_RING_SIZE   256
#define UART_RX_CHUNK        64
#define UART_RX_TIMEOUT_US 2000   /* flush a partial DMA RX burst after 2 ms idle */
#define UART_TX_TIMEOUT_MS 1000

/* The host drains these via `uart_recv`; the RX DMA callback fills them as
 * bytes land, decoupling capture from when the host actually reads. */
RING_BUF_DECLARE(uart1_rx_rb, UART_RX_RING_SIZE);
RING_BUF_DECLARE(uart2_rx_rb, UART_RX_RING_SIZE);

struct uart_channel {
    const struct device *dev;
    struct ring_buf     *rx_rb;
    /* Ping-pong buffers handed to the RX DMA; the callback alternates them so
     * reception is never disarmed between transfers. */
    uint8_t      rx_buf[2][UART_RX_CHUNK];
    uint8_t      rx_next;
    /* Persistent TX buffer: must outlive the async (DMA) uart_tx() transfer,
     * so we copy the payload here rather than pointing at the caller's stack. */
    uint8_t      tx_buf[UART_MAX_PAYLOAD_SIZE];
    struct k_sem tx_done;
};

/* Index 0 unused so channel numbers map 1:1 (USART1 -> 1, USART2 -> 2),
 * matching the README pin matrix (USART1 PA9/PA10, USART2 PA2/PA3). */
static struct uart_channel channels[] = {
    { 0 },
    { .dev = DEVICE_DT_GET(DT_ALIAS(hil_uart1)), .rx_rb = &uart1_rx_rb },
    { .dev = DEVICE_DT_GET(DT_ALIAS(hil_uart2)), .rx_rb = &uart2_rx_rb },
};

#define UART_CHANNEL_COUNT ((int)(ARRAY_SIZE(channels) - 1))

static struct uart_channel *resolve_channel(int channel)
{
    if (channel < 1 || channel > UART_CHANNEL_COUNT) {
        return NULL;
    }
    return &channels[channel];
}

static void uart_async_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    struct uart_channel *uc = user_data;

    switch (evt->type) {
    case UART_RX_RDY:
        /* New bytes in the active DMA buffer — copy the fresh region into the
         * ring (drops on overflow via the short return count). */
        ring_buf_put(uc->rx_rb,
                     &evt->data.rx.buf[evt->data.rx.offset],
                     evt->data.rx.len);
        break;

    case UART_RX_BUF_REQUEST: {
        /* Hand over the alternate buffer to keep RX continuously armed. */
        uint8_t idx = uc->rx_next;
        uc->rx_next ^= 1U;
        uart_rx_buf_rsp(dev, uc->rx_buf[idx], UART_RX_CHUNK);
        break;
    }

    case UART_RX_DISABLED:
        /* Steady state should never reach here; re-arm defensively. */
        uc->rx_next = 1U;
        uart_rx_enable(dev, uc->rx_buf[0], UART_RX_CHUNK, UART_RX_TIMEOUT_US);
        break;

    case UART_TX_DONE:
    case UART_TX_ABORTED:
        k_sem_give(&uc->tx_done);
        break;

    default:
        break;
    }
}

void handle_uart_send(int channel, const char *data, size_t len)
{
    struct uart_channel *uc = resolve_channel(channel);
    if (!uc) {
        LOG_ERR("Invalid UART channel %d", channel);
        return;
    }
    if (len > sizeof(uc->tx_buf)) {
        len = sizeof(uc->tx_buf);
    }

    memcpy(uc->tx_buf, data, len);

    k_sem_reset(&uc->tx_done);
    int err = uart_tx(uc->dev, uc->tx_buf, len, SYS_FOREVER_US);
    if (err) {
        LOG_ERR("UART%d DMA tx start failed (err %d)", channel, err);
        return;
    }

    /* Runs on the dispatcher thread (never an ISR), so blocking until the DMA
     * transfer completes is safe and keeps tx_buf valid for its duration. */
    if (k_sem_take(&uc->tx_done, K_MSEC(UART_TX_TIMEOUT_MS)) != 0) {
        LOG_ERR("UART%d DMA tx timeout", channel);
        uart_tx_abort(uc->dev);
        return;
    }

    LOG_INF("UART%d sent %u bytes (DMA)", channel, (uint32_t)len);
    LOG_HEXDUMP_INF(uc->tx_buf, len, "Sent");
}

int handle_uart_recv(int channel, uint8_t *out, size_t max_len, int timeout_ms)
{
    struct uart_channel *uc = resolve_channel(channel);
    if (!uc) {
        return -EINVAL;
    }

    /* Wait until max_len bytes have been captured by the RX DMA path or the
     * timeout elapses, then return whatever was collected. */
    int64_t deadline = k_uptime_get() + timeout_ms;
    size_t got = 0;

    while (got < max_len) {
        got += ring_buf_get(uc->rx_rb, out + got, max_len - got);
        if (got >= max_len || k_uptime_get() >= deadline) {
            break;
        }
        k_msleep(2);
    }

    LOG_INF("UART%d received %u bytes (DMA)", channel, (uint32_t)got);
    return (int)got;
}

/* Arm continuous DMA RX on every channel once the UART devices are up. */
static int uart_dma_init(void)
{
    for (int ch = 1; ch <= UART_CHANNEL_COUNT; ch++) {
        struct uart_channel *uc = &channels[ch];
        if (!device_is_ready(uc->dev)) {
            LOG_ERR("UART%d not ready at init", ch);
            continue;
        }

        k_sem_init(&uc->tx_done, 0, 1);
        ring_buf_reset(uc->rx_rb);

        int err = uart_callback_set(uc->dev, uart_async_cb, uc);
        if (err) {
            LOG_ERR("UART%d callback set failed (err %d)", ch, err);
            continue;
        }

        /* Start on buffer 0; the callback hands out buffer 1 next. */
        uc->rx_next = 1U;
        err = uart_rx_enable(uc->dev, uc->rx_buf[0], UART_RX_CHUNK, UART_RX_TIMEOUT_US);
        if (err) {
            LOG_ERR("UART%d DMA rx enable failed (err %d)", ch, err);
        }
    }
    return 0;
}
SYS_INIT(uart_dma_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

int verify_uart_ready(void)
{
    int err = 0;
    for (int ch = 1; ch <= UART_CHANNEL_COUNT; ch++) {
        if (!channels[ch].dev || !device_is_ready(channels[ch].dev)) {
            LOG_ERR("UART%d not ready", ch);
            err = -ENODEV;
        }
    }
    return err;
}
