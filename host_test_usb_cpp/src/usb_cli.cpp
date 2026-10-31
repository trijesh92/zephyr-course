#include "usb_cli.h"

#include <argparse/argparse.hpp>

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

/** Parse a "G" or "R" LED argument into a LedId. */
static bool parse_led_arg(const std::string& arg, LedId& led) {
    if (arg.empty()) {
        return false;
    }
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

UsbCli::UsbCli(std::unique_ptr<IUsbConnection> connection,
               std::unique_ptr<IUsbProtocol> protocol)
    : connection_(std::move(connection)), protocol_(std::move(protocol)) {}

void UsbCli::print_response_hex(const UsbResponse& resp) {
    if (!resp.payload.empty()) {
        std::cout << "Response bytes:";
        for (uint8_t b : resp.payload) {
            std::cout << " 0x" << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<unsigned>(b) << std::dec;
        }
        std::cout << std::endl;
    }
}

int UsbCli::run(int argc, char* argv[]) {
    argparse::ArgumentParser program("host_test_usb");
    program.add_description(
        "Host-side test application for Zephyr board's custom vendor USB device (WinUSB).");

    program.add_epilog(
        "Examples:\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 status\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 led-on G\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 led-off G\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 led-on R\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 led-off R\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 ctrl-status\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 ctrl-led-on G\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 ctrl-led-off R\n"
        "  host_test_usb.exe --vid 0x1234 --pid 0x5678 --timeout 3000 status");

    // ----- optional arguments -----
    program.add_argument("--vid")
        .help("USB Vendor ID (default: 0x1234)")
        .default_value<uint16_t>(0x1234u)
        .scan<'x', uint16_t>();

    program.add_argument("--pid")
        .help("USB Product ID (default: 0x5678)")
        .default_value<uint16_t>(0x5678u)
        .scan<'x', uint16_t>();

    program.add_argument("--timeout")
        .help("Timeout in milliseconds (default: 5000)")
        .default_value<unsigned int>(5000u)
        .scan<'u', unsigned int>();

    // ----- positional arguments -----
    program.add_argument("command")
        .help("Command: status | led-on G|R | led-off G|R | "
              "ctrl-status | ctrl-led-on G|R | ctrl-led-off G|R");

    program.add_argument("led")
        .help("LED target: G (Green) or R (Red)")
        .nargs(0, 1);

    // ----- parse -----
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    uint16_t vid = program.get<uint16_t>("--vid");
    uint16_t pid = program.get<uint16_t>("--pid");
    unsigned int timeout_ms = program.get<unsigned int>("--timeout");
    std::string command = program.get<std::string>("command");

    std::string led_arg;
    if (program.is_used("led")) {
        led_arg = program.get<std::string>("led");
    }

    // ----- connect -----
    if (!connection_->open(vid, pid)) {
        return 1;
    }

    std::cout << "Connected: " << connection_->device_info() << std::endl;
    std::cout << "Protocol:  " << protocol_->protocol_name() << std::endl;

    // ----- build controller on top of the injected connection/protocol -----
    UsbHostController app(std::move(connection_), std::move(protocol_));

    // ----- dispatch command -----
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
        std::cerr << program;
        app.disconnect();
        return 1;
    }

    app.disconnect();

    // ----- print result -----
    if (result.success) {
        std::cout << "OK: " << result.message << std::endl;
        print_response_hex(result.response);
        return 0;
    } else {
        std::cerr << "ERROR: " << result.message << std::endl;
        return 1;
    }
}
