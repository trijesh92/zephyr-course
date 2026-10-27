/*
 * Custom USB vendor device class with bulk IN/OUT endpoints.
 *
 * Protocol (command/response over bulk endpoints):
 *
 * Host -> Device (bulk OUT):
 *   [0] opcode  (0x01 SET_LED, 0x02 GET_STATUS)
 *   [1] arg     (LED command for SET_LED, 0 for GET_STATUS)
 *
 * Device -> Host (bulk IN):
 *   [0] status  (0x00 OK, 0xFF ERROR)
 *   [1] data    (LED states for GET_STATUS: bit0=green, bit1=red)
 *
 * Vendor control requests (bRequest = opcode) are also supported:
 *   SET_LED   (0x01): wValue = LED index (0=green,1=red), wIndex = state
 *   GET_STATUS (0x02): data stage returns led_states byte
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_ch9.h>

#include "usb_vendor.h"

LOG_MODULE_REGISTER(usb_vendor, LOG_LEVEL_INF);

#define USB_VENDOR_VID 0x1234u
#define USB_VENDOR_PID 0x5678u
#define USB_VENDOR_BULK_MPS 64u

#define USB_VENDOR_REQ_SET_LED   0x01u
#define USB_VENDOR_REQ_GET_STATUS 0x02u

#define USB_VENDOR_BULK_OUT_EP_ADDR 0x01u
#define USB_VENDOR_BULK_IN_EP_ADDR  0x81u

#define USB_VENDOR_FRAME_MAX 64u

#define LED_CMD_GREEN_ON   0x01u
#define LED_CMD_GREEN_OFF  0x02u
#define LED_CMD_RED_ON     0x03u
#define LED_CMD_RED_OFF    0x04u

#define LED_STATUS_OK      0x00u
#define LED_STATUS_ERROR   0xFFu

#define VENDOR_STATE_ENABLED   BIT(0)
#define VENDOR_STATE_RX_BUSY   BIT(1)
#define VENDOR_STATE_TX_BUSY   BIT(2)

#define GREEN_LED_NODE DT_NODELABEL(green_led)
#define RED_LED_NODE   DT_NODELABEL(red_led)

static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);
static const struct gpio_dt_spec red_led   = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);

/* -----------------------------------------------------------------------------------
 * Private data
 * -------------------------------------------------------------------------------- */
struct usb_vendor_data {
    uint8_t bulk_out_ep;
    uint8_t bulk_in_ep;
    atomic_t state;
    uint8_t led_states;
    uint8_t tx_buf[USB_VENDOR_FRAME_MAX];
    size_t tx_len;
};

static struct usb_vendor_data g_vendor;

/* -----------------------------------------------------------------------------------
 * Descriptors: language, manufacturer, product, configuration
 * -------------------------------------------------------------------------------- */
USBD_DESC_LANG_DEFINE(vendor_lang);
USBD_DESC_MANUFACTURER_DEFINE(vendor_mfr, "Zephyr Vendor");
USBD_DESC_PRODUCT_DEFINE(vendor_prd, "Vendor App");
USBD_DESC_CONFIG_DEFINE(vendor_cfg_str, "Vendor Config");

USBD_CONFIGURATION_DEFINE(vendor_cfg, USB_SCD_SELF_POWERED, 100, &vendor_cfg_str);

USBD_DEVICE_DEFINE(vendor_usb,
                   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   USB_VENDOR_VID, USB_VENDOR_PID);

/* -----------------------------------------------------------------------------------
 * Class descriptors: interface + bulk IN/OUT endpoints (FS)
 * -------------------------------------------------------------------------------- */
struct vendor_desc {
    struct usb_association_descriptor iad;
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_out_ep;
    struct usb_ep_descriptor if0_in_ep;
    struct usb_desc_header nil_desc;
};

