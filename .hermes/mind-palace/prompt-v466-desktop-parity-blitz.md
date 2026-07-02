# Next Session Prompt -- v466: Desktop Parity Blitz (Notifications + Settings + Profiles + Window Controls)

## Context

You are continuing the Slermes C translation project. Current state:
- **8,688/8,688 PORTED (100%)** -- all Python→C port complete
- **Build: CLEAN** -- zero errors
- **Scanner: 100%** -- slermes_parity_battleground.py confirms
- **Desktop Parity: 15 features implemented** (up from 5)

## What Was Done in v465

Created 10 new C source files + 8 headers:
- `src/pty.c` — PTY allocation (openpty/fork), resize, I/O
- `src/terminal.c` — VT100/xterm emulation with scrollback, selection
- `src/window_compositor.c` — multi-window management (open/focus/close)
- `src/chat_render.c` — markdown/code/tool-call rendering
- `src/chat_composer.c` — text input, autocomplete, slash commands
- `src/gateway_client.c` — WebSocket + JSON-RPC + streaming
- `src/clipboard.c` — platform clipboard (Wayland/xclip/xsel/pbcopy/Win32)
- `src/file_ops.c` — file read/write/browse, directory operations
- `src/gateway_probe.c` — gateway reachability check
- `src/window_stubs.c` — window_minimize/maximize/restore stubs

## Mission: Continue Desktop Parity Blitz

The desktop app has **111 features** across 14 areas. ~15 are now done.
We need to close more gaps, focusing on P1 features.

## Remaining P0 Gaps (not yet implemented as C files)

| Feature | TS Source | C Target | Notes |
|---------|-----------|----------|-------|
| Session delete | apps/desktop/src/app/session | src/desktop_app.c | Delete with confirmation |
| Session window | electron/session-windows.cjs | src/window_compositor.c | Open session in new window |
| Model picker | apps/desktop/src/app/models | src/desktop_app.c | Dialog to select model |
| Model switch | apps/desktop/src/app/models | src/desktop_app.c | Switch active model |
| Gateway URL config | electron/connection-config.cjs | src/gateway_client.c | Configure gateway URL |

## P1 Priorities (Next Batch)

| Feature | TS Source | C Target | Effort |
|---------|-----------|----------|--------|
| Window minimize/maximize/restore | electron/main.cjs | src/window_stubs.c | Small — implement per-platform |
| Window titlebar customization | electron/main.cjs | include/window.h | Medium |
| Window menu bar | electron/main.cjs | src/desktop_app.c | Medium |
| System tray icon | electron/main.cjs | src/desktop_app.c | Medium |
| Single instance lock | electron/main.cjs | src/desktop_app.c | Small |
| Terminal disposal | electron/main.cjs | src/terminal.c | Small |
| Session rename | apps/desktop/src/app/session | src/desktop_app.c | Small |
| Session archive | apps/desktop/src/app/session | src/desktop_app.c | Small |
| Session search | apps/desktop/src/app/session | src/desktop_app.c | Medium |
| Code syntax highlighting | react-shiki | src/chat_render.c | Large |
| File attachments | apps/desktop/src/app/chat/composer | src/chat_composer.c | Medium |
| Image paste | apps/desktop/src/app/chat/composer | src/chat_composer.c | Medium |
| Slash commands | apps/desktop/src/app/chat/composer | src/chat_composer.c | Medium |
| Tool call rendering | apps/desktop/src/app/chat | src/chat_render.c | Medium |
| Scroll to bottom | apps/desktop/src/app/chat | src/desktop_app.c | Small |
| Profile list/create/switch/delete | apps/desktop/src/app/profiles | src/desktop_app.c | Medium |
| Profile soul/model | apps/desktop/src/app/profiles | src/desktop_app.c | Medium |
| Settings page | apps/desktop/src/app/settings | src/desktop_app.c | Large |
| Theme switcher | apps/desktop/src/app/settings | src/desktop_app.c | Medium |
| Connection config | apps/desktop/src/app/settings | src/desktop_app.c | Medium |
| File dialog (open/save) | electron/main.cjs | src/file_ops.c | Medium |
| File browse | electron/main.cjs | src/file_ops.c | Small |
| Open external URL | electron/main.cjs | src/desktop_app.c | Small |
| Notifications | electron/main.cjs | src/desktop_app.c | Medium |
| Safe storage | electron/main.cjs | src/desktop_app.c | Medium |
| Check for updates | electron/main.cjs | src/gateway_probe.c | Small |
| Download/apply update | electron/main.cjs | src/gateway_probe.c | Medium |
| Auth ticket | electron/dashboard-token.cjs | src/gateway_client.c | Small |
| Connection revalidate | electron/main.cjs | src/gateway_client.c | Small |
| File read data URL | electron/main.cjs | src/file_ops.c | Done in v465 |
| File write | electron/main.cjs | src/file_ops.c | Done in v465 |
| File delete | electron/main.cjs | src/file_ops.c | Done in v465 |
| Directory create | electron/main.cjs | src/file_ops.c | Done in v465 |

## Implementation Strategy

**Batch 1 (this session):** Focus on the remaining P0 gaps + high-value P1 features:
1. Session delete confirmation
2. Model picker/switch integration
3. Gateway URL config
4. Window minimize/maximize/restore (implement in window_stubs.c → window_wayland.c)
5. Notifications (desktop notification API)
6. Settings page framework
7. Profile management (list/create/switch/delete)

## Key Files to Modify

- `src/desktop_app.c` — Expand with session delete, model picker, settings, profiles, notifications
- `src/window_stubs.c` — Implement window_minimize/maximize/restore for Wayland
- `src/terminal.c` — Add terminal disposal
- `src/chat_render.c` — Add syntax highlighting integration
- `src/chat_composer.c` — Add file attachments, image paste
- `src/gateway_client.c` — Add auth ticket, connection revalidation
- `src/gateway_probe.c` — Add update check/download
- `Makefile` — Add any new .o files

## Rules

1. **Build must stay CLEAN** -- zero errors after every commit
2. **Scanner must stay 100%** — don't break Python→C port
3. **"Rewriting from scratch in C"** — all new code must have real logic, no stubs
4. **PoP annotations** on all new functions matching Python source names
5. **hermes_log() + return NULL is NOT an implementation** — use fprintf(stderr, ...) for logging
6. **Test after every batch** — make && ./test_runner.sh
7. **Commit per batch** — don't accumulate uncommitted code
8. **Autonomous loop** — no questions, no choices, just implement
9. **Run prestige ritual** after every checkpoint

## Desktop Parity Audit Command

python3 tests/desktop_parity_audit.py --priority P0

## Session Workflow

1. Read this prompt
2. Run the desktop parity audit to see current gaps
3. Pick next batch (start with remaining P0 gaps)
4. Read Python/TS source for reference
5. Implement in C11 with real logic
6. Build, test, commit
7. Run prestige ritual
8. Move to next batch
9. Repeat until all P0 + high-priority P1 features are done
