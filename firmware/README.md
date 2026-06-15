# Testbench Firmware — HIL Controller (Zephyr, STM32F401 Blackpill)

> ↩ Part of [Pix-in-the-Loop](../README.md)

The **HIL Controller**: Zephyr firmware on an STM32F401 Blackpill that turns the
host's high-level test keywords into real electrical stimulus and measurement on
the DUT. The host ([Orchestrator](../Orchestrator/README.md)) drives it over a USB
CDC virtual COM port using Zephyr's interactive shell — every command lives under
the `ctrl` command tree. Writes are validated on the shell thread and **queued** to
a worker (asynchronous); reads **execute inline** and reply with the measured value,
so they are immune to USB/shell round-trip jitter.

For the runtime design (boot, async write path, SPI handshake, DMA-backed UART RX)
see the **[architecture deep-dive](docs/architecture.md)**.

## `ctrl` command tree

| Command | Args | Purpose |
| :--- | :--- | :--- |
| `gpio_set` | `<A\|B\|C> <pin 0-15> <0\|1>` | drive a digital output |
| `gpio_get` | `<A\|B\|C> <pin 0-15>` | read a digital input |
| `gpio_latency` | `<out_port> <out_pin> <in_port> <in_pin> <R\|F\|B> <timeout_ms>` | firmware-timed edge-response latency (µs) |
| `pwm_set` | `<ch 1-4> <freq_hz> <duty_permille 0-1000>` | drive a PWM output |
| `adc_sample` | `<channel>` | sample an ADC channel (raw 12-bit) |
| `spi_write` | `<ch 1-2> <hex_bytes>` | SPI master write |
| `spi_slave_wait` | `<ch 1-2> <timeout_ms> <hex_bytes>` | arm an SPI slave transceive |
| `spi_cfg` | `<ch 1-2> <freq_hz> <cpol> <cpha> <msb\|lsb>` | SPI line config |
| `spi_read` | `<ch 1-2> <timeout_ms>` | read the last SPI RX |
| `uart_send` | `<ch 1-2> <data>` | DMA UART send (ASCII) |
| `uart_send_hex` | `<ch 1-2> <hex_bytes>` | DMA UART send (raw bytes) |
| `uart_recv` | `<ch 1-2> <num_bytes> <timeout_ms>` | read from the DMA RX ring buffer |
| `uart_cfg` | `<ch 1-2> <baud> <N\|E\|O> <1\|2>` | UART line config |

Commands are registered in [`src/shell_cmds.c`](src/shell_cmds.c); the hardware
services live in [`src/*_service/`](src/) (gpio / adc / pwm / spi / uart).

## Pin allocation

The Blackpill pin map (USART, SPI, ADC, PWM, Pure-GPIO) is the
**[pin allocation matrix in the root README](../README.md)**.

## Build & flash

```bash
# from the zephyrproject root, with the Zephyr venv active; run inside firmware/
west build -b blackpill_f401cc . -p auto
west flash
```

The shell/console is the same USB CDC port the host connects to (e.g. `/dev/ttyACM0`).

## See also

- [Architecture deep-dive](docs/architecture.md) — boot, async write, SPI, DMA-UART diagrams
- [Orchestrator](../Orchestrator/README.md) — the host test runner that drives this firmware
- [DUT persona](../DUT/README.md) — the ESP32-S3 target under test
