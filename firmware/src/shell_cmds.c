/*
 * shell_cmds.c — Zephyr shell command tree for the `ctrl` command group.
 *
 * Each sub-command validates its arguments on the shell thread, then either
 * posts a message to io_msgq (GPIO, PWM, UART) or to a per-channel SPI queue
 * and returns immediately. The shell response ("Queued: …") reaches the host
 * before the hardware operation executes, which keeps shell round-trip latency
 * predictable regardless of how long the hardware operation takes.
 *
 * The only exception is uart_recv and gpio_get, which read hardware state
 * synchronously and reply with the result directly on the shell thread.
 *
 * Command surface:
 *   ctrl pwm_set       <ch 1-4> <freq_hz> <duty_permille 0-1000>
 *   ctrl gpio_set      <port A|B|C> <pin 0-15> <0|1>
 *   ctrl gpio_get      <port A|B|C> <pin 0-15>
 *   ctrl spi_write     <ch 1-2> <hex_bytes>
 *   ctrl spi_slave_wait <ch 1-2> <timeout_ms> <hex_bytes>
 *   ctrl uart_send     <ch 1-2> <data>
 *   ctrl uart_recv     <ch 1-2> <num_bytes> <timeout_ms>
 *   ctrl adc_sample    
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>
#include "threads.h"
#include "hardware.h"


/* Helper: parse a signed decimal string into *val.
 * Returns -EINVAL if the string is empty, non-numeric, or has trailing chars. */
static int parse_long(const char *str, long *val)
{
    char *endptr;
    long tmp = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') {
        return -EINVAL;
    }
    *val = tmp;
    return 0;
}

/* Helper: parse an unsigned decimal string into *val. */
static int parse_ulong(const char *str, unsigned long *val)
{
    char *endptr;
    unsigned long tmp = strtoul(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') {
        return -EINVAL;
    }
    *val = tmp;
    return 0;
}

#ifdef CONFIG_APP_PWM
/* =========================================================================
   PWM — ctrl pwm_set <ch 1-4> <freq_hz> <duty_permille 0-1000>
   Queues a PWM configuration update. The hardware applies it asynchronously.
   ========================================================================= */
static int cmd_pwm_set(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_error(sh, "Usage: ctrl pwm_set <channel 1-4> <frequency_hz> <duty_permille 0-1000>");
        return -EINVAL;
    }

    long channel_val;
    unsigned long frequency_val;
    unsigned long duty_val;

    if (parse_long(argv[1], &channel_val) != 0 || channel_val < 1 || channel_val > 4) {
        shell_error(sh, "Error: Invalid channel '%s'. Must be 1-4.", argv[1]);
        return -EINVAL;
    }
    if (parse_ulong(argv[2], &frequency_val) != 0 || frequency_val == 0) {
        shell_error(sh, "Error: Invalid frequency '%s'. Must be > 0.", argv[2]);
        return -EINVAL;
    }
    if (parse_ulong(argv[3], &duty_val) != 0 || duty_val > 1000) {
        shell_error(sh, "Error: Invalid duty cycle '%s'. Must be 0-1000 permille.", argv[3]);
        return -EINVAL;
    }

    struct io_cmd cmd = {
        .type = CMD_PWM_SET,
        .params.pwm = {
            .channel      = (int)channel_val,
            .frequency    = (uint32_t)frequency_val,
            .duty_permille = (uint32_t)duty_val
        }
    };

    int err = k_msgq_put(&io_msgq, &cmd, K_NO_WAIT);
    if (err) {
        shell_error(sh, "Error: Command queue full");
        return err;
    }

    shell_print(sh, "Queued: PWM ch%d -> %u Hz, %u.%u%% duty",
                (int)channel_val, (uint32_t)frequency_val,
                (uint32_t)duty_val / 10, (uint32_t)duty_val % 10);
    return 0;
}
#endif /* CONFIG_APP_PWM */

