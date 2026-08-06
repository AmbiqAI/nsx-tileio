# Changelog

All notable changes to the `nsx-tileio` project and its two modules are
documented here.

## [0.1.0] - 2026-08-06

### Added

- First coherent semantic release metadata for `nsx-tileio-usb` and
  `nsx-tileio-ble`; both modules are version `0.1.0`.
- Exact provenance, ownership, license, notice, compatibility, and immutable
  archive-release policy.
- Structural host tests covering metadata, target/dependency contracts,
  framing, UIO state requests, partial sends, and the absence of timed packets.

### Included

- USB UIO zero-length state-request framing.
- USB full-frame availability check that avoids blocking or emitting partial
  256-byte TileIO packets.
- Runtime qualification on Apollo4 Blue Plus and Apollo510B for both transports.

### Not included

- Timed signal packets (the experimental implementation was reverted before
  `main`).
- The draft BLE `uio_read_cb` change, registry promotion, or any published tag
  and release.

[0.1.0]: https://github.com/AmbiqAI/nsx-tileio/releases/tag/v0.1.0
