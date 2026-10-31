#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Enumeration of logical USB commands supported by the protocol.
 *
 * Keeping commands as an enum (rather than raw opcodes) decouples the
 * application from any particular wire encoding.
 */
enum class UsbCommand {
    GetStatus = 0,
    SetLed = 1
};

/**
 * @brief LED identifier.
 */
enum class LedId {
    Green = 0,
    Red = 1
};

/**
 * @brief Decoded USB response.
 *
 * The protocol implementation fills this in from raw wire bytes.
 */
struct UsbResponse {
    bool success = false;          ///< true if the device reported success
    uint8_t status_code = 0;       ///< raw status byte from the device
    std::vector<uint8_t> payload;  ///< additional response data bytes
    std::string description;       ///< human-readable description
};

/**
 * @brief Abstract interface for a USB protocol.
 *
 * A protocol is responsible for:
 *   - encoding a logical command into the wire frame (host -> device)
 *   - decoding a raw wire frame into a structured UsbResponse (device -> host)
 *
 * Deriving a new class from IUsbProtocol allows supporting a completely
 * different device protocol without touching the connection or application
 * layers (Open-Closed Principle).
 */
class IUsbProtocol {
public:
    IUsbProtocol() = default;
    virtual ~IUsbProtocol() = default;

    // Disable copying.
    IUsbProtocol(const IUsbProtocol&) = delete;
    IUsbProtocol& operator=(const IUsbProtocol&) = delete;

    /**
     * Encode a logical command into the wire frame.
     * @param cmd    logical command
     * @param led    LED target (used by SetLed)
     * @param led_on whether the LED should be turned on (used by SetLed)
     * @return wire frame bytes ready to send over the transport.
     */
    virtual std::vector<uint8_t> encode_request(UsbCommand cmd,
                                                LedId led,
                                                bool led_on) = 0;

    /**
     * Decode a raw response frame into structured data.
     * @param data   raw response bytes
     * @param length number of valid bytes in data
     * @return decoded response.
     */
    virtual UsbResponse decode_response(const uint8_t* data, size_t length) = 0;

    // Convenience encoders for the most common operations.
    virtual std::vector<uint8_t> encode_get_status() = 0;
    virtual std::vector<uint8_t> encode_set_led(LedId led, bool on) = 0;

    /// Maximum size of a request frame (in bytes).
    virtual size_t max_request_size() const = 0;

    /// Maximum size of a response frame (in bytes).
    virtual size_t max_response_size() const = 0;

    /// Human-readable protocol name (for logging / CLI display).
    virtual std::string protocol_name() const = 0;
};

#endif // USB_PROTOCOL_H
