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
| PORTED  | 12,085 / 14,045 (86.0%) |
| REAL_GAP| 1,958 (13.9%) — no N/A |
| PARTIAL | 2 (0.0%) |
| BOOTLEG | 7 (recursive_false_gap_hunter.py) |

**Phase (v668):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,204 ahead / 1,119 behind upstream/main (last merge 2026-08-04 (upstream fetched)). The behind-count is the staleness timer — run the stash→pull→fix→pop workflow, then re-port the delta.

_Generated 2026-08-04T04:35:06Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
