from logging import DEBUG

import serial
import struct
import csv
import time

DEBUG = False

PORT = "COM11"          # Windows example
# PORT = "/dev/ttyACM0"  # Linux example
# PORT = "/dev/cu.usbmodemXXXX"  # macOS example

BAUD = 1e6
OUTPUT_FILE = "mpu_data.csv"

# MAGIC = b'\xAA\xBB\xCC\xDD'
MAGIC = b'\xDD\xCC\xBB\xAA'

# sync (I), seq (I), timestamp_us (I), sample_count (H), sample_size (H)
HEADER_FORMAT = "<IIIHH"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# ax (h), ay (h), az (h), gx (h), gy (h), gz (h)
SAMPLE_FORMAT = "<hhhhhh"
SAMPLE_SIZE = struct.calcsize(SAMPLE_FORMAT)

def basic_read_test(ser):
    """Reads buffer and writes to text file, without parsing or syncing."""
    with open("raw_data.txt", "wb") as f:
        while True:
            data = ser.read(1024)
            if data:
                f.write(data)

def read_exact(ser, n):
    data = bytearray()
    while len(data) < n:
        chunk = ser.read(n - len(data))
        if chunk:
            data.extend(chunk)
    return bytes(data)

def find_packet_start(ser):
    idx = 0

    while True:
        b = ser.read(1)
        if not b:
            continue

        if b[0] == MAGIC[idx]:
            idx += 1
            if idx == len(MAGIC):
                if DEBUG:
                    print("Packet start found!")
                return
        else:
            idx = 0

def main():
    with serial.Serial(PORT, BAUD, timeout=1) as ser, open(OUTPUT_FILE, "w", newline="") as f:
        writer = csv.writer(f)

        writer.writerow([
            "pc_time",
            "seq",
            "timestamp_us",
            "ax_raw",
            "ay_raw",
            "az_raw",
            "gx_raw",
            "gy_raw",
            "gz_raw"
        ])

        print("Recording... Press Ctrl+C to stop.")

        # basic_read_test(ser)

        while True:
            
            find_packet_start(ser)

            rest_of_header = read_exact(ser, HEADER_SIZE - 4) # Already read 4 bytes of magic
            # seq (I), timestamp_us (I), sample_count (H), sample_size (H)
            seq, timestamp_us, sample_count, sample_size = struct.unpack("<IIHH", rest_of_header)

            payload_size = sample_count * SAMPLE_SIZE
            payload = read_exact(ser, payload_size)

            pc_time = time.time()

            for i in range(sample_count):
                offset = i * SAMPLE_SIZE
                sample = struct.unpack(
                    SAMPLE_FORMAT,
                    payload[offset:offset + SAMPLE_SIZE]
                )

                ax, ay, az, gx, gy, gz = sample

                writer.writerow([
                    pc_time,
                    seq,
                    timestamp_us,
                    ax,
                    ay,
                    az,
                    gx,
                    gy,
                    gz
                ])

            f.flush()

if __name__ == "__main__":
    main()