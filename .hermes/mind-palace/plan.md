# Plan — Slermes C Translation (v671)

## Current State (HISTORICAL — v666 baseline; see live block at bottom)

> The `<!-- PARITY:AUTO -->
| PORTED  | 13,330 / 14,045 (94.9%) |
| REAL_GAP| 710 (5.1%) — no N/A |
| PARTIAL | 5 (0.0%) |
| BOOTLEG | 3 (recursive_false_gap_hunter.py) |

**Phase (v671):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,309 ahead / 853 behind upstream/main (last merge 2026-08-09 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-09T07:38:25Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
