<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Goal & Mantra — Slermes C Translation

## Mantra
"Rewriting from scratch in C is the point. Every function must have real logic. No façades. No stubs. No placeholders."

## Core Directive
Close REAL_GAPs. Every watercup MISMATCH that flips to MATCH is a win.

## System ()
- Live scanner → `live_parity_scan.json` → walkway files (PARITY:AUTO)
- Watercup oracle → 22,604 trace-cup gates in parallel
- Bootleg hunter → 0 fake-looking stubs
- Gap triage → ranked by pure-function yield

## Next Mission
1. Fix ported watercup gates (read real C signatures)
2. Close pure-function gaps
3. Sync upstream (2,639 behind)
