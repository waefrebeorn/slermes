# Plan — Slermes C Translation (v671)

## Current State (HISTORICAL — v666 baseline; see live block at bottom)

> The `<!-- PARITY:AUTO -->
| PORTED  | 13,335 / 14,045 (94.9%) |
| REAL_GAP| 705 (5.0%) — no N/A |
| PARTIAL | 5 (0.0%) |
| BOOTLEG | 3 (recursive_false_gap_hunter.py) |

**Phase (v671):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,310 ahead / 856 behind upstream/main (last merge 2026-08-08 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-09T08:21:45Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
