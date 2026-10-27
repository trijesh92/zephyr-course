#!/usr/bin/env python3
"""Host-side test application for the Zephyr board's custom vendor USB device.

Protocol (bulk endpoints + control):
    Host -> Device (bulk OUT): command payload
    Device -> Host (bulk IN): response payload

Protocol frame (simple, newline terminated text is used through the
CDC-ACM-like serial abstraction; for a pure bulk implementation the same
frames are carried in the bulk IN/OUT payloads):

    CMD:      LED_ON <G|R>      - turn LED <led> on
    CMD:      LED_OFF <G|R>     - turn LED <led> off
    CMD:      STATUS              - query LED status
    RESP:     OK                  - command accepted
    RESP:     ERROR <code>       - command failed
    RESP:     STATUS <g> <r>     - LED states (1/0)

The test opens the vendor USB device (VID/PID configured on the board),
claims the interface and exercises the protocol.

Requirements (Windows):
    pip install pyusb
    - pyusb WinUSB backend is used automatically on Windows, or install
      libusb via the WinUSB driver.

Usage:
    python host_test_usb.py --vid 0x1234 --pid 0x5678 --port <port>
"""

import argparse
import sys
import time

import usb.core
import usb.util


def find_vendor_device(vid: int, pid: int):
    dev = usb.core.find(idVendor=vid, idProduct=pid)
    if dev is None:
        raise RuntimeError(f"Vendor USB device not found: VID=0x{vid:04X} PID=0x{pid:04X}")
    # Find first interface with bulk endpoints
    intf = usb.core.Interface(dev, 0)
    intf.set_configuration(dev.config_number)
    return dev, intf


def send_cmd(intf, cmd: str, timeout: float = 5.0) -> str:
    """Send a command (bulk OUT) and read the response (bulk IN)."""
    # For CDC-ACM-like transports the whole frame is written to the bulk OUT
    # endpoint and the response is read from the bulk IN endpoint.
    data_out = cmd.encode("utf-8") + b"\n"
    intf.write(data_out)  # bulk OUT write

    time.sleep(0.2)  # debounce / allow response

    # Bulk IN read -> response
    resp = b""
    while True:
        chunk = intf.read(64, 1000)  # endpoint 64? use bulk endpoint address
        if not chunk:
            break
        resp += chunk
        if b"\n" in resp:
            break
    return resp.decode("utf-8").strip()


def main() -> int:
    parser = argparse.ArgumentParser(description="Zephyr board vendor USB test")
    parser.add_argument("--vid", type=lambda x: int(x, 0), default=0x1234, help="USB VID")
    parser.add_argument("--pid", type=lambda x: int(x, 0), default=0x5678, help="USB PID")
    parser.add_argument("--port", default=None, help="Serial port path (ttyACMx) or USB port")
    args = parser.parse_args()

    dev, intf = find_vendor_device(args.vid, args.pid)

    print(f"Opened vendor USB device: VID=0x{args.vid:04X} PID=0x{args.pid:04X}")
    print(f"Interface claimed: {intf.number}")

    # Test: STATUS
    resp = send_cmd(intf, "STATUS")
    print(f"STATUS -> {resp}")

    # Test: LED command (e.g., LED_ON G)
    resp = send_cmd(intf, "LED_ON G")
    print(f"LED_ON G -> {resp}")

    resp = send_cmd(intf, "LED_OFF G")
    print(f"LED_OFF G -> {resp}")

    # Test: error path
    resp = send_cmd(intf, "BOGUS_CMD")
    print(f"BOGUS_CMD -> {resp}")

    dev.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
