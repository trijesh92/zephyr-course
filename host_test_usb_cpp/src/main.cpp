/*
 * main.cpp - Host-side test application for Zephyr board's custom vendor USB device
 *
 * Modular C++ host application using the Windows WinUSB API.
 *
 * Architecture (3 layers, dependency-injected):
 *   1. Transport        : IUsbConnection  <- WinUsbDevice  (WinUSB API)
 *   2. Protocol         : IUsbProtocol    <- LedUsbProtocol (wire frame encode/decode)
 *   3. Application      : UsbHostController (domain operations: get_status, set_led)
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <iostream>
#include <iomanip>
#include <memory>

#include <windows.h>

#include "usb_connection.h"
#include "usb_protocol.h"
#include "usb_app.h"

// Factory functions are defined in the .cpp files.
std::unique_ptr<IUsbConnection> create_winusb_connection();
std::unique_ptr<IUsbProtocol> create_led_usb_protocol();

/* ---------------------------------------------------------------------------
 * Command-line parsing helpers
 * ------------------------------------------------------------------------- */

static std::string trim_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static bool parse_u16_hex(const std::string& s, uint16_t& out) {
    try {
        unsigned long v = std::stoul(trim_quotes(s), nullptr, 0);
        if (v > 0xFFFFu) return false;
        out = static_cast<uint16_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parse_led_arg(const std::string& arg, LedId& led) {
    if (arg.empty()) return false;
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(arg[0])));
    if (c == 'G') {
        led = LedId::Green;
        return true;
    }
    if (c == 'R') {
        led = LedId::Red;
        return true;
    }
    return false;
}

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options] <command> [led]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --vid VID       USB Vendor ID (default: 0x1234)\n";
    std::cout << "  --pid PID       USB Product ID (default: 0x5678)\n";
    std::cout << "  --timeout MS    Timeout in milliseconds (default: 5000)\n";
    std::cout << "  --help, -h      Show this help\n";
    std::cout << "\nCommands (bulk endpoints):\n";
    std::cout << "  status          GET_STATUS - query LED states\n";
    std::cout << "  led-on G|R      SET_LED    - turn LED on\n";
    std::cout << "  led-off G|R     SET_LED    - turn LED off\n";
    std::cout << "\nCommands (vendor control transfers):\n";
    std::cout << "  ctrl-status     GET_STATUS - query LED states\n";
    std::cout << "  ctrl-led-on G|R SET_LED    - turn LED on\n";
    std::cout << "  ctrl-led-off G|R SET_LED   - turn LED off\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << prog_name << " --vid 0x1234 --pid 0x5678 status\n";
    std::cout << "  " << prog_name << " led-on G\n";
    std::cout << "  " << prog_name << " led-off R\n";
    std::cout << "  " << prog_name << " --vid 0x1234 --pid 0x5678 --timeout 3000 status\n";
}

static void print_response_hex(const UsbResponse& resp) {
    if (!resp.payload.empty()) {
        std::cout << "Response bytes:";
        for (uint8_t b : resp.payload) {
            std::cout << " 0x" << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<unsigned>(b) << std::dec;
        }
        std::cout << std::endl;
    }
}

/* ---------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
    uint16_t vid = 0x1234u;
    uint16_t pid = 0x5678u;
    unsigned int timeout_ms = 5000u;

    std::string command;
    std::string led_arg;

    // ---- parse arguments ----
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--vid" && i + 1 < argc) {
            if (!parse_u16_hex(argv[++i], vid)) {
                std::cerr << "Error: invalid --vid value: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--pid" && i + 1 < argc) {
            if (!parse_u16_hex(argv[++i], pid)) {
                std::cerr << "Error: invalid --pid value: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--timeout" && i + 1 < argc) {
            try {
                timeout_ms = static_cast<unsigned int>(
                    std::stoul(argv[++i], nullptr, 0));
            } catch (...) {
                std::cerr << "Error: invalid --timeout value: " << argv[i]
                          << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg[0] != '-' && command.empty()) {
            command = arg;
        } else if (arg[0] != '-' && !command.empty() && led_arg.empty()) {
            led_arg = arg;
        } else {
            std::cerr << "Error: unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    if (command.empty()) {
        std::cerr << "Error: no command specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // ---- build the modular stack ----
    auto connection = create_winusb_connection();
    auto protocol = create_led_usb_protocol();
    UsbHostController app(std::move(connection), std::move(protocol));

    // ---- connect ----
    if (!app.connect(vid, pid)) {
        return 1;
    }

    std::cout << "Connected: " << app.connection().device_info() << std::endl;
    std::cout << "Protocol:  " << app.protocol().protocol_name() << std::endl;

    // ---- dispatch command ----
    CommandResult result;

    if (command == "status") {
        result = app.get_status(timeout_ms);
    } else if (command == "led-on" || command == "led-off") {
        LedId led;
        if (!parse_led_arg(led_arg, led)) {
            std::cerr << "Error: " << command
                      << " requires a LED argument (G or R)" << std::endl;
            app.disconnect();
            return 1;
        }
        bool on = (command == "led-on");
        result = app.set_led(led, on, timeout_ms);
    } else if (command == "ctrl-status") {
        result = app.get_status_control(timeout_ms);
    } else if (command == "ctrl-led-on" || command == "ctrl-led-off") {
        LedId led;
        if (!parse_led_arg(led_arg, led)) {
            std::cerr << "Error: " << command
                      << " requires a LED argument (G or R)" << std::endl;
            app.disconnect();
            return 1;
        }
        bool on = (command == "ctrl-led-on");
        result = app.set_led_control(led, on, timeout_ms);
    } else {
        std::cerr << "Error: unknown command: " << command << std::endl;
        print_usage(argv[0]);
        app.disconnect();
        return 1;
    }

    app.disconnect();

    // ---- print result ----
    if (result.success) {
        std::cout << "OK: " << result.message << std::endl;
        print_response_hex(result.response);
        return 0;
    } else {
        std::cerr << "ERROR: " << result.message << std::endl;
        return 1;
    }
}
