/*
 * threads.c — Message queues and the io_cmd_dispatcher thread.
 *
 * Threading model
 * ---------------
 * All GPIO, PWM, and UART commands share a single dispatcher thread that drains
 * io_msgq. These operations are fast (sub-microsecond register writes or a short
 * DMA-bounded TX), so serialising them through one thread is cheap and keeps
 * the design simple.
 *
 * SPI is different: slave-mode transfers block until a master clocks data, which
 * can take arbitrarily long. To prevent a waiting SPI transfer from starving
 * subsequent GPIO/PWM commands, each SPI channel runs its own dedicated worker
 * thread (defined in hw_spi.c) with a higher priority than this dispatcher.
 */

#include <zephyr/logging/log.h>
#include "threads.h"
#include "hardware.h"

LOG_MODULE_REGISTER(app_threads, LOG_LEVEL_INF);

/* Queue depth: 16 entries absorbs a rapid burst of shell commands from the
 * host without blocking the shell thread. The shell produces at most one
 * command per round-trip, so this ceiling is never reached in normal use. */
#define IO_MSGQ_DEPTH  16

/* SPI queue depth: shallower because SPI commands are slower to execute and
 * the host waits for acknowledgement before sending the next one. */
#define SPI_MSGQ_DEPTH 8

#define MSGQ_ALIGNMENT 4

/* Stack and priority for the io_cmd_dispatcher.
 * Priority 5 is intentionally lower than the SPI workers (priority 4) so an
 * in-flight SPI transfer is not preempted by a burst of GPIO/PWM dispatches. */
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
        /* K_FOREVER: sleep with zero CPU cost until a command arrives. */
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
#ifdef CONFIG_APP_UART
            case CMD_UART_SEND:
                handle_uart_send(cmd.params.uart.channel, cmd.params.uart.data, cmd.params.uart.len);
                break;
            case CMD_UART_CONFIG:
                handle_uart_config(cmd.params.uart_cfg.channel, cmd.params.uart_cfg.baudrate,
                                   cmd.params.uart_cfg.parity, cmd.params.uart_cfg.stop_bits);
                break;
#endif
            default:
                break;
            }
        }
    }
}

/* Static thread definition. The dispatcher starts immediately at boot (delay=0)
 * and runs for the lifetime of the firmware. */
K_THREAD_DEFINE(io_worker_tid, IO_THREAD_STACK_SIZE, io_cmd_dispatcher, NULL, NULL, NULL, IO_THREAD_PRIORITY, 0, 0);
