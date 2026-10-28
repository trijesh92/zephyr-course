/*
 * main.cpp - Host-side test application for Zephyr board's custom vendor USB device
 *
 * Communicates with the custom USB vendor device using WinUSB (Windows USB API)
 * over bulk endpoints.
 *
 * Build (Windows, MinGW):
 *   mkdir build && cd build
 *   cmake -G "MinGW Makefiles" ..
 *   cmake --build .
 *
 * Build (Windows, MSVC):
 *   mkdir build && cd build
 *   cmake ..
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
 *   - Windows SDK (WinUSB API)
 *   - WinUSB driver installed on the device via Zadig
 *
 * Steps to set up on Windows:
 *   1. Download Zadig: https://zadig.akeo.ie/
 *   2. Plug in the FRDM board (VID=0x1234, PID=0x5678)
 *   3. Run Zadig -> Options -> List All Devices
 *   4. Select your device, choose "WinUSB" driver, click "Install Driver"
 *   5. Run: host_test_usb.exe --vid 0x1234 --pid 0x5678 status
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <format>

#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <cfgmgr32.h>

#include "usb_protocol.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")

/* ---------------------------------------------------------------------------
 * WinUSB device handle wrapper
 * ------------------------------------------------------------------------- */

class WinUsbDevice {
public:
    WinUsbDevice()
        : file_handle_(INVALID_HANDLE_VALUE)
        , winusb_handle_(NULL)
        , interface_index_(0)
    {}

    ~WinUsbDevice() {
        close();
    }

    // Open the first device matching VID/PID that has a WinUSB interface
    bool open(uint16_t vid, uint16_t pid) {
        HDEVINFO device_info = SetupDiGetClassDevs(
            &GUID_DEVINTERFACE_USB_DEVICE,
            nullptr, nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (device_info == INVALID_HANDLE_VALUE) {
            std::cerr << "SetupDiGetClassDevs failed: "
                      << GetLastError() << std::endl;
            return false;
        }

        bool found = false;
        DWORD member_index = 0;

        while (!found) {
            SP_DEVICE_INTERFACE_DATA iface_data = {};
            iface_data.cbSize = sizeof(iface_data);

            if (!SetupDiEnumDeviceInterfaces(
                    device_info, nullptr,
                    &GUID_DEVINTERFACE_USB_DEVICE,
                    member_index, &iface_data)) {
                break; // No more interfaces
            }

            // Get required buffer size for detail data
            DWORD required_size = 0;
            SetupDiGetDeviceInterfaceDetail(
                device_info, &iface_data, nullptr, 0,
                &required_size, nullptr);

            std::vector<BYTE> detail_buf(required_size);
            PSP_DEVICE_INTERFACE_DETAIL_DATA detail =
                reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detail_buf.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            if (!SetupDiGetDeviceInterfaceDetail(
                    device_info, &iface_data, detail,
                    required_size, nullptr, nullptr)) {
                member_index++;
                continue;
            }

            // Open the device
            HANDLE h = CreateFile(
                detail->DevicePath,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);

            if (h == INVALID_HANDLE_VALUE) {
                member_index++;
                continue;
            }

            // Check VID/PID by reading device properties
            std::string path(detail->DevicePath);
            std::string s = std::format("{:}", pid); // s == 2a

            int res = path.find(s);
            if (res == std::string::npos)
            {
              CloseHandle(h);
              member_index++;
              continue;
            }

            // Initialize WinUSB
            if (!WinUsb_Initialize(h, &winusb_handle_)) {
                std::cerr << "WinUsb_Initialize failed: " << GetLastError() << std::endl;
                CloseHandle(h);
                member_index++;
                continue;
            }

            file_handle_ = h;
            found = true;
            break;
        }

        SetupDiDestroyDeviceInfoList(device_info);

        if (!found) {
            std::cerr << "Device not found: VID=0x" << std::hex << vid
                      << " PID=0x" << pid << std::dec << std::endl;
            std::cerr << "Make sure the board is connected and WinUSB driver is installed." << std::endl;
            return false;
        }

        return true;
    }

    void close() {
        if (winusb_handle_) {
            WinUsb_Free(winusb_handle_);
            winusb_handle_ = nullptr;
        }
        if (file_handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_handle_);
            file_handle_ = INVALID_HANDLE_VALUE;
        }
    }

    // Send a bulk command and read the response
    int bulk_transfer(const uint8_t* cmd, size_t cmd_len,
                      uint8_t* resp, size_t* resp_len,
                      unsigned int timeout_ms) {
        if (!winusb_handle_) return -1;

        ULONG transferred = 0;

        // Write command to bulk OUT endpoint
        BOOL ok = WinUsb_WritePipe(
            winusb_handle_,
            USB_VENDOR_BULK_OUT_EP_ADDR,
            (PUCHAR)cmd, (ULONG)cmd_len,
            &transferred, nullptr);
        if (!ok) {
            std::cerr << "Bulk OUT transfer failed: " << GetLastError() << std::endl;
            return -1;
        }

        // Small delay to let device process
        Sleep(10);

        // Read response from bulk IN endpoint
        ULONG resp_transferred = 0;
        ok = WinUsb_ReadPipe(
            winusb_handle_,
            USB_VENDOR_BULK_IN_EP_ADDR,
            (PUCHAR)resp, (ULONG)*resp_len,
            &resp_transferred, nullptr);
        if (!ok) {
            std::cerr << "Bulk IN transfer failed: " << GetLastError() << std::endl;
            return -1;
        }

        *resp_len = resp_transferred;
        return 0;
    }

