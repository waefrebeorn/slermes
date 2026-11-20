<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Goal & Mantra — mind palace

## Mantra
"Rewriting from scratch in C is the point. Every function must have real
logic. No façades. No stubs. No placeholders."

## Core Directive
Close REAL_GAPs across the whole ecosystem — agent, tools, platforms,
plugins, and every third-party dependency. Nothing is N/A; nothing is "not
doable". Every watercup MISMATCH that flips to MATCH is a win; every oracle
that goes green with real cases behind it is a win.

## System (the detectors, not hand-typed numbers)
- Live scanner → `live_parity_scan.json` → walkway sentinels (`make parity`)
- Diff oracle → C vs LIVE Python outputs
- Watercup → trace-cup path parity (22,603 gates)
- Bootleg hunter → echo/no-work stubs
- Reality audit → body-level honest scoreboard

## Next Mission
1. Close pure-function REAL_GAPs (oracle + watercup proof per bite).
2. Expand watercup coverage; fix ported-gate harness signatures.
3. SLERM the third-party dependency front.
4. Sync upstream when behind; port the delta.
