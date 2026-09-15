# Goal & Mantra — Slermes C Translation ()

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
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 annotation-only stubs | 11 (PoP comment, no C body) |
|  └ Genuine gaps (no C at all) | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 (recursive_false_gap_hunter.py) |
| **Watercup** | 313 PASS / 22,604 total (1.4%) — 22,291 fail |

**Phase (v672):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 2,072 ahead / 2,560 behind upstream/main (last merge 2026-09-14 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-09-15T04:34:33Z from live scanner `tests/slermes_parity_battleground.py` + watercup oracle (22,604 trace-cup gates) — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
