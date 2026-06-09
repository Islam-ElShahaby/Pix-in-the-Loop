/*
 * hw_gpio.c — Lazy-configured GPIO driver for the HIL controller.
 *
 * Pins are not pre-configured at boot. Direction (INPUT or OUTPUT) is set on
 * the first access to each pin. This means the host can treat any available
 * pin as either an input or an output without requiring a setup step, and the
 * firmware never has to maintain a static pin-allocation table.
 *
 * Supported ports: GPIOA, GPIOB, GPIOC (all available on the STM32F401 Blackpill).
 */

#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include "hardware.h"

LOG_MODULE_REGISTER(hw_gpio, LOG_LEVEL_INF);

static const struct device *gpioa = DEVICE_DT_GET(DT_NODELABEL(gpioa));
static const struct device *gpiob = DEVICE_DT_GET(DT_NODELABEL(gpiob));
static const struct device *gpioc = DEVICE_DT_GET(DT_NODELABEL(gpioc));

/* One bit per pin. A set bit means that pin has already been configured as
 * OUTPUT, so subsequent writes skip the gpio_pin_configure() call. Input pins
 * are reconfigured on every read because a pin can be repurposed between calls. */
static uint16_t gpioa_output_mask = 0;
static uint16_t gpiob_output_mask = 0;
static uint16_t gpioc_output_mask = 0;

static const struct device *resolve_gpio_port(char port)
{
    switch (port) {
    case 'A': case 'a': return gpioa;
    case 'B': case 'b': return gpiob;
    case 'C': case 'c': return gpioc;
    default:            return NULL;
    }
}

static uint16_t *resolve_output_mask(const struct device *port_dev)
{
    if (port_dev == gpioa) return &gpioa_output_mask;
    if (port_dev == gpiob) return &gpiob_output_mask;
    if (port_dev == gpioc) return &gpioc_output_mask;
    return NULL;
}

void handle_gpio_set(char port, int pin, int value)
{
    const struct device *port_dev = resolve_gpio_port(port);
    if (!port_dev) {
        LOG_ERR("Invalid GPIO port '%c'", port);
        return;
    }

    uint16_t *mask = resolve_output_mask(port_dev);

    /* Lazy-configure as OUTPUT on first write to this pin. */
    if (mask && !(*mask & (1U << pin))) {
        int err = gpio_pin_configure(port_dev, pin, GPIO_OUTPUT);
        if (err) {
            LOG_ERR("GPIO %c%d configure OUTPUT failed (err %d)", port, pin, err);
            return;
        }
        *mask |= (1U << pin);
    }

    int err = gpio_pin_set_raw(port_dev, pin, value);
    if (err) {
        LOG_ERR("GPIO %c%d write failed (err %d)", port, pin, err);
    } else {
        LOG_INF("GPIO %c%d -> %d", port, pin, value);
    }
}

int handle_gpio_get(char port, int pin)
{
    const struct device *port_dev = resolve_gpio_port(port);
    if (!port_dev) {
        LOG_ERR("Invalid GPIO port '%c'", port);
        return -EINVAL;
    }

    uint16_t *mask = resolve_output_mask(port_dev);

    /* If pin was not configured as OUTPUT, configure as INPUT now. */
    if (mask && !(*mask & (1U << pin))) {
        int err = gpio_pin_configure(port_dev, pin, GPIO_INPUT);
        if (err) {
            LOG_ERR("GPIO %c%d configure INPUT failed (err %d)", port, pin, err);
            return err;
        }
    }

    int val = gpio_pin_get_raw(port_dev, pin);
    if (val < 0) {
        LOG_ERR("GPIO %c%d read failed (err %d)", port, pin, val);
    } else {
        LOG_INF("GPIO %c%d read -> %d", port, pin, val);
    }
    return val;
}

int verify_gpio_ready(void)
{
    int err = 0;
    if (!gpioa || !device_is_ready(gpioa)) { LOG_ERR("GPIOA not ready"); err = -ENODEV; }
    if (!gpiob || !device_is_ready(gpiob)) { LOG_ERR("GPIOB not ready"); err = -ENODEV; }
    if (!gpioc || !device_is_ready(gpioc)) { LOG_ERR("GPIOC not ready"); err = -ENODEV; }
    return err;
}
