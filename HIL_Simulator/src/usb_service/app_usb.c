#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>
#include "app_usb.h"

LOG_MODULE_REGISTER(usb_service, LOG_LEVEL_INF);

#define BLACKPILL_VID   0x2FE3
#define BLACKPILL_PID   0x0001
#define RING_BUF_SIZE   256

/* Next-Gen USBD Stack instantiation */
USBD_DEVICE_DEFINE(my_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), BLACKPILL_VID, BLACKPILL_PID);

/* Defining strings. These macro hooks attach themselves internally 
   to the nearest defined USBD device structure context inside this source file compilation unit. */
// USBD_DESC_LANG_DEFINE(my_lang);
// USBD_DESC_MANUFACTURER_DEFINE(my_mfr, "WeAct Studio");
// USBD_DESC_PRODUCT_DEFINE(my_product, "BlackPill CDC ACM");

/* Private Storage: Hardware pointer, ring buffer, and allocation thread-safety locks */
static const struct device *uart_dev;
static struct ring_buf rx_ringbuf;
static uint8_t rx_buffer_memory[RING_BUF_SIZE];
static K_SEM_DEFINE(rx_sem, 0, K_SEM_MAX_LIMIT); // Signals main thread when new data arrives

/**
 * @brief Asynchronous UART Interrupt Callback Handler
 */
static void uart_cdc_acm_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t rx_byte;
            int recv_len = uart_fifo_read(dev, &rx_byte, 1);
            if (recv_len > 0) {
                if (ring_buf_put(&rx_ringbuf, &rx_byte, 1) > 0) {
                    k_sem_give(&rx_sem);
                }
            }
        }
    }
}

int app_usb_init(void)
{
    uint32_t dtr = 0;
    int ret;

    uart_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("Virtual CDC ACM hardware interface is not ready.");
        return -ENODEV;
    }

    /* Initialize circular reception ring buffer data structure */
    ring_buf_init(&rx_ringbuf, sizeof(rx_buffer_memory), rx_buffer_memory);

    /* ✅ REMOVED: usbd_add_descriptor loops are gone.
       The language, manufacturer, and product macro defines above are bound 
       statically into iterable memory section sections by Zephyr's USBD engine 
       and loaded dynamically upon the usbd_init() sequence call below. */

    /* Initialize and activate the stack */
    ret = usbd_init(&my_usbd);
    if (ret != 0) {
        LOG_ERR("Failed to initialize modern USBD runtime: %d", ret);
        return ret;
    }

    /* Enable physical bus enumeration */
    ret = usbd_enable(&my_usbd);
    if (ret != 0) {
        LOG_ERR("Failed to enable USBD hardware block: %d", ret);
        return ret;
    }

    LOG_INF("Modern USB engine operational. Waiting for host connection...");

    /* Wait securely until a terminal console hooks up to our Virtual COM port */
    while (!dtr) {
        uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
        k_msleep(100);
    }
    LOG_INF("Host terminal pipeline open.");

    /* Clear any stale line noise from the hardware layer prior to enabling events */
    uint8_t dummy_flush;
    while (uart_fifo_read(uart_dev, &dummy_flush, 1) > 0);

    /* Attach our custom Interrupt Callback and activate the handler */
    uart_irq_callback_set(uart_dev, uart_cdc_acm_isr);
    
    /* Turn on hardware receiver data interrupts */
    uart_irq_rx_enable(uart_dev);

    LOG_INF("USB Asynchronous Rx/Tx Service Active.");
    return 0;
}

/**
 * @brief Public Transmission API (Synchronous Polling Method)
 */
int app_usb_transmit(const uint8_t *data, size_t length)
{
    if (uart_dev == NULL || !device_is_ready(uart_dev)) {
        return -ENODEV;
    }

    for (size_t i = 0; i < length; i++) {
        uart_poll_out(uart_dev, data[i]);
    }
    return 0;
}

/**
 * @brief Public Reception API (Thread-Safe Blocking Method)
 */
int app_usb_receive(uint8_t *out_data, size_t max_length, k_timeout_t timeout)
{
    if (k_sem_take(&rx_sem, timeout) != 0) {
        return 0; 
    }

    return ring_buf_get(&rx_ringbuf, out_data, max_length);
}