static struct vendor_desc vendor_fs_desc = {
    .iad = {
        .bLength = sizeof(struct usb_association_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE_ASSOC,
        .bFirstInterface = 0,
        .bInterfaceCount = 1,
        .bFunctionClass = USB_BCC_VENDOR,
        .bFunctionSubClass = 0,
        .bFunctionProtocol = 0,
        .iFunction = 0,
    },
    .if0 = {
        .bLength = sizeof(struct usb_if_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_BCC_VENDOR,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,
    },
    .if0_out_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = USB_VENDOR_BULK_OUT_EP_ADDR,
        .bmAttributes = USB_EP_TYPE_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(USB_VENDOR_BULK_MPS),
        .bInterval = 0x00,
    },
    .if0_in_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = USB_VENDOR_BULK_IN_EP_ADDR,
        .bmAttributes = USB_EP_TYPE_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(USB_VENDOR_BULK_MPS),
        .bInterval = 0x00,
    },
    .nil_desc = {
        .bLength = 0,
        .bDescriptorType = 0,
    },
};

static const struct usb_desc_header *vendor_fs_descs[] = {
    (struct usb_desc_header *)&vendor_fs_desc.iad,
    (struct usb_desc_header *)&vendor_fs_desc.if0,
    (struct usb_desc_header *)&vendor_fs_desc.if0_out_ep,
    (struct usb_desc_header *)&vendor_fs_desc.if0_in_ep,
    (struct usb_desc_header *)&vendor_fs_desc.nil_desc,
};

#if defined(CONFIG_USBD_SUPPORTS_HIGH_SPEED) && IS_ENABLED(CONFIG_USBD_SUPPORTS_HIGH_SPEED)
static struct vendor_desc vendor_hs_desc = {
    .iad = {
        .bLength = sizeof(struct usb_association_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE_ASSOC,
        .bFirstInterface = 0,
        .bInterfaceCount = 1,
        .bFunctionClass = USB_BCC_VENDOR,
        .bFunctionSubClass = 0,
        .bFunctionProtocol = 0,
        .iFunction = 0,
    },
    .if0 = {
        .bLength = sizeof(struct usb_if_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_BCC_VENDOR,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,
    },
    .if0_out_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = USB_VENDOR_BULK_OUT_EP_ADDR,
        .bmAttributes = USB_EP_TYPE_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(512),
        .bInterval = 0x00,
    },
    .if0_in_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = USB_VENDOR_BULK_IN_EP_ADDR,
        .bmAttributes = USB_EP_TYPE_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(512),
        .bInterval = 0x00,
    },
    .nil_desc = {
        .bLength = 0,
        .bDescriptorType = 0,
    },
};

static const struct usb_desc_header *vendor_hs_descs[] = {
    (struct usb_desc_header *)&vendor_hs_desc.iad,
    (struct usb_desc_header *)&vendor_hs_desc.if0,
    (struct usb_desc_header *)&vendor_hs_desc.if0_out_ep,
    (struct usb_desc_header *)&vendor_hs_desc.if0_in_ep,
    (struct usb_desc_header *)&vendor_hs_desc.nil_desc,
};
#endif /* CONFIG_USBD_SUPPORTS_HIGH_SPEED */

/* -----------------------------------------------------------------------------------
 * Class (function) API implementation
 * -------------------------------------------------------------------------------- */
static void vendor_class_feature_halt(struct usbd_class_data *const c_data,
                                      uint8_t ep, bool halted)
{
    ARG_UNUSED(c_data);
    ARG_UNUSED(ep);
    ARG_UNUSED(halted);
}

static void vendor_class_update(struct usbd_class_data *const c_data,
                                uint8_t iface, uint8_t alternate)
{
    ARG_UNUSED(c_data);
    ARG_UNUSED(iface);
    ARG_UNUSED(alternate);
}

