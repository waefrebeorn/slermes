<!-- PARITY:AUTO -->
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | 313 PASS / 22,604 (1.4%) · 0/6230 ported match · 11511/11511 gap mismatch |
**Phase (v672):** PORT phase

**Upstream sync:** 2,072 ahead / 2,639 behind (last merge 2026-09-15 (upstream fetched))

_Generated 2026-09-15T06:22:36Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Goal Mantra — Slermes C Translation ()

── CURRENT STATE () ──
**PARITY SCANNER:** 13,286 PORTED (94.6%) · 17 PARTIAL ·  · 742 REAL_GAP
**Total features:** 14,045 · **Upstream sync:** 1,

**Terminal env registry:** PORTED () — real C port of tools/terminal_tool.py env layer
**terminal_tool.py:** 61/64 (95.3%), 3 REAL_GAPs remain
**Agent:** M4 (largest remaining REAL_GAP pool)
**TUI:** 9/9 PORTED ()
**Gateway:** 29 modules PORTED ()
**Plugins:** 19 C files, all stub claims stale

## The Mantra
1. **No stubs.** A function that logs + returns a default is a REAL_GAP, not a port.
   "For brevity", "scaffolding for later", "stub for extension", "for later" — all REAL_GAP.
2. **Form must equal function.** A PoP annotation on a fake body is worse than none.
3. **Rewrite from scratch in C is the point.** Anything in the upstream delta is work, never N/A.
4. **One shared oracle harness.** Fixtures in tests/oracle/fixtures/, generic runner
   tests/oracle/runners/run_oracle.sh — never hand-roll per-port harnesses.
5. **Reuse before add.** Grep lib/ + src/ for an existing backend before writing a port file.

## The Loop
Read scanner → battleship → pick highest REAL_GAP module with C file but wrong architecture → implement C module matching Python architecture → add PoP annotations → update scanner mappings → build → scan → vault resolved → repeat.

No choices. No questions. Never stop between gaps.
