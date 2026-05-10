import serial
import struct
import csv
import time

PORT = "COM11"          # Windows example
# PORT = "/dev/ttyACM0"  # Linux example
# PORT = "/dev/cu.usbmodemXXXX"  # macOS example

BAUD = 1000000
OUTPUT_FILE = "accel_data.csv"

MAGIC = 0xA55A

HEADER_FORMAT = "<HHI"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

SAMPLE_FORMAT = "<Ifff"
SAMPLE_SIZE = struct.calcsize(SAMPLE_FORMAT)

def read_exact(ser, n):
    data = bytearray()
    while len(data) < n:
        chunk = ser.read(n - len(data))
        if chunk:
            data.extend(chunk)
    return bytes(data)

def find_packet_start(ser):
    while True:
        b1 = ser.read(1)
        if not b1:
            continue

        if b1[0] == 0x5A:
            b2 = ser.read(1)
            if b2 and b2[0] == 0xA5:
                return

def main():
    with serial.Serial(PORT, BAUD, timeout=1) as ser, open(OUTPUT_FILE, "w", newline="") as f:
        writer = csv.writer(f)

        writer.writerow([
            "pc_time",
            "packet_counter",
            "arduino_time_us",
            "ax_raw",
            "ay_raw",
            "az_raw"
        ])

        print("Recording... Press Ctrl+C to stop.")

        while True:
            find_packet_start(ser)

            rest_of_header = read_exact(ser, HEADER_SIZE - 2)
            sample_count, packet_counter = struct.unpack("<HI", rest_of_header)

            payload_size = sample_count * SAMPLE_SIZE
            payload = read_exact(ser, payload_size)

            pc_time = time.time()

            for i in range(sample_count):
                offset = i * SAMPLE_SIZE
                sample = struct.unpack(
                    SAMPLE_FORMAT,
                    payload[offset:offset + SAMPLE_SIZE]
                )

                t_us, ax, ay, az = sample

                writer.writerow([
                    pc_time,
                    packet_counter,
                    t_us,
                    ax,
                    ay,
                    az
                ])

            f.flush()

if __name__ == "__main__":
    main()