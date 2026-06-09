#include <zephyr/logging/log.h>
#include "threads.h"
#include "hardware.h"

LOG_MODULE_REGISTER(app_threads, LOG_LEVEL_INF);

#define IO_MSGQ_DEPTH         16
#define SPI_MSGQ_DEPTH        8
#define MSGQ_ALIGNMENT        4

#define IO_THREAD_STACK_SIZE  2048
#define IO_THREAD_PRIORITY    5


K_MSGQ_DEFINE(io_msgq, sizeof(struct io_cmd), IO_MSGQ_DEPTH, MSGQ_ALIGNMENT);
#ifdef CONFIG_APP_SPI
K_MSGQ_DEFINE(spi1_msgq, sizeof(struct spi_cmd), SPI_MSGQ_DEPTH, MSGQ_ALIGNMENT);
K_MSGQ_DEFINE(spi2_msgq, sizeof(struct spi_cmd), SPI_MSGQ_DEPTH, MSGQ_ALIGNMENT);
#endif

void io_cmd_dispatcher(void *p1, void *p2, void *p3)
{
    struct io_cmd cmd;
    LOG_INF("Command Dispatcher thread started (priority %d)", IO_THREAD_PRIORITY);

    while (1) {
        if (k_msgq_get(&io_msgq, &cmd, K_FOREVER) == 0) {
            switch (cmd.type) {
#ifdef CONFIG_APP_PWM
            case CMD_PWM_SET:
                handle_pwm_set(cmd.params.pwm.channel, cmd.params.pwm.frequency, cmd.params.pwm.duty_permille);
                break;
#endif
#ifdef CONFIG_APP_GPIO
            case CMD_GPIO_SET:
                handle_gpio_set(cmd.params.gpio.port, cmd.params.gpio.pin, cmd.params.gpio.value);
                break;
#endif
            default:
                break;
            }
        }
    }
}

/* Statically launch the threads */
K_THREAD_DEFINE(io_worker_tid, IO_THREAD_STACK_SIZE, io_cmd_dispatcher, NULL, NULL, NULL, IO_THREAD_PRIORITY, 0, 0);