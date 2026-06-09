#ifndef CONTROLLER_HARDWARE_H
#define CONTROLLER_HARDWARE_H

#include <zephyr/kernel.h>

/* Per-subsystem hardware verification (only defined when the
 * corresponding APP_* Kconfig option is enabled). */
int verify_pwm_ready(void);
int verify_gpio_ready(void);
int verify_spi_ready(void);

/* GPIO */
void handle_gpio_set(char port, int pin, int value);
int  handle_gpio_get(char port, int pin);

/* PWM */
void handle_pwm_set(int channel, uint32_t frequency, uint32_t duty_permille);

#endif /* CONTROLLER_HARDWARE_H */
