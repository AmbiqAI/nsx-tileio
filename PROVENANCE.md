# Provenance and release audit

## Release identity

| Item | Value |
| --- | --- |
| Project | `AmbiqAI/nsx-tileio` |
| Release candidate | `v0.1.0` |
| Audited main baseline | `07837ff4ac677dbb902ae4300e4608b81a446c50` |
| Stable registry pin audited | `1c2257aeeeccb21badbcc04e342af38c63ccc8b0` |
| Commits after registry pin | 7 |
| Release tag policy | immutable annotated `vMAJOR.MINOR.PATCH` tag |

The audited source is the fetched `origin/main` commit above. The eventual
`v0.1.0` tag must point at the later main commit that merges this release
foundation; that merge SHA is deliberately not guessed or published here. The
`agent/add-ble-uio-read-callback` branch
(`e3cfa78804d5cf16ccc62d640b258b115ca35074`) was intentionally not included:
it is a draft PR and changes the public BLE read contract after the release
source was selected.

## Dependency pins

| Dependency | Exact revision | Role |
| --- | --- | --- |
| `AmbiqAI/nsx-ambiq-sdk` | `2eba24ad776096784764cbe91c8176b434dd3bdf` | `nsx-core`, `nsx-usb`, `nsx-ble`, and their AmbiqSuite substrates |
| `AmbiqAI/ns-tileio` | `55bd4a79bc480e59eed1cca85a4335736d8887dc` | legacy source and API ownership reference |

The module metadata records only direct logical dependencies (`nsx-core` plus
`nsx-usb` or `nsx-ble`). The SDK pin is a reproducibility record, not a vendored
copy and not a request to update the neuralSPOT-X registry.

## Exact history from the stable pin

The seven commits between the registry pin and `origin/main` were audited in
topological order:

| Commit | Change | Release treatment |
| --- | --- | --- |
| `c273f12177c044fea4ec5c4e306db5ef857dbee6` | Accept a zero-length USB UIO frame as a request for the current state; retain 8-byte host updates. | Included and documented |
| `47356e883fc1c8ddf3f7fef24d01e656c7958045` | Experimental timed signal packet acceptance. | Superseded; not a release feature |
| `0841c75baab7418ae94e7774651ce1221f076040` | Experimental timed-frame packing follow-up. | Superseded; not a release feature |
| `013df3ed1b192bee6910d8d09a768e4e0394e1a5` | Reverted timed signal packet support. | Included as the current behavior |
| `8989009c169928fefb476962ea35d70598f45ae9` | Avoid blocking or emitting a partial USB frame when the endpoint cannot accept all 256 bytes. | Included and tested |
| `2f89e61ba42798cbb7133a816d6974cb12f42615` | Document raw, unframed host-to-device USB writes. | Included |
| `07837ff4ac677dbb902ae4300e4608b81a446c50` | Merge USB UIO state request work into `main`. | Release source |

`git show <commit>` is the authoritative patch. In particular, `slot_type == 3`
and any timed-packet API are not part of `v0.1.0`.

## Source ownership

`modules/nsx-tileio-usb` and `modules/nsx-tileio-ble` are NSX-specific adapter
implementations. They retain the useful `tio_*` API names for source migration
but do not copy the legacy task, dispatcher, board, or transport-substrate
ownership model. See `NOTICE`, `OWNERS.md`, and each module README for the
boundary.
