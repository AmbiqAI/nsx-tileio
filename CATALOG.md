# Module catalog entry

This is the exact catalog view intended for downstream neuralSPOT-X registry
promotion after the release is published. The registry should continue to
point at this project and should use the immutable tag, not a moving branch.

| Module | Metadata | CMake target | Required dependencies | Release version |
| --- | --- | --- | --- | --- |
| `nsx-tileio-usb` | `modules/nsx-tileio-usb/nsx-module.yaml` | `nsx::tileio_usb` | `nsx-core`, `nsx-usb` | `0.1.0` |
| `nsx-tileio-ble` | `modules/nsx-tileio-ble/nsx-module.yaml` | `nsx::tileio_ble` | `nsx-core`, `nsx-ble` | `0.1.0` |

The USB compatibility declaration covers Apollo4P, Apollo330P, Apollo510,
Apollo510B, and Apollo510L families supported by `nsx-usb`. The BLE declaration
covers Apollo3, Apollo3P, Apollo4P Blue, and Apollo510B supported by `nsx-ble`.
Release-qualified hardware is listed in `docs/compatibility.md`; catalog
support is not a claim of hardware validation for every row.

No registry, legacy repository, HPX, or application repository is changed by
this release-preparation branch.
