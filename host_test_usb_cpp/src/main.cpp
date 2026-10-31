/*
 * main.cpp - Host-side test application entry point for Zephyr board's custom
 *            vendor USB device (WinUSB).
 *
 * Architecture (3 layers, dependency-injected):
 *   1. Transport        : IUsbConnection  <- WinUsbDevice  (WinUSB API)
 *   2. Protocol         : IUsbProtocol    <- LedUsbProtocol (wire frame encode/decode)
 *   3. Application      : UsbHostController (domain operations)
 *   4. CLI              : UsbCli (argument parsing, dispatch, console output)
 *
 * main() is intentionally minimal: it only wires the factories together and
 * runs the CLI. All argument parsing and command dispatch live in UsbCli.
 *
 * Build (Windows, MinGW):
 *   mkdir build && cd build
 *   cmake -G "MinGW Makefiles" ..
 *   cmake --build .
 *
 * Build (Windows, MSVC / Visual Studio):
 *   mkdir build && cd build
 *   cmake ..
 *   cmake --build .
 *
 * Usage:
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 status
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 led-on G
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 led-off R
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 ctrl-status
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 ctrl-led-on G
 *
 * Requirements:
 *   - Windows SDK (WinUSB API)
 *   - WinUSB driver installed on the device via Zadig
 */

#include <cstdint>
#include <iostream>
#include <memory>

#include <windows.h>

#include "usb_cli.h"
#include "usb_connection.h"
#include "usb_protocol.h"

// Factory functions are defined in the .cpp files.
std::unique_ptr<IUsbConnection> create_winusb_connection();
std::unique_ptr<IUsbProtocol> create_led_usb_protocol();

int main(int argc, char* argv[]) {
    auto connection = create_winusb_connection();
    auto protocol = create_led_usb_protocol();

    UsbCli cli(std::move(connection), std::move(protocol));
    return cli.run(argc, argv);
}
