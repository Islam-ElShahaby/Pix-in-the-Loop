#ifndef APP_GPIO_H_
#define APP_GPIO_H_

#include <stdint.h>

/**
 * @brief Initializes both the output pin (led0) and the input pin (button0).
 * @return 0 on success, negative error code on failure.
 */
int app_gpio_init(void);

/**
 * @brief Toggles the state of the output pin (led0).
 */
void app_gpio_toggle(void);

/**
 * @brief Force-writes a specific logical value to the output pin (led0).
 * @param value 1 for active (logical high), 0 for inactive (logical low).
 * @return 0 on success, negative error code on failure.
 */
int app_gpio_write(int value);

/**
 * @brief Reads the current logical state of the input pin (button0).
 * @return 1 if the input is active (button pressed), 0 if inactive, or negative error.
 */
int app_gpio_read(void);

#endif /* APP_GPIO_H_ */
