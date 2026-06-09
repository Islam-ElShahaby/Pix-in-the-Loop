# Core application sources (always compiled)
target_sources(app PRIVATE
    main.c
    threads.c
    shell_cmds.c
)

# Optional hardware subsystems (toggled via Kconfig)
if(CONFIG_APP_GPIO)
    add_subdirectory(gpio_service)
endif()

if(CONFIG_APP_PWM)
    add_subdirectory(pwm_service)
endif()

if(CONFIG_APP_SPI)
    add_subdirectory(spi_service)
endif()

if(CONFIG_APP_UART)
    add_subdirectory(uart_service)
endif()
