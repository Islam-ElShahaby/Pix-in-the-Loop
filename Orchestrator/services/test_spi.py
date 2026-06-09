# services/test_spi.py
from services.usb_serial import USB_Serial

class SPITester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def transceive_byte(self, hex_val: int) -> str:
        return self.client.send_cmd(f"SPI BUF {hex_val}")

    def send_data(self, data: str) -> str:
        return self.client.send_cmd(f"SPI SEND {data}")