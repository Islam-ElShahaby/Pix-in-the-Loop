from services.usb_serial import USB_Serial


class GPIOTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def write_gpio(self, port: str, pin: int, state: int) -> str:
        if state not in (0, 1):
            raise ValueError("State must be 0 or 1.")
        return self.client.send_cmd(f"ctrl gpio_set {port.upper()} {pin} {state}")

    def read_gpio(self, port: str, pin: int) -> str:
        return self.client.send_cmd(f"ctrl gpio_get {port.upper()} {pin}")