#ifdef CONFIG_APP_GPIO
/* =========================================================================
   GPIO SET — ctrl gpio_set <port A|B|C> <pin 0-15> <0|1>
   argv[0] is the port letter, supplied by the subcommand dispatch mechanism.
   Queues a pin drive; direction is configured lazily by hw_gpio.c on first use.
   ========================================================================= */
static int cmd_gpio_set(const struct shell *sh, size_t argc, char **argv)
{
    char port_char = argv[0][0];

    if (argc < 3) {
        shell_error(sh, "Usage: ctrl gpio_set %s <pin 0-15> <value 0|1>", argv[0]);
        return -EINVAL;
    }

    long pin_val;
    long value_val;

    if (parse_long(argv[1], &pin_val) != 0 || pin_val < 0 || pin_val > 15) {
        shell_error(sh, "Error: Invalid pin '%s'. Must be 0-15.", argv[1]);
        return -EINVAL;
    }
    if (parse_long(argv[2], &value_val) != 0 || (value_val != 0 && value_val != 1)) {
        shell_error(sh, "Error: Invalid value '%s'. Must be 0 or 1.", argv[2]);
        return -EINVAL;
    }

    struct io_cmd cmd = {
        .type = CMD_GPIO_SET,
        .params.gpio = {
            .port  = port_char,
            .pin   = (int)pin_val,
            .value = (int)value_val
        }
    };

    int err = k_msgq_put(&io_msgq, &cmd, K_NO_WAIT);
    if (err) {
        shell_error(sh, "Error: Command queue full");
        return err;
    }

    shell_print(sh, "Queued: GPIO %c%d -> %d", port_char, (int)pin_val, (int)value_val);
    return 0;
}

/* =========================================================================
   GPIO GET — ctrl gpio_get <port A|B|C> <pin 0-15>
   Synchronous read: executes on the shell thread and replies immediately.
   ========================================================================= */
static int cmd_gpio_get(const struct shell *sh, size_t argc, char **argv)
{
    char port_char = argv[0][0];

    if (argc < 2) {
        shell_error(sh, "Usage: ctrl gpio_get %s <pin 0-15>", argv[0]);
        return -EINVAL;
    }

    long pin_val;
    if (parse_long(argv[1], &pin_val) != 0 || pin_val < 0 || pin_val > 15) {
        shell_error(sh, "Error: Invalid pin '%s'. Must be 0-15.", argv[1]);
        return -EINVAL;
    }

    int val = handle_gpio_get(port_char, (int)pin_val);
    if (val < 0) {
        shell_error(sh, "ERROR: GPIO_READ_FAILED (err %d)", val);
        return val;
    }

    shell_print(sh, "GPIO VALUE: %d", val);
    return 0;
}
#endif /* CONFIG_APP_GPIO */

#ifdef CONFIG_APP_SPI
/* =========================================================================
   SPI helpers
   ========================================================================= */

/* Factored out because both master-write and slave-wait share identical
 * enqueue logic — the only difference is the struct spi_cmd they carry. */
static int post_spi_cmd(const struct shell *sh, int channel, struct spi_cmd *cmd)
{
    struct k_msgq *queue = (channel == 1) ? &spi1_msgq : &spi2_msgq;
    int err = k_msgq_put(queue, cmd, K_NO_WAIT);
    if (err) {
        shell_error(sh, "Error: SPI%d queue full", channel);
    }
    return err;
}

/* =========================================================================
   SPI WRITE — ctrl spi_write <ch 1-2> <hex_bytes>
   Queues a master-mode transceive. hex_bytes must be an even-length hex
   string with no separators (e.g. "A5F0" sends 0xA5 followed by 0xF0).
   ========================================================================= */
