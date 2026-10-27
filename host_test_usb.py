#!/usr/bin/env python3
"""Host-side test application for the Zephyr board's custom vendor USB device.

Protocol (command/response over bulk endpoints + control transfers):

Host -> Device (bulk OUT endpoint 0x01):
    [0] opcode  (0x01 SET_LED, 0x02 GET_STATUS)
    [1] arg     (LED command for SET_LED: 0x01 Green ON, 0x02 Green OFF,
                0x03 Red ON, 0x04 Red OFF)

Device -> Host (bulk IN endpoint 0x81):
    [0] status  (0x00 OK, 0xFF ERROR)
    [1] data    (LED states for GET_STATUS: bit0=green, bit1=red)

Vendor control requests (bRequest = opcode) are also supported:
    SET_LED   (0x01): wValue = LED index (0=green,1=red), wIndex = state
    GET_STATUS (0x02): data stage returns led_states byte

Usage (Windows):
    python host_test_usb.py --vid 0x1234 --pid 0x5678

Requirements:
    pip install pyusb

USB backend setup on Windows (one of the following):
    1. WinUSB driver via Zadig (recommended):
       - Download Zadig: https://zadig.akeo.ie/
       - Plug in the FRDM board
       - Run Zadig -> Options -> List All Devices
       - Select your device (VID=0x1234, PID=0x5678)
       - Choose "WinUSB" driver -> click "Install Driver"
    2. libusb-win32 driver via Zadig + libusb-1.0.dll:
       - Install libusb-win32 driver via Zadig
       - Place libusb-1.0.dll in the same folder as this script or in PATH

pyusb will auto-detect the available backend. If no backend is found,
you'll get a "No backend available" error.

Steps:
    1. Flash the firmware to the board (west flash -d build-frdm -r jlink)
    2. Power-cycle the board (unplug USB, reset, replug USB)
    3. Install WinUSB driver via Zadig
    4. Run: python host_test_usb.py --vid 0x1234 --pid 0x5678
"""

import argparse
import sys
import time

import usb.core
import usb.util


def find_vendor_device(vid: int, pid: int):
    """Find the vendor USB device by VID/PID using pyusb's default backend."""
    dev = usb.core.find(idVendor=vid, idProduct=pid)
    if dev is None:
        raise RuntimeError(
            f"Vendor USB device not found: VID=0x{vid:04X} PID=0x{pid:04X}\n"
            f"  Make sure the board is connected and the USB device is enumerating.\n"
            f"  On Windows, install a WinUSB or libusb driver via Zadig.\n"
            f"  See the comments at the top of this file for setup instructions."
        )
    return dev


def claim_interface(dev):
    """Claim the vendor interface and return the interface number."""
    cfg = dev.get_active_configuration()
    intf = None

    # Find the vendor-specific interface (bInterfaceClass == 0xFF)
    for i in cfg:
        if i.bInterfaceClass == 0xFF:  # Vendor-specific class
            intf = i
            break

    if intf is None:
        raise RuntimeError("No vendor-specific interface (class 0xFF) found")

    iface_num = intf.bInterfaceNumber

    # Detach kernel driver if active
    if dev.is_kernel_driver_active(iface_num):
        try:
            dev.detach_kernel_driver(iface_num)
        except usb.core.USBError:
            pass

    # Claim the interface
    usb.util.claim_interface(dev, iface_num)
    return iface_num


def find_endpoints(dev, iface_num):
    """Find bulk IN and bulk OUT endpoint addresses for the interface."""
    cfg = dev.get_active_configuration()
    intf = usb.util.find_descriptor(cfg, bInterfaceNumber=iface_num)

    ep_out = None
    ep_in = None

    for ep in intf:
        ep_addr = ep.bEndpointAddress
        ep_type = ep.bmAttributes & 0x03  # Mask out transfer type

        if ep_type == 0x02:  # Bulk
            if usb.util.endpoint_direction(ep_addr) == usb.util.ENDPOINT_OUT:
                ep_out = ep_addr
            else:
                ep_in = ep_addr

    if ep_out is None or ep_in is None:
        raise RuntimeError(
            f"Could not find both bulk endpoints (OUT={ep_out}, IN={ep_in})"
        )

    return ep_out, ep_in


def send_bulk_command(dev, ep_out, ep_in, cmd_bytes, timeout=5000):
    """Send a command over bulk OUT and read the response over bulk IN."""
    # Write command to bulk OUT endpoint
    dev.write(ep_out, cmd_bytes, timeout)

    # Small delay to allow device to process
    time.sleep(0.1)

    # Read response from bulk IN endpoint
    try:
        resp = dev.read(ep_in, 64, timeout)
        return bytes(resp)
    except usb.core.USBError as e:
        if e.args and e.args[0] == "Pipe error":
            raise RuntimeError("Bulk IN pipe stalled - device may not be responding")
        raise


