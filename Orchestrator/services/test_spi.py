"""
test_spi — SPI service for the HIL orchestrator.

Wraps the `ctrl spi_write` and `ctrl spi_slave_wait` shell commands exposed
by the Zephyr firmware. Both channels use DMA-backed async transfers with
independent worker threads so they can operate concurrently.
"""

from services.usb_serial import USB_Serial


class SPITester:
    """Trigger SPI transfers via the HIL shell interface.

    Valid channels:
      1 = SPI1 (SCK PB3, MISO PB4, MOSI PB5)
      2 = SPI2 (SCK PB13, MISO PB14, MOSI PB15)
    """

    def __init__(self, client: USB_Serial):
        self.client = client

    def write_master(self, channel: int, hex_data: str) -> str:
        """Perform a SPI master-mode transceive.

        channel:  1 or 2
        hex_data: payload as a compact hex string with no separators,
                  e.g. "A5F0" sends 0xA5 followed by 0xF0. Must be
                  non-empty and even-length; maximum 32 bytes (64 hex chars).

        Returns the firmware acknowledgement, e.g.
        "Queued: SPI1 master write 'A5F0' (2 bytes)".
        """
        return self.client.send_cmd(f"ctrl spi_write {channel} {hex_data}")

    def slave_wait(self, channel: int, timeout_ms: int, hex_data: str) -> str:
        """Arm a SPI channel as slave and prepare a response payload.

        channel:    1 or 2
        timeout_ms: how long to wait for a master transfer before giving up
        hex_data:   data to clock out to the master during the transfer

        The firmware worker blocks until the master drives the clock or the
        timeout expires, then resets the peripheral ready for the next command.
        Returns the firmware acknowledgement, e.g.
        "Queued: SPI2 slave wait 500 ms 'DEADBEEF' (4 bytes)".
        """
        return self.client.send_cmd(f"ctrl spi_slave_wait {channel} {timeout_ms} {hex_data}")

    def configure(self, channel: int, freq_hz: int, cpol: int, cpha: int,
                  bit_order: str = "msb") -> str:
        """Update a SPI channel's clock and mode for subsequent transfers.

        channel:   1 or 2
        freq_hz:   bus clock in Hz (master mode)
        cpol:      clock polarity, 0 or 1
        cpha:      clock phase, 0 or 1   (cpol/cpha = the SPI mode matrix)
        bit_order: 'msb' or 'lsb' first

        Returns the firmware acknowledgement, e.g.
        "Queued: SPI1 cfg -> 1000000 Hz, cpol=0, cpha=1, msb".
        """
        return self.client.send_cmd(
            f"ctrl spi_cfg {channel} {freq_hz} {cpol} {cpha} {bit_order}"
        )

    def read_result(self, channel: int, timeout_ms: int = 500) -> str:
        """Return the bytes the peer clocked in during the last transfer.

        Pair this with a preceding write_master() / slave_wait() on the same
        channel: the firmware arms the readback when the transfer is queued, so
        this blocks until that specific transfer completes (or timeout_ms).
        Returns "SPI RX: <HEX>" (uppercase, no separators) or "SPI RX: TIMEOUT".
        """
        return self.client.send_cmd(f"ctrl spi_read {channel} {timeout_ms}")
