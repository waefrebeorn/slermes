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

# Roadmap — Slermes C Translation

The full battle order lives in [ROADMAP.md](../ROADMAP.md); the ranked,
actionable work list is [docs/WORK_DASHBOARD.md](../docs/WORK_DASHBOARD.md).
This file is the .hermes-side summary.

**Rewriting from scratch in C is the point.** Stubs that log + return NULL
are REAL_GAP. There is no N/A and no "not doable" — the whole ecosystem,
including every third-party dependency, is SLERM work.

## Fronts

1. **Close REAL_GAPs** — one module at a time: scanner detail → grep before
   writing → implement/alias → oracle + watercup proof → build → commit.
   Per-area gap order: hermes_cli > agent > gateway > tools > cron. Pick the
   axis that matches the ask (`scripts/parity_coverage.py --dirs|--fanin`).
2. **SLERM third-party dependencies** — whisper.cpp → c11_whisper (active),
   SQLite engine, ncurses (retired by the Lua UI), ctranslate2, and the
   long-horizon crypto/inflate/regex/raster targets. See
   [docs/third-party.md](../docs/third-party.md).
3. **Lua UI substrate** — `src/lv/` (upstream Wua) + `src/lua/slv_ui.c`;
   the ncurses TUI is a dead end on Windows. Lua serves parity; the gap list
   wins when they compete.
4. **Deepen verification** — watercup gates over the pure-function twin
   pool; oracle registry coverage; fail-closed gates everywhere.

## Live per-area numbers

Never hand-typed here. Run:

```bash
python scripts/parity_coverage.py --dirs      # module coverage per area
python tests/slermes_parity_battleground.py --detail   # function gaps
make parity                                   # refresh the sentinel above
```

## Definition of done

REAL_GAP 0, BOOTLEG 0, every port oracle- AND watercup-MATCH, all three
platforms build, every dependency SLERMed or actively being SLERMed. The
phase flips to COMPLETE on its own — no hand-edited victory.
