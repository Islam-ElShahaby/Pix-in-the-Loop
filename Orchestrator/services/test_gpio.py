# services/test_gpio.py
from services.usb_serial import USB_Serial

class GPIOTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def write_gpio(self, state: int) -> str:
        if(state not in (0, 1)):
            raise ValueError("State must be 0 or 1.")
        return self.client.send_cmd(f"GPIO WRITE {state}")

    def read_gpio(self) -> str:
        return self.client.send_cmd("GPIO READ")
