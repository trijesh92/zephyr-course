/*
 * main.cpp - Host-side test application for Zephyr board's custom vendor USB device
 *
 * Communicates with the custom USB vendor device using libusb-1.0 over bulk endpoints.
 *
 * Build (Windows):
 *   mkdir build && cd build
 *   cmake -G "MinGW Makefiles" ..
 *   cmake --build .
 *
 * Usage:
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 status
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 led-on G
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 led-off G
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 led-on R
 *   host_test_usb.exe --vid 0x1234 --pid 0x5678 led-off R
 *
 * Requirements:
 *   - libusb-1.0 development package installed
 *   - WinUSB or libusb-win32 driver installed on the device via Zadig
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

#include <libusb-1.0/libusb.h>

#include "usb_protocol.h"

/* ---------------------------------------------------------------------------
 * Helper functions
 * ------------------------------------------------------------------------- */

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options] <command>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --vid VID       USB Vendor ID (default: 0x1234)\n";
    std::cout << "  --pid PID       USB Product ID (default: 0x5678)\n";
    std::cout << "  --timeout MS    Timeout in milliseconds (default: 5000)\n";
    std::cout << "\nCommands:\n";
    std::cout << "  status          GET_STATUS - query LED states\n";
    std::cout << "  led-on G        SET_LED    - turn Green LED on\n";
    std::cout << "  led-off G       SET_LED    - turn Green LED off\n";
    std::cout << "  led-on R        SET_LED    - turn Red LED on\n";
    std::cout << "  led-off R       SET_LED    - turn Red LED off\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << prog_name << " --vid 0x1234 --pid 0x5678 status\n";
    std::cout << "  " << prog_name << " led-on G\n";
    std::cout << "  " << prog_name << " led-off R\n";
}

static uint8_t led_arg_from_cmd(const std::string& cmd) {
    if (cmd == "led-on" || cmd == "led-off") {
        return 0; // will be set by caller based on LED letter
    }
    return 0;
}

static uint8_t led_cmd_byte(const std::string& subcmd, char led_letter) {
    uint8_t base = (subcmd == "led-on") ? 0x00u : 0x01u; // 0x00=ON base, 0x01=OFF base

    if (led_letter == 'G' || led_letter == 'g') {
        return base ? LED_CMD_GREEN_OFF : LED_CMD_GREEN_ON;
    } else if (led_letter == 'R' || led_letter == 'r') {
        return base ? LED_CMD_RED_OFF : LED_CMD_RED_ON;
    }

    return 0xFFu; // invalid
}

/* ---------------------------------------------------------------------------
 * USB device handle wrapper
 * ------------------------------------------------------------------------- */

class UsbDevice {
public:
    UsbDevice() : dev_handle_(nullptr), ctx_(nullptr) {}

    ~UsbDevice() {
        close();
    }

    bool open(uint16_t vid, uint16_t pid) {
        int ret = libusb_init(&ctx_);
        if (ret < 0) {
            std::cerr << "Failed to initialize libusb: "
                      << libusb_error_name(ret) << std::endl;
            return false;
        }

        dev_handle_ = libusb_open_device_with_vid_pid(ctx_, vid, pid);
        if (!dev_handle_) {
            std::cerr << "Device not found: VID=0x" << std::hex << vid
                      << " PID=0x" << pid << std::dec << std::endl;
            std::cerr << "Make sure the board is connected and enumerating." << std::endl;
            libusb_exit(ctx_);
            ctx_ = nullptr;
            return false;
        }

        // Claim interface 0 (vendor-specific interface)
        ret = libusb_claim_interface(dev_handle_, 0);
        if (ret < 0) {
            std::cerr << "Failed to claim interface: "
                      << libusb_error_name(ret) << std::endl;
            libusb_close(dev_handle_);
            dev_handle_ = nullptr;
            libusb_exit(ctx_);
            ctx_ = nullptr;
            return false;
        }

        return true;
    }

    void close() {
        if (dev_handle_) {
            libusb_release_interface(dev_handle_, 0);
            libusb_close(dev_handle_);
            dev_handle_ = nullptr;
        }
        if (ctx_) {
            libusb_exit(ctx_);
            ctx_ = nullptr;
        }
    }

