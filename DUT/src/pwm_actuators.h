#pragma once
#include <stdbool.h>
#include <stdint.h>

void pwm_actuators_start(void);
bool pwm_actuator_set(uint32_t duty_permille);
bool pwm_actuator_off(void);
