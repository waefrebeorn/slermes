# Plan — Slermes C Translation (v671)

## Current State (HISTORICAL — v666 baseline; see live block at bottom)

> The `<!-- PARITY:AUTO -->
| PORTED  | 13,581 / 15,942 (85.2%) |
| REAL_GAP| 2,329 (14.6%) — no N/A |
|  ├ DA-4 annotation-only stubs | 473 (PoP comment, no C body) |
|  └ Genuine gaps (no C at all) | 1,856 |
| PARTIAL | 32 (0.2%) |
| BOOTLEG | 0 (recursive_false_gap_hunter.py) |

**Phase (v671):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,640 ahead / 3 behind upstream/main (last merge 2026-08-19 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-19T07:45:26Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
