#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "app_gpio.h"

LOG_MODULE_REGISTER(gpio_service, LOG_LEVEL_INF);

/* Pull both hardware pin structures from your app.overlay aliases */
static const struct gpio_dt_spec out_pin = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec in_pin  = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);

int app_gpio_init(void) {
    int ret;

    /* 1. Verify and configure the Output Slot (PC13) */
    if (!gpio_is_ready_dt(&out_pin)) {
        LOG_ERR("Output GPIO controller device not ready.");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&out_pin, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure output pin: %d", ret);
        return ret;
    }

    /* 2. Verify and configure the Input Slot (PB10) */
    if (!gpio_is_ready_dt(&in_pin)) {
        LOG_ERR("Input GPIO controller device not ready.");
        return -ENODEV;
    }
    /* Note: GPIO_INPUT pulls the extra flags (like GPIO_PULL_UP) from your overlay automatically */
    ret = gpio_pin_configure_dt(&in_pin, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure input pin: %d", ret);
        return ret;
    }

    LOG_INF("GPIO Service successfully initialized (Output: PC13, Input: PB10).");
    return 0;
}

void app_gpio_toggle(void) {
    gpio_pin_toggle_dt(&out_pin);
}

int app_gpio_write(int value) {
    int ret = gpio_pin_set_dt(&out_pin, value);
    if (ret < 0) {
        LOG_ERR("Failed to write to output pin: %d", ret);
    }
    return ret;
}

int app_gpio_read(void) {
    int value = gpio_pin_get_dt(&in_pin);
    if (value < 0) {
        LOG_ERR("Failed to read from input pin: %d", value);
    }
    return value;
}
