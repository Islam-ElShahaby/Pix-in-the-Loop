#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include "pwm_actuators.h"

LOG_MODULE_REGISTER(sensor_peer, LOG_LEVEL_INF);

static const struct device *uart2 = DEVICE_DT_GET(DT_ALIAS(uart_peer));

static void peer_parse(char *rx_buf, int rx_len)
{
    rx_buf[rx_len] = '\0';
    // Scenario 3: Parse "TEMP:xx"
    int temp;
    if (sscanf(rx_buf, "TEMP:%d", &temp) == 1) {
        LOG_INF("Received TEMP=%d", temp);
        if (temp >= 100) pwm_actuator_set(1000);
        else if (temp <= 0) pwm_actuator_set(0);
        else pwm_actuator_set(temp * 10);
    }
}

static void peer_thread_func(void *arg1, void *arg2, void *arg3)
{
    char rx_buf[64];
    int rx_len = 0;
    int idle = 0;

    /* Same idle-gap framing as the command link: the bench shell delivers each
     * frame as one burst with no trailing newline. */
    while (1) {
        unsigned char c;
        if (uart_poll_in(uart2, &c) == 0) {
            if (c == '\n' || c == '\r') {
                if (rx_len > 0) {
                    peer_parse(rx_buf, rx_len);
                    rx_len = 0;
                }
            } else if (rx_len < (int)sizeof(rx_buf) - 1) {
                rx_buf[rx_len++] = c;
            }
            idle = 0;
        } else {
            if (rx_len > 0 && ++idle >= 3) {
                peer_parse(rx_buf, rx_len);
                rx_len = 0;
                idle = 0;
            }
            k_sleep(K_MSEC(2));
        }
    }
}

K_THREAD_DEFINE(peer_tid, 1024, peer_thread_func, NULL, NULL, NULL, 7, 0, 0);

void sensor_peer_start(void)
{
    if (!device_is_ready(uart2)) {
        LOG_ERR("UART2 not ready");
    }
}