def run_bulk_protocol_test(dev, ep_out, ep_in):
    """Run the bulk endpoint command/response protocol test."""
    print("\n=== Bulk Protocol Test ===")

    # Test 1: GET_STATUS via bulk
    print("Sending GET_STATUS via bulk...")
    resp = send_bulk_command(dev, ep_out, ep_in, bytes([0x02, 0x00]))
    print(f"  Response: {[hex(b) for b in resp]}")
    if resp and resp[0] == 0x00:
        print(f"  LED states: green={resp[1] & 1}, red={(resp[1] >> 1) & 1}")
    else:
        print("  ERROR: unexpected response")

    # Test 2: SET_LED Green ON via bulk
    print("Sending SET_LED Green ON via bulk...")
    resp = send_bulk_command(dev, ep_out, ep_in, bytes([0x01, 0x01]))
    print(f"  Response: {[hex(b) for b in resp]}")

    time.sleep(0.5)

    # Test 3: GET_STATUS via bulk again
    print("Sending GET_STATUS via bulk (after Green ON)...")
    resp = send_bulk_command(dev, ep_out, ep_in, bytes([0x02, 0x00]))
    print(f"  Response: {[hex(b) for b in resp]}")
    if resp and resp[0] == 0x00:
        print(f"  LED states: green={resp[1] & 1}, red={(resp[1] >> 1) & 1}")

    time.sleep(0.5)

    # Test 4: SET_LED Green OFF via bulk
    print("Sending SET_LED Green OFF via bulk...")
    resp = send_bulk_command(dev, ep_out, ep_in, bytes([0x01, 0x02]))
    print(f"  Response: {[hex(b) for b in resp]}")

    # Test 5: Invalid command
    print("Sending invalid command via bulk...")
    resp = send_bulk_command(dev, ep_out, ep_in, bytes([0xFF, 0x00]))
    print(f"  Response: {[hex(b) for b in resp]}")


def run_control_protocol_test(dev):
    """Run the vendor control request protocol test."""
    print("\n=== Control Transfer Protocol Test ===")

    # Test GET_STATUS via control transfer (Device-to-Host, Vendor, Device)
    bm_req_type = (
        usb.util.ENDPOINT_IN
        | usb.util.CTRL_TYPE_VENDOR
        | usb.util.CTRL_RECIPIENT_DEVICE
    )
    print("Sending GET_STATUS via control transfer...")
    try:
        result = dev.ctrl_transfer(
            bm_req_type, 0x02, 0x0000, 0x0000, 1, 5000
        )
        print(f"  Response: {[hex(b) for b in result]}")
    except usb.core.USBError as e:
        print(f"  Error: {e}")

    # Test SET_LED Green ON via control transfer (Host-to-Device, Vendor, Device)
    bm_req_type = (
        usb.util.ENDPOINT_OUT
        | usb.util.CTRL_TYPE_VENDOR
        | usb.util.CTRL_RECIPIENT_DEVICE
    )
    print("Sending SET_LED Green ON via control transfer...")
    try:
        result = dev.ctrl_transfer(
            bm_req_type, 0x01, 0x0001, 0x0000, b"", 5000
        )
        print(f"  Result: {result}")
    except usb.core.USBError as e:
        print(f"  Error: {e}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Zephyr board vendor USB test (bulk endpoints)"
    )
    parser.add_argument(
        "--vid", type=lambda x: int(x, 0), default=0x1234, help="USB Vendor ID"
    )
    parser.add_argument(
        "--pid", type=lambda x: int(x, 0), default=0x5678, help="USB Product ID"
    )
    parser.add_argument(
        "--protocol",
        choices=["bulk", "control", "both"],
        default="bulk",
        help="Protocol to test (default: bulk)",
    )
    args = parser.parse_args()

    # Find the device using pyusb's default backend
    try:
        dev = find_vendor_device(args.vid, args.pid)
    except RuntimeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    print(
        f"Found device: VID=0x{args.vid:04X} PID=0x{args.pid:04X} "
        f"(bus={dev.bus}, address={dev.address})"
    )

    # Claim the vendor interface
    try:
        iface_num = claim_interface(dev)
        print(f"Claimed interface: {iface_num}")
    except RuntimeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        dev.close()
        return 1

    # Find bulk endpoints
    try:
        ep_out, ep_in = find_endpoints(dev, iface_num)
        print(f"Bulk OUT endpoint: 0x{ep_out:02X}")
        print(f"Bulk IN endpoint:  0x{ep_in:02X}")
    except RuntimeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        usb.util.release_interface(dev, iface_num)
        dev.close()
        return 1

    try:
        if args.protocol in ("bulk", "both"):
            run_bulk_protocol_test(dev, ep_out, ep_in)

        if args.protocol in ("control", "both"):
            run_control_protocol_test(dev)
    finally:
        # Release the interface and close the device
        usb.util.release_interface(dev, iface_num)
        dev.close()
        print("\nDevice closed.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
