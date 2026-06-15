# DUT persona — "VENT-CTRL" (Zephyr, ESP32-S3)

> ↩ Part of [Pix-in-the-Loop](../README.md) · see also [Orchestrator](../Orchestrator/README.md) · [Testbench firmware](../firmware/README.md)

A ventilation/climate-controller persona on **Zephyr** that the STM32 Blackpill
bench drives through a set of high-level **integration scenarios** (multi-
peripheral, real-world behaviours rather than isolated bus checks).

Board target: `esp32s3_devkitc/esp32s3/procpu`.

## What it does

The DUT runs a small **state machine** plus a few always-on responders:

| Behaviour | Trigger | Effect | Source |
| :--- | :--- | :--- | :--- |
| **Wake** | rising edge on Button (GPIO5) | `SLEEP`→`RUN`: heartbeat starts toggling, motor PWM → 50 % | `src/gpio_io.c` |
| **Sleep** | `SLEEP` on the command link | `RUN`→`SLEEP`: motor off, heartbeat off, and **clears** fault/relay/tamper | `src/gpio_io.c`, `src/cmd_link.c` |
| **Overheat shutdown** | thermistor ADC > 2000 mV *(while RUN)* | asserts Fault (GPIO10) + shuts the motor PWM off (latched until SLEEP) | `src/dut_main.c`, `src/adc_dut.c` |
| **Intrusion** | rising edge on Tamper (GPIO6) | increments a counter, turns on Relay (GPIO11), sends `ALERT: TAMPER DETECTED` on the command link | `src/gpio_io.c` |
| **Telemetry** | `TEMP:<n>` on the sensor peer (UART2) | sets motor PWM proportional to `n` (`n`‰×10, clamped 0–100 %) | `src/sensor_peer.c` |
| **Heartbeat** | 500 ms timer *(while RUN)* | toggles GPIO12 at ~1 Hz | `src/gpio_io.c` |
| **Liveness** | `PING` on the command link | replies `PONG` | `src/cmd_link.c` |

Boot state is `SLEEP`. Both UART links poll (`uart_poll_in`/`uart_poll_out`)
from dedicated threads.

> **Framing:** the bench delivers each UART command as one burst with **no
> terminator** (its shell strips the trailing newline), so both links frame on
> the **idle gap** after the burst (≈6 ms quiet) — an explicit `\n`/`\r` is also
> honoured if present. Don't reintroduce newline-only framing.

## Command link (UART1) — ASCII

| Command | Reply | Meaning |
| :--- | :--- | :--- |
| `PING` | `PONG` | liveness |
| `SLEEP` | `SLEEPOK` | enter SLEEP (also clears the latched alarm outputs) |
| *(other)* | `ERR` | unknown command |
| *(unsolicited)* | `ALERT: TAMPER DETECTED` | emitted when the Tamper input fires |

## Sensor peer (UART2)

`TEMP:<n>` sets the motor PWM duty: `n ≥ 100` → 100 %, `n ≤ 0` → 0 %, otherwise
`n × 10` permille (e.g. `TEMP:50` → 500 ‰ = 50 % → ≈1.65 V after the bench RC).

## Wiring (DUT ↔ bench)

Both sides are 3.3 V; tie a common ground. Bench pins are fixed by its firmware;
DUT pins are set in [`src/dut_pins.h`](src/dut_pins.h) / [`app.overlay`](app.overlay).
Full harness incl. RC filters: [`../Orchestrator/wiring_dut.md`](../Orchestrator/wiring_dut.md).

| Signal | DUT pin | Dir | Bench pin |
| :--- | :--- | :---: | :--- |
| Command link TX / RX | GPIO17 / GPIO18 | ↔ | USART1 PA10 (RX) / PA9 (TX) |
| Sensor peer TX / RX | GPIO8 / GPIO9 | ↔ | USART2 PA3 (RX) / PA2 (TX) |
| Thermistor (ADC1_CH3) | GPIO4 | ← | PWM1 PA8 *(via RC 10 kΩ/100 nF)* |
| Motor PWM (LEDC) | GPIO14 | → | ADC IN4 PA4 *(via RC 10 kΩ/100 nF)* |
| Button (wake) | GPIO5 | ← | PB2 |
| Tamper | GPIO6 | ← | PB7 |
| Fault | GPIO10 | → | PB9 |
| Relay | GPIO11 | → | PB10 |
| Heartbeat | GPIO12 | → | PB12 |
| Status LED | GPIO13 | → | PC14 |
| Ground | GND | — | GND |
