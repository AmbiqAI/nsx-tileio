# nsx-tileio

NSX transport-wrapper repository for the legacy TileIO modules from:

- `~/Ambiq/neuralspot/ns-modules/ns-tileio`

## Status

This repository is still an **active migration checkpoint**, but both transport
ports now have validated runtime implementations.

The legacy codebase naturally splits into two transport-specific wrappers, so
this repo is being structured as two NSX modules under one git repository:

- `modules/nsx-tileio-usb`
- `modules/nsx-tileio-ble`

Current module status:

- `nsx-tileio-usb`: runtime validated on Apollo4 Blue Plus and Apollo510B with
  a Python host tool
- `nsx-tileio-ble`: runtime validated on Apollo4 Blue Plus and Apollo510B with
  a local BLE test app and Python host tool

The modules intentionally stay transport-focused:

- `nsx-tileio-usb` owns TileIO packet framing and the USB vendor-channel bridge
  on top of `nsx-usb`
- `nsx-tileio-ble` owns the TileIO BLE service and characteristic wiring on top
  of `nsx-ble`
- applications continue to own board/runtime policy rather than handing it back
  to a legacy-style framework layer

## Goals

- Preserve the useful TileIO app-facing API shape where practical.
- Port USB onto the public `nsx-usb` API rather than direct TinyUSB internals.
- Port BLE onto `nsx-ble` while preserving the NSX rule that the application
  owns dispatcher tasking, IRQ policy, and other board/runtime policy.
- Avoid carrying forward old neuralSPOT harness ownership patterns.

## Reference validation assets

The current end-to-end bring-up apps remain in local app space while the module
APIs settle:

- USB:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_usb_test`
- BLE:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_ble_test`

## Documents

- `docs/migration-status.md` - current migration plan, findings, and status
- `modules/nsx-tileio-usb/README.md` - module-specific USB API, contract, and
  host notes
- `modules/nsx-tileio-ble/README.md` - module-specific BLE API, contract, and
  reference usage
