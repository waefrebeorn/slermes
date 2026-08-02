# Goal & Mantra — Slermes C Translation (v666)

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
