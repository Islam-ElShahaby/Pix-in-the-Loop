"""
test_adc — ADC service for the HIL orchestrator.

Wraps the `ctrl adc_sample` shell command exposed by the Zephyr firmware.
The firmware reads from ADC1, which is connected to eight dedicated analog
pins on the STM32F401 Blackpill.
"""

from services.usb_serial import USB_Serial


class ADCTester:
    """Sample analog input channels via the HIL shell interface.

    Valid channels: 0, 1, 4, 5, 6, 7, 8, 9  (ADC1 input numbers).
    Channels 2 and 3 are not available — those pins are occupied by USART2
    (PA2 = TX2, PA3 = RX2) per the pin allocation matrix.
    """

    def __init__(self, client: USB_Serial):
        self.client = client

    def sample_raw(self, channel: int) -> str:
        """Sample one ADC channel and return the raw 12-bit reading.

        Returns a string in the form "ADC VALUE: <n>" where n is 0–4095,
        representing 0–3.3 V linearly (LSB ≈ 0.8 mV).
        """
        return self.client.send_cmd(f"ctrl adc_sample {channel}")
