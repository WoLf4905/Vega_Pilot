#!/usr/bin/env python3

import time
from pymavlink import mavutil

PORT = "/dev/ttyUSB0"
BAUD_RATE = 115200
EXPECTED_MESSAGES = 5

connection = mavutil.mavlink_connection(
    PORT,
    baud=BAUD_RATE,
    dialect="common"
)

print(f"Listening on {PORT} at {BAUD_RATE} baud")
print("Waiting for HEARTBEAT messages...")
print("Press RESET on ARIES V3 after this message appears.\n")

previous_time = None
received_count = 0

try:
    while received_count < EXPECTED_MESSAGES:

        message = connection.recv_match(
            type="HEARTBEAT",
            blocking=True,
            timeout=5
        )

        if message is None:
            print("\nTimeout waiting for the next HEARTBEAT.")
            break

        current_time = time.monotonic()
        received_count += 1

        print(f"HEARTBEAT #{received_count}")
        print(f"System ID:       {message.get_srcSystem()}")
        print(f"Component ID:    {message.get_srcComponent()}")
        print(f"Vehicle type:    {message.type}")
        print(f"System status:   {message.system_status}")

        if previous_time is not None:
            interval = current_time - previous_time
            print(f"Interval:        {interval:.4f} seconds")

        print("-----------------------------------")

        previous_time = current_time

    print(f"\nReceived {received_count}/{EXPECTED_MESSAGES} HEARTBEAT messages.")

    if received_count == EXPECTED_MESSAGES:
        print("PASS: Periodic MAVLink HEARTBEAT transmission verified.")
    else:
        print("FAIL: Did not receive all expected HEARTBEAT messages.")

finally:
    connection.close()