static int cmd_spi_write(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_error(sh, "Usage: ctrl spi_write <channel 1-2> <hex_bytes e.g. A0B5>");
        return -EINVAL;
    }

    long channel_val;
    if (parse_long(argv[1], &channel_val) != 0 || channel_val < 1 || channel_val > 2) {
        shell_error(sh, "Error: Invalid SPI channel '%s'. Must be 1 or 2.", argv[1]);
        return -EINVAL;
    }

    char *hex_str = argv[2];
    size_t hex_len = strlen(hex_str);
    if (hex_len == 0 || hex_len % 2 != 0) {
        shell_error(sh, "Error: Hex data must be a non-empty even-length string.");
        return -EINVAL;
    }

    struct spi_cmd cmd = {
        .is_slave  = false,
        .timeout_ms = 0,
        .len       = hex_len / 2
    };

    if (cmd.len > SPI_MAX_PAYLOAD_SIZE) {
        shell_error(sh, "Error: Payload exceeds max %d bytes.", SPI_MAX_PAYLOAD_SIZE);
        return -EINVAL;
    }

    for (size_t i = 0; i < cmd.len; i++) {
        char byte_chars[3] = { hex_str[2 * i], hex_str[2 * i + 1], '\0' };
        char *endptr;
        unsigned long val = strtoul(byte_chars, &endptr, 16);
        if (endptr == byte_chars || *endptr != '\0') {
            shell_error(sh, "Error: Invalid hex at byte %zu.", i);
            return -EINVAL;
        }
        cmd.tx_buf[i] = (uint8_t)val;
    }

    int channel = (int)channel_val;
    if (post_spi_cmd(sh, channel, &cmd) == 0) {
        shell_print(sh, "Queued: SPI%d master write '%s' (%u bytes)", channel, hex_str, cmd.len);
    }
    return 0;
}

/* =========================================================================
   SPI SLAVE WAIT — ctrl spi_slave_wait <ch 1-2> <timeout_ms> <hex_bytes>
   Arms the channel as a slave and simultaneously prepares tx_buf as the
   data to clock out during the master-driven transfer. Blocks the SPI worker
   thread until data is received or timeout_ms elapses.
   ========================================================================= */
static int cmd_spi_slave_wait(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_error(sh, "Usage: ctrl spi_slave_wait <channel 1-2> <timeout_ms> <hex_bytes>");
        return -EINVAL;
    }

    long channel_val;
    if (parse_long(argv[1], &channel_val) != 0 || channel_val < 1 || channel_val > 2) {
        shell_error(sh, "Error: Invalid SPI channel '%s'. Must be 1 or 2.", argv[1]);
        return -EINVAL;
    }

    long timeout_val;
    if (parse_long(argv[2], &timeout_val) != 0 || timeout_val < 0) {
        shell_error(sh, "Error: Invalid timeout '%s'. Must be >= 0.", argv[2]);
        return -EINVAL;
    }

    char *hex_str = argv[3];
    size_t hex_len = strlen(hex_str);
    if (hex_len == 0 || hex_len % 2 != 0) {
        shell_error(sh, "Error: Hex data must be a non-empty even-length string.");
        return -EINVAL;
    }

    struct spi_cmd cmd = {
        .is_slave   = true,
        .timeout_ms = (int)timeout_val,
        .len        = hex_len / 2
    };

    if (cmd.len > SPI_MAX_PAYLOAD_SIZE) {
        shell_error(sh, "Error: Payload exceeds max %d bytes.", SPI_MAX_PAYLOAD_SIZE);
        return -EINVAL;
    }

    for (size_t i = 0; i < cmd.len; i++) {
        char byte_chars[3] = { hex_str[2 * i], hex_str[2 * i + 1], '\0' };
        char *endptr;
        unsigned long val = strtoul(byte_chars, &endptr, 16);
        if (endptr == byte_chars || *endptr != '\0') {
            shell_error(sh, "Error: Invalid hex at byte %zu.", i);
            return -EINVAL;
        }
        cmd.tx_buf[i] = (uint8_t)val;
    }

    int channel = (int)channel_val;
    if (post_spi_cmd(sh, channel, &cmd) == 0) {
        shell_print(sh, "Queued: SPI%d slave wait %d ms '%s' (%u bytes)",
                    channel, cmd.timeout_ms, hex_str, cmd.len);
    }
    return 0;
}
#endif /* CONFIG_APP_SPI */

