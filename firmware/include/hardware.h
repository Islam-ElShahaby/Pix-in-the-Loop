/*
 * hardware.h — Interface boundary between the shell command layer and the
 * hardware driver implementations.
 *
 * Shell command handlers call only the functions declared here; they never
 * touch driver APIs directly. This keeps the shell module free of hardware
 * details and makes each subsystem independently testable.
 *
 * The verify_*_ready() functions are called once at boot. They return 0 on
 * success or a negative errno if any required device is not available. A
 * non-zero return causes main() to log a fatal error but does not halt the
 * system — the shell remains usable for diagnosis.
 */

#ifndef CONTROLLER_HARDWARE_H
#define CONTROLLER_HARDWARE_H

#include <zephyr/kernel.h>

/* Per-subsystem readiness checks — only defined when the corresponding
 * APP_* Kconfig option is enabled. */
int verify_pwm_ready(void);
int verify_gpio_ready(void);
int verify_spi_ready(void);
int verify_uart_ready(void);
int verify_adc_ready(void);

/* GPIO — port is 'A', 'B', or 'C'; pin is 0–15; value is 0 or 1.
 * Direction is configured lazily on first access (see hw_gpio.c). */
void handle_gpio_set(char port, int pin, int value);
int  handle_gpio_get(char port, int pin); /* returns pin level (0/1) or negative errno */

/* PWM — channel 1–4; frequency in Hz; duty_permille in parts per thousand
 * (0 = always off, 1000 = always on). */
void handle_pwm_set(int channel, uint32_t frequency, uint32_t duty_permille);

/* UART — channel 1 (USART1, PA9/PA10) or 2 (USART2, PA2/PA3).
 * handle_uart_send: DMA TX; blocks until the transfer completes or times out.
 * handle_uart_recv: drains the DMA RX ring buffer; blocks until max_len bytes
 *   are available or timeout_ms elapses. Returns number of bytes placed in out,
 *   or negative errno on error.
 * handle_uart_config: reconfigures the line at runtime. parity is 'N', 'E', or
 *   'O'; stop_bits is 1 or 2; data bits stay at 8. Briefly disables and re-arms
 *   the DMA RX path around the change. Returns 0 or a negative errno. */
void handle_uart_send(int channel, const char *data, size_t len);
int  handle_uart_recv(int channel, uint8_t *out, size_t max_len, int timeout_ms);
int  handle_uart_config(int channel, uint32_t baudrate, char parity, int stop_bits);

int handle_adc_sample_channel(int channel);

#endif /* CONTROLLER_HARDWARE_H */
