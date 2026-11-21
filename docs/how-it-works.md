<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# How It Works — Slermes C CLI Depth Documentation

## Overview

The Slermes C CLI is the terminal interface of the C11 SLERM of the Python
Hermes Agent. It runs as a single native binary with no Python runtime
dependency for core operation.

This document describes the data flow, the QoL features available to the
user, and how the components assemble. Every file and function named here
was verified against the source tree at rewrite time. Behavior parity is
proven by the differential oracle suite (`tests/oracle/registry.json`,
`bash tests/run_all_oracles.sh`) and the watercup gates — not by prose in
this document. Live port counts live in the `PARITY:AUTO` sentinel above
and in `docs/parity-summary.md`.

---

## 1. Entry Point & Startup

The real entry point is `main()` in **`src/main.c`** (not `src/cli/main.c`,
which does not exist). There is no `getopt_long`: top-level dispatch is a
sequence of `strcmp(argv[1], ...)` checks, mirroring Python argparse
behavior (including exit codes).

```
main                              [src/main.c]
  ├─ winsock_init()               Windows only: WSAStartup so libhttp works
  │                               on plain MinGW builds (P78)
  ├─ install_safe_stdio()         ignore SIGPIPE — broken pipe must not
  │                               crash the agent (L03)
  ├─ load_env()                   libdotenv: $SLERMES_HOME/.env, else
  │                               ~/.slermes/.env, else ~/.hermes/.env;
  │                               strips non-ASCII from values of keys
  │                               ending _API_KEY/_TOKEN/_SECRET/_KEY
  ├─ --version / -v               → print_banner(), exit 0
  ├─ --help / -h                  → cli_print_usage() [src/cli/cli.c], exit 0
  ├─ unknown leading dash-flag    → usage error, exit 2 (argparse parity)
  ├─ subcommand dispatch (strcmp on argv[1]):
  │    gateway       → hermes_gateway_main        [src/gateway/server.c]
  │    cron          → hermes_cron_main           [src/cron/scheduler.c]
  │    init          → hermes_config_init         [src/hermes_cli/config_setup.c]
  │    setup         → hermes_config_setup_interactive [src/hermes_cli/config_setup.c]
  │    doctor        → inline diagnostics (version, config, API keys)
  │    update        → cmd_update                 [src/hermes_cli/write_approval_commands.c]
  │    completions   → inline bash / zsh / install scripts
  │    acp           → acp_server_new/run/free    [src/acp/server.c]
  │    mcp-serve     → mcp_serve_start            [src/mcp_serve.c]
  │    api-server    → api_server_start           [src/hermes_cli/api_server.c]
  ├─ --tui / tui     → tui_fullscreen_run         [src/cli/tui_fullscreen.c]
  │                    (requires HAS_NCURSES_TUI; otherwise error, exit 1)
  ├─ first-run welcome banner     onboarding.json "cli_welcome" flag
  │                               (is_seen / mark_seen)
  ├─ mcp_init_all()               connect configured MCP servers
  └─ cli_main(argc, argv)         [src/cli/cli.c]
```

Recognized top-level flags (the `KNOWN_FLAGS` table in `src/main.c`):
`--help`, `-h`, `--version`, `-v`, `--json`, `--session`, `--profile`,
`--eval`, `-e`, `--tui`. Any other leading-dash argument exits 2 with
"slermes: error: unrecognized argument" — it never falls through into
interactive mode. `mcp_serve` and `api_server` are accepted as underscore
aliases of `mcp-serve` / `api-server`.

On Windows, `WinMain` (SDL_MAIN_HANDLED is defined project-wide) forwards
to `main(__argc, __argv)`.

### cli_main startup sequence (src/cli/cli.c)

1. **Display + logging**: `display_init()`, `setup_logging()`.
2. **Interactivity**: `g_cli.interactive = isatty(STDIN_FILENO)`.
3. **Config**: `hermes_config_load()` + `hermes_config_load_env()`
   (`src/cli/config.c`, `config.yaml` via libyaml with `!include` support);
   SIGHUP-based config reload armed (`hermes_config_setup_reload`).
