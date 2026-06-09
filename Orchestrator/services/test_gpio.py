"""
test_gpio — GPIO service for the HIL orchestrator.

Wraps the `ctrl gpio_set` and `ctrl gpio_get` shell commands exposed by the
Zephyr firmware. The firmware configures pin direction lazily on first access,
so no explicit setup step is required before reading or writing a pin.
"""

from services.usb_serial import USB_Serial


class GPIOTester:
    """Send GPIO stimulus and read pin state over the HIL shell interface.

    Valid ports:  'A', 'B', or 'C'
    Valid pins:   0–15
    Valid states: 0 (low) or 1 (high)
    """

    def __init__(self, client: USB_Serial):
        self.client = client

    def write_gpio(self, port: str, pin: int, state: int) -> str:
        """Drive a GPIO pin to a logic level.

        Validates that state is 0 or 1 before sending; raises ValueError otherwise.
        Returns the firmware acknowledgement, e.g. "Queued: GPIO B5 -> 1".
        """
        if state not in (0, 1):
            raise ValueError("State must be 0 or 1.")
        return self.client.send_cmd(f"ctrl gpio_set {port.upper()} {pin} {state}")

    def read_gpio(self, port: str, pin: int) -> str:
        """Read the current logic level of a GPIO pin.

        Returns "GPIO VALUE: 0" or "GPIO VALUE: 1".
        """
        return self.client.send_cmd(f"ctrl gpio_get {port.upper()} {pin}")
