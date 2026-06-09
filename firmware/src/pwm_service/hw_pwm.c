/*
 * hw_pwm.c — Independent PWM output driver for the HIL controller.
 *
 * Each of the four channels is backed by a distinct hardware timer
 * (TIM1, TIM2, TIM10, TIM4), so all four can run at entirely different
 * frequencies simultaneously. A shared timer would force all channels on it
 * to use the same period, which would prevent mixed-frequency stimulation.
 *
 * Pin assignments (from the overlay / pin allocation matrix):
 *   ch1 = PA8  (TIM1_CH1)
 *   ch2 = PA15 (TIM2_CH1)
 *   ch3 = PB8  (TIM10_CH1)
 *   ch4 = PB6  (TIM4_CH1)
 */

#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include "hardware.h"

LOG_MODULE_REGISTER(hw_pwm, LOG_LEVEL_INF);

#define PWM_F1_NODE DT_ALIAS(pwm_f1)
#define PWM_F2_NODE DT_ALIAS(pwm_f2)
#define PWM_F3_NODE DT_ALIAS(pwm_f3)
#define PWM_F4_NODE DT_ALIAS(pwm_f4)

static const struct pwm_dt_spec pwm1 = PWM_DT_SPEC_GET(PWM_F1_NODE);
static const struct pwm_dt_spec pwm2 = PWM_DT_SPEC_GET(PWM_F2_NODE);
static const struct pwm_dt_spec pwm3 = PWM_DT_SPEC_GET(PWM_F3_NODE);
static const struct pwm_dt_spec pwm4 = PWM_DT_SPEC_GET(PWM_F4_NODE);

/* Index 0 is NULL so that shell channel numbers (1–4) map directly to array
 * indices without requiring a subtraction at the call site. */
static const struct pwm_dt_spec *const pwm_channels[] = {
    NULL, &pwm1, &pwm2, &pwm3, &pwm4
};

void handle_pwm_set(int channel, uint32_t frequency, uint32_t duty_permille)
{
    const struct pwm_dt_spec *spec = pwm_channels[channel];
    uint32_t period_ns = 1000000000U / frequency;
    uint32_t pulse_ns  = (uint64_t)period_ns * duty_permille / 1000U;

    /* pwm_set_dt sets both period and pulse width in one call, which is
     * necessary because pwm_set_pulse_dt keeps the device-tree default period
     * and would silently ignore the requested frequency. */
    int err = pwm_set_dt(spec, period_ns, pulse_ns);
    if (err) {
        LOG_ERR("PWM channel %d set failed (err %d)", channel, err);
    } else {
        LOG_INF("PWM ch%d -> %u Hz, %u.%u%% duty",
               channel, frequency, duty_permille / 10, duty_permille % 10);
    }
}

int verify_pwm_ready(void)
{
    int err = 0;
    if (!pwm_is_ready_dt(&pwm1)) { LOG_ERR("PWM1 spec not ready"); err = -ENODEV; }
    if (!pwm_is_ready_dt(&pwm2)) { LOG_ERR("PWM2 spec not ready"); err = -ENODEV; }
    if (!pwm_is_ready_dt(&pwm3)) { LOG_ERR("PWM3 spec not ready"); err = -ENODEV; }
    if (!pwm_is_ready_dt(&pwm4)) { LOG_ERR("PWM4 spec not ready"); err = -ENODEV; }
    return err;
}