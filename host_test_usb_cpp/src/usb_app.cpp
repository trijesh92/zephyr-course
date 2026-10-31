#include "usb_app.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <windows.h> // for Sleep()

UsbHostController::UsbHostController(std::unique_ptr<IUsbConnection> connection,
                                     std::unique_ptr<IUsbProtocol> protocol)
    : connection_(std::move(connection)), protocol_(std::move(protocol)) {}

bool UsbHostController::connect(uint16_t vid, uint16_t pid) {
    return connection_->open(vid, pid);
}

void UsbHostController::disconnect() {
    connection_->close();
}

bool UsbHostController::is_connected() const {
    return connection_->is_open();
}

CommandResult UsbHostController::send_bulk(UsbCommand cmd,
                                           LedId led,
                                           bool on,
                                           unsigned int timeout_ms) {
    CommandResult result;

    auto request = protocol_->encode_request(cmd, led, on);
    if (request.empty()) {
        result.success = false;
        result.message = "Failed to encode request";
        return result;
    }

    uint8_t resp_buf[256] = {0};
    size_t resp_len = sizeof(resp_buf);

    if (!connection_->bulk_write(connection_->bulk_out_address(),
                                 request.data(), request.size(),
                                 timeout_ms)) {
        result.success = false;
        result.message = "Bulk OUT transfer failed";
        return result;
    }

    // Small delay to allow the device to process the command.
    Sleep(10);

    if (!connection_->bulk_read(connection_->bulk_in_address(),
                                resp_buf, &resp_len, timeout_ms)) {
        result.success = false;
        result.message = "Bulk IN transfer failed";
        return result;
    }

    result.response = protocol_->decode_response(resp_buf, resp_len);
    result.success = result.response.success;
    result.message = result.response.description;
    return result;
}

CommandResult UsbHostController::get_status(unsigned int timeout_ms) {
    return send_bulk(UsbCommand::GetStatus, LedId::Green, false, timeout_ms);
}

CommandResult UsbHostController::set_led(LedId led, bool on,
                                         unsigned int timeout_ms) {
    return send_bulk(UsbCommand::SetLed, led, on, timeout_ms);
}

CommandResult UsbHostController::send_control(uint8_t request_type,
                                              uint8_t request,
                                              uint16_t value,
                                              uint16_t index,
                                              const uint8_t* data,
                                              uint16_t length,
                                              unsigned int timeout_ms) {
    CommandResult result;

    uint8_t resp_buf[256] = {0};
    uint16_t resp_len = static_cast<uint16_t>(sizeof(resp_buf));

    if (!connection_->control_transfer(request_type, request, value, index,
                                       const_cast<uint8_t*>(data), length,
                                       timeout_ms)) {
        result.success = false;
        result.message = "Control transfer failed";
        return result;
    }

    // For IN control transfers, data has been filled by the driver.
    result.response = protocol_->decode_response(resp_buf, resp_len);
    result.success = result.response.success;
    result.message = result.response.description;
    return result;
}

CommandResult UsbHostController::get_status_control(unsigned int timeout_ms) {
    // bmRequestType: 1100 0000b = Device-to-Host | Vendor | Device
    constexpr uint8_t kReqTypeIn = 0xC0u;
    return send_control(kReqTypeIn,
                        static_cast<uint8_t>(UsbCommand::GetStatus),
                        0x0000, 0x0000, nullptr, 0, timeout_ms);
}

CommandResult UsbHostController::set_led_control(LedId led, bool on,
                                                 unsigned int timeout_ms) {
    // bmRequestType: 0100 0000b = Host-to-Device | Vendor | Device
    constexpr uint8_t kReqTypeOut = 0x40u;

    uint16_t value = (led == LedId::Green ? 0x0000 : 0x0100);
    value |= (on ? 0x0001 : 0x0000);

    return send_control(kReqTypeOut,
                        static_cast<uint8_t>(UsbCommand::SetLed),
                        value, 0x0000, nullptr, 0, timeout_ms);
}
