/*
 * threads.h — Shared message queues and thread entry points.
 *
 * This header is the glue between the shell command layer (producers) and the
 * hardware worker threads (consumers). The shell handlers post commands here;
 * they never call hardware functions directly.
 *
 * Threading model:
 *   io_msgq      → io_cmd_dispatcher  (GPIO, PWM, UART — fast, non-blocking ops)
 *   spi1_msgq    → spi_worker_entry   (SPI channel 1 — can block for ms-range transfers)
 *   spi2_msgq    → spi_worker_entry   (SPI channel 2 — independent of channel 1)
 *
 * SPI gets its own per-channel worker precisely because slave-mode transfers
 * block until the master clocks data. Routing them through io_msgq would stall
 * every subsequent GPIO and PWM command for the duration of the wait.
 */

#ifndef CONTROLLER_THREADS_H
#define CONTROLLER_THREADS_H

#include <zephyr/kernel.h>
#include "types.h"

/* io_msgq: carries CMD_PWM_SET, CMD_GPIO_SET, and CMD_UART_SEND commands.
 * Producers: shell command handlers (shell thread context).
 * Consumer:  io_cmd_dispatcher. */
extern struct k_msgq io_msgq;

/* spi1_msgq / spi2_msgq: carry struct spi_cmd for USART1 and USART2 respectively.
 * Producers: cmd_spi_write / cmd_spi_slave_wait (shell thread context).
 * Consumers: one spi_worker_entry thread per channel. */
extern struct k_msgq spi1_msgq;
extern struct k_msgq spi2_msgq;

/* io_cmd_dispatcher: drains io_msgq forever, dispatching each command to the
 * appropriate handle_*() function. Runs at a lower priority than SPI workers
 * so in-flight SPI transfers are not preempted by GPIO/PWM bursts. */
void io_cmd_dispatcher(void *p1, void *p2, void *p3);

/* spi_worker_entry: shared entry point for both SPI channel workers.
 * p1 must point to a const struct spi_worker_config (defined in hw_spi.c)
 * that identifies the device, its DMA queue, and its reset controller line.
 * p2 and p3 are unused. */
void spi_worker_entry(void *p1, void *p2, void *p3);

#endif /* CONTROLLER_THREADS_H */
