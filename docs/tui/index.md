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

# Terminal UI (TUI) Reference

## What exists today

The full-screen TUI is an **ncurses** application implemented in C11:

- `src/cli/tui_*.c` — 12 modules split out of the original monolith:
  `tui_fullscreen.c` (main loop, init, streaming, gateway FIFO), `tui_win.c` (layout,
  pane windows, message rendering), `tui_input.c` (input editing helpers),
  `tui_stream.c` (streamed-token rendering), `tui_slash_cmds.c` (TUI-local slash
  handlers for modal overlays), `tui_session_browser.c`, `tui_config_editor.c`,
  `tui_model_picker.c`, `tui_image_viewer.c`, `tui_plugin_hub.c`, `tui_todo_panel.c`,
  `tui_views.c`.
- `src/hermes_cli/tui_*.c` — support modules: `tui_entry.c` (lifecycle wrapper),
  `tui_eventpub.c` (event publishing), `tui_slash_worker.c` (slash dispatch worker),
  `tui_json_rpc.c` (JSON-RPC 2.0 method registry), `tui_layout.c`, `tui_render.c`,
  `tui_websocket.c` (poll()-based WebSocket transport for remote TUI connections),
  `tui_gateway_compute_host.c`.

The feature set is organized as phases P189–P200 in `tui_fullscreen.c`'s header:
split-pane layout, multi-line input with slash autocomplete and emoji picker,
role-colored messages, token streaming, live tool feed, status bar, session
browse/search/load/delete/export, interactive config editor, sixel/kitty image
viewing, themes/skins, gateway backend, and a responsive (mobile) layout.

## Build and launch (POSIX only)

- `make tui` links `slermes-tui` with `-DHAS_NCURSES_TUI` (see `build/targets.mk`).
- A main binary built with `HAS_NCURSES_TUI` accepts `slermes --tui` / `slermes tui`
  (`src/main.c`); without the flag it prints an error saying ncurses is required.
- The build needs ncursesw/panel/tinfo and links `-lasound` (ALSA). The runtime uses
  `mkfifo`, `/dev/null`, and `/proc`-style POSIX facilities.

**Windows status: the ncurses TUI is a dead end on this platform.** As
`src/lua/slv_ui.h` states: ncurses "cannot build on Windows at all: no ncurses import
libs and no mkfifo in the MinGW headers". Note also that `TUI_SRC`/`TUI_OBJ` in
`build/config.mk` still point at `src/cli/tui_eventpub.c` etc., although those modules
now live in `src/hermes_cli/` (and `tui_transport.c` no longer exists) — the TUI build
metadata is partially stale.

## Direction: the Lua UI substrate

The project direction (WWW/Wua) is to replace curses — and SDL, and C# — with **Lua as
the UI substrate**:

- `src/lv/` — *slerm-Lua* (`slv`), a from-scratch Lua 5.4 reimplementation in C11
  (lexer, parser, evaluator, arena allocator, stdlib: base/string/table/math plus
  minimal os/io; coroutines are explicitly not implemented).
- `src/lua/slv_ui.c` / `slv_ui.h` — the seed of the Lua UI host: a w×h character-cell
  buffer owned by the C host, exposed to Lua only through a `term` table
  (`term.width`, `term.height`, `term.clear()`, `term.put(x,y,s)`, `term.get(x,y)`,
  `term.fill(ch)`, `term.refresh()`). Scripts can draw but cannot corrupt host state.

This page documents the ncurses TUI that exists today; new UI work should target the
Lua substrate rather than extending the curses code.

## Layout

Four panes (`pane_id_t` in `src/cli/tui_state.h`):

```
┌────────────────────────────┬─────────────────────┐
│ PANE_HISTORY               │ PANE_TOOL_FEED      │
│ message history (large)    │ live tool calls     │
├────────────────────────────┴─────────────────────┤
│ PANE_INPUT   prompt entry                        │
├──────────────────────────────────────────────────┤
│ PANE_STATUS  model/provider, tokens, budget      │
└──────────────────────────────────────────────────┘
```

Minimum terminal size is 40×8; below 80 columns the layout switches to
`TUI_LAYOUT_MOBILE`. Modal overlays (session browser, config editor, model picker,
image viewer, plugin hub, todo panel, gateway status, cron/log views, help) draw on
top and are dismissed with `Esc` or `q`.

## Keyboard shortcuts (verified against the input loop)

| Key | Action |
|-----|--------|
| `Enter` | Submit the input line |
| `Tab` | Trigger slash autocomplete; cycle suggestions; select emoji |
| `Up` / `Down` | Input history navigation |
| `PageUp` / `PageDown` | Scroll the history pane |
| `Left` / `Right` / `Home` / `End` | Move cursor in input |
| `Backspace` / `Delete` | Delete character before / at cursor |
| `Ctrl+C` | Abort streaming while streaming; quit when idle |
| `Ctrl+L` | Full redraw |
| `Ctrl+W` | Delete word backward |
| `Ctrl+K` | Delete to end of line |
| `Ctrl+P` | Toggle FPS overlay |
| `Esc` / `q` | Close modal overlay |

Older docs listed `Ctrl+D` (exit), `Ctrl+U` (clear line), and `Shift+Enter` (newline);
none of these are implemented in the current input loop.

## Slash-command dispatch

`tui_process_input()` first tries `tui_slash_dispatch()` (TUI-local commands that open
modal overlays), then falls back to the shared registry: `commands_dispatch()` →
`commands_try_skill()` → `commands_try_quick()` — the same `COMMANDS[]` table the CLI
uses (see [CLI index](../cli/index.md)). Unknown commands print an error line.

## Backend transports

Two layers exist, and they are different:

1. **Legacy FIFO transport (P199)** in `tui_fullscreen.c`: a named pipe
   (`/tmp/slermes-tui-rpc`, POSIX `mkfifo`) carrying newline-delimited, simplified
   JSON-RPC. Incoming methods handled: `print`, `error`, `stream`, `stream_done`,
   `tool_status`; outgoing messages go to `<fifo>.out`. This is not the gateway's
   full method list.
2. **JSON-RPC 2.0 registry** in `src/hermes_cli/tui_json_rpc.c`: proper method
   dispatch with typed params and standard errors — 92 registered methods
   (`grep -c 'tui_rpc_register(' src/hermes_cli/tui_json_rpc.c`), covering prompts,
   sessions, config, tools, skills, plugins, cron, approvals, billing, browser,
   delegation, attachments, and pets. The 8 pet methods are `pet_info`, `pet_cells`,
   `pet_gallery`, `pet_select`, `pet_remove`, `pet_disable`, `pet_scale`, `pet_thumb`.

## Pet integration

The `/pet` command (shared registry) manages the Petdex mascot: `info`, `gallery`,
`select <slug>`, `remove <slug>`, `disable`, `scale <n>`, plus `enable` and `hatch`
forms in the handler. See [Pet System](../pet/index.md) for states, rendering modes,
and configuration.
