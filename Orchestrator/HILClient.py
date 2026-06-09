# HILCLient.py
from services.usb_serial import USB_Serial
from services.test_gpio import GPIOTester
from services.test_adc import ADCTester
from services.test_pwm import PWMTester
from services.test_spi import SPITester
from services.test_uart import UARTTester

class HILClient:
    """
    Unified entry point for Robot Framework.
    Exposes all sub-services as Robot Keywords under a single library.
    """
    ROBOT_LIBRARY_SCOPE = 'SUITE' # Persists the connection across the whole test file

    def __init__(self, port="/dev/ttyACM0", baud=115200):
        self.client = USB_Serial(port_path=port, baud_rate=baud)
        
        self.gpio = GPIOTester(self.client)
        self.adc = ADCTester(self.client)
        self.pwm = PWMTester(self.client)
        self.spi = SPITester(self.client)
        self.uart = UARTTester(self.client)

    def initialize_hil_system(self):
        """Keyword to open the serial port connection."""
        success = self.client.initialize()
        if not success:
            raise RuntimeError("CRITICAL: Failed to open serial port to Black Pill board!")

    def terminate_hil_system(self):
        """Keyword to safely close the connection."""
        self.client.close()

    # --- Delegate Service Methods out as Keywords ---
    def write_gpio(self, state):
        return self.gpio.write_gpio(state)

    def read_gpio(self):
        return self.gpio.read_gpio()

    def sample_adc_channel(self):
        return self.adc.sample_raw()

    def set_pwm_duty_cycle(self, value):
        return self.pwm.set_duty_cycle(value)

    def spi_transceive_byte(self, hex_val):
        return self.spi.transceive_byte(hex_val)
    
    def spi_send_data(self, data):
        return self.spi.send_data(data)

    def uart_send_data(self, data): 
        return self.uart.send_data(data)
    
    def uart_transceive_string(self, data):
        return self.uart.transceive_string(data)


# if __name__ == "__main__":
#     hil = HILClient()
#     hil.initialize_hil_system()
#     print(hil.write_gpio(1))
#     hil.terminate_hil_system()