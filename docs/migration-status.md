# nsx-tileio migration plan and status

## Scope

Create a new repository at:

- `/home/adamp/Ambiq/neuralspotx/nsx-modules/nsx-tileio`

Port the legacy TileIO code from:

- `/home/adamp/Ambiq/neuralspot/ns-modules/ns-tileio`

The port will be split into two NSX modules in the same repo:

- `nsx-tileio-usb`
- `nsx-tileio-ble`

## Current status

- Repository scaffold created.
- New git repo initialized on branch `main`.
- Placeholder module directories created for USB and BLE.
- Initial NSX module metadata and CMake scaffolding added.
- `nsx-tileio-usb` is now validated end-to-end on Apollo4 Blue Plus.
- `nsx-tileio-usb` is now also validated end-to-end on Apollo510B.
- `nsx-tileio-ble` is now validated end-to-end on Apollo4 Blue Plus and
  Apollo510B with a local app and host tool.
- Detailed migration findings captured below.

## Legacy source inventory

### USB transport

- `tio-usb/includes-api/tio_usb.h`
- `tio-usb/src/tio_usb.c`
- `tio-usb/src/ringbuffer.c`
- `tio-usb/src/ringbuffer.h`

### BLE transport

- `tio-ble/includes-api/tio_ble.h`
- `tio-ble/src/tio_ble.c`

## Migration findings

### USB

The legacy USB implementation is the larger refactor.

It currently depends on:

- `ns_ambiqsuite_harness.h`
- `ns_lp_printf`
- direct TinyUSB/vendor internals such as:
  - `vendor_device.h`
  - `tud_vendor_mounted()`
  - `tud_vendor_write_available()`
  - `webusb_send_data()`
  - `webusb_register_raw_cb()`
  - `usb_string_desc_arr`

The NSX port should instead use the public `nsx-usb` interface:

- `nsx_usb_init()`
- `nsx_usb_vendor_send()`
- `nsx_usb_vendor_read_nb()`
- runtime `nsx_usb_device_desc_t` descriptor configuration

Likely dependencies:

- `nsx-core`
- `nsx-soc-hal`
- `nsx-usb`

Current implementation direction:

- `tio_usb_*` public entry points are being preserved
- packet framing/parsing is implemented on top of the public vendor path:
  - `nsx_usb_init()`
  - `nsx_usb_vendor_send()`
  - `nsx_usb_vendor_read_nb()`
  - `nsx_usb_vendor_connected()`
- the wrapper owns private SRAM-backed USB DMA buffers for now
- direct TinyUSB descriptor/global mutation has been removed in favor of
  runtime `nsx_usb_device_desc_t` configuration
- first successful vendor RX is now treated as an active TileIO connection so
  non-browser hosts (PyUSB/libusb) can receive TileIO responses without relying
  on a WebUSB-only control handshake

Still-open USB design points:

- whether to keep or reintroduce auto-derived serial strings from MCU device ID
- whether the current private ringbuffer remains the long-term receive path
- whether the wrapper should eventually expose a richer init/config struct for
  caller-owned buffers

### BLE

The legacy BLE implementation maps much more directly onto the existing
`nsx-ble` wrapper because the `ns_ble_*` API was intentionally preserved in NSX.

The main migration concern is ownership:

- legacy `tio-ble` owns static WSF pools
- legacy `tio-ble` owns `TioBleTask()` and calls `wsfOsDispatcher()` itself

For NSX, the BLE module should **not** reclaim those policies from the
application. The app should continue to own:

- dispatcher task creation
- IRQ glue
- WSF pool sizing policy
- board/runtime power and scheduling policy

Likely dependencies:

- `nsx-core`
- `nsx-ble`

Remaining BLE design questions:

- whether to publish a trimmed example once the API settles further
- whether the current `nsx-ble` CCC/timer notify behavior should be refined for
  cleaner send-on-demand semantics

Current implementation direction:

- `TioBleTask()` is intentionally dropped in the NSX port
- the public API preserves `tio_ble_init()` and the TileIO send helpers
- the module now owns only the TileIO BLE service/characteristic definition and
  callback wiring on top of `nsx-ble`
- the app must supply the WSF pool config and still owns dispatcher tasking, IRQ
  glue, and board/radio policy
