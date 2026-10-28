/*
 * usb_protocol.h - USB vendor device protocol definitions
 *
 * Protocol (command/response over bulk endpoints):
 *
 * Host -> Device (bulk OUT endpoint 0x01):
 *   [0] opcode  (0x01 SET_LED, 0x02 GET_STATUS)
 *   [1] arg     (LED command for SET_LED)
 *
 * Device -> Host (bulk IN endpoint 0x81):
 *   [0] status  (0x00 OK, 0xFF ERROR)
 *   [1] data    (LED states for GET_STATUS: bit0=green, bit1=red)
 *
 * Vendor control requests (bRequest = opcode) are also supported:
 *   SET_LED   (0x01): wValue = LED index (0=green,1=red), wIndex = state
 *   GET_STATUS (0x02): data stage returns led_states byte
 */

#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H

#include <cstdint>
#include <windows.h>
#include <initguid.h>

// USB device identifiers
#define USB_VENDOR_VID  1234u
#define USB_VENDOR_PID  5678u

// Bulk endpoint addresses
#define USB_VENDOR_BULK_OUT_EP_ADDR  0x01u
#define USB_VENDOR_BULK_IN_EP_ADDR   0x81u
#define USB_VENDOR_BULK_MPS          64u

// Vendor request opcodes
#define USB_VENDOR_REQ_SET_LED       0x01u
#define USB_VENDOR_REQ_GET_STATUS    0x02u

// LED commands (arg for SET_LED)
#define LED_CMD_GREEN_ON    0x01u
#define LED_CMD_GREEN_OFF   0x02u
#define LED_CMD_RED_ON      0x03u
#define LED_CMD_RED_OFF     0x04u

// Response status codes
#define LED_STATUS_OK       0x00u
#define LED_STATUS_ERROR    0xFFu

// Frame sizes
#define USB_VENDOR_CMD_FRAME_SIZE   2u
#define USB_VENDOR_RESP_FRAME_SIZE  2u

// WinUSB device interface class GUID (all USB devices)
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE,
    0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

// WinUSB interface GUID (WinUSB driver)
DEFINE_GUID(WINUSB_INTERFACE_GUID,
    0xDC5A3E3DL, 0x9B97, 0x46F6, 0x88, 0x4F, 0xD2, 0xA9, 0x4C, 0x7D, 0x5E, 0x6D);

#endif /* USB_PROTOCOL_H */
