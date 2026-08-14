# Goal & Mantra — Slermes C Translation (v671)

## Core Directive

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
| PORTED  | 5,543 / 13,404 (41.4%) |
| REAL_GAP| 7,861 (58.6%) — no N/A |
|  ├ DA-4 annotation-only stubs | 7,813 (PoP comment, no C body) |
|  └ Genuine gaps (no C at all) | 48 |
| PARTIAL | 0 (0.0%) |
| BOOTLEG | 117 (recursive_false_gap_hunter.py) |

**Phase (v671):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,566 ahead / 666 behind upstream/main (last merge 2026-08-14 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-14T23:15:26Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
