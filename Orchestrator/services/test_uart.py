from services.usb_serial import USB_Serial


class UARTTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def send_data(self, channel: int, data: str) -> str:
        return self.client.send_cmd(f"ctrl uart_send {channel} {data}")
