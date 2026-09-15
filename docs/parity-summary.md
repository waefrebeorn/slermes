# Slermes Parity Summary

## Overall Numbers (live)

<!-- PARITY:AUTO -->
| Classification | Count | Percentage | Meaning |
|----------------|-------|------------|----------|
| **PORTED** | 6,466 | 35.5% | C11 implementation with PoP annotation + real C body |
| **REAL_GAP** | 11,715 | 64.3% | Honest gaps (not yet ported) |
|  ├ DA-4 annotation-only stubs | 11 | — | PoP comment with NO C function body (scanner was fooled) |
|  └ Genuine gaps (no C at all) | 11,704 | — | No C equivalent exists anywhere |
| **PARTIAL** | 38 | 0.2% | All C fns now carry PoP annotations |
| **BOOTLEG** | 0 | — | No-work echo stubs (recursive_false_gap_hunter.py) |
| **Watercup** | 313 PASS / 22,604 total (1.4%) | Trace-cup parity — C ports matching LIVE Python behavior |
| **TOTAL** | 18,219 | 100% | All Python functions/methods scanned |

> **Generated 2026-09-15T04:34:33Z by `make parity-walkway` from the live scanner + watercup oracle (22,604 trace-cup gates).** The PORT phase (v398→v667) is legacy — this table is the single source of truth for completeness. Do not hand-edit.
<!-- /PARITY:AUTO -->

