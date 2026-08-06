# Release policy

## `v0.1.0`

`v0.1.0` is the first coherent semantic release of this repository. Both
modules started at metadata version `0.0.0`, there are no prior tags or
published releases, and the APIs have runtime validation but no compatibility
promise from an earlier semantic series. A `0.1.0` release therefore records
the first usable public contract without implying `1.0.0` stability.

The project and both module metadata versions are always identical. The root
`version.txt` is the release source of truth and is checked against both
`nsx-module.yaml` files.

## Scope

This release includes:

- USB fixed 256-byte TileIO framing over `nsx-usb`, including zero-length UIO
  state requests and the non-blocking full-frame send guard.
- BLE TileIO GATT service and signal, metric, and 8-byte UIO helpers over
  `nsx-ble`.
- Runtime validation on Apollo4 Blue Plus and Apollo510B for both transports.
- Compatibility, provenance, ownership, license, and reproducible archive
  metadata.

This release does **not** include:

- timed signal packets or a timed-packet public API;
- the draft BLE `uio_read_cb` change;
- examples, host utilities, a new board-support layer, or registry changes;
- publication of a tag or GitHub release by this pull request.

## Immutable publication

The release workflow is manual by design. A maintainer supplies the expected
version, the workflow verifies that the exact main commit passed CI, refuses to
retarget an existing tag, creates an annotated `vMAJOR.MINOR.PATCH` tag, and
uploads `nsx-tileio-VERSION.tar.gz` plus its SHA-256 checksum. The workflow is
the only supported publication path.

For future changes, use patch releases for fixes that preserve the public
contract, minor releases for additive compatible APIs, and major releases for
breaking changes. USB framing, BLE UUIDs, callback timing, and app ownership
are compatibility surfaces.
