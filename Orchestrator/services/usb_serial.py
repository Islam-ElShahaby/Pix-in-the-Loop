import serial

class USB_Serial:
    """
    Core engine managing raw port initialization and stream input/output.
    """
    def __init__(self, port_path="/dev/ttyUSB0", baud_rate=115200, timeout=2.0):
        self.port_path = port_path
        self.baud_rate = baud_rate
        self.timeout = timeout
        self.ser = None

    def initialize(self):
        try:
            self.ser = serial.Serial(self.port_path, self.baud_rate, timeout=self.timeout)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            return True
        except serial.SerialException as e:
            print(f"❌ Connection error on {self.port_path}: {e}")
            return False

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_cmd(self, command_str: str) -> str:
        """
        Sends an ASCII string and extracts the stripped functional response.
        """
        if not self.ser or not self.ser.is_open:
            raise RuntimeError("Port not open.")
            
        full_payload = f"{command_str.strip()}\r\n"
        self.ser.write(full_payload.encode('utf-8'))
        self.ser.flush()

        # Discard the input character echo from main.c
        _ = self.ser.readline()
        # Recover the true protocol response line
        return self.ser.readline().decode('utf-8').strip()