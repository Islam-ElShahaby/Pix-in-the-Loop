"""
test_uart — UART service for the HIL orchestrator.

Wraps the `ctrl uart_send` and `ctrl uart_recv` shell commands exposed by the
Zephyr firmware. Both TX and RX are DMA-backed: TX fires a DMA transfer and
blocks the firmware dispatcher until it completes; RX is captured continuously
into a per-channel ring buffer so bytes are not lost between commands.
"""

from services.usb_serial import USB_Serial


class UARTTester:
    """Transmit and receive serial data via the HIL shell interface.

    Valid channels:
      1 = USART1 (TX PA9, RX PA10)
      2 = USART2 (TX PA2, RX PA3)

    Both channels default to 115200 baud 8N1 (from the overlay) and can be
    changed at runtime with set_config().
    """

    def __init__(self, client: USB_Serial):
        self.client = client

    def send_data(self, channel: int, data: str) -> str:
        """Transmit an ASCII string over a DUT-facing USART channel.

        channel: 1 or 2
        data:    ASCII string, 1–63 characters

        Returns the firmware acknowledgement, e.g.
        "Queued: UART1 send 'HELLO' (5 bytes)".
        """
        return self.client.send_cmd(f"ctrl uart_send {channel} {data}")

    def recv_data(self, channel: int, num_bytes: int, timeout_ms: int) -> str:
        """Read bytes captured by the DMA RX path on a USART channel.

        channel:    1 or 2
        num_bytes:  number of bytes to wait for (1–63)
        timeout_ms: maximum time to wait; returns partial data if fewer bytes
                    arrived before the deadline

        Because RX capture runs continuously in the background, bytes sent by a
        peer before this call runs are already in the ring buffer and will be
        returned immediately without waiting for the timeout.
        Returns "UART DATA: <payload>" where payload is the received ASCII string.
        """
        return self.client.send_cmd(f"ctrl uart_recv {channel} {num_bytes} {timeout_ms}")

    def set_config(self, channel: int, baud: int, parity: str = "N", stop_bits: int = 1) -> str:
        """Reconfigure a USART channel's line settings at runtime.

        channel:   1 or 2
        baud:      baud rate in bits per second, e.g. 9600 or 115200
        parity:    'N' (none), 'E' (even), or 'O' (odd)
        stop_bits: 1 or 2

        Data bits are fixed at 8. The firmware briefly tears down and re-arms its
        DMA RX path while reprogramming the USART, so any peer must use matching
        settings afterward. Returns the firmware acknowledgement, e.g.
        "Queued: UART1 cfg -> 9600 baud, parity E, 1 stop bit(s)".
        """
        return self.client.send_cmd(f"ctrl uart_cfg {channel} {baud} {parity} {stop_bits}")
