# Goal & Mantra — Slermes C Translation (v668)

## Core Directive
All Python functions ported to C — live scanner: 4,884/9,731 (50.2%) PORTED, 4,774 REAL_GAP, 73 PARTIAL. Now: keep closing REAL_GAPs, then build desktop.

## Mantra
"Rewriting from scratch in C is the point." Every function must have real logic.
No façades. No stubs. No placeholders. Triple-check: plumber, painter, devil.

## Next Missions
1. **Desktop parity** (99 features missing) — terminal, PTY, chat, clipboard, WebSocket
2. **Web app parity** (111 API endpoints missing) — expand web_app.c
3. **TUI parity** — audit curses/terminal vs TS
4. **CLI parity** — audit hermes_cli vs TS
5. **CUA integration** — wire computer_use.c MCP client to cua-driver on macOS

## CUA Status
✅ `src/tools/computer_use.c` — 2,135 lines, noop/X11/Wayland backends
✅ `include/hermes_computer_use.h` — full backend vtable
⏳ MCP client wiring to cua-driver (macOS only)

<!-- PARITY:AUTO -->
| PORTED  | 12,252 / 12,274 (99.8%) |
| REAL_GAP| 22 (0.2%) — no N/A |
| PARTIAL | 0 (0.0%) |
| BOOTLEG | 0 (recursive_false_gap_hunter.py) |

**Phase (v668):** MATCH phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. PORT phase (v398→v667) is legacy — complete; every function does real observable work and matches the Python original.

**Upstream sync checkpoint:** 1,200 ahead / 1,113 behind upstream/main (last merge 2026-07-30 (Merge upstream/main (21 commits); keep C11 fork deletions)). The behind-count is the staleness timer — run the stash→pull→fix→pop workflow, then re-port the delta.

_Generated 2026-08-04T01:19:23Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
