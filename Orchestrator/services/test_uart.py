# services/test_uart.py
from services.usb_serial import USB_Serial

class UARTTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def send_data(self, data: str) -> str:
        return self.client.send_cmd(f"UART SEND {data}")
    
    def transceive_string(self, data: str) -> str:
        return self.client.send_cmd(f"UART TRANSCEIVE {data}")
