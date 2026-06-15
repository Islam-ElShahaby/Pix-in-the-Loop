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

/* Latency-measurement state. A single measurement runs at a time (the shell
 * thread blocks in handle_gpio_latency until it completes or times out), so a
 * single shared callback/semaphore/timestamp is sufficient. The ISR records the
 * cycle counter the instant the response edge arrives and wakes the caller. */
static struct gpio_callback latency_cb;
static K_SEM_DEFINE(latency_sem, 0, 1);
static volatile uint32_t latency_t1;

static void latency_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    latency_t1 = k_cycle_get_32();
    k_sem_give(&latency_sem);
}

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

int handle_gpio_latency(char out_port, int out_pin, char in_port, int in_pin,
                        int edge, int timeout_ms)
{
    const struct device *out_dev = resolve_gpio_port(out_port);
    const struct device *in_dev  = resolve_gpio_port(in_port);
    if (!out_dev || !in_dev) {
        LOG_ERR("Latency: invalid port (out '%c', in '%c')", out_port, in_port);
        return -EINVAL;
    }

    /* Map the response-edge selector to an interrupt trigger. */
    gpio_flags_t int_flags;
    switch (edge) {
    case 'R': case 'r': int_flags = GPIO_INT_EDGE_RISING;  break;
    case 'F': case 'f': int_flags = GPIO_INT_EDGE_FALLING; break;
    case 'B': case 'b': int_flags = GPIO_INT_EDGE_BOTH;    break;
    default:
        LOG_ERR("Latency: invalid edge '%c' (use R/F/B)", edge);
        return -EINVAL;
    }

    int err;

    /* Stimulus output, idle low. Drive it explicitly and keep the lazy-config
     * masks coherent: mark out_pin as OUTPUT, clear in_pin's OUTPUT bit so a
     * later gpio_get re-reads it as an input. */
    err = gpio_pin_configure(out_dev, out_pin, GPIO_OUTPUT);
    if (err) {
        LOG_ERR("Latency: out %c%d configure failed (err %d)", out_port, out_pin, err);
        return err;
    }
    gpio_pin_set_raw(out_dev, out_pin, 0);
    uint16_t *out_mask = resolve_output_mask(out_dev);
    if (out_mask) *out_mask |= (1U << out_pin);

    err = gpio_pin_configure(in_dev, in_pin, GPIO_INPUT);
    if (err) {
        LOG_ERR("Latency: in %c%d configure failed (err %d)", in_port, in_pin, err);
        return err;
    }
    uint16_t *in_mask = resolve_output_mask(in_dev);
    if (in_mask) *in_mask &= ~(1U << in_pin);

    /* Arm the response-edge interrupt. */
    gpio_init_callback(&latency_cb, latency_isr, BIT(in_pin));
    err = gpio_add_callback(in_dev, &latency_cb);
    if (err) {
        LOG_ERR("Latency: add_callback failed (err %d)", err);
        return err;
    }
    k_sem_reset(&latency_sem);
    err = gpio_pin_interrupt_configure(in_dev, in_pin, int_flags);
    if (err) {
        LOG_ERR("Latency: interrupt_configure failed (err %d)", err);
        gpio_remove_callback(in_dev, &latency_cb);
        return err;
    }

    /* Let the line settle, then fire the stimulus edge and time it. irq_lock
     * keeps the t0 capture and the edge atomic so nothing perturbs the start. */
    k_busy_wait(50);
    unsigned int key = irq_lock();
    uint32_t t0 = k_cycle_get_32();
    gpio_pin_set_raw(out_dev, out_pin, 1);
    irq_unlock(key);

    err = k_sem_take(&latency_sem, K_MSEC(timeout_ms));
    uint32_t t1 = latency_t1;

    /* Tear down: silence the interrupt, drop the callback, idle the stimulus. */
    gpio_pin_interrupt_configure(in_dev, in_pin, GPIO_INT_DISABLE);
    gpio_remove_callback(in_dev, &latency_cb);
    gpio_pin_set_raw(out_dev, out_pin, 0);

    if (err) {
        LOG_WRN("Latency: no response edge within %d ms", timeout_ms);
        return -ETIMEDOUT;
    }

    uint32_t us = k_cyc_to_us_floor32(t1 - t0);
    LOG_INF("Latency: %c%d -> %c%d = %u us", out_port, out_pin, in_port, in_pin, us);
    return (int)us;
}

int verify_gpio_ready(void)
{
    int err = 0;
    if (!gpioa || !device_is_ready(gpioa)) { LOG_ERR("GPIOA not ready"); err = -ENODEV; }
    if (!gpiob || !device_is_ready(gpiob)) { LOG_ERR("GPIOB not ready"); err = -ENODEV; }
    if (!gpioc || !device_is_ready(gpioc)) { LOG_ERR("GPIOC not ready"); err = -ENODEV; }
    return err;
}
