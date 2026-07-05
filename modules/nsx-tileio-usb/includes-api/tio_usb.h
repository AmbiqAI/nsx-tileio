#ifndef TIO_USB_H
#define TIO_USB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIO_USB_PACKET_LEN 256u
#define TIO_USB_VENDOR_ID 0xCAFEu
#define TIO_USB_PRODUCT_ID 0x0001u

typedef void (*pfnSlotUpdate)(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length);
typedef void (*pfnUioUpdate)(const uint8_t *data, uint32_t length);

typedef struct {
    volatile pfnUioUpdate uio_update_cb;
    volatile pfnSlotUpdate slot_update_cb;

    const char *manufacturer;
    const char *product;
    const char *serial;
    const char *cdc_interface;
    const char *vendor_interface;
    const char *webusb_url;
    uint16_t vid;
    uint16_t pid;
    uint32_t poll_interval_us;
    uint32_t timeout_ms;
} tio_usb_context_t;

uint32_t tio_usb_init(tio_usb_context_t *ctx);
uint32_t tio_usb_tx_available(void);
uint32_t tio_usb_pack_slot_data(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length,
    uint8_t *packet);
uint32_t tio_usb_send_slot_packet(uint8_t *buffer, uint32_t length);
uint32_t tio_usb_send_slot_data(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length);
uint32_t tio_usb_send_uio_state(const uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif

