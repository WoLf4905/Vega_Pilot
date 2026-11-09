
#!/usr/bin/env python3

"""
Binary UART full-duplex stress test for ARIES V3.

Tests all possible byte values from 0x00 to 0xFF.

A dedicated reader thread continuously receives echoed
data while the main thread transmits the payload.
"""

import argparse
import queue
import sys
import threading
import time

try:
    import serial
except ImportError:
    print(
        "pyserial is missing. Install it using:",
        "python3 -m pip install pyserial",
        file=sys.stderr
    )
    raise SystemExit(2)


def make_payload(length: int) -> bytes:
    """
    Generate repeating binary sequence:

    00 01 02 03 ... FC FD FE FF
    00 01 02 03 ... FC FD FE FF

    Includes every possible uint8_t value.
    """

    pattern = bytes(range(256))

    repetitions = (length + 255) // 256

    return (pattern * repetitions)[:length]


def read_worker(
    port: serial.Serial,
    chunks: queue.Queue,
    stop: threading.Event
) -> None:

    """
    Continuously read serial data in a separate thread.
    """

    while not stop.is_set() or port.in_waiting:

        data = port.read(256)

        if data:
            chunks.put(data)


def main() -> int:

    parser = argparse.ArgumentParser(
        description=__doc__
    )

    parser.add_argument(
        "--port",
        default="/dev/ttyUSB0"
    )

    parser.add_argument(
        "--baud",
        type=int,
        default=115200
    )

    parser.add_argument(
        "--bytes",
        dest="byte_count",
        type=int,
        default=1_000_000
    )

    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="Additional timeout after estimated wire time"
    )

    args = parser.parse_args()

    if (
        args.byte_count <= 0
        or args.baud <= 0
        or args.timeout <= 0
    ):
        parser.error(
            "--bytes, --baud and --timeout must be positive"
        )

    payload = make_payload(args.byte_count)

    chunks = queue.Queue()

    stop_reader = threading.Event()

    reader = None

    print(
        f"Opening {args.port} at {args.baud} baud"
    )

    print(
        "Binary UART test: all values 0x00 through 0xFF"
    )

    print(
        "Close Arduino IDE Serial Monitor before running this test."
    )

    try:

        port = serial.Serial()

        port.port = args.port
        port.baudrate = args.baud

        port.timeout = 0.05

        # 8N1 = 10 bits per byte.
        wire_time = (
            len(payload) * 10.0 / args.baud
        )

        port.write_timeout = wire_time + args.timeout

        port.xonxoff = False
        port.rtscts = False
        port.dsrdtr = False

        port.dtr = False
        port.rts = False

        port.open()

        try:

            time.sleep(0.15)

            # Clear old serial data.
            port.reset_input_buffer()
            port.reset_output_buffer()

            # Start receiver before transmission.
            reader = threading.Thread(
                target=read_worker,
                args=(
                    port,
                    chunks,
                    stop_reader
                ),
                daemon=True
            )

            reader.start()

            print("\nStarting binary transmission...")

            started = time.monotonic()

            written = port.write(payload)

            port.flush()

            if written != len(payload):

                print(
                    f"ERROR: wrote {written}/{len(payload)} bytes",
                    file=sys.stderr
                )

                return 1

            deadline = (
                started
                + wire_time
                + args.timeout
            )

            echoed = bytearray()

            # Collect echoed bytes.
            while (
                len(echoed) < len(payload)
                and time.monotonic() < deadline
            ):

                try:

                    data = chunks.get(timeout=0.05)

                    echoed.extend(data)

                except queue.Empty:

                    pass

            # Stop reader and collect remaining data.
            stop_reader.set()

            reader.join(timeout=2.0)

            while True:

                try:

                    echoed.extend(
                        chunks.get_nowait()
                    )

                except queue.Empty:

                    break

            elapsed = time.monotonic() - started

            print("\n========== TEST RESULT ==========")

            if echoed == payload:

                print(
                    f"PASS: all {len(payload)} bytes echoed exactly."
                )

            else:

                mismatch = next(
                    (
                        i
                        for i, (received, expected)
                        in enumerate(zip(echoed, payload))
                        if received != expected
                    ),
                    min(len(echoed), len(payload))
                )

                print(
                    "FAIL: binary data does not match."
                )

                print(
                    f"Sent:     {len(payload)} bytes"
                )

                print(
                    f"Received: {len(echoed)} bytes"
                )

                print(
                    f"Missing/extra bytes: "
                    f"{len(payload) - len(echoed)}"
                )

                if (
                    mismatch < len(echoed)
                    and mismatch < len(payload)
                ):

                    print(
                        f"First mismatch at offset: {mismatch}"
                    )

                    print(
                        f"Received: 0x{echoed[mismatch]:02X}"
                    )

                    print(
                        f"Expected: 0x{payload[mismatch]:02X}"
                    )

                else:

                    print(
                        f"First length boundary: {mismatch}"
                    )

                print(
                    "Check RX/TX ring-buffer handling."
                )

                return 1

            print(
                f"Elapsed: {elapsed:.2f} seconds"
            )

            print(
                f"Effective rate: "
                f"{len(payload) / elapsed:.1f} bytes/second"
            )

            print("=================================")

            return 0

        finally:

            stop_reader.set()

            if reader is not None:
                reader.join(timeout=2.0)

            port.close()

    except serial.SerialException as exc:

        print(
            f"Serial error: {exc}",
            file=sys.stderr
        )

        return 2


if __name__ == "__main__":

    raise SystemExit(main())
