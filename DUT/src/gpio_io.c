#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "dut_pins.h"
#include "gpio_io.h"
#include "pwm_actuators.h"
#include "cmd_link.h"

LOG_MODULE_REGISTER(gpio_io, LOG_LEVEL_INF);

static const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static struct gpio_callback btn_cb_data;
static struct gpio_callback tamper_cb_data;

static system_state_t current_state = SYSTEM_STATE_SLEEP;
static int tamper_count = 0;
static struct k_timer hb_timer;
static bool hb_state = false;

static void hb_expiry(struct k_timer *timer_id)
{
    if (current_state == SYSTEM_STATE_RUN) {
        hb_state = !hb_state;
        gpio_pin_set(gpio0, GPIO_OUT_HEARTBEAT, hb_state ? 1 : 0);
    } else {
        gpio_pin_set(gpio0, GPIO_OUT_HEARTBEAT, 0);
    }
}

static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (current_state == SYSTEM_STATE_SLEEP) {
        gpio_set_system_state(SYSTEM_STATE_RUN);
    }
}

static void tamper_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    tamper_count++;
    gpio_set_relay(true); // Scenario 2: turn on relay
    cmd_link_send_alert("ALERT: TAMPER DETECTED");
}

void gpio_io_start(void)
{
    if (!device_is_ready(gpio0)) {
        LOG_ERR("GPIO device not ready");
        return;
    }

    gpio_pin_configure(gpio0, GPIO_IN_BUTTON,  GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_configure(gpio0, GPIO_IN_TAMPER,  GPIO_INPUT | GPIO_PULL_DOWN);

    gpio_pin_interrupt_configure(gpio0, GPIO_IN_BUTTON,  GPIO_INT_EDGE_RISING);
    gpio_pin_interrupt_configure(gpio0, GPIO_IN_TAMPER,  GPIO_INT_EDGE_RISING);

    gpio_init_callback(&btn_cb_data, button_isr, BIT(GPIO_IN_BUTTON));
    gpio_add_callback(gpio0, &btn_cb_data);

    gpio_init_callback(&tamper_cb_data, tamper_isr, BIT(GPIO_IN_TAMPER));
    gpio_add_callback(gpio0, &tamper_cb_data);

    gpio_pin_configure(gpio0, GPIO_OUT_STATUS_LED, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio0, GPIO_OUT_FAULT,      GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio0, GPIO_OUT_RELAY,      GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio0, GPIO_OUT_HEARTBEAT,  GPIO_OUTPUT_INACTIVE);

    k_timer_init(&hb_timer, hb_expiry, NULL);
    k_timer_start(&hb_timer, K_MSEC(500), K_MSEC(500));
}

void gpio_set_fault(bool active)
{
    gpio_pin_set(gpio0, GPIO_OUT_FAULT, active ? 1 : 0);
}

void gpio_set_relay(bool active)
{
    gpio_pin_set(gpio0, GPIO_OUT_RELAY, active ? 1 : 0);
}

void gpio_set_system_state(system_state_t state)
{
    current_state = state;
    if (state == SYSTEM_STATE_RUN) {
        LOG_INF("Entering RUN state");
        pwm_actuator_set(500); // 50% default
    } else {
        LOG_INF("Entering SLEEP state");
        pwm_actuator_off();
        /* SLEEP is the system's clean/idle state: clear the latched alarm
         * outputs so each test run (which ends by sleeping) starts fresh. */
        gpio_set_relay(false);
        gpio_set_fault(false);
        tamper_count = 0;
    }
}

system_state_t gpio_get_system_state(void)
{
    return current_state;
}

int gpio_get_tamper_count(void)
{
    return tamper_count;
}
