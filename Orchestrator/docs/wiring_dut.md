# Wiring Guide — Integration Scenarios (STM32 bench ↔ ESP32-S3 DUT)

> ↩ Part of [Pix-in-the-Loop](../../../README.md) · [Orchestrator](../../README.md) · [DUT persona](../../DUT/README.md)

Both sides are 3.3 V; tie a common ground. The bench (STM32F401 Blackpill) pins
are fixed by its firmware (see the Pin Allocation Matrix in the top-level
`README.md`); the DUT (ESP32-S3) pins are chosen in
[`DUT/src/dut_pins.h`](../../DUT/src/dut_pins.h) /
[`DUT/app.overlay`](../../DUT/app.overlay).

The bench Robot terminal is over the bench's USB port. The DUT console/log is on
the ESP32-S3 **USB-Serial-JTAG** ("USB" port on the DevKitC), so it never shares
pins with the command link.

## Master wiring table

| Signal | Bench pin (STM32) | Dir | DUT pin (ESP32-S3) | Notes |
| :--- | :--- | :---: | :--- | :--- |
| **Cmd link TX→RX** | USART1 TX (PA9)  | → | GPIO18 (uart1 RX) | Command & alert link |
| **Cmd link RX←TX** | USART1 RX (PA10) | ← | GPIO17 (uart1 TX) | |
| **Sensor peer TX→RX** | USART2 TX (PA2) | → | GPIO9 (uart2 RX) | Simulated telemetry |
| **Sensor peer RX←TX** | USART2 RX (PA3) | ← | GPIO8 (uart2 TX) | |
| **Thermistor (analog)** | PWM1 (PA8) | → | GPIO4 (ADC1_CH3) | *RC filter, 10 kΩ / 100 nF* |
| **Motor PWM (analog)** | ADC IN4 (PA4) | ← | GPIO14 (LEDC) | *RC filter, 10 kΩ / 100 nF* |
| **Button (wake)** | PB2 (Pure GPIO out) | → | GPIO5 | rising edge |
| **Tamper** | PB7 (Pure GPIO out) | → | GPIO6 | rising edge |
| **Fault** | PB9 (Pure GPIO in) | ← | GPIO10 | asserted on overheat |
| **Relay** | PB10 (Pure GPIO in) | ← | GPIO11 | set on tamper |
| **Heartbeat** | PB12 (Pure GPIO in) | ← | GPIO12 | 1 Hz toggle while RUN |
| **Status LED** *(optional)* | PC14 (Pure GPIO in) | ← | GPIO13 | toggled by LED command |
| **Ground** | GND | — | GND | common reference |

> The bench discrete signals use only its dedicated **Pure GPIO** pins
> (PB2, PB7, PB9, PB10, PB12, PC13–15). Do **not** use PB1 / PB14 / PB15 / PA8 —
> those are ADC / SPI2 / PWM on the bench and would contend with the DUT outputs.

## RC filters (PWM ↔ ADC)

Two analog paths each need a first-order RC low-pass (τ ≈ 1 ms):

1. **Thermistor sim** — bench PWM1 (PA8) → 10 kΩ → node → 100 nF → GND; node → DUT GPIO4.
2. **Motor readback** — DUT GPIO14 → 10 kΩ → node → 100 nF → GND; node → bench ADC IN4 (PA4).

The PWM runs fast enough (≥1 kHz) that ripple at the node is negligible, so the
ADC reads a steady voltage proportional to the duty cycle.