/* Host -> device control transfer: vendor command dispatch */
static int vendor_class_control_to_dev(struct usbd_class_data *const c_data,
                                       const struct usb_setup_packet *const setup,
                                       const struct net_buf *const buf)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);
    uint8_t opcode = setup->bRequest;
    uint8_t led_idx = (uint8_t)(setup->wValue >> 8);
    uint8_t state = (uint8_t)(setup->wValue & 0xFF);
    int ret = 0;

    ARG_UNUSED(buf);

    if (USB_REQTYPE_GET_TYPE(setup->bmRequestType) != USB_REQTYPE_TYPE_VENDOR ||
        USB_REQTYPE_GET_RECIPIENT(setup->bmRequestType) != USB_REQTYPE_RECIPIENT_DEVICE) {
        return -ENOTSUP;
    }

    switch (opcode) {
    case USB_VENDOR_REQ_SET_LED:
        if (led_idx == 0) { /* green */
            if (state) {
                gpio_pin_set_dt(&green_led, 1);
                data->led_states |= BIT(0);
            } else {
                gpio_pin_set_dt(&green_led, 0);
                data->led_states &= ~BIT(0);
            }
            LOG_INF("USB CTRL: Green LED %s", state ? "ON" : "OFF");
        } else if (led_idx == 1) { /* red */
            if (state) {
                gpio_pin_set_dt(&red_led, 1);
                data->led_states |= BIT(1);
            } else {
                gpio_pin_set_dt(&red_led, 0);
                data->led_states &= ~BIT(1);
            }
            LOG_INF("USB CTRL: Red LED %s", state ? "ON" : "OFF");
        } else {
            ret = -ENOTSUP;
        }
        break;
    case USB_VENDOR_REQ_GET_STATUS:
        /* No data stage; host reads status via bulk IN */
        break;
    default:
        ret = -ENOTSUP;
        break;
    }

    return ret;
}

/* Device -> host control transfer: vendor response */
static int vendor_class_control_to_host(struct usbd_class_data *const c_data,
                                        const struct usb_setup_packet *const setup,
                                        struct net_buf *const buf)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);
    uint8_t opcode = setup->bRequest;
    int ret = 0;

    if (USB_REQTYPE_GET_TYPE(setup->bmRequestType) != USB_REQTYPE_TYPE_VENDOR ||
        USB_REQTYPE_GET_RECIPIENT(setup->bmRequestType) != USB_REQTYPE_RECIPIENT_DEVICE) {
        return -ENOTSUP;
    }

    switch (opcode) {
    case USB_VENDOR_REQ_GET_STATUS:
        net_buf_add_u8(buf, data->led_states);
        break;
    default:
        ret = -ENOTSUP;
        break;
    }

    return ret;
}

/* Endpoint request completion handler (bulk IN/OUT) */
static int vendor_submit_bulk_out(struct usbd_class_data *const c_data);
static void vendor_process_command(struct usbd_class_data *const c_data,
                                   const uint8_t *cmd, size_t len);

static int vendor_class_request(struct usbd_class_data *const c_data,
                                struct net_buf *buf, int err)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);
    struct udc_buf_info *bi;
    uint8_t ep;

    bi = udc_get_buf_info(buf);
    ep = bi->ep;

    if (err) {
        if (err == -ECONNABORTED) {
            LOG_WRN("USB: transfer ep 0x%02x cancelled", ep);
        } else {
            LOG_ERR("USB: transfer ep 0x%02x failed: %d", ep, err);
        }
        if (ep == data->bulk_out_ep) {
            atomic_clear_bit(&data->state, VENDOR_STATE_RX_BUSY);
        }
        if (ep == data->bulk_in_ep) {
            atomic_clear_bit(&data->state, VENDOR_STATE_TX_BUSY);
        }
        net_buf_unref(buf);
        /* Resubmit RX to keep receiving commands */
        if (atomic_test_bit(&data->state, VENDOR_STATE_ENABLED) &&
            ep == data->bulk_out_ep) {
            vendor_submit_bulk_out(c_data);
        }
        return 0;
    }

    if (ep == data->bulk_out_ep) {
        /* RX: command received */
        atomic_clear_bit(&data->state, VENDOR_STATE_RX_BUSY);

        if (buf->len >= 2) {
            vendor_process_command(c_data, buf->data, buf->len);
        }

        net_buf_unref(buf);

        /* Resubmit RX to keep receiving commands */
        if (atomic_test_bit(&data->state, VENDOR_STATE_ENABLED)) {
            vendor_submit_bulk_out(c_data);
        }
    }

    if (ep == data->bulk_in_ep) {
        /* TX: response sent */
        atomic_clear_bit(&data->state, VENDOR_STATE_TX_BUSY);
        net_buf_unref(buf);

        /* Resubmit RX to keep receiving commands */
        if (atomic_test_bit(&data->state, VENDOR_STATE_ENABLED)) {
            vendor_submit_bulk_out(c_data);
        }
    }

    return 0;
}

