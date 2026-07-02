# Next Session Prompt — v465: Desktop Parity Blitz (Terminal + PTY + Chat + CUA)

## Context

You are continuing the Slermes C translation project. Current state:
- **8,688/8,688 PORTED (100%)** — all Python→C port complete
- **Build: CLEAN** — zero errors
- **Scanner: 100%** — slermes_parity_battleground.py confirms
- **Triple DA audit suite** created (triple_devil_advocate.py, ts_to_c_parity.py, desktop_parity_audit.py)
- **Prestige ritual complete** — all walkway files at v465, user-level synced

## Mission: Desktop Parity Blitz

The desktop app (Electron/TS) has **111 features** across 14 areas. Only **5 are done** (4%).
We need to close the **99 missing features** by implementing them in C11.

## CUA Clarification

CUA (cua-driver) is **already implemented** in `src/tools/computer_use.c` (2,135 lines).
It provides desktop CONTROL (screen capture, mouse, keyboard) for AI agents.
It is NOT a desktop application framework.

The desktop app (`src/desktop_app.c`, 650 lines) is a separate stub that needs 99 features.
CUA and desktop_app are complementary: CUA controls the desktop, desktop_app IS the app.

## Architecture

The current `desktop_app.c` (650 lines, 18 functions) is a Win32-only stub.
The window layer (`window_wayland.c`, `window_win32.c`, `window_macos.m`) is done.
Everything else needs to be built on top.

**Recommended approach:**
1. Keep the window layer as-is (Wayland/Win32/macOS)
2. Add a **compositor layer** that manages multiple windows
3. Add a **UI rendering layer** (immediate-mode GUI, NOT React)
4. Add a **WebSocket client** for gateway communication
5. Add **PTY/terminal** support (libvterm or custom VT100)
6. Add **native OS integrations** (clipboard, notifications, file dialogs)

## Priority Order (P0 First)

| Priority | Feature | TS Source | C Target | Effort |
|----------|---------|-----------|----------|--------|
| P0 | PTY allocation | node-pty | src/pty.c | Medium |
| P0 | Terminal emulation | @xterm/xterm | src/terminal.c | Large |
| P0 | Terminal resize | electron/main.cjs | src/terminal.c | Small |
| P0 | Terminal input/output | electron/main.cjs | src/terminal.c | Medium |
| P0 | Session create/switch/delete | apps/desktop/src/app/session | src/desktop_app.c | Medium |
| P0 | Message rendering (markdown) | apps/desktop/src/app/chat | src/chat_render.c | Large |
| P0 | Message composer | apps/desktop/src/app/chat/composer | src/chat_composer.c | Large |
| P0 | Streaming responses | apps/desktop/src/app/chat | src/gateway_client.c | Medium |
| P0 | Clipboard read/write | electron/main.cjs | src/clipboard.c | Small |
| P0 | File read | electron/main.cjs | src/file_ops.c | Small |
| P0 | WebSocket client | @hermes/shared | src/ws_client.c | Medium |
| P0 | Gateway probe | electron/gateway-ws-probe.cjs | src/gateway_probe.c | Small |
| P0 | Model picker/switch | apps/desktop/src/app/models | src/desktop_app.c | Medium |

## Implementation Strategy

**Batch approach:** Implement multiple features per commit. Group by dependency:
1. **Batch 1:** PTY + Terminal + Terminal I/O (all depend on each other)
2. **Batch 2:** Session management + Window management (multi-window)
3. **Batch 3:** Chat rendering + Composer + Streaming (all UI)
4. **Batch 4:** Native integrations (clipboard, file ops, notifications)
5. **Batch 5:** WebSocket client + Gateway probe + API client

## Key Files to Create/Modify

- `src/terminal.c` / `src/terminal.h` — VT100/xterm emulation + PTY
- `src/chat_render.c` / `src/chat_render.h` — Markdown/code rendering
- `src/chat_composer.c` / `src/chat_composer.h` — Input with autocomplete
- `src/gateway_client.c` / `src/gateway_client.h` — WebSocket + JSON-RPC
- `src/clipboard.c` / `src/clipboard.h` — Platform clipboard
- `src/file_ops.c` / `src/file_ops.h` — File read/write/browse
- `src/desktop_app.c` — Expand with session management
- `src/window_compositor.c` / `src/window_compositor.h` — Multi-window management
- `Makefile` — Add new .o files

## Rules

1. **Build must stay CLEAN** — zero errors after every commit
2. **Scanner must stay 100%** — don't break Python→C port
3. **"Rewriting from scratch in C"** — all new code must have real logic, no stubs
4. **PoP annotations** on all new functions matching Python source names
5. **hermes_log() + return NULL is NOT an implementation**
6. **Test after every batch** — make && ./test_runner.sh
7. **Commit per batch** — don't accumulate uncommitted code
8. **Autonomous loop** — no questions, no choices, just implement
9. **Run prestige ritual** after every checkpoint (slermes-prestige-ritual skill)

## Tools Available

- `tests/triple_devil_advocate.py` — 3-layer audit
- `tests/ts_to_c_parity.py` — TS→C parity checker
- `tests/desktop_parity_audit.py` — 111-feature gap tracker
- `tests/plumber_deep_dive.py` — Signature cross-reference
- `slermes-prestige-ritual` skill — Run after every checkpoint

## Desktop Parity Audit Command

```bash
python3 tests/desktop_parity_audit.py --priority P0
```

## Reference: What the TS Desktop App Does

The Electron app (`apps/desktop/electron/main.cjs`, 6,762 lines) provides:
- **63 IPC handlers** between main process and renderer
- **30+ Electron APIs** (BrowserWindow, Menu, Notification, clipboard, dialog, etc.)
- **33 Node modules** (node-pty, @xterm/xterm, react, etc.)
- **Key features:** chat with markdown/code, terminal, session management, profiles, models, settings, updates, file operations, native OS integrations

## Window System (Already Done)

- `src/window_wayland.c` — xdg-shell + EGL (252 lines, 47 functions)
- `src/window_win32.c` — Win32 native (736 lines, 41 functions)
- `src/window_macos.m` — Cocoa NSWindow (320 lines, partial)
- `include/window.h` — Cross-platform API (308 lines)

## Session Workflow

1. Read this prompt
2. Run `python3 tests/desktop_parity_audit.py --priority P0` to see gaps
3. Pick next batch (start with PTY + Terminal)
4. Read Python/TS source for reference
5. Implement in C11 with real logic
6. Build, test, commit
7. Run prestige ritual
8. Move to next batch
9. Repeat until all P0 features are done
