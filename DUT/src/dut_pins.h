#pragma once

#include <zephyr/drivers/gpio.h>

/*
 * DUT (ESP32-S3) pin map for the Integration Scenarios persona.
 *
 * All signals live on the gpio0 bank (GPIO0-21). Strapping pins (GPIO0/3/45/46),
 * the SPI flash/PSRAM pins (GPIO26-37) and the USB-Serial-JTAG pins (GPIO19/20,
 * used for the console) are intentionally avoided.
 *
 * Bench side is the STM32 Blackpill testbench (fixed pins, see ../../firmware
 * and Orchestrator/wiring_dut.md):
 *   uart1  <-> USART1 (PA9 TX / PA10 RX)
 *   uart2  <-> USART2 (PA2 TX / PA3 RX)
 *   ADC in <-  bench PWM1 (PA8) via RC
 *   PWM out -> bench ADC IN4 (PA4) via RC
 *   discrete GPIO <-> bench "Pure GPIO" pins (PB2/PB7/PB9/PB10/PB12/PC14)
 */

/* --- UART command link (uart1) --- */
#define GPIO_UART_CMD_TX    17  /* -> bench USART1 RX (PA10) */
#define GPIO_UART_CMD_RX    18  /* <- bench USART1 TX (PA9)  */

/* --- UART sensor peer (uart2) --- */
#define GPIO_UART_PEER_TX   8   /* -> bench USART2 RX (PA3) */
#define GPIO_UART_PEER_RX   9   /* <- bench USART2 TX (PA2) */

/* --- Inputs from bench --- */
#define GPIO_IN_BUTTON      5   /* <- bench PB2 */
#define GPIO_IN_TAMPER      6   /* <- bench PB7 */

/* --- Analog / PWM (managed by their Zephyr drivers) --- */
#define GPIO_ADC_THERMISTOR 4   /* GPIO4 = ADC1 channel 3; <- bench PWM1 (PA8) via RC */
#define GPIO_PWM_MOTOR      14  /* LEDC CH0; -> bench ADC IN4 (PA4) via RC */

/* --- Outputs to bench --- */
#define GPIO_OUT_FAULT      10  /* -> bench PB9  */
#define GPIO_OUT_RELAY      11  /* -> bench PB10 */
#define GPIO_OUT_HEARTBEAT  12  /* -> bench PB12 */
#define GPIO_OUT_STATUS_LED 13  /* -> bench PC14 (optional) */
