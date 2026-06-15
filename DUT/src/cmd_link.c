#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include "gpio_io.h"

LOG_MODULE_REGISTER(cmd_link, LOG_LEVEL_INF);

static const struct device *uart1 = DEVICE_DT_GET(DT_ALIAS(uart_cmd));

static void cmd_send(const char *msg)
{
    for (int i = 0; msg[i] != '\0'; i++) {
        uart_poll_out(uart1, msg[i]);
    }
    uart_poll_out(uart1, '\n');
}

void cmd_link_send_alert(const char *msg)
{
    cmd_send(msg);
}

static void cmd_dispatch(char *rx_buf, int rx_len)
{
    rx_buf[rx_len] = '\0';
    char out[64] = "ERR";

    if (strncmp(rx_buf, "SLEEP", 5) == 0) {
        gpio_set_system_state(SYSTEM_STATE_SLEEP);
        snprintf(out, sizeof(out), "SLEEPOK");
    } else if (strncmp(rx_buf, "PING", 4) == 0) {
        snprintf(out, sizeof(out), "PONG");
    }
    cmd_send(out);
}

static void cmd_thread_func(void *arg1, void *arg2, void *arg3)
{
    char rx_buf[64];
    int rx_len = 0;
    int idle = 0;

    /* The bench delivers each command as a single burst with NO terminator
     * (its shell strips the trailing newline), so frame on the idle gap that
     * follows the burst -- while still honouring an explicit \n/\r if one
     * ever arrives. */
    while (1) {
        unsigned char c;
        if (uart_poll_in(uart1, &c) == 0) {
            if (c == '\n' || c == '\r') {
                if (rx_len > 0) {
                    cmd_dispatch(rx_buf, rx_len);
                    rx_len = 0;
                }
            } else if (rx_len < (int)sizeof(rx_buf) - 1) {
                rx_buf[rx_len++] = c;
            }
            idle = 0;
        } else {
            if (rx_len > 0 && ++idle >= 3) {   /* ~6 ms quiet => burst complete */
                cmd_dispatch(rx_buf, rx_len);
                rx_len = 0;
                idle = 0;
            }
            k_sleep(K_MSEC(2));
        }
    }
}

K_THREAD_DEFINE(cmd_tid, 1024, cmd_thread_func, NULL, NULL, NULL, 6, 0, 0);

void cmd_link_start(void)
{
    if (!device_is_ready(uart1)) {
        LOG_ERR("UART1 not ready");
    }
}
