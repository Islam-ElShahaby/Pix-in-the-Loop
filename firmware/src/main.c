#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
#include "hardware.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Verify only the hardware subsystems that are enabled via Kconfig. */
static int verify_hardware_readiness(void)
{
    int err = 0;
#ifdef CONFIG_APP_PWM
    if (verify_pwm_ready()  != 0) err = -ENODEV;
#endif
#ifdef CONFIG_APP_GPIO
    if (verify_gpio_ready() != 0) err = -ENODEV;
#endif
#ifdef CONFIG_APP_SPI
    if (verify_spi_ready()  != 0) err = -ENODEV;
#endif
#ifdef CONFIG_APP_UART
    if (verify_uart_ready() != 0) err = -ENODEV;
#endif
#ifdef CONFIG_APP_ADC
    if (verify_adc_ready()  != 0) err = -ENODEV;
#endif
    return err;
}

int main(void)
{
    /* Initialize the USB Stack to activate Virtual COM interface */
    int ret = usb_enable(NULL);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB stack (err %d)", ret);
        return ret;
    }

    /* Verify core hardware components early at boot */
    if (verify_hardware_readiness() != 0) {
        LOG_ERR("FATAL: Hardware initialization failed.");
    } else {
        LOG_INF("Controller successfully initialized. USB CDC Shell is active.");
    }

    return 0;
}
