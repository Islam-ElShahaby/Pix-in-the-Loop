#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/reset.h>
#include <zephyr/dt-bindings/reset/stm32f2_4_7_reset.h>
#include "threads.h"
#include "hardware.h"

LOG_MODULE_REGISTER(hw_spi, LOG_LEVEL_INF);

#define SPI_BUS_FREQUENCY                   1000000U
#define SPI_SLAVE_MAX_CONSECUTIVE_ERRORS    5  

#define SPI_THREAD_STACK_SIZE 2048
#define SPI_THREAD_PRIORITY   4

/* Per-channel static configuration passed to the generic SPI worker.
 * The STM32 SPI binding does not expose a `resets` property, so the reset
 * controller (rctl) and per-peripheral reset id are referenced directly:
 *   SPI1 -> RCC_APB2RSTR bit 12, SPI2 -> RCC_APB1RSTR bit 14. */
struct spi_worker_config {
    const struct device  *dev;
    struct k_msgq        *queue;
    int                   channel;
    const struct device  *reset_dev;
    uint32_t              reset_id;
};

static const struct spi_worker_config spi1_cfg = {
    .dev       = DEVICE_DT_GET(DT_NODELABEL(spi1)),
    .queue     = &spi1_msgq,
    .channel   = 1,
    .reset_dev = DEVICE_DT_GET(DT_NODELABEL(rctl)),
    .reset_id  = STM32_RESET(APB2, 12),
};

static const struct spi_worker_config spi2_cfg = {
    .dev       = DEVICE_DT_GET(DT_NODELABEL(spi2)),
    .queue     = &spi2_msgq,
    .channel   = 2,
    .reset_dev = DEVICE_DT_GET(DT_NODELABEL(rctl)),
    .reset_id  = STM32_RESET(APB1, 14),
};

/* Perform a clean peripheral reset after a failed/aborted async SPI transaction.
 * Must be called AFTER spi_release so the driver mutex is not held during reset.
 * The assert+deassert cycle resets both the SPI peripheral registers and
 * terminates any in-flight DMA request on the associated streams. */
static void spi_peripheral_reset(const struct spi_worker_config *cfg)
{
    int err;

    err = reset_line_assert(cfg->reset_dev, cfg->reset_id);
    if (err) {
        LOG_ERR("SPI%d reset assert failed (err %d)", cfg->channel, err);
        return;
    }

    /* Hold reset for at least 1 us; busy-wait avoids scheduler interference
     * at a point where peripheral state is intentionally undefined */
    k_busy_wait(5U);

    err = reset_line_deassert(cfg->reset_dev, cfg->reset_id);
    if (err) {
        LOG_ERR("SPI%d reset deassert failed (err %d)", cfg->channel, err);
        return;
    }

    /* Brief stabilisation delay before the peripheral accepts new config */
    k_busy_wait(10U);

    LOG_WRN("SPI%d peripheral reset complete", cfg->channel);
}

void spi_worker_entry(void *p1, void *p2, void *p3)
{
    const struct spi_worker_config *cfg = (const struct spi_worker_config *)p1;
    struct spi_cmd cmd;

    LOG_INF("SPI%d worker thread started", cfg->channel);

    struct spi_config config = {
        .frequency = SPI_BUS_FREQUENCY,
        .slave     = 0,
    };

    struct k_poll_signal async_sig;
    k_poll_signal_init(&async_sig);
    struct k_poll_event event = K_POLL_EVENT_INITIALIZER(
        K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &async_sig);

    while (1) {
        if (k_msgq_get(cfg->queue, &cmd, K_FOREVER) != 0) {
            continue;
        }

        if (!cfg->dev) {
            LOG_ERR("SPI%d device is NULL", cfg->channel);
            continue;
        }

        uint8_t rx_buf[SPI_MAX_PAYLOAD_SIZE] = {0};

        struct spi_buf tx = { .buf = cmd.tx_buf, .len = cmd.len };
        struct spi_buf rx = { .buf = rx_buf,     .len = cmd.len };

        struct spi_buf_set tx_bufs = { .buffers = &tx, .count = 1 };
        struct spi_buf_set rx_bufs = { .buffers = &rx, .count = 1 };

        config.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
                           (cmd.is_slave ? SPI_OP_MODE_SLAVE : SPI_OP_MODE_MASTER);

        if (cmd.is_slave) {
            k_poll_signal_reset(&async_sig);
            event.state = K_POLL_STATE_NOT_READY;

            int err = spi_transceive_signal(cfg->dev, &config,
                                            &tx_bufs, &rx_bufs, &async_sig);
            if (err) {
                LOG_ERR("SPI%d async transceive failed to start (err %d)",
                        cfg->channel, err);
                continue;
            }

            err = k_poll(&event, 1, K_MSEC(cmd.timeout_ms));

            if (err == -EAGAIN) {
                LOG_ERR("SPI%d slave timeout after %d ms — resetting peripheral",
                        cfg->channel, cmd.timeout_ms);

                /* Step 1: release driver lock BEFORE touching hardware.
                 * Reversing this order risks deadlock inside the driver. */
                spi_release(cfg->dev, &config);

                /* Step 2: assert+deassert reset — aborts DMA and clears
                 * SPI SR/CR registers without needing to touch them directly */
                spi_peripheral_reset(cfg);

            } else if (err) {
                LOG_ERR("SPI%d k_poll error (err %d)", cfg->channel, err);
                spi_release(cfg->dev, &config);
            } else {
                LOG_INF("SPI%d slave transceive complete", cfg->channel);
                LOG_HEXDUMP_INF(cmd.tx_buf, cmd.len, "Sent");
                LOG_HEXDUMP_INF(rx_buf,     cmd.len, "Recv");
            }

        } else {
            int err = spi_transceive(cfg->dev, &config, &tx_bufs, &rx_bufs);
            if (err) {
                LOG_ERR("SPI%d master transceive failed (err %d)", cfg->channel, err);
            } else {
                LOG_INF("SPI%d master transceive complete", cfg->channel);
                LOG_HEXDUMP_INF(cmd.tx_buf, cmd.len, "Sent");
                LOG_HEXDUMP_INF(rx_buf,     cmd.len, "Recv");
            }
        }
    }
}

K_THREAD_DEFINE(spi1_worker_tid, SPI_THREAD_STACK_SIZE,
                spi_worker_entry, (void *)&spi1_cfg, NULL, NULL,
                SPI_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(spi2_worker_tid, SPI_THREAD_STACK_SIZE,
                spi_worker_entry, (void *)&spi2_cfg, NULL, NULL,
                SPI_THREAD_PRIORITY, 0, 0);

int verify_spi_ready(void)
{
    int err = 0;
    if (!device_is_ready(spi1_cfg.dev)) { LOG_ERR("SPI1 device not ready"); err = -ENODEV; }
    if (!device_is_ready(spi2_cfg.dev)) { LOG_ERR("SPI2 device not ready"); err = -ENODEV; }
    if (!device_is_ready(spi1_cfg.reset_dev)) { LOG_ERR("SPI1 reset controller not ready"); err = -ENODEV; }
    if (!device_is_ready(spi2_cfg.reset_dev)) { LOG_ERR("SPI2 reset controller not ready"); err = -ENODEV; }
    return err;
}
