<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
