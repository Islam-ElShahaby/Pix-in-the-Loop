# services/test_adc.py
from services.usb_serial import USB_Serial

class ADCTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def sample_raw(self) -> str:
        return self.client.send_cmd("ADC SAMPLE")
