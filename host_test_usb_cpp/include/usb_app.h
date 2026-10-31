#ifndef USB_APP_H
#define USB_APP_H

#include "usb_connection.h"
#include "usb_protocol.h"

#include <cstdint>
#include <memory>
#include <string>

/**
 * @brief Result of a USB command execution.
 */
struct CommandResult {
    bool success = false;
    std::string message;
    UsbResponse response;
};

/**
 * @brief Application controller that wires a transport connection to a
 *        protocol implementation.
 *
 * This class exposes domain-specific operations (get_status, set_led) over
 * both bulk endpoints and vendor control transfers.  It knows nothing about
 * WinUSB internals or wire byte layouts - those are delegated to the
 * injected IUsbConnection and IUsbProtocol instances.
 */
class UsbHostController {
public:
    UsbHostController(std::unique_ptr<IUsbConnection> connection,
                      std::unique_ptr<IUsbProtocol> protocol);
    ~UsbHostController() = default;

    // Non-copyable, non-movable.
    UsbHostController(const UsbHostController&) = delete;
    UsbHostController& operator=(const UsbHostController&) = delete;

    /** Open the USB device. */
    bool connect(uint16_t vid, uint16_t pid);

    /** Close the USB device. */
    void disconnect();

    /** @return true if a device is currently connected. */
    bool is_connected() const;

    // ----- bulk endpoint operations -----

    /** Query device LED status over bulk endpoints. */
    CommandResult get_status(unsigned int timeout_ms = 5000);

    /** Set an LED on/off over bulk endpoints. */
    CommandResult set_led(LedId led, bool on, unsigned int timeout_ms = 5000);

    // ----- vendor control transfer operations -----

    /** Query device LED status over a vendor control transfer. */
    CommandResult get_status_control(unsigned int timeout_ms = 5000);

    /** Set an LED on/off over a vendor control transfer. */
    CommandResult set_led_control(LedId led, bool on, unsigned int timeout_ms = 5000);

    const IUsbConnection& connection() const { return *connection_; }
    const IUsbProtocol& protocol() const { return *protocol_; }

private:
    CommandResult send_bulk(UsbCommand cmd, LedId led, bool on,
                            unsigned int timeout_ms);

    CommandResult send_control(uint8_t request_type, uint8_t request,
                               uint16_t value, uint16_t index,
                               const uint8_t* data, uint16_t length,
                               unsigned int timeout_ms);

    std::unique_ptr<IUsbConnection> connection_;
    std::unique_ptr<IUsbProtocol> protocol_;
};

#endif // USB_APP_H
