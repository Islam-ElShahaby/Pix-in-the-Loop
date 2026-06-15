#pragma once
#include <stdbool.h>

typedef enum {
    SYSTEM_STATE_SLEEP = 0,
    SYSTEM_STATE_RUN = 1
} system_state_t;

void gpio_io_start(void);
void gpio_set_fault(bool active);
void gpio_set_relay(bool active);
void gpio_set_system_state(system_state_t state);
system_state_t gpio_get_system_state(void);
int gpio_get_tamper_count(void);
