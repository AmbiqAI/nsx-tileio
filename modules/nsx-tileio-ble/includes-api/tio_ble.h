#ifndef TIO_BLE_H
#define TIO_BLE_H

#include <stdint.h>
#include "ns_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIO_BLE_SLOT_COUNT             4u
#define TIO_BLE_SLOT_DATA_MAX_LEN      240u
#define TIO_BLE_SLOT_SIG_BUF_LEN       (TIO_BLE_SLOT_DATA_MAX_LEN + 2u)
#define TIO_BLE_SLOT_MET_BUF_LEN       (TIO_BLE_SLOT_DATA_MAX_LEN + 2u)
#define TIO_BLE_UIO_BUF_LEN            8u
#define TIO_BLE_DEFAULT_BASE_HANDLE    0x0800u
#define TIO_BLE_DEFAULT_NOTIFY_PERIOD_MS 1000u

typedef void (*pfnSlotUpdate)(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length);
typedef void (*pfnUioUpdate)(const uint8_t *data, uint32_t length);

typedef struct {
    /* App callbacks for host->target TileIO traffic. */
    volatile pfnUioUpdate uio_update_cb;
    volatile pfnSlotUpdate slot_update_cb;

    /* Required app-owned BLE policy/configuration. */
    ns_ble_pool_config_t *pool_config;
    const char *service_name;
    uint16_t base_handle;
    uint16_t notify_period_ms;

    /* Optional metadata and diagnostics hooks forwarded to nsx-ble. */
    const ns_ble_device_info_t *device_info;
    const ns_ble_connection_config_t *connection_config;
    ns_ble_event_handler_t event_handler;
    void *event_context;
} tio_ble_context_t;

/*
 * App contract:
 * - call ns_ble_pre_init() from the app before the BLE dispatcher starts
 * - own WSF pool sizing, dispatcher task creation, IRQ glue, and board/radio
 *   policy in the application
 * - call tio_ble_init() from the app-owned BLE setup path (typically the radio
 *   task) to construct and start the TileIO BLE service on top of nsx-ble
 */

/* Builds and starts the TileIO BLE GATT service. */
uint32_t tio_ble_init(tio_ble_context_t *ctx);

/* Sends TileIO signal (slot_type=0) or metric (slot_type=1) data. */
uint32_t tio_ble_send_slot_data(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length);

/* Sends the 8-byte TileIO UIO state. */
uint32_t tio_ble_send_uio_state(const uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif
