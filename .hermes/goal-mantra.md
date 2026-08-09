# Goal Mantra — Slermes C Translation (v671)

── CURRENT STATE (v671) ──
**PARITY SCANNER:** 13,286 PORTED (94.6%) · 17 PARTIAL · 3 BOOTLEG · 742 REAL_GAP
**Total features:** 14,045 · **Upstream sync:** 1,306 ahead / 842 behind

**Terminal env registry:** PORTED (v671) — real C port of tools/terminal_tool.py env layer
**terminal_tool.py:** 61/64 (95.3%), 3 REAL_GAPs remain
**Agent:** M4 (largest remaining REAL_GAP pool)
**TUI:** 9/9 PORTED (v351)
**Gateway:** 29 modules PORTED (v353)
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
