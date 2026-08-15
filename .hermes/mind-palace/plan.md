# Plan — Slermes C Translation (v671)

## Current State (HISTORICAL — v666 baseline; see live block at bottom)

> The `<!-- PARITY:AUTO -->
| PORTED  | 12,921 / 13,404 (96.4%) |
| REAL_GAP| 391 (2.9%) — no N/A |
|  ├ DA-4 annotation-only stubs | 2 (PoP comment, no C body) |
|  └ Genuine gaps (no C at all) | 389 |
| PARTIAL | 92 (0.7%) |
| BOOTLEG | 325 (recursive_false_gap_hunter.py) |

**Phase (v671):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,597 ahead / 928 behind upstream/main (last merge 2026-08-15 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-15T07:28:02Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
