import re
import time
import serial

SHELL_PROMPT = b"hil:~$ "

# Matches VT100/ANSI escape sequences (e.g. the shell's colored prompt "\x1b[1;32m"
# and reset "\x1b[m"). The shell emits these on the same channel as responses.
_ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


class USB_Serial:
    """
    Serial transport for the Zephyr Shell-based HIL firmware.
    Handles the shell prompt/echo protocol transparently.
    """

    def __init__(self, port_path="/dev/ttyACM0", baud_rate=115200, timeout=3.0):
        self.port_path = port_path
        self.baud_rate = baud_rate
        self.timeout = timeout
        self.ser = None

    def initialize(self) -> bool:
        try:
            self.ser = serial.Serial(self.port_path, self.baud_rate, timeout=self.timeout)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            time.sleep(0.5)
            # Nudge the shell to emit its prompt
            self.ser.write(b"\r\n")
            self.ser.flush()
            return self._wait_for_prompt(timeout_s=5.0)
        except serial.SerialException as e:
            print(f"Connection error on {self.port_path}: {e}")
            return False

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_cmd(self, command_str: str) -> str:
        """
        Send an ASCII shell command and return the first meaningful response line.

        The response is taken from the window between this command's *echo* and the
        following prompt. Anchoring on the echo makes parsing robust against async
        deferred-log output (and the prompt reprints it triggers) from earlier
        commands, which otherwise interleave on the same USB CDC channel.
        """
        if not self.ser or not self.ser.is_open:
            raise RuntimeError("Port not open.")

        cmd = command_str.strip()
        self.ser.reset_input_buffer()
        self.ser.write(f"{cmd}\r\n".encode("utf-8"))
        self.ser.flush()

        text = self._read_command_window(cmd)
        return self._parse_window(text, cmd)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _wait_for_prompt(self, timeout_s: float) -> bool:
        raw = b""
        deadline = time.monotonic() + timeout_s
        while SHELL_PROMPT not in raw and time.monotonic() < deadline:
            raw += self.ser.read(self.ser.in_waiting or 1)
        return SHELL_PROMPT in raw

    def _read_command_window(self, cmd: str) -> str:
        """Accumulate (ANSI-stripped) output until our echo is followed by a prompt."""
        prompt = SHELL_PROMPT.decode()
        raw = b""
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            chunk = self.ser.read(self.ser.in_waiting or 1)
            if chunk:
                raw += chunk
            text = _ANSI_RE.sub("", raw.decode("utf-8", errors="replace"))
            idx = text.find(cmd)
            if idx != -1 and prompt in text[idx + len(cmd):]:
                break
        return _ANSI_RE.sub("", raw.decode("utf-8", errors="replace"))

    # Zephyr deferred-log severity tags. Log records flush asynchronously on the
    # same USB CDC channel as shell replies, so they can interleave with command
    # output; they must be filtered out of the response channel.
    _LOG_MARKERS = ("<inf>", "<wrn>", "<err>", "<dbg>")

    def _is_log_line(self, line: str) -> bool:
        return any(marker in line for marker in self._LOG_MARKERS)

    def _parse_window(self, text: str, cmd: str) -> str:
        """Extract the first meaningful line between the command echo and prompt."""
        prompt = SHELL_PROMPT.decode()
        idx = text.find(cmd)
        if idx != -1:
            text = text[idx + len(cmd):]
        pidx = text.find(prompt)
        if pidx != -1:
            text = text[:pidx]
        lines = [l.strip() for l in text.splitlines()]
        meaningful = [
            l for l in lines
            if l
            and not l.startswith("hil:~$")
            and not self._is_log_line(l)
        ]
        return meaningful[0] if meaningful else ""