static void vendor_class_suspended(struct usbd_class_data *const c_data)
{
    ARG_UNUSED(c_data);
}

static void vendor_class_resumed(struct usbd_class_data *const c_data)
{
    ARG_UNUSED(c_data);
}

static void vendor_class_sof(struct usbd_class_data *const c_data)
{
    ARG_UNUSED(c_data);
}

static void vendor_class_enable(struct usbd_class_data *const c_data)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);

    if (!atomic_test_and_set_bit(&data->state, VENDOR_STATE_ENABLED)) {
        LOG_INF("USB: vendor class enabled");
        vendor_submit_bulk_out(c_data);
    }
}

static void vendor_class_disable(struct usbd_class_data *const c_data)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);

    atomic_clear_bit(&data->state, VENDOR_STATE_ENABLED);
    atomic_clear_bit(&data->state, VENDOR_STATE_RX_BUSY);
    atomic_clear_bit(&data->state, VENDOR_STATE_TX_BUSY);
    LOG_INF("USB: vendor class disabled");
}

static int vendor_class_init(struct usbd_class_data *const c_data)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);
    int ret;

    data->bulk_out_ep = USB_VENDOR_BULK_OUT_EP_ADDR;
    data->bulk_in_ep = USB_VENDOR_BULK_IN_EP_ADDR;
    data->led_states = 0;
    data->tx_len = 0;

    /* Configure LED GPIO pins as outputs */
    if (!device_is_ready(green_led.port)) {
        LOG_ERR("USB: Green LED GPIO device not ready");
        return -ENODEV;
    }
    if (!device_is_ready(red_led.port)) {
        LOG_ERR("USB: Red LED GPIO device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("USB: Failed to configure Green LED GPIO: %d", ret);
        return ret;
    }
    ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("USB: Failed to configure Red LED GPIO: %d", ret);
        return ret;
    }

    /* Start with both LEDs off (active-low: logical 0 = OFF) */
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&red_led, 0);

    LOG_INF("USB: vendor class init");
    return 0;
}

static void vendor_class_shutdown(struct usbd_class_data *const c_data)
{
    ARG_UNUSED(c_data);
}

static void *vendor_class_get_desc(struct usbd_class_data *const c_data,
                                   const enum usbd_speed speed)
{
    ARG_UNUSED(c_data);

#if defined(CONFIG_USBD_SUPPORTS_HIGH_SPEED) && IS_ENABLED(CONFIG_USBD_SUPPORTS_HIGH_SPEED)
    if (speed == USBD_SPEED_HS) {
        return (void *)vendor_hs_descs;
    }
#endif
    return (void *)vendor_fs_descs;
}

static const struct usbd_class_api vendor_class_api = {
    .feature_halt = vendor_class_feature_halt,
    .update = vendor_class_update,
    .control_to_dev = vendor_class_control_to_dev,
    .control_to_host = vendor_class_control_to_host,
    .request = vendor_class_request,
    .suspended = vendor_class_suspended,
    .resumed = vendor_class_resumed,
    .sof = vendor_class_sof,
    .enable = vendor_class_enable,
    .disable = vendor_class_disable,
    .init = vendor_class_init,
    .shutdown = vendor_class_shutdown,
    .get_desc = vendor_class_get_desc,
};

/* Vendor request table (vendor-specific opcodes handled via control transfers) */
static const uint8_t vendor_vreq_table[] = {
    USB_VENDOR_REQ_SET_LED,
    USB_VENDOR_REQ_GET_STATUS,
};
static const struct usbd_cctx_vendor_req vendor_vreq = VENDOR_REQ_DEFINE(vendor_vreq_table, 2);

