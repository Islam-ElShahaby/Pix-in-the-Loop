"""
HILClient — Robot Framework library entry point for the Pix-in-the-Loop platform.

Import this class as the Library in a Robot Framework suite to get access to all
HIL keywords. Each public method here is a Robot keyword; the underlying serial
protocol and per-peripheral command formatting are handled by the service modules.

Usage in a .robot file:
    Library    HILClient.py    /dev/ttyACM0    115200

The HIL Controller firmware responds to every command with a single line on the
USB CDC serial port. Fire-and-forget commands (GPIO set, PWM, SPI write, UART send)
return "Queued: …". Read commands (GPIO get, ADC, UART recv) return the measured
value. Error responses begin with "Error:" or "ERROR:".
"""

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
        """Open the serial port and wait for the HIL shell prompt.

        Must be called before any other keyword (typically in Suite Setup).
        Raises RuntimeError if the board is unreachable or the prompt does not
        appear within the connection timeout.
        """
        if not self.client.initialize():
            raise RuntimeError("CRITICAL: Failed to connect to HIL Controller — is the board plugged in?")

    def terminate_hil_system(self):
        """Close the serial connection. Safe to call even if never opened."""
        self.client.close()

    # --- GPIO ---

    def write_gpio(self, port, pin, state):
        """Drive a GPIO pin high or low.

        port:  'A', 'B', or 'C'
        pin:   0–15
        state: 0 (low) or 1 (high)

        Returns the firmware acknowledgement string, e.g. "Queued: GPIO B5 -> 1".
        """
        return self.gpio.write_gpio(port, int(pin), int(state))

    def read_gpio(self, port, pin):
        """Read the current logic level of a GPIO pin.

        port: 'A', 'B', or 'C'
        pin:  0–15

        Returns "GPIO VALUE: 0" or "GPIO VALUE: 1".
        """
        return self.gpio.read_gpio(port, int(pin))

    # --- ADC ---

    def sample_adc_channel(self, channel):
        """Sample one ADC channel and return the raw 12-bit reading.

        channel: ADC1 input number — valid values are 0, 1, 4, 5, 6, 7, 8, 9.
                 Channels 2 and 3 are unavailable (occupied by USART2 on PA2/PA3).

        Returns a string in the form "ADC VALUE: <n>" where n is 0–4095,
        representing 0–3.3 V linearly.
        """
        return self.adc.sample_raw(int(channel))

    # --- PWM ---

    def set_pwm_duty_cycle(self, channel, frequency, duty_permille):
        """Configure a PWM channel's frequency and duty cycle.

        channel:       1–4 (each backed by an independent hardware timer)
        frequency:     output frequency in Hz (> 0)
        duty_permille: duty cycle in parts per thousand — 0 = always off,
                       500 = 50%, 1000 = always on

        Returns the firmware acknowledgement string, e.g. "Queued: PWM ch1 -> 1000 Hz, 50.0% duty".
        """
        return self.pwm.set_duty_cycle(int(channel), int(frequency), int(duty_permille))

    # --- SPI ---

    def spi_write(self, channel, hex_data):
        """Perform a SPI master-mode write on the specified channel.

        channel:  1 (SPI1, PB3/4/5) or 2 (SPI2, PB13/14/15)
        hex_data: payload as a hex string with no separators, e.g. "A5F0"
                  (must be non-empty and even-length; max 32 bytes = 64 hex chars)

        Returns the firmware acknowledgement string, e.g. "Queued: SPI1 master write 'A5F0' (2 bytes)".
        """
        return self.spi.write_master(int(channel), hex_data)

    def spi_slave_wait(self, channel, timeout_ms, hex_data):
        """Arm a SPI channel as slave and prepare a response payload.

        channel:    1 or 2
        timeout_ms: maximum time to wait for a master transfer (milliseconds)
        hex_data:   data to clock out during the master-driven transfer (hex string)

        Returns the firmware acknowledgement string, e.g.
        "Queued: SPI2 slave wait 500 ms 'DEADBEEF' (4 bytes)".
        """
        return self.spi.slave_wait(int(channel), int(timeout_ms), hex_data)

    # --- UART ---

    def uart_send_data(self, channel, data):
        """Transmit an ASCII string over a DUT-facing USART channel.

        channel: 1 (USART1, PA9 TX / PA10 RX) or 2 (USART2, PA2 TX / PA3 RX)
        data:    ASCII string, 1–63 characters

        The transmission is DMA-backed; the firmware blocks the dispatcher thread
        until the DMA transfer completes before processing the next command.
        Returns the firmware acknowledgement, e.g. "Queued: UART1 send 'HELLO' (5 bytes)".
        """
        return self.uart.send_data(int(channel), data)

    def uart_receive_data(self, channel, num_bytes, timeout_ms):
        """Read bytes captured by the DMA RX path on a USART channel.

        channel:    1 or 2
        num_bytes:  number of bytes to wait for (1–63)
        timeout_ms: maximum time to wait before returning partial data

        RX is captured continuously into a ring buffer by the DMA callback, so
        bytes sent by a peer before this keyword runs are not lost. Returns
        "UART DATA: <payload>" where payload is the received ASCII string,
        or a shorter string if fewer than num_bytes arrived before the timeout.
        """
        return self.uart.recv_data(int(channel), int(num_bytes), int(timeout_ms))
