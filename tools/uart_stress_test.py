
import argparse
import queue
import sys
import threading
import time

try:
    import serial
except ImportError:
    print("pyserial is missing. Install with: python3 -m pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


def make_payload(length: int) -> bytes:
    # Printable ASCII, excluding '?' which is the firmware diagnostics command.
    pattern = bytes(v for v in range(0x20, 0x7F) if v != ord("?"))
    return (pattern * ((length + len(pattern) - 1) // len(pattern)))[:length]


def read_worker(port: serial.Serial, chunks: queue.Queue, stop: threading.Event) -> None:
    while not stop.is_set() or port.in_waiting:
        data = port.read(256)
        if data:
            chunks.put(data)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--bytes", dest="byte_count", type=int, default=10000)
    parser.add_argument("--timeout", type=float, default=5.0,
                        help="extra timeout after estimated wire time")
    args = parser.parse_args()

    if args.byte_count <= 0 or args.baud <= 0 or args.timeout <= 0:
        parser.error("--bytes, --baud and --timeout must be positive")

    payload = make_payload(args.byte_count)
    chunks: queue.Queue[bytes] = queue.Queue()
    stop_reader = threading.Event()

    print(f"Opening {args.port} at {args.baud} baud")
    print("Close Arduino IDE Serial Monitor before running this test.")

    try:
        port = serial.Serial()
        port.port = args.port
        port.baudrate = args.baud
        port.timeout = 0.05
        port.write_timeout = max(args.timeout, 500.0)
        port.xonxoff = False
        port.rtscts = False
        port.dsrdtr = False
        port.dtr = False
        port.rts = False
        port.open()

        try:
            time.sleep(0.15)
            port.reset_input_buffer()
            port.reset_output_buffer()

            reader = threading.Thread(
                target=read_worker,
                args=(port, chunks, stop_reader),
                daemon=True,
            )
            reader.start()

            started = time.monotonic()
            written = port.write(payload)
            port.flush()
            if written != len(payload):
                print(f"ERROR: wrote {written}/{len(payload)} bytes", file=sys.stderr)
                return 1

            # 8N1 is 10 transmitted bits per payload byte.
            wire_time = len(payload) * 10.0 / args.baud
            deadline = started + wire_time + args.timeout
            echoed = bytearray()

            while len(echoed) < len(payload) and time.monotonic() < deadline:
                try:
                    echoed.extend(chunks.get(timeout=0.05))
                except queue.Empty:
                    pass

            # Stop the reader and collect any final bytes it already received.
            stop_reader.set()
            reader.join(timeout=1.0)
            while True:
                try:
                    echoed.extend(chunks.get_nowait())
                except queue.Empty:
                    break

            elapsed = time.monotonic() - started
            if echoed != payload:
                mismatch = next(
                    (i for i, (a, b) in enumerate(zip(echoed, payload)) if a != b),
                    min(len(echoed), len(payload)),
                )
                print("FAIL: echoed data does not match transmitted data.")
                print(f"Sent: {len(payload)} bytes; received: {len(echoed)} bytes")
                if mismatch < len(echoed) and mismatch < len(payload):
                    print(f"First mismatch at offset {mismatch}: received 0x{echoed[mismatch]:02X}, "
                          f"expected 0x{payload[mismatch]:02X}")
                else:
                    print(f"First length boundary/mismatch: offset {mismatch}")
                print("The reader was active during transmission in this test.")
                return 1

            print(f"PASS: all {len(payload)} bytes echoed exactly.")
            print(f"Elapsed: {elapsed:.2f} s; effective rate: {len(payload)/elapsed:.1f} bytes/s")

            # Query firmware counters after the echo comparison.
            port.reset_input_buffer()
            port.write(b"?")
            port.flush()
            diagnostic = bytearray()
            quiet_deadline = time.monotonic() + 3.0
            last_data = time.monotonic()
            while time.monotonic() < quiet_deadline:
                part = port.read(256)
                if part:
                    diagnostic.extend(part)
                    last_data = time.monotonic()
                elif diagnostic and time.monotonic() - last_data > 0.35:
                    break

            print("\nFirmware diagnostics:")
            if diagnostic:
                sys.stdout.write(diagnostic.decode("ascii", errors="replace"))
                if not diagnostic.endswith(b"\n"):
                    print()
            else:
                print("No diagnostic response received; verify the firmware '?' command.")

        finally:
            stop_reader.set()
            port.close()

    except serial.SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())