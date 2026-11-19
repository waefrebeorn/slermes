<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
