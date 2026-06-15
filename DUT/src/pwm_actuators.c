#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include "pwm_actuators.h"

LOG_MODULE_REGISTER(pwm_act, LOG_LEVEL_INF);

static const struct pwm_dt_spec act = PWM_DT_SPEC_GET(DT_ALIAS(pwm_motor));

void pwm_actuators_start(void)
{
    if (!pwm_is_ready_dt(&act)) {
        LOG_ERR("PWM motor not ready");
        return;
    }
    pwm_actuator_off();
    LOG_INF("PWM ready");
}

bool pwm_actuator_set(uint32_t duty_permille)
{
    if (duty_permille > 1000) return false;
    uint32_t period_ns = 100000; // 10 kHz
    uint32_t pulse_ns = (period_ns / 1000) * duty_permille;
    int err = pwm_set_dt(&act, period_ns, pulse_ns);
    return err == 0;
}

bool pwm_actuator_off(void)
{
    return pwm_actuator_set(0);
}
