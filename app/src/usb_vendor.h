#ifndef APP_SRC_USB_VENDOR_H_
#define APP_SRC_USB_VENDOR_H_

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the custom USB vendor device (descriptors, config, class, usbd_init/enable). */
int usb_vendor_init(void);

/* Handle a vendor control request (opcode-based dispatch). */
void usb_vendor_handle_vendor_request(uint8_t opcode, const uint8_t *data, size_t len);

/* Send a response over the bulk IN endpoint. */
void usb_vendor_send_response(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_SRC_USB_VENDOR_H_ */