- local test app and host tool now exist:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_ble_test`
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_ble_test/tools/tileio_ble_host.py`
- Apollo4 Blue Plus runtime validation now passes on probe `1160001481`:
  - target attach / flash succeeded after fixing the board power/header path
  - TileIO BLE service advertised from the AP4 board
  - Linux host connected and received valid TileIO signal / metric / UIO
    notifications
  - host UIO write callback was confirmed on-target via direct RAM inspection
    (`g_app.uio_events` incremented and `last_uio` contained the host payload)
- Apollo510B runtime validation now passes on probe `1160002954`:
  - flashed `tileio_ble_test` to target `AP510NFA-CBR`
  - Linux host discovered the TileIO BLE service at `00:20:50:42:40:90`
  - host connected and received valid TileIO signal / metric / UIO
    notifications
  - host UIO write callback was confirmed on-target via direct RAM inspection
    (`g_app.uio_events` incremented and `last_uio` contained the host payload)

## Recommended implementation order

1. Implement `nsx-tileio-usb`
2. Implement `nsx-tileio-ble`
3. Add minimal smoke examples after the wrappers settle

## Validation plan

### Scaffold / initial implementation phase

- repository layout should be ready for incremental implementation
- USB wrapper should compile in a disposable superbuild against public NSX
  dependency headers

### USB implementation phase

- compile-only validation against `nsx-ambiq-sdk`
- build on boards already supported by `nsx-usb`

### BLE implementation phase

- compile-only validation against `nsx-ambiq-sdk`
- build on boards currently supported by `nsx-ble`
  - Apollo3 / Apollo3P
  - Apollo4P Blue
  - Apollo510B

## Near-term tasks

- Continue hardening the new `tio-usb` port and its init/config contract
- Consider extracting a trimmed publishable example from the local USB/BLE test
  apps once the wrapper APIs settle
- Preserve useful source compatibility without over-owning app behavior

## Validation completed so far

- `cmake -S /home/adamp/Ambiq/neuralspotx/nsx-modules/nsx-tileio -B /tmp/nsx-tileio-configure`
  passed during the scaffold phase
- a disposable stub superbuild compiled `nsx-tileio-usb` successfully against
  public `nsx-core` and `nsx-usb` headers, producing `libnsx_tileio_usb.a`
- app-level AP4 validation completed with:
  - local app: `~/Ambiq/neuralspotx/nsx-apps/tileio_usb_test`
  - board: `apollo4p_blue_kxr_evb`
  - probe: `1160001350`
  - stable USB enumeration as `cafe:0001 Ambiq NSX TileIO USB Test`
  - CDC node materialized as `/dev/ttyACM3`
  - vendor interface confirmed as interface `2`, OUT `0x03`, IN `0x83`
  - host->device TileIO packet write validated
  - target-side packet parse validated
  - device->host TileIO signal/metric frame reads validated via Python host tool
- app-level AP510B validation completed with:
  - local app: `~/Ambiq/neuralspotx/nsx-apps/tileio_usb_test`
  - board: `apollo510b_evb`
  - probe: `1160002954`
  - correct target confirmed during flash as `AP510NFA-CBR` / Apollo5 Cortex-M55
  - stable USB enumeration as `cafe:0001 Ambiq NSX TileIO USB Test`
  - host tool targeted the AP510B instance explicitly via USB bus/address because
    another TileIO USB device was also attached
  - initial reads arrived as 128-byte USB chunks rather than whole 256-byte
    TileIO frames, so the Python host tool was hardened to reassemble and
    resynchronize on valid TileIO frame boundaries
  - host->device TileIO packet write validated
  - target-side packet parse validated by direct RAM inspection of `g_app`
  - device->host TileIO signal/metric/UIO frame reads validated via the updated
    Python host tool

## Host-tooling notes

- A reference Python host tool now exists at:
  - `~/Ambiq/neuralspotx/nsx-apps/tileio_usb_test/tools/tileio_usb_host.py`
- This is intentionally a simple step-by-step tool and is a good candidate to
  evolve into a shared TileIO host utility later, including for BLE testing.
- The host tool now supports `--bus` / `--address` selection so multiple
  simultaneously attached TileIO USB boards can be targeted safely.
- During Linux validation, raw USB access required a udev rule so the device
  node under `/dev/bus/usb/...` became writable by `plugdev`.
- The WebUSB-style interface control request (`0x22`) still returns a stall
  under the tested PyUSB/libusb host path, but bulk OUT/IN traffic works and is
  sufficient for the current demo/validation flow.
