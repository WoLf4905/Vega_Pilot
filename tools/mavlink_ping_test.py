#!/usr/bin/env python3

import time
from pymavlink import mavutil

PORT = "/dev/ttyUSB0"
BAUD_RATE = 115200

HOST_SYSTEM_ID = 255
HOST_COMPONENT_ID = 190

BOARD_SYSTEM_ID = 1
BOARD_COMPONENT_ID = 1

PING_SEQUENCE = 42

connection = mavutil.mavlink_connection(
    PORT,
    baud=BAUD_RATE,
    source_system=HOST_SYSTEM_ID,
    source_component=HOST_COMPONENT_ID,
    dialect="common"
)

try:
    print(f"Connected to {PORT} at {BAUD_RATE} baud")
    print("Waiting for ARIES V3 HEARTBEAT...")

    heartbeat = connection.recv_match(
        type="HEARTBEAT",
        blocking=True,
        timeout=15
    )

    if heartbeat is None:
        print("FAIL: No HEARTBEAT received.")
        raise SystemExit(1)

    print(
        f"HEARTBEAT received from "
        f"system {heartbeat.get_srcSystem()}, "
        f"component {heartbeat.get_srcComponent()}"
    )

    if (heartbeat.get_srcSystem() != BOARD_SYSTEM_ID or
            heartbeat.get_srcComponent() != BOARD_COMPONENT_ID):
        print("FAIL: HEARTBEAT came from an unexpected source.")
        raise SystemExit(1)

    # Generate a timestamp in microseconds.
    request_timestamp = time.monotonic_ns() // 1000

    print("\nSending MAVLink PING to ARIES V3...")
    print(f"PING sequence: {PING_SEQUENCE}")

    connection.mav.ping_send(
        request_timestamp,
        PING_SEQUENCE,
        BOARD_SYSTEM_ID,
        BOARD_COMPONENT_ID
    )

    print("Waiting for PING response...")

    response = connection.recv_match(
        type="PING",
        blocking=True,
        timeout=5
    )

    if response is None:
        print("FAIL: No PING response received.")
        raise SystemExit(1)

    print("\n========== PING RESPONSE ==========")
    print(f"Source system:       {response.get_srcSystem()}")
    print(f"Source component:    {response.get_srcComponent()}")
    print(f"Target system:       {response.target_system}")
    print(f"Target component:    {response.target_component}")
    print(f"Timestamp:           {response.time_usec}")
    print(f"Sequence:            {response.seq}")

    valid = (
        response.get_srcSystem() == BOARD_SYSTEM_ID
        and response.get_srcComponent() == BOARD_COMPONENT_ID
        and response.target_system == HOST_SYSTEM_ID
        and response.target_component == HOST_COMPONENT_ID
        and response.time_usec == request_timestamp
        and response.seq == PING_SEQUENCE
    )

    print("===================================")

    if valid:
        print("\nPASS: MAVLink PING round-trip successful.")
    else:
        print("\nFAIL: PING response fields did not match.")

        raise SystemExit(1)

finally:
    connection.close()
