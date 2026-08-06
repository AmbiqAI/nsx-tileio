# `v0.1.0` compatibility matrix

The rows below describe module metadata and release qualification. A listed
board means the underlying NSX dependency advertises support; “validated” means
the TileIO wrapper was exercised by the local bring-up app and host tool.

| Module | Board / SoC | Toolchains | Qualification |
| --- | --- | --- | --- |
| USB | `apollo4p_blue_kxr_evb` / Apollo4P | Arm GCC, Armclang, ATfE | Runtime validated |
| USB | `apollo510b_evb` / Apollo510B | Arm GCC, Armclang, ATfE | Runtime validated |
| USB | Apollo4P EVB, Apollo4P Blue KBR, Apollo330mP, Apollo510, Apollo510B, Apollo510DL | Arm GCC, Armclang, ATfE | Dependency/catalog support; target smoke required before claiming validation |
| BLE | `apollo4p_blue_kxr_evb` / Apollo4P Blue Plus | Arm GCC, Armclang, ATfE | Runtime validated |
| BLE | `apollo510b_evb` / Apollo510B | Arm GCC, Armclang, ATfE | Runtime validated |
| BLE | `apollo3_evb` / Apollo3; `apollo3p_evb` / Apollo3P; Apollo4P Blue KBR | Arm GCC, Armclang, ATfE | Dependency/catalog support; target smoke required before claiming validation |

`nsx-tileio-usb` requires `nsx-core` and `nsx-usb`. `nsx-tileio-ble` requires
`nsx-core` and `nsx-ble`; the latter transitively requires Cordio and FreeRTOS.
Applications must provide board startup, cache/power policy, and (for BLE) WSF
pools, dispatcher tasking, and IRQ glue.

The compatibility declarations are not claims that every listed board was
flashed for this release. CI target smoke should use the exact dependency pin
in `PROVENANCE.md` and a real board profile when those dependencies and
toolchains are available.
