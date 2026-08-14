# Plan — Slermes C Translation (v671)

## Current State (HISTORICAL — v666 baseline; see live block at bottom)

> The `<!-- PARITY:AUTO -->
| PORTED  | 5,543 / 13,404 (41.4%) |
| REAL_GAP| 7,861 (58.6%) — no N/A |
|  ├ DA-4 annotation-only stubs | 7,813 (PoP comment, no C body) |
|  └ Genuine gaps (no C at all) | 48 |
| PARTIAL | 0 (0.0%) |
| BOOTLEG | 117 (recursive_false_gap_hunter.py) |

**Phase (v671):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,566 ahead / 666 behind upstream/main (last merge 2026-08-14 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-14T23:15:26Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