#ifdef CONFIG_APP_UART
/* =========================================================================
   UART SEND — ctrl uart_send <ch 1-2> <data>
   Queues a DMA TX on the specified USART. The dispatcher thread blocks until
   the DMA transfer completes, but the shell thread returns immediately.
   ========================================================================= */
static int cmd_uart_send(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_error(sh, "Usage: ctrl uart_send <channel 1-2> <data>");
        return -EINVAL;
    }

    long channel_val;
    if (parse_long(argv[1], &channel_val) != 0 || channel_val < 1 || channel_val > 2) {
        shell_error(sh, "Error: Invalid UART channel '%s'. Must be 1 or 2.", argv[1]);
        return -EINVAL;
    }

    const char *data = argv[2];
    size_t len = strlen(data);
    if (len == 0 || len >= UART_MAX_PAYLOAD_SIZE) {
        shell_error(sh, "Error: Data must be 1-%d chars.", UART_MAX_PAYLOAD_SIZE - 1);
        return -EINVAL;
    }

    struct io_cmd cmd = {
        .type = CMD_UART_SEND,
        .params.uart = {
            .channel = (int)channel_val,
            .len     = len
        }
    };
    memcpy(cmd.params.uart.data, data, len);

    int err = k_msgq_put(&io_msgq, &cmd, K_NO_WAIT);
    if (err) {
        shell_error(sh, "Error: Command queue full");
        return err;
    }

    shell_print(sh, "Queued: UART%d send '%s' (%u bytes)", (int)channel_val, data, (uint32_t)len);
    return 0;
}

/* =========================================================================
   UART RECV — ctrl uart_recv <ch 1-2> <num_bytes> <timeout_ms>
   Synchronous read from the DMA RX ring buffer. Runs on the shell thread.
   Blocks until num_bytes are available or timeout_ms elapses, then prints
   whatever was collected. The ring buffer is filled continuously by the
   DMA RX callback, so bytes sent by a peer before this command runs are
   not lost.
   ========================================================================= */
static int cmd_uart_recv(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_error(sh, "Usage: ctrl uart_recv <channel 1-2> <num_bytes> <timeout_ms>");
        return -EINVAL;
    }

    long channel_val;
    long nbytes_val;
    long timeout_val;

    if (parse_long(argv[1], &channel_val) != 0 || channel_val < 1 || channel_val > 2) {
        shell_error(sh, "Error: Invalid UART channel '%s'. Must be 1 or 2.", argv[1]);
        return -EINVAL;
    }
    if (parse_long(argv[2], &nbytes_val) != 0 || nbytes_val < 1 || nbytes_val >= UART_MAX_PAYLOAD_SIZE) {
        shell_error(sh, "Error: Invalid byte count '%s'. Must be 1-%d.", argv[2], UART_MAX_PAYLOAD_SIZE - 1);
        return -EINVAL;
    }
    if (parse_long(argv[3], &timeout_val) != 0 || timeout_val < 0) {
        shell_error(sh, "Error: Invalid timeout '%s'. Must be >= 0.", argv[3]);
        return -EINVAL;
    }

    uint8_t buf[UART_MAX_PAYLOAD_SIZE];
    int n = handle_uart_recv((int)channel_val, buf, (size_t)nbytes_val, (int)timeout_val);
    if (n < 0) {
        shell_error(sh, "ERROR: UART_READ_FAILED (err %d)", n);
        return n;
    }

    buf[n] = '\0';
    shell_print(sh, "UART DATA: %s", buf);
    return 0;
}
#endif /* CONFIG_APP_UART */

