#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>
#include "threads.h"
#include "hardware.h"


/* Helper: parse signed long */
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

/* Helper: parse unsigned long */
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
   PWM
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
   GPIO SET  (argv[0] = port letter from subcommand)
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
   GPIO GET  (argv[0] = port letter from subcommand) — synchronous read
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
   SPI
   ========================================================================= */
static int post_spi_cmd(const struct shell *sh, int channel, struct spi_cmd *cmd)
{
    struct k_msgq *queue = (channel == 1) ? &spi1_msgq : &spi2_msgq;
    int err = k_msgq_put(queue, cmd, K_NO_WAIT);
    if (err) {
        shell_error(sh, "Error: SPI%d queue full", channel);
    }
    return err;
}

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
    SHELL_CMD_ARG(pwm_set,       NULL,               "Set PWM: <ch 1-4> <freq_hz> <duty_permille 0-1000>",            cmd_pwm_set,       4, 0),
#endif
#ifdef CONFIG_APP_GPIO
    SHELL_CMD_ARG(gpio_set,      &sub_gpio_set_ports, "Set GPIO output: <port A|B|C> <pin 0-15> <0|1>",               NULL,              3, 0),
    SHELL_CMD_ARG(gpio_get,      &sub_gpio_get_ports, "Read GPIO input: <port A|B|C> <pin 0-15>",                     NULL,              2, 0),
#endif
#ifdef CONFIG_APP_SPI
    SHELL_CMD_ARG(spi_write,     NULL,               "SPI master write: <channel 1-2> <hex_bytes>",                   cmd_spi_write,     3, 0),
    SHELL_CMD_ARG(spi_slave_wait,NULL,               "SPI slave transceive: <channel 1-2> <timeout_ms> <hex_bytes>",  cmd_spi_slave_wait, 4, 0),
#endif
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ctrl, &sub_ctrl, "HIL hardware control commands", NULL);
