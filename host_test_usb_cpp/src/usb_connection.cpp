#include "usb_connection.h"

#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <cfgmgr32.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

// Link against the Windows SDK libraries.
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
#pragma comment(lib, "cfgmgr32.lib")

namespace {

// WinUSB device interface class GUID (matches all USB devices).
// {A5DCBF10-6530-11D2-901F-00C04FB951ED}
const GUID kGuidDevInterfaceUsbDevice = {
    0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};

/**
 * Parse VID and PID from a Windows USB device instance path.
 *
 * Path format example:
 *   \\?\usb#vid_1234&pid_5678&mi_00#{...}
 *
 * @param device_path  the symbolic link path from SetupDi
 * @param vid          output VID
 * @param pid          output PID
 * @return true if both VID and PID were successfully parsed.
 */
bool parse_vid_pid_from_path(const std::string& device_path,
                             uint16_t& vid,
                             uint16_t& pid) {
    std::string lower = device_path;
    for (auto& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    auto vid_pos = lower.find("vid_");
    auto pid_pos = lower.find("pid_");
    if (vid_pos == std::string::npos || pid_pos == std::string::npos) {
        return false;
    }

    // Extract the hex token following "vid_" / "pid_" (stops at '&' or '#').
    auto extract_hex = [&](size_t pos) -> std::string {
        std::string token = lower.substr(pos + 4); // skip "vid_" / "pid_"
        size_t end = token.find_first_of("&#");
        if (end != std::string::npos) {
            token = token.substr(0, end);
        }
        return token;
    };

    std::string vid_str = extract_hex(vid_pos);
    std::string pid_str = extract_hex(pid_pos);

    if (vid_str.empty() || pid_str.empty()) {
        return false;
    }

    try {
        vid = static_cast<uint16_t>(std::stoul(vid_str, nullptr, 16));
        pid = static_cast<uint16_t>(std::stoul(pid_str, nullptr, 16));
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

/* =========================================================================
 * WinUsbDevice - concrete IUsbConnection implementation using the WinUSB API
 * ========================================================================= */

class WinUsbDevice : public IUsbConnection {
public:
    WinUsbDevice() = default;
    ~WinUsbDevice() override { close(); }

    WinUsbDevice(const WinUsbDevice&) = delete;
    WinUsbDevice& operator=(const WinUsbDevice&) = delete;

    bool open(uint16_t vid, uint16_t pid) override;
    void close() override;
    bool is_open() const override { return file_handle_ != INVALID_HANDLE_VALUE; }

    bool bulk_write(uint8_t endpoint_address,
                    const uint8_t* data,
                    size_t length,
                    unsigned int timeout_ms) override;
    bool bulk_read(uint8_t endpoint_address,
                   uint8_t* data,
                   size_t* length,
                   unsigned int timeout_ms) override;
    bool control_transfer(uint8_t request_type,
                          uint8_t request,
                          uint16_t value,
                          uint16_t index,
                          uint8_t* data,
                          uint16_t& length,
                          unsigned int timeout_ms) override;

    uint8_t bulk_out_address() const override { return bulk_out_ep_; }
    uint8_t bulk_in_address() const override { return bulk_in_ep_; }

    std::string device_info() const override;

private:
    bool find_bulk_pipes();

    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE winusb_handle_ = nullptr;

    uint8_t bulk_out_ep_ = 0;
    uint8_t bulk_in_ep_ = 0;
    uint16_t vid_ = 0;
    uint16_t pid_ = 0;
};

bool WinUsbDevice::open(uint16_t vid, uint16_t pid) {
    // Enumerate all USB device interfaces.
    HDEVINFO device_info = SetupDiGetClassDevs(
        &kGuidDevInterfaceUsbDevice, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (device_info == INVALID_HANDLE_VALUE) {
        std::cerr << "SetupDiGetClassDevs failed: " << GetLastError()
                  << std::endl;
        return false;
    }

    bool found = false;
    DWORD member_index = 0;

    while (!found) {
        SP_DEVICE_INTERFACE_DATA iface_data = {};
        iface_data.cbSize = sizeof(iface_data);

        if (!SetupDiEnumDeviceInterfaces(device_info, nullptr,
                                         &kGuidDevInterfaceUsbDevice,
                                         member_index, &iface_data)) {
            break; // no more interfaces
        }

        // Query required buffer size for the detail data.
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetail(device_info, &iface_data, nullptr, 0,
                                        &required_size, nullptr);

        std::vector<BYTE> detail_buf(required_size);
        PSP_DEVICE_INTERFACE_DETAIL_DATA detail =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detail_buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(device_info, &iface_data, detail,
                                             required_size, nullptr, nullptr)) {
            member_index++;
            continue;
        }

        std::string device_path(detail->DevicePath);

        // Match VID/PID by parsing the device path before opening.
        uint16_t found_vid = 0, found_pid = 0;
        if (!parse_vid_pid_from_path(device_path, found_vid, found_pid) ||
            found_vid != vid || found_pid != pid) {
            member_index++;
            continue;
        }

        // Open the device handle.
        HANDLE h = CreateFile(
            detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            member_index++;
            continue;
        }

        // Initialize WinUSB.
        WINUSB_INTERFACE_HANDLE winusb_handle = nullptr;
        if (!WinUsb_Initialize(h, &winusb_handle)) {
            std::cerr << "WinUsb_Initialize failed: " << GetLastError()
                      << std::endl;
            CloseHandle(h);
            member_index++;
            continue;
        }

        // Temporarily assign so find_bulk_pipes can use the handle.
        file_handle_ = h;
        winusb_handle_ = winusb_handle;

        if (!find_bulk_pipes()) {
            std::cerr << "No bulk endpoints found on device" << std::endl;
            close();
            member_index++;
            continue;
        }

        vid_ = vid;
        pid_ = pid;
        found = true;
    }

    SetupDiDestroyDeviceInfoList(device_info);

    if (!found) {
        std::cerr << "Device not found: VID=0x" << std::hex << vid
                  << " PID=0x" << pid << std::dec << std::endl;
        std::cerr << "Make sure the board is connected and the WinUSB driver "
                     "is installed (e.g. via Zadig)." << std::endl;
        return false;
    }

    return true;
}

void WinUsbDevice::close() {
    if (winusb_handle_) {
        WinUsb_Free(winusb_handle_);
        winusb_handle_ = nullptr;
    }
    if (file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
    bulk_out_ep_ = 0;
    bulk_in_ep_ = 0;
}

bool WinUsbDevice::find_bulk_pipes() {
    if (!winusb_handle_) {
        return false;
    }

    UCHAR pipe_index = 0;
    WINUSB_PIPE_INFORMATION pipe_info = {};

    while (WinUsb_QueryPipe(winusb_handle_, 0, pipe_index, &pipe_info)) {
        // USBD_PIPE_TYPE values: Control=0, Isochronous=1, Bulk=2, Interrupt=3
        if (pipe_info.PipeType == 2) { // Bulk
            UCHAR addr = pipe_info.PipeId;
            bool is_in = (addr & 0x80) != 0;
            if (is_in) {
                bulk_in_ep_ = addr;
            } else {
                bulk_out_ep_ = addr;
            }
        }
        pipe_index++;
    }

    return (bulk_out_ep_ != 0) && (bulk_in_ep_ != 0);
}

bool WinUsbDevice::bulk_write(uint8_t endpoint_address,
                              const uint8_t* data,
                              size_t length,
                              unsigned int timeout_ms) {
    (void)timeout_ms; // WinUSB synchronous API ignores the timeout parameter
    if (!winusb_handle_) {
        return false;
    }

    ULONG transferred = 0;
    BOOL ok = WinUsb_WritePipe(winusb_handle_, endpoint_address,
                               (PUCHAR)data, (ULONG)length, &transferred, nullptr);
    return ok != FALSE;
}

bool WinUsbDevice::bulk_read(uint8_t endpoint_address,
                             uint8_t* data,
                             size_t* length,
                             unsigned int timeout_ms) {
    (void)timeout_ms; // WinUSB synchronous API ignores the timeout parameter
    if (!winusb_handle_) {
        return false;
    }

    ULONG transferred = 0;
    BOOL ok = WinUsb_ReadPipe(winusb_handle_, endpoint_address, (PUCHAR)data,
                              (ULONG)*length, &transferred, nullptr);
    if (!ok) {
        return false;
    }

    *length = static_cast<size_t>(transferred);
    return true;
}

bool WinUsbDevice::control_transfer(uint8_t request_type,
                                    uint8_t request,
                                    uint16_t value,
                                    uint16_t index,
                                    uint8_t* data,
                                    uint16_t& length,
                                    unsigned int timeout_ms) {
    (void)timeout_ms; // WinUSB synchronous API ignores the timeout parameter
    if (!winusb_handle_) {
        return false;
    }

    WINUSB_SETUP_PACKET setup = {};
    setup.RequestType = request_type;
    setup.Request = request;
    setup.Value = value;
    setup.Index = index;
    setup.Length = length;

    ULONG transferred = 0;
    BOOL ok = WinUsb_ControlTransfer(winusb_handle_, setup, data, length,
                                     &transferred, nullptr);
    if (!ok) {
        return false;
    }

    length = static_cast<uint16_t>(transferred);
    return true;
}

std::string WinUsbDevice::device_info() const {
    std::ostringstream oss;
    oss << "VID=0x" << std::hex << std::setw(4) << std::setfill('0') << vid_
        << " PID=0x" << std::setw(4) << pid_
        << " BULK_OUT=0x" << std::setw(2) << static_cast<unsigned>(bulk_out_ep_)
        << " BULK_IN=0x" << std::setw(2) << static_cast<unsigned>(bulk_in_ep_);
    return oss.str();
}

/* =========================================================================
 * Factory function
 * ========================================================================= */

std::unique_ptr<IUsbConnection> create_winusb_connection() {
    return std::make_unique<WinUsbDevice>();
}
