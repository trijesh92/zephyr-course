#ifndef USB_CONNECTION_H
#define USB_CONNECTION_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Abstract interface for a USB connection.
 *
 * Implementations handle the low-level transport (WinUSB, libusb, etc.)
 * and are responsible for device discovery, opening/closing, and raw
 * bulk/control transfers.  This abstraction lets the rest of the
 * application stay transport-agnostic.
 */
class IUsbConnection {
public:
    IUsbConnection() = default;
    virtual ~IUsbConnection() = default;

    // Disable copying - connections own OS handles.
    IUsbConnection(const IUsbConnection&) = delete;
    IUsbConnection& operator=(const IUsbConnection&) = delete;

    /**
     * Open the first device matching the given VID/PID.
     * @return true on success.
     */
    virtual bool open(uint16_t vid, uint16_t pid) = 0;

    /** Close the connection and release all handles. */
    virtual void close() = 0;

    /** @return true if a device is currently open. */
    virtual bool is_open() const = 0;

    /**
     * Write to a bulk OUT endpoint.
     * @param endpoint_address  endpoint address (e.g. 0x01)
     * @param data              data to send
     * @param length            number of bytes to send
     * @param timeout_ms        transfer timeout
     * @return true on success.
     */
    virtual bool bulk_write(uint8_t endpoint_address,
                            const uint8_t* data,
                            size_t length,
                            unsigned int timeout_ms) = 0;

    /**
     * Read from a bulk IN endpoint.
     * @param endpoint_address  endpoint address (e.g. 0x81)
     * @param data              output buffer
     * @param length            in: buffer size, out: bytes actually read
     * @param timeout_ms        transfer timeout
     * @return true on success.
     */
    virtual bool bulk_read(uint8_t endpoint_address,
                           uint8_t* data,
                           size_t* length,
                           unsigned int timeout_ms) = 0;

    /**
     * Send a vendor control transfer.
     * @param request_type  bmRequestType
     * @param request       bRequest
     * @param value         wValue
     * @param index         wIndex
     * @param data          data stage (in/out depending on direction)
     * @param length        in: buffer size / expected length, out: transferred
     * @param timeout_ms    transfer timeout
     * @return true on success.
     */
    virtual bool control_transfer(uint8_t request_type,
                                  uint8_t request,
                                  uint16_t value,
                                  uint16_t index,
                                  uint8_t* data,
                                  uint16_t& length,
                                  unsigned int timeout_ms) = 0;

    /** Bulk OUT endpoint address discovered during open(). */
    virtual uint8_t bulk_out_address() const = 0;

    /** Bulk IN endpoint address discovered during open(). */
    virtual uint8_t bulk_in_address() const = 0;

    /** Human-readable device information (VID/PID/endpoints). */
    virtual std::string device_info() const = 0;
};

#endif // USB_CONNECTION_H
