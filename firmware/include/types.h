/*
 * types.h — Shared IPC types for the HIL controller firmware.
 *
 * This is the single source of truth for every command type that can travel
 * from the shell layer to the hardware workers. The shell thread produces
 * messages; the io_cmd_dispatcher and per-channel SPI workers consume them.
 * Nothing outside this file should define its own command representation.
 */

#ifndef CONTROLLER_TYPES_H
#define CONTROLLER_TYPES_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/*
 * Payload size ceilings.
 *
 * SPI_MAX_PAYLOAD_SIZE: largest single SPI transfer the DMA buffers will hold.
 * UART_MAX_PAYLOAD_SIZE: largest single ASCII payload accepted by uart_send /
 *   returned by uart_recv. Kept well under a typical shell line limit so the
 *   entire command fits in one shell token with room for the verb and channel.
 */
#define SPI_MAX_PAYLOAD_SIZE  32
#define UART_MAX_PAYLOAD_SIZE 64

/*
 * Command types dispatched through io_msgq.
 *
 * CMD_PWM_SET  — handled by the io_cmd_dispatcher; calls handle_pwm_set().
 * CMD_GPIO_SET — handled by the io_cmd_dispatcher; calls handle_gpio_set().
 * CMD_UART_SEND — handled by the io_cmd_dispatcher; calls handle_uart_send().
 * CMD_UART_CONFIG — handled by the io_cmd_dispatcher; calls handle_uart_config().
 *
 * SPI commands travel on dedicated per-channel queues (spi1_msgq / spi2_msgq)
 * and are NOT represented here because SPI transfers can block for tens of
 * milliseconds; keeping them off io_msgq prevents them from stalling GPIO/PWM.
 */
enum cmd_type {
    CMD_PWM_SET,
    CMD_GPIO_SET,
    CMD_UART_SEND,
    CMD_UART_CONFIG
};

/*
 * Generic command message posted to io_msgq.
 *
 * Exactly one union member is valid at a time, selected by `type`. The shell
 * command handlers populate the appropriate member before calling k_msgq_put.
 */
struct io_cmd {
    enum cmd_type type;
    union {
        struct {
            int      channel;        /* 1–4 */
            uint32_t frequency;      /* Hz */
            uint32_t duty_permille;  /* 0–1000 (parts per thousand) */
        } pwm;

        struct {
            char port;   /* 'A', 'B', or 'C' */
            int  pin;    /* 0–15 */
            int  value;  /* 0 or 1 */
        } gpio;

        struct {
            int    channel;                    /* 1 or 2 */
            char   data[UART_MAX_PAYLOAD_SIZE]; /* ASCII payload, not NUL-terminated in transit */
            size_t len;                        /* number of valid bytes in data[] */
        } uart;

        struct {
            int      channel;    /* 1 or 2 */
            uint32_t baudrate;   /* bits per second, e.g. 9600 / 115200 */
            char     parity;     /* 'N' (none), 'E' (even), or 'O' (odd) */
            int      stop_bits;  /* 1 or 2 */
        } uart_cfg;
    } params;
};

/*
 * SPI command message posted to spi1_msgq or spi2_msgq.
 *
 * is_slave == false: master transceive (blocking, synchronous).
 * is_slave == true:  arm as slave for up to timeout_ms before timing out.
 */
struct spi_cmd {
    bool    is_slave;
    int     timeout_ms;
    uint8_t tx_buf[SPI_MAX_PAYLOAD_SIZE];
    size_t  len;
};

#endif /* CONTROLLER_TYPES_H */