    // Send a bulk command and read the response
    int bulk_transfer(const uint8_t* cmd, size_t cmd_len,
                      uint8_t* resp, size_t* resp_len,
                      unsigned int timeout_ms) {
        if (!dev_handle_) return -1;

        int transferred = 0;
        int ret = libusb_bulk_transfer(dev_handle_,
                                       USB_VENDOR_BULK_OUT_EP_ADDR,
                                       (unsigned char*)cmd, cmd_len,
                                       &transferred, timeout_ms);
        if (ret < 0) {
            std::cerr << "Bulk OUT transfer failed: "
                      << libusb_error_name(ret) << std::endl;
            return ret;
        }

        // Small delay to let device process
        libusb_handle_events_timeout_completed(ctx_, nullptr, nullptr);

        size_t resp_size = *resp_len;
        transferred = 0;
        ret = libusb_bulk_transfer(dev_handle_,
                                   USB_VENDOR_BULK_IN_EP_ADDR,
                                   (unsigned char*)resp, resp_size,
                                   &transferred, timeout_ms);
        if (ret < 0) {
            std::cerr << "Bulk IN transfer failed: "
                      << libusb_error_name(ret) << std::endl;
            return ret;
        }

        *resp_len = transferred;
        return 0;
    }

    // Send a vendor control request
    int control_transfer(uint8_t request_type, uint8_t request,
                         uint16_t value, uint16_t index,
                         uint8_t* data, uint16_t length,
                         unsigned int timeout_ms) {
        if (!dev_handle_) return -1;

        int ret = libusb_control_transfer(dev_handle_,
                                          request_type, request,
                                          value, index,
                                          data, length,
                                          timeout_ms);
        if (ret < 0) {
            std::cerr << "Control transfer failed: "
                      << libusb_error_name(ret) << std::endl;
            return ret;
        }
        return ret;
    }

private:
    libusb_device_handle* dev_handle_;
    libusb_context* ctx_;
};

/* ---------------------------------------------------------------------------
 * Command handlers
 * ------------------------------------------------------------------------- */

int cmd_status(UsbDevice& usb, unsigned int timeout_ms) {
    uint8_t cmd[USB_VENDOR_CMD_FRAME_SIZE] = {
        USB_VENDOR_REQ_GET_STATUS, 0x00
    };
    uint8_t resp[USB_VENDOR_RESP_FRAME_SIZE] = {0};
    size_t resp_len = sizeof(resp);

    std::cout << "Sending GET_STATUS via bulk..." << std::endl;

    int ret = usb.bulk_transfer(cmd, sizeof(cmd), resp, &resp_len, timeout_ms);
    if (ret < 0) return 1;

    std::cout << "Response:";
    for (size_t i = 0; i < resp_len; i++) {
        std::cout << " 0x" << std::hex << (unsigned int)resp[i] << std::dec;
    }
    std::cout << std::endl;

    if (resp_len >= 1 && resp[0] == LED_STATUS_OK) {
        if (resp_len >= 2) {
            std::cout << "LED states: green="
                      << ((resp[1] & 0x01) ? "ON" : "OFF")
                      << ", red="
                      << ((resp[1] & 0x02) ? "ON" : "OFF")
                      << std::endl;
        }
        return 0;
    }

    std::cout << "ERROR: unexpected response" << std::endl;
    return 1;
}

int cmd_set_led(UsbDevice& usb, const std::string& subcmd, char led_letter,
                unsigned int timeout_ms) {
    uint8_t led_byte = led_cmd_byte(subcmd, led_letter);
    if (led_byte == 0xFFu) {
        std::cerr << "Invalid LED command: " << subcmd << " " << led_letter << std::endl;
        return 1;
    }

    uint8_t cmd[USB_VENDOR_CMD_FRAME_SIZE] = {
        USB_VENDOR_REQ_SET_LED, led_byte
    };
    uint8_t resp[USB_VENDOR_RESP_FRAME_SIZE] = {0};
    size_t resp_len = sizeof(resp);

    std::string led_name = (led_letter == 'G' || led_letter == 'g') ? "Green" : "Red";
    std::string action = (subcmd == "led-on") ? "ON" : "OFF";

    std::cout << "Sending SET_LED " << led_name << " " << action
              << " via bulk..." << std::endl;

    int ret = usb.bulk_transfer(cmd, sizeof(cmd), resp, &resp_len, timeout_ms);
    if (ret < 0) return 1;

    std::cout << "Response:";
    for (size_t i = 0; i < resp_len; i++) {
        std::cout << " 0x" << std::hex << (unsigned int)resp[i] << std::dec;
    }
    std::cout << std::endl;

    if (resp_len >= 1 && resp[0] == LED_STATUS_OK) {
        std::cout << "OK: " << led_name << " LED " << action << std::endl;
        return 0;
    }

    std::cout << "ERROR: command failed" << std::endl;
    return 1;
}

int cmd_control_status(UsbDevice& usb, unsigned int timeout_ms) {
    // Device-to-Host, Vendor, Device recipient
    uint8_t request_type = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                           LIBUSB_RECIPIENT_DEVICE;
    uint8_t data[1] = {0};

    std::cout << "Sending GET_STATUS via control transfer..." << std::endl;

    int ret = usb.control_transfer(request_type,
                                   USB_VENDOR_REQ_GET_STATUS,
                                   0x0000, 0x0000,
                                   data, sizeof(data),
                                   timeout_ms);
    if (ret < 0) return 1;

    std::cout << "Response: 0x" << std::hex << (unsigned int)data[0] << std::dec
              << std::endl;
    std::cout << "LED states: green="
              << ((data[0] & 0x01) ? "ON" : "OFF")
              << ", red="
              << ((data[0] & 0x02) ? "ON" : "OFF")
              << std::endl;

    return 0;
}

int cmd_control_set_led(UsbDevice& usb, char led_letter, bool on,
                        unsigned int timeout_ms) {
    // Host-to-Device, Vendor, Device recipient
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
                           LIBUSB_RECIPIENT_DEVICE;

    uint16_t value = (led_letter == 'G' || led_letter == 'g') ? 0x0000 : 0x0100;
    value |= (on ? 0x0001 : 0x0000);

    std::string led_name = (led_letter == 'G' || led_letter == 'g') ? "Green" : "Red";
    std::string action = on ? "ON" : "OFF";

    std::cout << "Sending SET_LED " << led_name << " " << action
              << " via control transfer..." << std::endl;

    int ret = usb.control_transfer(request_type,
                                   USB_VENDOR_REQ_SET_LED,
                                   value, 0x0000,
                                   nullptr, 0,
                                   timeout_ms);
    if (ret < 0) return 1;

    std::cout << "OK: " << led_name << " LED " << action << std::endl;
    return 0;
}

/* ---------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
    uint16_t vid = USB_VENDOR_VID;
    uint16_t pid = USB_VENDOR_PID;
    unsigned int timeout_ms = 5000;

    // Parse arguments
    int arg_idx = 1;
    std::string command;

    while (arg_idx < argc) {
        std::string arg = argv[arg_idx];

        if (arg == "--vid" && arg_idx + 1 < argc) {
            vid = (uint16_t)strtoul(argv[++arg_idx], nullptr, 0);
        } else if (arg == "--pid" && arg_idx + 1 < argc) {
            pid = (uint16_t)strtoul(argv[++arg_idx], nullptr, 0);
        } else if (arg == "--timeout" && arg_idx + 1 < argc) {
            timeout_ms = (unsigned int)strtoul(argv[++arg_idx], nullptr, 0);
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg[0] != '-') {
            command = arg;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
        arg_idx++;
    }

    if (command.empty()) {
        std::cerr << "Error: no command specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // Open USB device
    UsbDevice usb;
    if (!usb.open(vid, pid)) {
        return 1;
    }

    std::cout << "Connected to USB vendor device (VID=0x"
              << std::hex << vid << " PID=0x" << pid << std::dec << ")"
              << std::endl;

    int result = 0;

    // Dispatch command
    if (command == "status") {
        result = cmd_status(usb, timeout_ms);
    } else if (command == "led-on" || command == "led-off") {
        if (arg_idx >= argc) {
            std::cerr << "Error: " << command << " requires a LED argument (G or R)"
                      << std::endl;
            result = 1;
        } else {
            char led_letter = argv[arg_idx][0];
            result = cmd_set_led(usb, command, led_letter, timeout_ms);
        }
    } else if (command == "ctrl-status") {
        result = cmd_control_status(usb, timeout_ms);
    } else if (command == "ctrl-led-on" || command == "ctrl-led-off") {
        if (arg_idx >= argc) {
            std::cerr << "Error: " << command << " requires a LED argument (G or R)"
                      << std::endl;
            result = 1;
        } else {
            char led_letter = argv[arg_idx][0];
            bool on = (command == "ctrl-led-on");
            result = cmd_control_set_led(usb, led_letter, on, timeout_ms);
        }
    } else {
        std::cerr << "Error: unknown command: " << command << std::endl;
        print_usage(argv[0]);
        result = 1;
    }

    usb.close();
    return result;
}