USBD_DEFINE_CLASS(vendor_class, &vendor_class_api, &g_vendor, &vendor_vreq);

/* -----------------------------------------------------------------------------------
 * Bulk transfer helpers
 * -------------------------------------------------------------------------------- */
static int vendor_submit_bulk_out(struct usbd_class_data *const c_data)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);
    struct net_buf *buf;
    int err;

    if (!atomic_test_bit(&data->state, VENDOR_STATE_ENABLED)) {
        return -EPERM;
    }

    if (atomic_test_and_set_bit(&data->state, VENDOR_STATE_RX_BUSY)) {
        return -EBUSY;
    }

    buf = usbd_ep_buf_alloc(c_data, data->bulk_out_ep, USB_VENDOR_FRAME_MAX);
    if (buf == NULL) {
        atomic_clear_bit(&data->state, VENDOR_STATE_RX_BUSY);
        LOG_ERR("USB: failed to allocate bulk OUT buffer");
        return -ENOMEM;
    }

    err = usbd_ep_enqueue(c_data, buf);
    if (err) {
        LOG_ERR("USB: failed to enqueue bulk OUT: %d", err);
        net_buf_unref(buf);
        atomic_clear_bit(&data->state, VENDOR_STATE_RX_BUSY);
    }

    return err;
}

static int vendor_submit_bulk_in(struct usbd_class_data *const c_data)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);
    struct net_buf *buf;
    int err;

    if (!atomic_test_bit(&data->state, VENDOR_STATE_ENABLED)) {
        return -EPERM;
    }

    if (atomic_test_and_set_bit(&data->state, VENDOR_STATE_TX_BUSY)) {
        return -EBUSY;
    }

    buf = usbd_ep_buf_alloc(c_data, data->bulk_in_ep, USB_VENDOR_FRAME_MAX);
    if (buf == NULL) {
        atomic_clear_bit(&data->state, VENDOR_STATE_TX_BUSY);
        LOG_ERR("USB: failed to allocate bulk IN buffer");
        return -ENOMEM;
    }

    net_buf_add_mem(buf, data->tx_buf, data->tx_len);
    err = usbd_ep_enqueue(c_data, buf);
    if (err) {
        LOG_ERR("USB: failed to enqueue bulk IN: %d", err);
        net_buf_unref(buf);
        atomic_clear_bit(&data->state, VENDOR_STATE_TX_BUSY);
    }

    return err;
}

/* -----------------------------------------------------------------------------------
 * Command processing
 * -------------------------------------------------------------------------------- */
static void vendor_process_command(struct usbd_class_data *const c_data,
                                   const uint8_t *cmd, size_t len)
{
    struct usb_vendor_data *data = usbd_class_get_private(c_data);
    uint8_t opcode = cmd[0];
    uint8_t arg = (len > 1) ? cmd[1] : 0;
    uint8_t status = LED_STATUS_OK;

    data->tx_buf[0] = LED_STATUS_OK;
    data->tx_len = 1;

    switch (opcode) {
    case USB_VENDOR_REQ_SET_LED:
        if (arg == LED_CMD_GREEN_ON) {
            gpio_pin_set_dt(&green_led, 1);
            data->led_states |= BIT(0);
            LOG_INF("USB: Green LED ON");
        } else if (arg == LED_CMD_GREEN_OFF) {
            gpio_pin_set_dt(&green_led, 0);
            data->led_states &= ~BIT(0);
            LOG_INF("USB: Green LED OFF");
        } else if (arg == LED_CMD_RED_ON) {
            gpio_pin_set_dt(&red_led, 1);
            data->led_states |= BIT(1);
            LOG_INF("USB: Red LED ON");
        } else if (arg == LED_CMD_RED_OFF) {
            gpio_pin_set_dt(&red_led, 0);
            data->led_states &= ~BIT(1);
            LOG_INF("USB: Red LED OFF");
        } else {
            status = LED_STATUS_ERROR;
        }
        break;
    case USB_VENDOR_REQ_GET_STATUS:
        data->tx_buf[1] = data->led_states;
        data->tx_len = 2;
        break;
    default:
        status = LED_STATUS_ERROR;
        break;
    }

    data->tx_buf[0] = status;

    /* Send response via bulk IN */
    vendor_submit_bulk_in(c_data);
}

