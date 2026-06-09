from services.usb_serial import USB_Serial


class ADCTester:
    def __init__(self, client: USB_Serial):
        self.client = client

    def sample_raw(self, channel: int) -> str:
        return self.client.send_cmd(f"ctrl adc_sample {channel}")
