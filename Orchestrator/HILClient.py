from services.usb_serial import USB_Serial
from services.test_gpio import GPIOTester
from services.test_adc import ADCTester
from services.test_pwm import PWMTester
from services.test_spi import SPITester
from services.test_uart import UARTTester


class HILClient:
    """
    Unified Robot Framework library entry point.
    Exposes all HIL peripherals as Robot Keywords under a single library.
    """
    ROBOT_LIBRARY_SCOPE = 'SUITE'

    def __init__(self, port="/dev/ttyACM0", baud=115200):
        self.client = USB_Serial(port_path=port, baud_rate=int(baud))
        self.gpio = GPIOTester(self.client)
        self.adc  = ADCTester(self.client)
        self.pwm  = PWMTester(self.client)
        self.spi  = SPITester(self.client)
        self.uart = UARTTester(self.client)

    def initialize_hil_system(self):
        """Open the serial port and wait for the HIL shell prompt."""
        if not self.client.initialize():
            raise RuntimeError("CRITICAL: Failed to connect to HIL Controller — is the board plugged in?")

    def terminate_hil_system(self):
        """Close the serial connection."""
        self.client.close()

    # --- GPIO ---
    def write_gpio(self, port, pin, state):
        return self.gpio.write_gpio(port, int(pin), int(state))

    def read_gpio(self, port, pin):
        return self.gpio.read_gpio(port, int(pin))

    # --- ADC ---
    def sample_adc_channel(self, channel):
        return self.adc.sample_raw(int(channel))

    # --- PWM ---
    def set_pwm_duty_cycle(self, channel, frequency, duty_permille):
        return self.pwm.set_duty_cycle(int(channel), int(frequency), int(duty_permille))

    # --- SPI ---
    def spi_write(self, channel, hex_data):
        return self.spi.write_master(int(channel), hex_data)

    def spi_slave_wait(self, channel, timeout_ms, hex_data):
        return self.spi.slave_wait(int(channel), int(timeout_ms), hex_data)

    # --- UART ---
    def uart_send_data(self, channel, data):
        return self.uart.send_data(int(channel), data)