4. **Terminal resize**: SIGWINCH handler redraws the prompt (D13).
5. **xAI retirement warning** for retired models in config (L04).
6. **Security policy**: TIRITH engine init from `config.security` (O13).
7. **Audit**: `audit_init()` + rotation (10 MB, 5 files, 30 days — O12).
8. **Skin**: `cli_skin_init()` + `display_set_skin()` (lib/libskin).
9. **Agent**: `init_agent()` (defined in `src/agent/agent_loop.c`), then
   tool-event callback and streaming callback wiring.
10. **Tools + plugins**: `tools_init_all()`, plugin discovery under
    `~/.slermes/plugins`, registry copied into agent state.
11. **Config → agent**: `agent_configure_from_config()` applies all LLM
    fields; personality, YOLO, fast mode, compression, spinner style,
    CDP URL applied.
12. **Hardening**: file-permission hardening (O15), persistent approval
    allowlist loaded from `allowlist.json`.

### Argument handling inside cli_main

- `--json` — order-independent, stripped from argv; responses wrapped by
  `cli_json_respond()`.
- `--profile <name>` — `hermes_set_profile()`, consumes the value.
- `--session <id>` — attach to a stored session (interactive or one-shot).
- `-e` / `--eval` — batch-mode marker, skipped before message assembly.
- One-shot: a leading non-flag argument is looked up in `COMMANDS[]` via
  `commands_resolve()` and dispatched as a slash command
  (`slermes /model list` works); `chat` enters interactive mode;
  `version` prints the banner. Otherwise the remaining args are joined
  into one message and sent through `agent_chat()`.
- Unknown flags exit 1; unknown `/commands` exit 1 (never sent to the LLM).
- Pipe mode: non-TTY stdin with no args reads lines via `fgets` and
  processes each as slash command or message (H09).

---

## 2. CLI Interactive Loop

### The core loop (src/cli/cli.c)

```c
while (g_cli.running) {
    hermes_config_check_reload(...);      /* SIGHUP config reload (P19) */
    /* SIGWINCH: redraw prompt on resize */
    /* prompt symbol from skin branding, default "hermes>" */
    if (g_type_ahead_len > 0)
        line_edit_set_text(g_le, g_type_ahead_buf);  /* inject type-ahead */
    char *line = line_edit_read(g_le, prompt);
    if (!line) break;                     /* EOF / Ctrl-C / Ctrl-D */
    /* backslash-continuation: strip trailing '\', read more with ">" prompt */
    if (input[0] == '/') {
        if (commands_dispatch(input, &g_cli.agent)) continue;
        if (commands_try_skill(input, &g_cli.agent)) continue;
        if (commands_try_quick(input, &g_cli.agent)) continue;
        /* "Unknown command: ... Try /help" */
    } else {
        display_kawaii_start(&g_cli.spinner, "thinking", true);
        /* start type-ahead reader thread */
        char *resp = agent_chat(&g_cli.agent, input);
        /* stop + join type-ahead reader */
        /* streaming: tokens already printed by cli_stream_cb;
           otherwise hermes_markdown_render + display_panel */
        /* --json: cli_json_respond; statusbar if enabled */
    }
}
agent_close_db(...);
line_edit_save_history(g_le, "~/.slermes_history");
agent_free(...);
```

### Type-ahead buffer

During the LLM call a background pthread (`type_ahead_reader`) captures
stdin keystrokes into `g_type_ahead_buf` (`TYPE_AHEAD_BUF_SIZE` = 8192).
When `agent_chat()` returns, the thread is stopped and joined; before the
next `line_edit_read()` the buffered text is injected with
`line_edit_set_text()` so the user's next message is already typed out:

```
LLM thinking...  ──→  type_ahead_reader thread  ──→  line_edit_set_text
                      (captures keystrokes into       (pre-populates editor
                       g_type_ahead_buf)               on next read)
```

### Continuation lines

If a line ends with `\`, the backslash is stripped and a continuation line
is read with a `>` prompt, appended with a separating space. The editor
also supports Alt+Enter (literal newline) and bracketed paste
(`ESC[200~ … ESC[201~`).

---

## 3. Line Editor (QoL Deep Dive)

**Library**: `lib/liblineedit/line_edit.c` — self-contained, termios raw
mode, zero external dependencies (no readline, no ncurses). Public API in
`line_edit.h`: `line_edit_create/read/free/set_text`,
`line_edit_load_history/save_history`, `line_edit_get_mode`, kill/yank/
transpose/word-movement helpers.

### Keyboard navigation

| Key | Action |
|-----|--------|
| ← → | Cursor left/right by character |
| ↑ ↓ | History navigation (previous/next) |
| Alt+B / Alt+F | Word backward / word forward |
| Home / Ctrl+A | Start of line |
| End / Ctrl+E | End of line |

(Word movement is Alt+B/Alt+F or vi `w`/`b`; the editor does not parse
Ctrl+arrow escape sequences.)

### Editing keys

| Key | Action |
|-----|--------|
| Backspace | Delete character before cursor |
| Delete | Delete character at cursor |
| Ctrl+K | Kill to end of line (into kill ring) |
| Ctrl+Y | Yank last killed text |
| Ctrl+W | Kill word backward |
| Alt+D | Kill word forward |
| Ctrl+T | Transpose characters |
| Ctrl+U | Clear entire line |
| Ctrl+L | Clear screen |
| Ctrl+C | Cancel current input (returns NULL) |
| Ctrl+D | EOF on empty line (returns NULL, exits loop) |
| Alt+Enter | Insert literal newline |
| Tab | Tab completion |

### Tab completion

The completion callback is `cli_complete()` in `src/cli/cli.c`: it
prefix-matches against the full `COMMANDS[]` table — names and aliases
(`/n` → `/new`, `/c` → `/clear`, etc.). Single match completes inline;
multiple matches are listed.

### History

- **In-memory**: `MAX_HISTORY` = 100 entries (`line_edit.h`).
- **Persistent**: saved to `~/.slermes_history` on exit, loaded on startup
  (paths built in `cli.c`).
- **Search**: Ctrl+R opens reverse interactive search
  (`(reverse-i-search)` prompt, substring match from most recent).
  Ctrl+R again cycles to older matches; Enter accepts; ESC or Ctrl+G
  cancels and restores the original line.

### Vi mode

Standalone Escape switches INSERT → NORMAL; the prompt shows a
`[NORMAL]` / `[VISUAL]` mode indicator.

- **NORMAL**: `h`/`l` move, `j`/`k` history, `x`/`X` delete char,
  `dd`/`D` delete line, `yy`/`Y` yank line, `p` paste, `u` undo,
  `r` replace char, `~` toggle case, `s`/`C` change.
- **VISUAL**: `v`/`V` enter character-wise selection; `x`/`d` delete,
  `y` yank the selection; ESC exits visual mode.
- **Count prefix**: e.g. `3dd`, `2x` repeat the operation.
- **Search**: `/` forward, `?` backward, `n`/`N` repeat same/opposite
  direction (`line_edit_search_internal`).
- **Repeat**: `.` repeats the last change (`vi_last_change_op`).

---

## 4. Slash Command System

### Command registration

Commands are declared in the `COMMANDS[]` table in
**`src/hermes_cli/partial_compress.c`** (historical filename — the file
header reads "commands.c — Slash command definitions"; it is the first
entry of `CUPBOARD_HERMES_CLI`, so it is the definition linked into the
binary). `src/hermes_cli/commands.c` is an **unlinked legacy twin** (96 entries,
in no object list) — it is NOT the dispatch table. Entries use
`command_def_t` (fields: `.name`, `.alias`, `.description`,
`.category`, `.args_hint`, `.subcommands`, `.handler`, `.cli_only`,
`.gateway_only`):

```c
const command_def_t COMMANDS[] = {
    {.name="/new",   .alias="/n", .description="Start a new conversation session",
     .category="Session", .handler=cmd_new},
    {.name="/model", .alias="/m", .description="Model mgmt: list, show, providers, set",
     .category="Config", .subcommands="list,show,providers,set", .handler=cmd_model},
    /* ... */
    {.name=NULL, /* sentinel */},
};
```

The table currently holds **110 commands and 26 aliases across 17
categories** (count it yourself:
`grep -c '{\.name="/' src/hermes_cli/partial_compress.c`; the runtime
source of truth is `commands_count()` in the same file). The full
per-command reference is [cli/index.md](cli/index.md).

### Dispatch

```
user types "/model list"
  → commands_dispatch(input, state)
      → commands_resolve(input)
          1. case-insensitive exact match on .name or .alias
          2. else prefix match (command prefix of input, or input prefix
             of command) — accepted only when exactly ONE candidate
             matches (ambiguity → NULL)
      → handler(args, state)   e.g. cmd_model("list", state)

fallbacks when dispatch returns false:
  → commands_try_skill()   skill-defined commands
  → commands_try_quick()   user-defined quick commands from config
  → "Unknown command: ... Try /help"
```

The same surface works one-shot from the shell: `slermes /model list`
(cli_main resolves through the same `COMMANDS[]` table).

### Command categories (counted from the linked registry)

| Category | Count | Examples |
|----------|-------|---------|
| Session | 38 | /new, /undo, /save, /retry, /compress, /branch, /snapshot, /queue, /steer |
| System | 13 | /exit, /stop, /update, /doctor, /dump, /logs, /dashboard, /deps, /python |
| Config | 12 | /model, /config, /setup, /voice, /fast, /topic, /yolo, /reasoning |
| Tools | 7 | /tools, /tools-verify, /commands, /toolsets, /image, /paste, /browser |
| Gateway | 7 | /platforms, /gateway, /webhook, /platform, /restart, /sethome, /handoff |
| Display | 7 | /redraw, /verbose, /skin, /indicator, /statusbar, /busy, /pet |
| Security | 5 | /approve, /deny, /secrets, /auth, /key |
| Skills | 5 | /skills-hub, /skills, /bundles, /curator, /reload-skills |
| Tools & Skills | 4 | parity commands spanning both surfaces |
| Configuration | 3 | parity config commands |
| MCP | 2 | /mcp, /reload-mcp |
| Info | 2 | /usage, /insights |
| Plugins | 1 | /plugins |
| Memory | 1 | /memory |
| Cron | 1 | /cron |
| Kanban | 1 | /kanban |
| Help | 1 | /help |

---

## 5. Display System

### Layered architecture

```
response rendering in cli.c
  (hermes_markdown_render + display_panel / display_panel_hex)
  └─ display core                 [src/agent/display_core.c]
       display_printf / display_set_fg / display_set_style /
       display_panel / display_statusbar / display_tool_activity
       └─ ANSI helpers            [lib/libansi/ansi.c]
```

Public API: `include/hermes_display.h`. There is no `src/cli/display.c`;
the display core is `display_core.c`, and the `display_print_*` helpers
are implemented in `src/hermes_cli/cli_output.c`.

### Color system

Named colors (`display_color_t`): `DISPLAY_BLACK`, `DISPLAY_RED`,
`DISPLAY_GREEN`, `DISPLAY_YELLOW`, `DISPLAY_BLUE`, `DISPLAY_MAGENTA`,
`DISPLAY_CYAN`, `DISPLAY_WHITE`, `DISPLAY_DEFAULT`.

Styles (`display_style_t`): `DISPLAY_NORMAL`, `DISPLAY_BOLD`,
`DISPLAY_DIM`, `DISPLAY_ITALIC`, `DISPLAY_UNDERLINE`.

Beyond 16-color ANSI: `display_set_fg_rgb`, `display_set_fg_256`,
`display_printf_hex`, and skin-driven hex colors (`cli_skin_color`,
`display_panel_hex`) via lib/libskin.

### Spinner

`display_kawaii_t` is the thinking indicator. The CLI calls
`display_kawaii_start()` before `agent_chat()` and `display_kawaii_stop()`
after; in streaming mode `cli_stream_cb` stops it on the first token.
`display_kawaii_tick()` advances frames (thinking faces/verbs from
`display_get_thinking_faces/verbs`, optional skin "wings"). Start/stop
wrap the call on the main thread — there is no separate spinner thread.

### Display helpers

| Function | Purpose |
|----------|---------|
| `display_print_info` | Dim info line |
| `display_print_success` | Green ✓ success |
| `display_print_warning` | Yellow ⚠ warning |
| `display_print_error` | Red ✗ error |
| `display_print_error_rich` | Error + suggestion text |
| `display_panel` / `display_panel_hex` | Bordered response/status panels |
| `display_statusbar` | Model / session / token / cost status bar |
| `display_tool_activity` | Tool activity feed line |
| `display_progress_*` | Progress bar init/update/done |

---

## 6. Interactive Widgets

### Curses widget library (lib/libcurses_widget/)

Port of the Python curses UI: keyboard-navigable interactive menus for
tool/skill/settings configuration (`curses_widget.c` / `.h`).

| Widget | Purpose |
|--------|---------|
| `cw_checklist` | Multi-select with checkboxes |
| `cw_radiolist` | Single-select radio list |
| `cw_picker` | Single-select with cancel row |
| `cw_confirm` | Yes/No dialog |
| `cw_fallback_checklist/radiolist/picker` | Numbered text fallbacks when curses isn't available |

Keys (from the widget event loop): ↑↓ (or `j`/`k`) navigate, Space
toggles, Enter confirms, `q` cancels, `/` opens the fuzzy/substring
search filter, ESC exits search mode. Matching support lives in
lib/libfuzzymatch.

### Line editor (lib/liblineedit/)

Described in Section 3: readline-like editing with emacs and vi
keybindings, history search, tab completion, type-ahead injection.

---

## 7. Agent Loop

### Data flow

```
User input (cli.c)
  → agent_chat                    [src/agent/conversation_loop.c]
    → run_conversation
      → system prompt assembly    system_prompt_build_stable /
                                  system_prompt_build_volatile
                                  [src/agent/system_prompt.c]
                                  (context files, skills, environment,
                                   CWD via prompt_builder.c)
      → provider chat             [src/agent/provider.c factory +
                                   provider_*.c implementations]
      → stream response           [src/agent/llm_stream.c;
                                   diagnostics: stream_diag.c;
                                   tokens → stream_cb → CLI]
      → tool calls?  ──YES──
      │     execute_tool_calls_concurrent / _sequential
      │       → tool_executor.c
      │       → registry_find     [src/tools/registry.c]
      │       → tool handler      [src/tools/*.c]
      │       → tool_result.c     → back into messages, loop continues
      └─ NO: return final response
```

`agent_run_conversation()` is also exported (`include/hermes_agent.h`)
for the gateway and ACP surfaces.

### Provider routing

`src/agent/provider.c` is the provider registry/factory:
`register_provider_builtins()`, `find_provider_ops()`,
`provider_create()`, credential-pool hookup. Implementations:

```
provider_openai.c            OpenAI-compatible chat completions
provider_anthropic.c         Anthropic Messages API
provider_google.c            Google AI / Gemini
provider_codex_responses.c   Codex responses API
provider_custom.c            custom OpenAI-compatible endpoints
provider_bedrock.c           AWS Bedrock (+ provider_bedrock_sign.c)
provider_azure.c             Azure OpenAI
provider_xai.c               xAI
provider_openrouter.c        OpenRouter
provider_deepseek.c          DeepSeek
```

Model metadata resolution: `provider_metadata.c`, `model_metadata.c`,
`models_dev.c`, `provider_model_caps.c`, `provider_endpoint_detect.c`.

### Tool dispatch

```
tool_call from LLM
  → src/agent/tool_executor.c
    → src/tools/registry.c      (match by function name)
      → handler (src/tools/terminal.c, src/tools/file.c, ...)
      → result via src/tools/tool_result.c
```

Deferred tool discovery: `src/tools/tool_search.c` implements BM25
catalog scoring (`bm25_score`, port of `tools/tool_search_catalog.py`).

---

## 8. TUI Subsystem (ncurses)

### Architecture

`slermes --tui` calls `tui_fullscreen_run()` **directly from main.c**
(`src/cli/tui_fullscreen.c` — init, event loop, cleanup;
`tui_fullscreen_init()` sets up ncurses). `tui_entry_run()`
(`src/hermes_cli/tui_entry.c`) is a preflight wrapper (terminal check,
start-result/exit-reason messages) that also delegates to
`tui_fullscreen_run()`.

Wired into the fullscreen TUI:

```
tui_fullscreen.c
  ├─ tui_eventpub_init(RPC_FIFO_PATH)   [src/hermes_cli/tui_eventpub.c]
  │    FIFO event publishing; tui_eventpub_to_json serializes events
  ├─ tui_slash_init / tui_slash_register_batch / tui_slash_dispatch
  │    [src/hermes_cli/tui_slash_worker.c]
  │    handlers: src/cli/tui_slash_cmds.c — a TUI-specific registry
  │    (cmd_help, cmd_quit, cmd_clear, cmd_sessions, cmd_config,
  │     cmd_theme, ...), separate from the CLI COMMANDS[] table
  └─ chat rendering via src/chat_render.c + local render cache
```

Additional TUI units in `src/hermes_cli/`: `tui_layout.c` (pane layout
management), `tui_render.c` (cell buffer, `tui_render_flush` to an
ncurses window), `tui_json_rpc.c`, `tui_websocket.c`, `tui_transport.h`
(JSON-RPC transport layer). More fullscreen panels live in `src/cli/`:
`tui_input.c`, `tui_stream.c`, `tui_views.c`, `tui_win.c`,
`tui_session_browser.c`, `tui_model_picker.c`, `tui_config_editor.c`,
`tui_plugin_hub.c`, `tui_todo_panel.c`, `tui_image_viewer.c`.

Build: requires `HAS_NCURSES_TUI` (`make tui`); ncurses is bundled under
`lib/libncurses`. Without it, `--tui` prints an error and exits 1.

---

## 9. Gateway Integration

### CLI-to-gateway bridge

```
CLI (src/cli/cli.c)               Gateway (src/gateway/)
  ├─ agent_chat loop                ├─ server.c   hermes_gateway_main
  ├─ slash commands                 │             (start/stop/restart/status)
  ├─ display output                 ├─ run.c      message pipeline
  └─ config/paths                   ├─ gw_session.c + session.c  sessions
                                    ├─ platforms/*.c  telegram, discord,
                                    │   slack, signal, whatsapp, matrix,
                                    │   email, feishu, dingtalk, ...
                                    └─ stream_events.c  event dispatch
```

### Startup modes (verified against src/main.c)

- **CLI**: `./slermes` — interactive line-editor interface
- **One-shot**: `./slermes "prompt"` — execute and exit
- **TUI**: `./slermes --tui` — ncurses fullscreen interface
- **Gateway**: `./slermes gateway start` — multi-platform daemon
- **ACP**: `./slermes acp` — JSON-RPC over stdio
- **MCP serve**: `./slermes mcp-serve [port]` — MCP **HTTP** server,
  default port 9100, endpoint `POST /mcp` (not stdio)
- **API server**: `./slermes api-server [port]` — OpenAI-compatible REST,
  default port 9101 (`/v1/chat/completions`, `/v1/models`, `/health`, ...)

---

## 10. QoL Features Summary

### Input experience
- [x] Arrow-key navigation (↑↓ history, ←→ cursor)
- [x] Tab completion for all slash commands (96 at rewrite time) + aliases
- [x] Ctrl+R reverse history search
- [x] Multi-line input: backslash continuation, Alt+Enter, bracketed paste
- [x] Type-ahead buffering during LLM thinking
- [x] Vi mode (normal, insert, visual, counts, search, dot-repeat)
- [x] Emacs keybindings (kill/yank, transpose, word movement)
- [x] History persistence across sessions (`~/.slermes_history`)

### Display
- [x] ANSI 16-color + RGB/256/hex color support
- [x] Bold, dim, italic, underline styles
- [x] Kawaii spinner around LLM calls (stops on first streamed token)
- [x] Status bar (model, session, tokens, cost)
- [x] Rich error formatting (error + suggestion)
- [x] Success/warning/info color convention
- [x] Skins/themes (lib/libskin, `/skin`)

### Navigation
- [x] 110 slash commands across 17 categories (see §4)
- [x] Command aliases (/n → /new, /c → /clear)
- [x] Unambiguous prefix resolution + tab completion
- [x] Help system (/help, /commands)

### Interactive widgets
- [x] Multi-select checklists (curses + text fallback)
- [x] Single-select radio lists / pickers (curses + text fallback)
- [x] Confirmation dialogs
- [x] Fuzzy/substring search in menus
- [x] Numbered text fallback when no curses

### Session management
- [x] New/clear/undo/retry/compress/branch/reset
- [x] Session save/load/resume (`--session`, /load, /resume)
- [x] Session search with FTS5 sanitisation (`src/cli/session_search_tool.c`)
- [x] Session export/import (JSON/markdown — /session-export, /session-import)
- [x] Snapshot/rollback (/snapshot, /rollback)
- [x] Token usage tracking (/usage, /stats, status bar)

### Configuration
- [x] Model management (/model list|show|providers|set)
- [x] Provider auth status (/auth)
- [x] Secret management (/secrets list|get|sync|status)
- [x] Profile management (--profile, /profile)
- [x] Skin/theme switching (/skin)
- [x] Reasoning level control (/reasoning)
- [x] Voice mode (/voice)
- [x] Fast mode toggle (/fast)
- [x] YOLO mode toggle (/yolo)

### Background operations
- [x] Background prompts (/background)
- [x] Queue system (/queue)
- [x] Process management (/stop)
- [x] Gateway management (/gateway status|list|stop|setup|restart)
- [x] Cron job management (/cron list|status)

### Advanced
- [x] MCP server management (/mcp, /reload-mcp)
- [x] Kanban board interaction (/kanban)
- [x] Plugin management (/plugins)
- [x] Skills hub (/skills-hub search|show|list|sync)
- [x] Browser tool connection via CDP (/browser)
- [x] Webhook subscriptions (/webhook)
- [x] Dashboard launch (/dashboard)
- [x] Doctor diagnostics (/doctor, `slermes doctor`)
- [x] Shell completions: `/completions bash|zsh|fish`
      (`src/hermes_cli/completion.c`); top-level `slermes completions`
      emits bash/zsh/install scripts
- [x] File attachment (/image, /paste)

---

## 11. File Layout

```
slermes/
├── slermes                    ← compiled binary
├── makefile                   ← top-level build (includes build/*.mk)
├── build/                     ← config.mk, objects.mk, rules.mk,
│                                targets.mk, test.mk, dev.mk, ...
├── src/
│   ├── main.c                 ← REAL entry point (strcmp dispatch)
│   ├── cli/
│   │   ├── cli.c              ← interactive CLI loop + cli_main
│   │   ├── commands.c         ← legacy twin (NOT linked; live table is
│   │   │                          hermes_cli/partial_compress.c COMMANDS[])
│   │   ├── config.c           ← config.yaml loading (libyaml)
│   │   ├── config_setup.c     ← init / interactive setup
│   │   ├── config_setup_wizard.c, setup_wizard_ui.c,
│   │   │   setup_wizard_select.c   ← setup wizard
│   │   ├── paths.c            ← SLERMES_HOME / profile-aware paths
│   │   ├── display_core.c     ← ANSI display core
│   │   ├── cli_cmd_*.c        ← command handler groups
│   │   ├── session_search_tool.c  ← FTS5 session search
│   │   ├── tui_fullscreen.c + tui_*.c  ← ncurses TUI panels
│   │   └── cli_gaps.c         ← Python module mapping surface
│   ├── hermes_cli/            ← ported CLI command surface
│   │   ├── write_approval_commands.c  ← cmd_doctor, cmd_deps, cmd_update
│   │   ├── cli_output.c       ← display_print_* helpers
│   │   ├── completion.c       ← bash/zsh/fish completion generation
│   │   ├── tui_entry.c, tui_eventpub.c, tui_layout.c, tui_render.c,
│   │   │   tui_json_rpc.c, tui_slash_worker.c, tui_websocket.c
│   │   └── goals_manager.c, loops_manager.c, ...
│   ├── agent/
│   │   ├── conversation_loop.c  ← run_conversation / agent_chat
│   │   ├── agent_loop.c         ← init_agent, session DB ops
│   │   ├── system_prompt.c, prompt_builder.c
│   │   ├── provider.c + provider_*.c
│   │   ├── tool_executor.c
│   │   └── agent_gaps.c       ← agent Python module mapping
│   ├── tools/
│   │   ├── registry.c         ← tool registry
│   │   ├── tool_search.c      ← BM25 deferred tool search
│   │   ├── tool_result.c, terminal.c, ...
│   ├── gateway/
│   │   ├── server.c, run.c, gw_session.c, session.c,
│   │   │   session_context.c, stream_events.c, ...
│   │   └── platforms/*.c      ← platform adapters
│   ├── cron/
│   │   ├── scheduler.c        ← hermes_cron_main
│   │   └── cron_cli.c, jobs.c, ...
│   ├── acp/                   ← ACP stdio server
│   ├── provider/              ← OAuth/token modules (google_oauth.c, ...)
│   ├── mcp_serve.c, api_server.c, session_db.c, ...
├── lib/                       ← 74 sub-libraries, including:
│   ├── liblineedit/           ← readline-like line editor
│   ├── libcurses_widget/      ← interactive curses widgets
│   ├── libansi/               ← ANSI escape helpers
│   ├── libfuzzymatch/         ← fuzzy text matching
│   ├── libncurses/            ← bundled ncurses (tui build)
│   ├── libyaml/, libhttp/, libjson/, libdb/, libdotenv/, libskin/, ...
└── include/
    └── hermes*.h, cli.h, ...  ← public API headers
```

---

## 12. Python Bridge Dependencies

### Auto-install

The C binary works standalone — no Python required for core operation.
For features that delegate to Python SDKs (cloud sandboxes, TTS/STT
providers, gateway plugins), install the bridge dependencies:

```bash
# Three install paths, same result:
make python-deps           # build/dev.mk target
slermes /deps              # cmd_deps → pip install -r requirements-bridge.txt
./setup-slermes.sh         # interactive setup
```

All three read `requirements-bridge.txt` (packages organized by category:
inference SDKs, cloud sandboxes, provider-specific SDKs, CLI tools,
gateway plugins).

### What needs Python bridge mode

| Feature | Python package | C equivalent |
|---------|--------------|-------------|
| OpenAI inference | `openai` | `provider_openai.c` (libhttp) |
| Anthropic inference | `anthropic` | `provider_anthropic.c` (libhttp) |
| Google OAuth | `google-auth-oauthlib` | `src/provider/google_oauth.c` (direct OAuth2) |
| Azure AD auth | `azure-identity` | `provider_azure.c` (API-key auth) |
| Docker sandbox | `docker` | `src/tools/terminal.c` (docker/docker-compose backends) |
| SSH sandbox | `paramiko` | `src/tools/terminal.c` (ssh/scp backends) |
| Modal sandbox | `modal` | `src/tools/terminal.c` (modal backend option) |
| Mistral inference | `mistralai` | OpenAI-compatible provider path (no dedicated provider_mistral.c unit) |
| AWS Bedrock | `boto3` | `provider_bedrock.c` + `provider_bedrock_sign.c` (libhttp) |
| Gateway plugins | `wechatpy`, `dingtalk-sdk`, etc. | `src/gateway/platforms/*.c` |

### See also

- `docs/third-party.md` — third-party/plugin port status catalog
- `requirements-bridge.txt` — pip requirements file
- `cmd_deps` in `src/hermes_cli/write_approval_commands.c`
- `build/dev.mk` — `python-deps` target

---

## 13. Build Targets

The build is modular: `makefile` includes `build/*.mk`
(config, libs-config, objects, rules, targets, test, install, dist, dev,
plugins, clean). Default goal is `all` → `phase5` → `slermes`.

```bash
make -j$(nproc)            # default build (phase5 → slermes binary)
make tui                   # with ncurses TUI (HAS_NCURSES_TUI)
make static                # static binary (-static -Os -s)
make asan                  # AddressSanitizer build (build/test.mk)
make test                  # test suite (build/test.mk)
make python-deps           # install Python bridge deps (build/dev.mk)
make help                  # list targets
```

Parity/oracle commands (not build targets): see `make parity`,
`bash tests/run_all_oracles.sh`, `python scripts/audit_oracle_registry.py`,
and the watercup gates under `tests/watercup/`.
