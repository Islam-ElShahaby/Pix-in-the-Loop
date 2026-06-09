from services.usb_serial import USB_Serial


class SPITester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def write_master(self, channel: int, hex_data: str) -> str:
        return self.client.send_cmd(f"ctrl spi_write {channel} {hex_data}")

    def slave_wait(self, channel: int, timeout_ms: int, hex_data: str) -> str:
        return self.client.send_cmd(f"ctrl spi_slave_wait {channel} {timeout_ms} {hex_data}")
