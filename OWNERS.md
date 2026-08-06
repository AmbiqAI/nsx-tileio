# Ownership

Ambiq maintains this repository and owns the release decision for both
`nsx-tileio-usb` and `nsx-tileio-ble`.

The modules are transport adapters, not board-support packages. Changes to
`nsx-core`, `nsx-usb`, `nsx-ble`, TinyUSB, Cordio, AmbiqSuite, or the legacy
`ns-tileio` repository remain owned and released by those projects.

Release qualification requires review from an nsx-tileio maintainer and a
successful CI run for the exact commit being tagged. The stable neuralSPOT-X
registry is a downstream consumer and is not modified by this repository's
release automation.
