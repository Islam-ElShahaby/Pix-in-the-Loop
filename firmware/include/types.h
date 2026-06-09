#ifndef CONTROLLER_TYPES_H
#define CONTROLLER_TYPES_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/* Defines */
#define SPI_MAX_PAYLOAD_SIZE 32

/* Command Types */
enum cmd_type {
    CMD_PWM_SET,
    CMD_GPIO_SET
};

struct io_cmd {
    enum cmd_type type;
    union {
        struct {
            int channel;
            uint32_t frequency;
            uint32_t duty_permille;
        } pwm;

        struct {
            char port;
            int pin;
            int value;
        } gpio;
    } params;
};

struct spi_cmd {
    bool is_slave;
    int timeout_ms;
    uint8_t tx_buf[SPI_MAX_PAYLOAD_SIZE];
    size_t len;
};

#endif /* CONTROLLER_TYPES_H */