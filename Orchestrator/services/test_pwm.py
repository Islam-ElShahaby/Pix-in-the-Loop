from services.usb_serial import USB_Serial


class PWMTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def set_duty_cycle(self, channel: int, frequency: int, duty_permille: int) -> str:
        return self.client.send_cmd(f"ctrl pwm_set {channel} {frequency} {duty_permille}")
