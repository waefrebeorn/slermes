# Next Session Prompt — Copy-Paste Ready

---

```
Goal: Close function-level parity gaps (REAL_GAP) across ALL Python modules —
and continue feature/API parity for desktop, web, skills, docs, scripts, tests,
configs. Rewriting from scratch in C is the point; there is NO N/A.

State (historical baseline — 2026-07-12; live counts are in the
`<!-- PARITY:AUTO -->
| PORTED  | 12,252 / 12,274 (99.8%) |
| REAL_GAP| 22 (0.2%) — no N/A |
| PARTIAL | 0 (0.0%) |
| BOOTLEG | 0 (recursive_false_gap_hunter.py) |

**Phase (v668):** MATCH phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. PORT phase (v398→v667) is legacy — complete; every function does real observable work and matches the Python original.

**Upstream sync checkpoint:** 1,200 ahead / 1,113 behind upstream/main (last merge 2026-07-30 (Merge upstream/main (21 commits); keep C11 fork deletions)). The behind-count is the staleness timer — run the stash→pull→fix→pop workflow, then re-port the delta.

_Generated 2026-08-04T01:19:23Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
