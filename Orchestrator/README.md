# Orchestrator — Host Test Runner (Robot Framework)

> ↩ Part of [Pix-in-the-Loop](../README.md)

The host side of the platform. **Robot Framework** runs keyword-driven `.robot`
suites; [`HILClient.py`](HILClient.py) translates each keyword into an ASCII `ctrl`
command and ships it to the [testbench firmware](../firmware/README.md) over the USB
CDC serial port (`/dev/ttyACM0`). Results are emitted as color-coded HTML.

## Run

```bash
cd Orchestrator
PYTHONPATH=. robot --outputdir results IntegrationScenarios.robot
```

Produces `results/report.html`, `results/log.html`, and `results/output.xml`.

## Suites

| Suite | What it exercises |
| :--- | :--- |
| [`IntegrationScenarios.robot`](IntegrationScenarios.robot) | End-to-end scenarios against the ESP32-S3 DUT — overheat shutdown, intrusion→relay, telemetry→PWM, and the boot/run/sleep state machine, plus a PING liveness check. Requires the [DUT](../DUT/README.md) wired per [`wiring_dut.md`](wiring_dut.md). |
| [`TestCases.robot`](TestCases.robot) | Bench-only self-test via loopback (USART1↔USART2 crossover, SPI1↔SPI2, fixed GPIO/PWM). No DUT required. |

## Keyword library (`HILClient.py` + `services/`)

[`HILClient.py`](HILClient.py) is the Robot library; each domain is implemented in
[`services/`](services):

| Group | Keywords | Source |
| :--- | :--- | :--- |
| Session | `Initialize Hil System`, `Terminate Hil System` | [`services/usb_serial.py`](services/usb_serial.py) |
| GPIO | `Write Gpio`, `Read Gpio` | [`services/test_gpio.py`](services/test_gpio.py) |
| ADC | `Sample Adc Channel` | [`services/test_adc.py`](services/test_adc.py) |
| PWM | `Set Pwm Duty Cycle` | [`services/test_pwm.py`](services/test_pwm.py) |
| SPI | `Spi Write`, `Spi Slave Wait`, `Spi Read Result`, `Spi Configure` | [`services/test_spi.py`](services/test_spi.py) |
| UART | `Uart Send Data`, `Uart Receive Data`, `Uart Send Hex`, `Uart Set Config` | [`services/test_uart.py`](services/test_uart.py) |
| Latency | `Measure Gpio Latency`, `Measure Gpio Latency Us` | [`services/test_latency.py`](services/test_latency.py) |

> **Gotcha:** `Uart Send Data` reaches the DUT **without a trailing terminator** —
> the bench shell strips the newline and tokenizes the payload. DUT-side parsers
> must frame on the idle gap after the burst, not on `\n`. Use `Uart Send Hex` to
> transmit raw bytes (e.g. an explicit `0A`).

## See also

- [Wiring guide](docs/wiring_dut.md) — DUT ↔ bench harness and RC filters
- [DUT persona](../DUT/README.md) — what the integration scenarios talk to
- [Testbench firmware](../firmware/README.md) — the `ctrl` shell these keywords drive
