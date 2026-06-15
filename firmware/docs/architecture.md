# Testbench Firmware Architecture

> ↩ [Testbench firmware](../README.md) · [Pix-in-the-Loop root](../../README.md)

The testbench firmware is the **HIL Controller** that runs on the STM32F401 Blackpill
(`firmware/`). The host (RobotFramework / `HILClient.py`) drives it over a USB CDC
virtual COM port using the Zephyr shell `ctrl` command tree. Each command is validated
on the shell thread and then either **queued** to a worker thread (asynchronous writes)
or **executed inline** (synchronous reads).

Two views are provided:

- **Static diagram** — the structural decomposition: layers, modules, IPC objects,
  threads, and the hardware they own.
- **Dynamic diagram** — the runtime behaviour: boot, the async write path, the SPI
  round-trip handshake, and the DMA-backed UART receive path.

---

## Static diagram (structure)

![Static code diagram](Pix-In-The-Loop-StaticCodeDiagram.svg)

---

## Dynamic diagram (behaviour)

### 1. Boot

```mermaid
sequenceDiagram
    autonumber
    participant K as Zephyr kernel
    participant M as main()
    participant SI as SYS_INIT (uart_dma_init)
    participant T as Static threads
    participant HW as Peripherals

    K->>SI: APPLICATION init — arm continuous DMA RX on USART1/2
    K->>T: start io_cmd_dispatcher (p5), spi1/2_worker (p4)
    T-->>T: each blocks on k_msgq_get(K_FOREVER)
    K->>M: main()
    M->>M: usb_enable(NULL)  (CDC ACM up)
    M->>HW: verify_*_ready() for each enabled subsystem
    alt all ready
        M-->>M: LOG "Controller initialized, shell active"
    else any device not ready
        M-->>M: LOG_ERR "Hardware init failed" (shell stays usable)
    end
```

### 2. Asynchronous write path (PWM / GPIO set / UART send / cfg)

```mermaid
sequenceDiagram
    autonumber
    actor Host
    participant SH as Shell thread (shell_cmds.c)
    participant Q as io_msgq
    participant D as io_cmd_dispatcher (p5)
    participant S as hw_*.c service
    participant HW as Peripheral

    Host->>SH: ctrl gpio_set A 5 1
    SH->>SH: parse + validate args
    SH->>Q: k_msgq_put(cmd, K_NO_WAIT)
    SH-->>Host: "Queued: GPIO A5 -> 1"  (returns immediately)
    Note over D: was blocked on k_msgq_get(K_FOREVER)
    Q-->>D: cmd delivered
    D->>S: handle_gpio_set(port, pin, val)
    S->>HW: gpio_pin_set_raw (lazy-configure dir on first use)
    Note over Host,HW: reply reaches host before the write executes →<br/>predictable shell round-trip regardless of HW timing
```

### 3. SPI round-trip — `spi_write` then `spi_read` (cross-thread handshake)

```mermaid
sequenceDiagram
    autonumber
    actor Host
    participant SH as Shell thread
    participant SEM as spi_results[ch].done sem
    participant Q as spiN_msgq
    participant W as spiN_worker (p4)
    participant HW as SPIn peripheral

    Host->>SH: ctrl spi_write 1 A5F0
    SH->>SEM: spi_arm_read(ch) → k_sem_reset (clear stale completion)
    SH->>Q: post_spi_cmd → k_msgq_put
    SH-->>Host: "Queued: SPI1 master write ..."
    Q-->>W: k_msgq_get
    W->>HW: spi_transceive (master) / spi_transceive_signal (slave)
    alt slave timeout
        W->>HW: spi_release + reset_line assert/deassert (abort DMA)
        W->>SEM: spi_publish_result(ok=false) → k_sem_give
    else success
        W->>SEM: spi_publish_result(rx,len,ok=true) → k_sem_give
    end

    Host->>SH: ctrl spi_read 1 1000
    SH->>SEM: handle_spi_read → k_sem_take(K_MSEC(timeout))
    alt completion available / arrives
        SEM-->>SH: taken → copy rx bytes
        SH-->>Host: "SPI RX: A5F0"
    else timeout
        SH-->>Host: "SPI RX: TIMEOUT"
    end
```

### 4. DMA-backed UART receive (decoupled capture)

```mermaid
sequenceDiagram
    autonumber
    actor Host
    participant SH as Shell thread
    participant CB as uart_async_cb (ISR ctx)
    participant RB as RX ring buffer
    participant DMA as UART RX DMA
    participant Peer as DUT (on USART)

    Note over CB,DMA: armed at boot by uart_dma_init (double-buffered)
    Peer-->>DMA: bytes on the wire
    DMA-->>CB: UART_RX_RDY
    CB->>RB: ring_buf_put(fresh region)
    DMA-->>CB: UART_RX_BUF_REQUEST
    CB-->>DMA: hand over alternate buffer (stay armed)

    Host->>SH: ctrl uart_recv 1 4 500
    loop until n bytes or timeout
        SH->>RB: ring_buf_get
        SH->>SH: k_msleep(2) if short
    end
    SH-->>Host: "UART DATA: ...."
    Note over Host,Peer: capture is continuous, so bytes that arrived<br/>before uart_recv ran are not lost
```

> The synchronous reads **gpio_get**, **gpio_latency** (fires a stimulus edge and times
> the response via a GPIO ISR + `latency_sem`), and **adc_sample** follow the same inline
> pattern as `spi_read`/`uart_recv`: they run entirely on the shell thread and reply with
> the measured value, so they are immune to USB/shell round-trip jitter.
