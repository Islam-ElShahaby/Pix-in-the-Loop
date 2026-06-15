#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "gpio_io.h"
#include "pwm_actuators.h"
#include "adc_dut.h"
#include "sensor_peer.h"
#include "cmd_link.h"

LOG_MODULE_REGISTER(dut_main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("DUT Integration Persona starting");

    gpio_io_start();
    pwm_actuators_start();
    adc_dut_start();
    sensor_peer_start();
    cmd_link_start();

    LOG_INF("DUT ready - running state machine");
    
    // Simple main loop to check ADC (Scenario 1)
    while (1) {
        if (gpio_get_system_state() == SYSTEM_STATE_RUN) {
            uint16_t mv = adc_dut_read_mv();
            if (mv > 2000) { // Overheating threshold! (e.g. 2.0V)
                LOG_WRN("Overheating detected! ADC=%u mV", mv);
                gpio_set_fault(true);
                pwm_actuator_off();
                // Stay in fault state until reset (simplification for test)
            }
        }
        k_sleep(K_MSEC(100));
    }
    return 0;
}
