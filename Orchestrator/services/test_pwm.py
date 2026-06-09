"""
test_pwm — PWM service for the HIL orchestrator.

Wraps the `ctrl pwm_set` shell command exposed by the Zephyr firmware.
Each of the four channels is driven by an independent hardware timer, so
all four can run at different frequencies simultaneously.
"""

from services.usb_serial import USB_Serial


class PWMTester:
    """Configure PWM outputs via the HIL shell interface.

    Valid channels: 1–4
      ch1 = PA8  (TIM1),  ch2 = PA15 (TIM2)
      ch3 = PB8  (TIM10), ch4 = PB6  (TIM4)
    """

    def __init__(self, client: USB_Serial):
        self.client = client

    def set_duty_cycle(self, channel: int, frequency: int, duty_permille: int) -> str:
        """Set the frequency and duty cycle of a PWM channel.

        channel:       1–4
        frequency:     output frequency in Hz (must be > 0)
        duty_permille: duty cycle in parts per thousand — 0 = always off,
                       500 = 50% duty, 1000 = always on

        Returns the firmware acknowledgement, e.g.
        "Queued: PWM ch1 -> 1000 Hz, 50.0% duty".
        """
        return self.client.send_cmd(f"ctrl pwm_set {channel} {frequency} {duty_permille}")
