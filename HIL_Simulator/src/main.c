#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

#include "app_gpio.h"
#include "app_usb.h"

#ifdef CONFIG_APP_ADC_SERVICE
#include "app_adc.h"
#endif

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

#define CMD_BUFFER_SIZE  128
#define RESP_BUFFER_SIZE 128

static uint8_t cmd_buf[CMD_BUFFER_SIZE];
static size_t cmd_idx = 0;

/**
 * @brief Helper function to send raw string replies back over the USB line
 */
static void send_response(const char *msg)
{
    app_usb_transmit((const uint8_t *)msg, strlen(msg));
}

/* =========================================================================
   INDIVIDUAL COMPONENT SUB-HANDLERS (Isolated Command Logic)
   ========================================================================= */

static void handle_gpio_command(const char *action, int arg)
{
    char resp[RESP_BUFFER_SIZE];

    if (strncmp(action, "READ", 4) == 0) {
        int val = app_gpio_read();
        if (val >= 0) {
            snprintf(resp, sizeof(resp), "GPIO VALUE: %d\r\n", val);
            send_response(resp);
        } else {
            send_response("ERROR: GPIO_READ_FAILED\r\n");
        }
    } 
    else if (strncmp(action, "WRITE", 5) == 0) {
        if (arg == 0 || arg == 1) {
            app_gpio_write(arg);
            send_response("GPIO WRITE OK\r\n");
        } else {
            send_response("ERROR: VALUE_MUST_BE_0_OR_1\r\n");
        }
    } 
    else {
        send_response("ERROR: UNKNOWN_GPIO_ACTION\r\n");
    }
}

static void handle_adc_command(const char *action, int arg)
{
    ARG_UNUSED(arg);
    char resp[RESP_BUFFER_SIZE];

#ifdef CONFIG_APP_ADC_SERVICE
    if (strncmp(action, "SAMPLE", 6) == 0) {
        int16_t raw_val = 0;
        if (app_adc_sample_raw(&raw_val) == 0) {
            snprintf(resp, sizeof(resp), "ADC VALUE: %d\r\n", raw_val);
            send_response(resp);
        } else {
            send_response("ERROR: ADC_SAMPLE_FAILED\r\n");
        }
    } else {
        send_response("ERROR: UNKNOWN_ADC_ACTION\r\n");
    }
#else
    send_response("ERROR: ADC_SERVICE_DISABLED\r\n");
#endif
}

static void handle_pwm_command(const char *action, int arg)
{
    char resp[RESP_BUFFER_SIZE];

    if (strncmp(action, "SET", 3) == 0) {
        snprintf(resp, sizeof(resp), "PWM SET TO %d STUB OK\r\n", arg);
        send_response(resp);
    } else {
        send_response("ERROR: UNKNOWN_PWM_ACTION\r\n");
    }
}

static void handle_spi_command(const char *action, int arg)
{
    char resp[RESP_BUFFER_SIZE];

    if (strncmp(action, "BUF", 3) == 0) {
        snprintf(resp, sizeof(resp), "SPI RX: 0x00 STUB OK (TX: 0x%02X)\r\n", arg);
        send_response(resp);
    } else {
        send_response("ERROR: UNKNOWN_SPI_ACTION\r\n");
    }
}

static void handle_uart_command(const char *action, int arg)
{
    ARG_UNUSED(arg);
    if (strncmp(action, "SEND", 4) == 0) {
        send_response("UART SEND STUB OK\r\n");
    } else {
        send_response("ERROR: UNKNOWN_UART_ACTION\r\n");
    }
}

/* =========================================================================
   MAIN ROUTER: Extracts the Module and passes execution down
   ========================================================================= */
static void process_command(char *line)
{
    char module[16] = {0};
    char action[16] = {0};
    int arg = 0;

    /* Parse space-separated arguments: <MODULE> <ACTION> [ARGUMENT] */
    int parsed_count = sscanf(line, "%15s %15s %d", module, action, &arg);
    if (parsed_count < 2) {
        send_response("ERROR: INVALID_SYNTAX\r\n");
        return;
    }

    /* Route directly to the matching component function */
    if (strncmp(module, "GPIO", 4) == 0) {
        handle_gpio_command(action, arg);
    } 
    else if (strncmp(module, "ADC", 3) == 0) {
        handle_adc_command(action, arg);
    } 
    else if (strncmp(module, "PWM", 3) == 0) {
        handle_pwm_command(action, arg);
    } 
    else if (strncmp(module, "SPI", 3) == 0) {
        handle_spi_command(action, arg);
    } 
    else if (strncmp(module, "UART", 4) == 0) {
        handle_uart_command(action, arg);
    } 
    else {
        send_response("ERROR: UNKNOWN_MODULE\r\n");
    }
}

/* =========================================================================
   APPLICATION ENTRY POINT
   ========================================================================= */
int main(void)
{
    LOG_INF("Launching Clean Command-Driven HIL Simulator Workspace...");

#ifdef CONFIG_APP_GPIO_SERVICE
    app_gpio_init();
#endif

#ifdef CONFIG_APP_ADC_SERVICE
    app_adc_init();
#endif

    app_usb_init();
    send_response("\r\n=== HIL Simulator Virtual Interface Connected ===\r\n");

    uint8_t rx_byte;

    while (1) {
        int read_bytes = app_usb_receive(&rx_byte, 1, K_MSEC(100));
        
        if (read_bytes > 0) {
            app_usb_transmit(&rx_byte, 1); // Echo character back

            if (rx_byte == '\n' || rx_byte == '\r') {
                if (cmd_idx > 0) {
                    cmd_buf[cmd_idx] = '\0'; 
                    send_response("\r\n");   
                    process_command((char *)cmd_buf);
                    cmd_idx = 0;             
                }
            } 
            else {
                if (cmd_idx < (CMD_BUFFER_SIZE - 1)) {
                    cmd_buf[cmd_idx++] = rx_byte;
                } else {
                    send_response("\r\nERROR: COMMAND_BUFFER_OVERFLOW\r\n");
                    cmd_idx = 0;
                }
            }
        }
    }
    return 0;
}
