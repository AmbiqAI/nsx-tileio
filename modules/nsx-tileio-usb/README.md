# nsx-tileio-usb

NSX USB transport wrapper ported from the legacy TileIO USB implementation:

- `~/Ambiq/neuralspot/ns-modules/ns-tileio/tio-usb`

It carries the TileIO framing and host/device packet flow onto the public
`nsx-usb` vendor-channel API rather than direct TinyUSB internals.

## Dependencies

- `nsx-core`
- `nsx-usb`

## Status

Validated with the local `tileio_usb_test` app on:

- Apollo4 Blue Plus (`apollo4p_blue_kxr_evb`)
- Apollo510B (`apollo510b_evb`)

The module:

- preserves the basic `tio_usb_*` API shape
- implements fixed-length TileIO packet framing over the `nsx-usb` vendor
  channel
- parses host-to-device traffic through the public `nsx_usb_vendor_read_nb()`
  callback/poll model
- validates device-to-host responses and telemetry with a Python host tool
- owns its private USB DMA buffers for now
- removes legacy direct TinyUSB/global descriptor mutation

## Public API

Header:

- `includes-api/tio_usb.h`

Main entry points:

- `tio_usb_init(tio_usb_context_t *ctx)`
- `tio_usb_tx_available()`
- `tio_usb_pack_slot_data(...)`
- `tio_usb_send_slot_packet(...)`
- `tio_usb_send_slot_data(...)`
- `tio_usb_send_uio_state(...)`

Key configuration in `tio_usb_context_t`:

- app callbacks:
  - `slot_update_cb`
  - `uio_update_cb`
- USB identity and strings:
  - `manufacturer`
  - `product`
  - `serial`
  - `cdc_interface`
  - `vendor_interface`
  - `webusb_url`
- optional VID/PID override:
  - `vid`
  - `pid`
- optional `nsx-usb` timing knobs:
  - `poll_interval_us`
  - `timeout_ms`

## App contract

`nsx-tileio-usb` is intentionally not a full application harness.

The application still owns:

- core/board startup and cache policy
- logging/diagnostics policy
- its main-loop or task topology
- when TileIO traffic is generated
- product identity values passed through `tio_usb_context_t`

The module owns:

- TileIO packet packing and validation
- USB vendor RX parsing and callback dispatch
- bridging TileIO TX onto the public `nsx-usb` vendor channel

This keeps the wrapper focused on the transport instead of reintroducing a
legacy framework layer.

## Minimal integration example

```c
int main(void) {
    tio_usb_context_t tio = {
        .uio_update_cb = uio_update_cb,
        .slot_update_cb = slot_update_cb,
        .manufacturer = "Ambiq",
        .product = "NSX TileIO USB Test",
        .serial = "TIO-AP4",
        .cdc_interface = "NSX CDC",
        .vendor_interface = "TileIO Vendor",
        .webusb_url = "tileio.local",
        .vid = TIO_USB_VENDOR_ID,
        .pid = TIO_USB_PRODUCT_ID,
    };

    if (tio_usb_init(&tio) != NSX_STATUS_SUCCESS) {
        for (;;) {}
    }

    while (1) {
        if (tio_usb_tx_available() != 0u) {
            (void)tio_usb_send_slot_data(0, 0, signal_payload, signal_len);
            (void)tio_usb_send_uio_state(uio_payload, sizeof(uio_payload));
        }

        nsx_delay_us(APP_LOOP_SLEEP_US);
    }
}
```

## Host behavior note

Browser/WebUSB hosts and PyUSB/libusb hosts do not behave identically:

- browser/WebUSB clients typically issue the interface class control request
  `0x22` (`SET_CONTROL_LINE_STATE`)
- the PyUSB/libusb path used during Linux validation can still stall on that
  request while bulk OUT/IN traffic itself works correctly

To support both host styles, `nsx-tileio-usb` now treats the first valid vendor
RX packet as an active connection for TileIO TX purposes. This keeps the
WebUSB/browser path working while also allowing a simple Python/libusb host tool
to exchange frames without browser-only control semantics.

## Reference app and host tool

The current reference bring-up app is kept outside the module repo in local app
space while the module API settles:

- app:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_usb_test`
- host tool:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_usb_test/tools/tileio_usb_host.py`

Useful host commands:

```bash
./tools/tileio_usb_host.py --list-devices
./tools/tileio_usb_host.py --frames 4 --transfers 10 --timeout-ms 2000 --read-size 2048
./tools/tileio_usb_host.py --bus 1 --address 47 --frames 4 --transfers 10 --timeout-ms 2000 --read-size 2048
```

The host tool:

- packs a valid TileIO host->device frame
- writes it to the vendor OUT endpoint
- reads one or more TileIO frames from the vendor IN endpoint
- validates start/stop bytes and CRC

## Linux USB permissions

For raw PyUSB/libusb access, the `/dev/bus/usb/...` node must be accessible to
the current user. During validation, this was solved by applying a udev rule
that makes the TileIO USB device group-writable by `plugdev`.

## Current limitations

- whether to expose caller-owned buffers in the long-term init contract
- whether to factor the host frame pack/unpack logic into a shared TileIO
  utility layer for future BLE tooling
