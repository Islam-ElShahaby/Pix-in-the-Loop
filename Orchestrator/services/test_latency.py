"""
test_latency — GPIO interrupt-latency service for the HIL orchestrator.

Wraps the `ctrl gpio_latency` shell command exposed by the Zephyr firmware. The
bench drives a rising stimulus edge on one pin and times — *in firmware*, using
the CPU cycle counter — how long until the requested edge appears on a second
pin. Because the measurement is firmware-timed, it is immune to the millisecond-
scale USB/shell round-trip jitter that would dominate a host-side stopwatch.

Typical use: the bench stimulus pin wires to a DUT input that fires an ISR; the
DUT ISR drives a response edge back to the bench input pin. The returned number
is the DUT's interrupt-to-output round-trip latency in microseconds.
"""

from services.usb_serial import USB_Serial


class LatencyTester:
    """Measure edge-response latency via the HIL shell interface.

    Valid ports: 'A', 'B', or 'C'
    Valid pins:  0–15
    Valid edges: 'R' (rising), 'F' (falling), 'B' (both) — applied to the
                 response pin.
    """

    def __init__(self, client: USB_Serial):
        self.client = client

    def measure(self, out_port: str, out_pin: int, in_port: str, in_pin: int,
                edge: str = "R", timeout_ms: int = 100) -> str:
        """Run one latency measurement and return the raw firmware line.

        Drives a rising edge on out_port/out_pin and waits for `edge` on
        in_port/in_pin. Returns "LATENCY US: <n>" on success or
        "LATENCY: TIMEOUT" if no edge arrived within timeout_ms.
        """
        return self.client.send_cmd(
            f"ctrl gpio_latency {out_port.upper()} {out_pin} "
            f"{in_port.upper()} {in_pin} {edge.upper()} {timeout_ms}"
        )

    def measure_us(self, out_port: str, out_pin: int, in_port: str, in_pin: int,
                   edge: str = "R", timeout_ms: int = 100) -> int:
        """Run one measurement and return the latency in microseconds as an int.

        Returns -1 if the firmware reported a timeout (no response edge), so
        callers can assert numerically. Raises ValueError on an unparsable reply.
        """
        resp = self.measure(out_port, out_pin, in_port, in_pin, edge, timeout_ms)
        if "TIMEOUT" in resp:
            return -1
        marker = "LATENCY US:"
        idx = resp.find(marker)
        if idx == -1:
            raise ValueError(f"Unexpected latency reply: {resp!r}")
        return int(resp[idx + len(marker):].strip())
