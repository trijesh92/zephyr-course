#include "usb_protocol.h"

#include <sstream>
#include <iomanip>

namespace {

// ------------------------------------------------------------------
// LedUsbProtocol constants (the wire encoding used by the Zephyr device)
// ------------------------------------------------------------------

constexpr uint8_t kReqSetLed = 0x01u;
constexpr uint8_t kReqGetStatus = 0x02u;

constexpr uint8_t kStatusOk = 0x00u;
constexpr uint8_t kStatusError = 0xFFu;

constexpr uint8_t kLedGreenOn = 0x01u;
constexpr uint8_t kLedGreenOff = 0x02u;
constexpr uint8_t kLedRedOn = 0x03u;
constexpr uint8_t kLedRedOff = 0x04u;

constexpr size_t kCmdFrameSize = 2u;
constexpr size_t kRespFrameSize = 2u;

} // namespace

/* =========================================================================
 * LedUsbProtocol - concrete protocol for the existing Zephyr vendor device
 *
 * Wire format (bulk endpoints):
 *   Host -> Device : [opcode][arg]
 *     opcode 0x01 = SET_LED, arg = LED command byte
 *     opcode 0x02 = GET_STATUS, arg = 0x00
 *   Device -> Host : [status][data]
 *     status 0x00 = OK, 0xFF = ERROR
 *     data      = LED states (bit0=green, bit1=red)
 * ========================================================================= */

class LedUsbProtocol : public IUsbProtocol {
public:
    // IUsbProtocol interface
    std::vector<uint8_t> encode_request(UsbCommand cmd,
                                        LedId led,
                                        bool led_on) override;

    UsbResponse decode_response(const uint8_t* data, size_t length) override;

    std::vector<uint8_t> encode_get_status() override;
    std::vector<uint8_t> encode_set_led(LedId led, bool on) override;

    size_t max_request_size() const override { return kCmdFrameSize; }
    size_t max_response_size() const override { return kRespFrameSize; }
    std::string protocol_name() const override {
        return "LedUsbProtocol (bulk 2-byte frame)";
    }
};

std::vector<uint8_t> LedUsbProtocol::encode_request(UsbCommand cmd,
                                                    LedId led,
                                                    bool led_on) {
    switch (cmd) {
        case UsbCommand::GetStatus:
            return encode_get_status();
        case UsbCommand::SetLed:
            return encode_set_led(led, led_on);
    }
    return {}; // unreachable
}

std::vector<uint8_t> LedUsbProtocol::encode_get_status() {
    return {kReqGetStatus, 0x00};
}

std::vector<uint8_t> LedUsbProtocol::encode_set_led(LedId led, bool on) {
    uint8_t arg;
    if (led == LedId::Green) {
        arg = on ? kLedGreenOn : kLedGreenOff;
    } else {
        arg = on ? kLedRedOn : kLedRedOff;
    }
    return {kReqSetLed, arg};
}

UsbResponse LedUsbProtocol::decode_response(const uint8_t* data,
                                            size_t length) {
    UsbResponse resp;

    if (length < 1) {
        resp.success = false;
        resp.status_code = kStatusError;
        resp.description = "Response too short (expected at least 1 byte)";
        return resp;
    }

    resp.status_code = data[0];
    resp.success = (data[0] == kStatusOk);

    if (length >= 2) {
        resp.payload.push_back(data[1]);
        bool green = (data[1] & 0x01) != 0;
        bool red = (data[1] & 0x02) != 0;
        resp.description =
            "Status=OK, LED states: green=" + std::string(green ? "ON" : "OFF") +
            ", red=" + std::string(red ? "ON" : "OFF");
    } else {
        resp.description = resp.success ? "Status=OK" : "Status=ERROR";
    }

    return resp;
}

/* =========================================================================
 * Factory function
 * ========================================================================= */

std::unique_ptr<IUsbProtocol> create_led_usb_protocol() {
    return std::make_unique<LedUsbProtocol>();
}