#ifdef CONFIG_APP_ADC
/* =========================================================================
   ADC SAMPLE — ctrl adc_sample <channel 0|1|4|5|6|7|8|9>
   Synchronous read: executes on the shell thread and replies immediately.
   ========================================================================= */
static int cmd_adc_sample(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 1) {
        shell_error(sh, "Usage: ctrl adc_sample <channel>");
        return -EINVAL;
    }

    long chnl_val;

    if (parse_long(argv[0], &chnl_val) != 0) {
        shell_error(sh, "Error: Invalid channel '%s'", argv[0]);
        return -EINVAL;
    }

    if (chnl_val < 0 || chnl_val > 9 || chnl_val == 2 || chnl_val == 3) {
        shell_error(sh, "Error: Invalid channel '%d' (valid: 0,1,4-9)", (int)chnl_val);
        return -EINVAL;
    }

    int val = handle_adc_sample_channel((int)chnl_val);
    if (val < 0) {
        shell_error(sh, "ADC sample failed (err %d)", val);
        return val;
    }

    shell_print(sh, "ADC CH%d => %d", (int)chnl_val, val);
    return 0;
}
#endif /* CONFIG_APP_ADC */

/* =========================================================================
   Subcommand trees
   ========================================================================= */
#ifdef CONFIG_APP_GPIO
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_set_ports,
    SHELL_CMD_ARG(A, NULL, "Port A", cmd_gpio_set, 3, 0),
    SHELL_CMD_ARG(B, NULL, "Port B", cmd_gpio_set, 3, 0),
    SHELL_CMD_ARG(C, NULL, "Port C", cmd_gpio_set, 3, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_get_ports,
    SHELL_CMD_ARG(A, NULL, "Port A", cmd_gpio_get, 2, 0),
    SHELL_CMD_ARG(B, NULL, "Port B", cmd_gpio_get, 2, 0),
    SHELL_CMD_ARG(C, NULL, "Port C", cmd_gpio_get, 2, 0),
    SHELL_SUBCMD_SET_END
);
#endif /* CONFIG_APP_GPIO */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ctrl,
#ifdef CONFIG_APP_PWM
    SHELL_CMD_ARG(pwm_set,        NULL,                "Set PWM: <ch 1-4> <freq_hz> <duty_permille 0-1000>",            cmd_pwm_set,        4, 0),
#endif
#ifdef CONFIG_APP_GPIO
    SHELL_CMD_ARG(gpio_set,       &sub_gpio_set_ports, "Set GPIO output: <port A|B|C> <pin 0-15> <0|1>",                NULL,               3, 0),
    SHELL_CMD_ARG(gpio_get,       &sub_gpio_get_ports, "Read GPIO input: <port A|B|C> <pin 0-15>",                      NULL,               2, 0),
#endif
#ifdef CONFIG_APP_SPI
    SHELL_CMD_ARG(spi_write,      NULL,                "SPI master write: <ch 1-2> <hex_bytes>",                        cmd_spi_write,      3, 0),
    SHELL_CMD_ARG(spi_slave_wait, NULL,                "SPI slave transceive: <ch 1-2> <timeout_ms> <hex_bytes>",       cmd_spi_slave_wait, 4, 0),
#endif
#ifdef CONFIG_APP_UART
    SHELL_CMD_ARG(uart_send,      NULL,                "UART DMA send: <ch 1-2> <data>",                                cmd_uart_send,      3, 0),
    SHELL_CMD_ARG(uart_recv,      NULL,                "UART DMA receive: <ch 1-2> <num_bytes> <timeout_ms>",           cmd_uart_recv,      4, 0),
#endif
#ifdef CONFIG_APP_ADC
    SHELL_CMD_ARG(adc_sample,     NULL,                 "ADC sample: <channel>",                                        cmd_adc_sample,     2, 0),
#endif
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ctrl, &sub_ctrl, "HIL hardware control commands", NULL);