/* -----------------------------------------------------------------------------------
 * Class lifecycle: register descriptors/config, init device, enable
 * -------------------------------------------------------------------------------- */
static int vendor_class_register(struct usbd_context *ctx,
                                 const enum usbd_speed speed, uint8_t cfg)
{
    int ret;

    ret = usbd_add_descriptor(ctx, &vendor_lang);
    if (ret) {
        return ret;
    }
    ret = usbd_add_descriptor(ctx, &vendor_mfr);
    if (ret) {
        return ret;
    }
    ret = usbd_add_descriptor(ctx, &vendor_prd);
    if (ret) {
        return ret;
    }

    /* Configuration string descriptor is added by usbd_add_configuration */
    ret = usbd_add_configuration(ctx, speed, &vendor_cfg);
    if (ret) {
        return ret;
    }

    ret = usbd_register_class(ctx, "vendor_class", speed, cfg);
    if (ret) {
        return ret;
    }

    return 0;
}

static int vendor_usb_init(const struct usbd_context *ctx)
{
    static const enum usbd_speed speeds[] = {
        USBD_SPEED_FS,
#if defined(CONFIG_USBD_SUPPORTS_HIGH_SPEED) && IS_ENABLED(CONFIG_USBD_SUPPORTS_HIGH_SPEED)
        USBD_SPEED_HS,
#endif
    };
    int ret;

    for (size_t i = 0; i < ARRAY_SIZE(speeds); i++) {
        ret = vendor_class_register((struct usbd_context *)ctx, speeds[i], 1);
        if (ret) {
            LOG_ERR("USB: failed to register class for speed %u: %d",
                    (unsigned int)speeds[i], ret);
            return ret;
        }
    }

    /* Set device class triple explicitly for host compatibility */
    ret = usbd_device_set_code_triple((struct usbd_context *)ctx, USBD_SPEED_FS,
                                      USB_BCC_VENDOR, 0x00, 0x00);
    if (ret) {
        LOG_ERR("USB: failed to set device code triple: %d", ret);
        return ret;
    }

    ret = usbd_init((struct usbd_context *)ctx);
    if (ret) {
        LOG_ERR("USB: usbd_init failed: %d", ret);
        return ret;
    }

    LOG_INF("USB: device stack initialized");
    return 0;
}

static int vendor_usb_enable(const struct usbd_context *ctx)
{
    return usbd_enable((struct usbd_context *)ctx);
}

/* -----------------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------------- */
int usb_vendor_init(void)
{
    struct usbd_context *ctx = &vendor_usb;
    int ret;

    ret = vendor_usb_init(ctx);
    if (ret) {
        LOG_ERR("USB: failed to initialize vendor device: %d", ret);
        return ret;
    }

    ret = vendor_usb_enable(ctx);
    if (ret) {
        LOG_ERR("USB: failed to enable vendor device: %d", ret);
        return ret;
    }

    LOG_INF("USB: vendor device initialized and enabled (VID=0x%04X PID=0x%04X)",
            USB_VENDOR_VID, USB_VENDOR_PID);
    return 0;
}

void usb_vendor_handle_vendor_request(uint8_t opcode, const uint8_t *data, size_t len)
{
    /* Handled via control transfer callbacks (control_to_dev/control_to_host) */
    ARG_UNUSED(opcode);
    ARG_UNUSED(data);
    ARG_UNUSED(len);
}

void usb_vendor_send_response(const uint8_t *data, size_t len)
{
    /* Handled via bulk IN endpoint */
    ARG_UNUSED(data);
    ARG_UNUSED(len);
}
