# services/test_pwm.py
from services.usb_serial import USB_Serial
        
class PWMTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def set_duty_cycle(self, value: int) -> str:
        return self.client.send_cmd(f"PWM SET {value}")
