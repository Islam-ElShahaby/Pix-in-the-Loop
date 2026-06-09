target_sources(app PRIVATE main.c)

if(CONFIG_APP_GPIO)
    add_subdirectory(gpio_service)
endif()

if(CONFIG_APP_UART)
    add_subdirectory(uart_service)
endif()

if(CONFIG_APP_SPI)
    add_subdirectory(spi_service)
endif()

if(CONFIG_APP_ADC)
    add_subdirectory(adc_service)
endif()

if(CONFIG_APP_PWM)
    add_subdirectory(pwm_service)
endif()

if(CONFIG_APP_USB_SERVICE)
    add_subdirectory(usb_service)
endif()