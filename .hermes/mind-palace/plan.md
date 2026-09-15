# Plan — Slermes C Translation ()

## Current State (HISTORICAL —  baseline; see live block at bottom)

> The `<!-- PARITY:AUTO -->
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 annotation-only stubs | 11 (PoP comment, no C body) |
|  └ Genuine gaps (no C at all) | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 (recursive_false_gap_hunter.py) |
| **Watercup** | 313 PASS / 22,604 total (1.4%) — 22,291 fail |

**Phase (v672):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 2,072 ahead / 2,560 behind upstream/main (last merge 2026-09-14 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-09-15T04:34:33Z from live scanner `tests/slermes_parity_battleground.py` + watercup oracle (22,604 trace-cup gates) — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
