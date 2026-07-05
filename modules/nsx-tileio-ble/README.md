# nsx-tileio-ble

TileIO BLE transport wrapper for NSX, ported from the legacy TileIO BLE module:

- `~/Ambiq/neuralspot/ns-modules/ns-tileio/tio-ble`

It builds the TileIO GATT service on top of `nsx-ble` while keeping BLE tasking,
IRQ glue, WSF pool sizing, and board/radio policy in the application.

## Dependencies

- `nsx-core`
- `nsx-ble`

## Status

Validated with the local `tileio_ble_test` app on:

- Apollo4 Blue Plus (`apollo4p_blue_kxr_evb`)
- Apollo510B (`apollo510b_evb`)

The module:

- preserves the basic `tio_ble_*` API shape
- builds the TileIO BLE service and characteristics inside the module
- provides TileIO send helpers for signal / metric / UIO data
- intentionally does **not** own dispatcher tasking, WSF pool policy, IRQ
  setup, or board/radio policy

## Public API

Header:

- `includes-api/tio_ble.h`

Main entry points:

- `tio_ble_init(tio_ble_context_t *ctx)`
- `tio_ble_send_slot_data(...)`
- `tio_ble_send_uio_state(...)`

Key configuration in `tio_ble_context_t`:

- app callbacks:
  - `slot_update_cb`
  - `uio_update_cb`
- required app-owned BLE config:
  - `pool_config`
- optional service tuning:
  - `service_name`
  - `base_handle`
  - `notify_period_ms`
- optional `nsx-ble` metadata/diagnostics:
  - `device_info`
  - `connection_config`
  - `event_handler`
  - `event_context`

## App contract

`nsx-tileio-ble` is intentionally not a full BLE framework.

The application must still own:

- `ns_ble_pre_init()` timing
- WSF pool sizing/allocation
- FreeRTOS task creation
- the `wsfOsDispatcher()` loop
- board-specific BLE IRQ glue
- board/radio power, cache, and startup policy

This keeps the boundary consistent with `ble_webble` and avoids reintroducing
the old neuralSPOT framework ownership model.

## TileIO GATT model

Primary service UUID:

```text
eecb7db8-8b2d-402c-b995-825538b49328
```

Characteristics:

| Purpose | UUID |
| --- | --- |
| Slot 0 signal | `5bca2754-ac7e-4a27-a127-0f328791057a` |
| Slot 1 signal | `45415793-a0e9-4740-bca4-ce90bd61839f` |
| Slot 2 signal | `dd19792c-63f1-420f-920c-c58bada8efb9` |
| Slot 3 signal | `f1f69158-0bd6-4cab-90a8-528baf74cc74` |
| Slot 0 metric | `44a3a7b8-d7c8-4932-9a10-d99dd63775ae` |
| Slot 1 metric | `e64fa683-4628-48c5-bede-824aaa7c3f5b` |
| Slot 2 metric | `b9d28f53-65f0-4392-afbc-c602f9dc3c8b` |
| Slot 3 metric | `917c9eb4-3dbc-4cb3-bba2-ec4e288083f4` |
| UIO | `b9488d48-069b-47f7-94f0-387f7fbfd1fa` |

Payload model:

- signal / metric values are fixed-length BLE characteristic values where:
  - bytes `0..1` encode payload length (little endian)
  - bytes `2..` hold the TileIO payload
- UIO is a fixed 8-byte characteristic

## Minimal integration example

This is the intended usage pattern inside an app-owned radio task:

```c
static ns_ble_pool_config_t app_wsf_buffers = {
    .pool = app_pool,
    .poolSize = sizeof(app_pool),
    .desc = app_desc,
    .descNum = APP_WSF_BUFFER_POOLS,
};

static void radio_task(void *arg) {
    tio_ble_context_t tio = {
        .slot_update_cb = slot_update_cb,
        .uio_update_cb = uio_update_cb,
        .pool_config = &app_wsf_buffers,
        .service_name = "TIO-AP4",
        .base_handle = TIO_BLE_DEFAULT_BASE_HANDLE,
        .notify_period_ms = 200,
        .device_info = &app_device_info,
        .connection_config = &app_conn_config,
        .event_handler = app_ble_event_handler,
    };

    ns_ble_pre_init();

    if (tio_ble_init(&tio) != NS_STATUS_SUCCESS) {
        /* app-owned failure policy */
        for (;;) {}
    }

    while (1) {
        wsfOsDispatcher();
    }
}
```

Sending data from application code:

```c
uint8_t signal_payload[32];
uint8_t metric_payload[8];
uint8_t uio_payload[8];

(void)tio_ble_send_slot_data(0, 0, signal_payload, sizeof(signal_payload));
(void)tio_ble_send_slot_data(1, 1, metric_payload, sizeof(metric_payload));
(void)tio_ble_send_uio_state(uio_payload, sizeof(uio_payload));
```

## Reference app and host tool

The current reference bring-up app is kept outside the module repo in local app
space while the module API settles:

- app:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_ble_test`
- host tool:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_ble_test/tools/tileio_ble_host.py`

Useful host commands:

```bash
python tools/tileio_ble_host.py --list
python tools/tileio_ble_host.py --address 00:20:50:42:40:90 --notify-seconds 3
python tools/tileio_ble_host.py --address B8:5D:70:D5:6E:50 --uio 0102030405060708
```

## Example bundling decision

For now, the full bring-up app is **not** bundled into this module repo.

Reason:

- the current app is still explicitly app-owned policy code
- it includes board power/cache/IRQ/tasking details that are better kept in app
  space than presented as module-owned behavior
- the local app remains a better validation target while the module API settles

Instead, this README includes the minimal integration pattern and points to the
clean local reference app/tooling used for validation.

## Current limitations

- the BLE service still uses the current `nsx-ble` CCC/timer model, so TileIO
  notify characteristics are configured for async/send-on-demand use but still
  depend on that underlying wrapper behavior
- the reference app/host tooling are still local-path bring-up assets rather
  than published examples