    // Send a vendor control request
    int control_transfer(uint8_t request_type, uint8_t request,
                         uint16_t value, uint16_t index,
                         uint8_t* data, uint16_t length,
                         unsigned int timeout_ms) {
        if (!winusb_handle_) return -1;

        WINUSB_SETUP_PACKET setup = {};
        setup.RequestType = request_type;
        setup.Request = request;
        setup.Value = value;
        setup.Index = index;
        setup.Length = length;

        ULONG transferred = 0;
        BOOL ok = WinUsb_ControlTransfer(
            winusb_handle_, setup, data, length,
            &transferred, nullptr);
        if (!ok) {
            std::cerr << "Control transfer failed: " << GetLastError() << std::endl;
            return -1;
        }
        return (int)transferred;
    }

private:
    // Check if the opened device matches the given VID/PID
    bool check_vid_pid(HANDLE h, uint16_t vid, uint16_t pid) {
        // Use WinUsb_QueryDeviceInformation isn't available for VID/PID directly.
        // Instead, we open the device properties via SetupAPI.
        // For simplicity, we assume the first WinUSB device matching VID/PID
        // found through SetupDi is ours. Since CreateFile already opened it,
        // we can't easily re-query VID/PID here without the device info set.
        // So we accept any WinUSB device opened this way.
        // A more robust implementation would match VID/PID in the enumeration
        // loop using device instance ID properties.
        (void)h; (void)vid; (void)pid;
        return true;
    }

    HANDLE file_handle_;
    WINUSB_INTERFACE_HANDLE winusb_handle_;
    uint8_t interface_index_;
};

/* ---------------------------------------------------------------------------
 * Command handlers
 * ------------------------------------------------------------------------- */

int cmd_status(WinUsbDevice& usb, unsigned int timeout_ms) {
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
        std::cout << " 0x" << std::hex << std::setw(2) << std::setfill('0')
                  << (unsigned int)resp[i] << std::dec;
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

int cmd_set_led(WinUsbDevice& usb, const std::string& subcmd, char led_letter,
                unsigned int timeout_ms) {
    uint8_t led_byte;
    if (subcmd == "led-on") {
        led_byte = (led_letter == 'G' || led_letter == 'g') ? LED_CMD_GREEN_ON
                                                            : LED_CMD_RED_ON;
    } else { // led-off
        led_byte = (led_letter == 'G' || led_letter == 'g') ? LED_CMD_GREEN_OFF
                                                            : LED_CMD_RED_OFF;
    }

    if (led_byte == 0x00u || led_byte == 0xFFu) {
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
        std::cout << " 0x" << std::hex << std::setw(2) << std::setfill('0')
                  << (unsigned int)resp[i] << std::dec;
    }
    std::cout << std::endl;

    if (resp_len >= 1 && resp[0] == LED_STATUS_OK) {
        std::cout << "OK: " << led_name << " LED " << action << std::endl;
        return 0;
    }

    std::cout << "ERROR: command failed" << std::endl;
    return 1;
}

int cmd_control_status(WinUsbDevice& usb, unsigned int timeout_ms) {
    // Device-to-Host, Vendor, Device recipient
    uint8_t request_type = 0xC0; // 1100 0000: Dir=1(Device->Host), Type=2(Vendor), Rec=0(Device)
    uint8_t data[1] = {0};

    std::cout << "Sending GET_STATUS via control transfer..." << std::endl;

    int ret = usb.control_transfer(request_type,
                                   USB_VENDOR_REQ_GET_STATUS,
                                   0x0000, 0x0000,
                                   data, sizeof(data),
                                   timeout_ms);
    if (ret < 0) return 1;

    std::cout << "Response: 0x" << std::hex << std::setw(2) << std::setfill('0')
              << (unsigned int)data[0] << std::dec << std::endl;
    std::cout << "LED states: green="
              << ((data[0] & 0x01) ? "ON" : "OFF")
              << ", red="
              << ((data[0] & 0x02) ? "ON" : "OFF")
              << std::endl;

    return 0;
}

int cmd_control_set_led(WinUsbDevice& usb, char led_letter, bool on,
                        unsigned int timeout_ms) {
    // Host-to-Device, Vendor, Device recipient
    uint8_t request_type = 0x40; // 0100 0000: Dir=0(Host->Device), Type=2(Vendor), Rec=0(Device)

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

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options] <command>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --vid VID       USB Vendor ID (default: 0x1234)\n";
    std::cout << "  --pid PID       USB Product ID (default: 0x5678)\n";
    std::cout << "  --timeout MS    Timeout in milliseconds (default: 5000)\n";
    std::cout << "\nCommands:\n";
    std::cout << "  status          GET_STATUS - query LED states (bulk)\n";
    std::cout << "  led-on G        SET_LED    - turn Green LED on (bulk)\n";
    std::cout << "  led-off G       SET_LED    - turn Green LED off (bulk)\n";
    std::cout << "  led-on R        SET_LED    - turn Red LED on (bulk)\n";
    std::cout << "  led-off R       SET_LED    - turn Red LED off (bulk)\n";
    std::cout << "  ctrl-status     GET_STATUS - query LED states (control transfer)\n";
    std::cout << "  ctrl-led-on G   SET_LED    - turn Green LED on (control transfer)\n";
    std::cout << "  ctrl-led-off G  SET_LED    - turn Green LED off (control transfer)\n";
    std::cout << "  ctrl-led-on R   SET_LED    - turn Red LED on (control transfer)\n";
    std::cout << "  ctrl-led-off R  SET_LED    - turn Red LED off (control transfer)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << prog_name << " --vid 0x1234 --pid 0x5678 status\n";
    std::cout << "  " << prog_name << " led-on G\n";
    std::cout << "  " << prog_name << " led-off R\n";
}

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

    // Open USB device via WinUSB
    WinUsbDevice usb;
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
