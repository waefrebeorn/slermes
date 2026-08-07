# How It Works — Slermes C CLI Depth Documentation

## Overview

The Slermes C CLI is a complete terminal user interface for interacting with
LLM agents. It mirrors the full feature set of the Python `hermes-cli` with
native C performance and zero Python runtime dependency.

This document describes the full data flow, the QoL features available to the
user, and how every component assembles into a working system.

---

## 1. Entry Point & Startup

```
main                         [src/cli/main.c]
  ├─ parse args (getopt_long)
  ├─ init paths                [src/cli/paths.c]
  ├─ load config               [src/cli/config.c]
  ├─ init agent                [src/agent/agent_init.c]
  ├─ init display              [src/cli/display.c]
  ├─ ─ TUI mode? ──
  │    YES → tui_entry_run   [src/cli/tui_entry.c]
  │              └─ ncurses full-screen TUI
  │    NO  → cli_main_loop   [src/cli/cli.c]
  └─ cleanup & exit
```

### Startup Sequence

1. **Argument parsing** (`cli_main` in main.c): Uses `getopt_long` for
   `--session`, `--model`, `--provider`, `--json`, `--tui`, `--help`, `--version`.

2. **Path resolution** (`paths.c`): Resolves `SLERMES_HOME` / `~/.slermes` /
   `~/.hermes` with profile support. All runtime data lives under this
   directory.

3. **Config loading** (`config.c`): Parses `config.yaml` via the `libyaml`
   library into the `hermes_config_t` struct (~200 fields). Supports
   multi-provider, env var override, profile merging.

4. **Agent init** (`agent_init.c`): Creates `agent_state_t` with provider
   selection, credential pool, model routing, checkpointer, and context
   manager.

5. **Display init** (`display.c` / `display_core.c`): Initializes ANSI color
   support, kawaii spinner, terminal width detection.

6. **UI dispatch**: If `--tui` flag is set, enters ncurses full-screen TUI;
   otherwise enters the line-editor CLI loop.

---

## 2. CLI Interactive Loop

### The Core Loop (cli.c)

```c
while (g_cli.running) {
    // 1. Print prompt with display helpers
    // 2. Read a line from the line editor
    prompt = build_prompt(state);
    char *line = line_edit_read(g_le, prompt);

    if (!line) continue;  // EOF / Ctrl-C
    if (line[0] == '/') {
        handle_slash_command(line, state);   // dispatch to commands.c
    } else {
        agent_run_conversation(state, line);  // send to LLM
    }
    free(line);
}
```

### Type-Ahead Buffer

During LLM processing, a background thread captures stdin keystrokes into a
type-ahead buffer. When the LLM returns, the buffered input is injected into
the line editor so the user's next message is already typed out:

```
LLM thinking...  ──→  Type-ahead thread   ──→  line_edit_set_text
                       (captures keystrokes       (pre-populates editor
                        to type_ahead_buf)         on next read)
```

### Continuation Lines

Multi-line input is supported: if a line ends with `\` the editor prompts for
a continuation line with `>> ` prefix, appending it to the current input.

---

## 3. Line Editor (QoL Deep Dive)

**Library**: `lib/liblineedit/line_edit.c` — self-contained, termios-based,
zero external dependencies (no readline, no ncurses).

### Keyboard Navigation

| Key | Action |
|-----|--------|
| ← → | Cursor left/right by character |
| ↑ ↓ | History navigation (previous/next) |
| Ctrl + ← | Word backward |
| Ctrl + → | Word forward |
| Home / Ctrl+A | Start of line |
| End / Ctrl+E | End of line |

### Editing Keys

| Key | Action |
|-----|--------|
| Backspace | Delete character before cursor |
| Delete | Delete character at cursor |
| Ctrl+K | Kill to end of line (yankable) |
| Ctrl+Y | Yank last killed text |
| Ctrl+W | Kill word backward |
| Alt+D | Kill word forward |
| Ctrl+T | Transpose characters |
| Ctrl+U | Kill entire line |
| Ctrl+L | Clear screen |
| Ctrl+C | Cancel current input (returns NULL) |
| Ctrl+D | EOF on empty line (exits) |
| Tab | Tab completion |

### Tab Completion

Tab completion matches against the full command table (93 commands) using
prefix matching. Aliases (`/n` → `/new`, `/c` → `/clear`, etc.) are also
completed.

### History

- **In-memory**: Up to 100 entries tracked during session
- **Persistent**: Saved to `~/.slermes/history` on exit, loaded on startup
- **Search**: Ctrl+R opens reverse interactive search (like bash's `reverse-i-search`)
  - Type characters to narrow; Enter to accept the match
  - Exits on ESC or completion

### Vi Mode

Toggle between INSERT and NORMAL (vi) mode with Escape:
- **NORMAL mode**: `h/j/k/l` movement, `x` delete, `dd` delete line,
  `yy` yank line, `p` paste, `u` undo, `.` repeat last change
- **VISUAL mode**: `v` to enter, arrow keys to select, `d` to delete
- **Count prefix**: `3dd` deletes 3 lines, `2x` deletes 2 chars
- **Search**: `/` for forward search, `?` for backward, `n`/`N` to repeat
- **Last change repeat**: `.` repeats the last edit operation

---

## 4. Slash Command System

### Command Registration

Commands are declared in a static table in `commands.c`:

```c
static const command_def_t COMMANDS[] = {
    {.name="/new",     .alias="/n",  .description="Start a new conversation",
     .category="Session", .handler=cmd_new},
    {.name="/model",   .alias="/m",  .description="Model management",
     .category="Config", .handler=cmd_model, .subcommands="list,show,set"},
    // ... 93 total
};
```

### Dispatch

```
user types "/model list"
  → commands_resolve("/model list")
     ├─ match prefix "/model" in COMMANDS[]
     ├─ extract "list" as subcommand arg
     └─ call cmd_model("list", state)

fallback: if "/model" not found, try alias matching
  → "/m list" → resolve "/m" → matches .alias="/m" → cmd_model("list", state)
```

### Command Categories

93 commands across 14 categories:

| Category | Count | Examples |
|----------|-------|---------|
| Session | 20 | /new, /undo, /save, /retry, /compress, /branch, /snapshot |
| Config | 9 | /model, /config, /setup, /voice, /fast, /topic |
| Display | 5 | /redraw, /verbose, /skin, /indicator, /statusbar |
| Tools | 3 | /tools, /tools-verify, /toolsets, /image |
| Skills | 5 | /skills-hub, /skills, /bundles, /curator, /reload-skills |
| System | 10 | /exit, /help, /stop, /update, /doctor, /dump, /logs |
| Security | 4 | /approve, /deny, /secrets, /key |
| Gateway | 6 | /platforms, /gateway, /webhook, /platform, /restart, /sethome |
| Plugins | 1 | /plugins |
| MCP | 2 | /mcp, /reload-mcp |
| Memory | 1 | /memory |
| Cron | 1 | /cron |
| Kanban | 1 | /kanban |
| Help | 1 | /help |

---

## 5. Display System

### Layered Architecture

```
cli_display_response         [src/cli/display.c]
  └─ display_printf          [src/cli/display_core.c]
       ├─ display_set_fg
       ├─ display_set_style
       └─ ansi escape codes    [lib/libansi/ansi.c]
```

### Color System

Named colors: `DISPLAY_RED`, `DISPLAY_GREEN`, `DISPLAY_YELLOW`,
`DISPLAY_BLUE`, `DISPLAY_MAGENTA`, `DISPLAY_CYAN`, `DISPLAY_WHITE`,
`DISPLAY_DEFAULT`.

Styles: `DISPLAY_NORMAL`, `DISPLAY_BOLD`, `DISPLAY_DIM`, `DISPLAY_ITALIC`.

### Spinner

The `display_kawaii_t` spinner provides an animated thinking indicator during
LLM processing. Thread-safe, uses a separate output stream to avoid mixing
with LLM output.

### Display Helpers

| Function | Purpose |
|----------|---------|
| `cli_display_response` | Render assistant response (content + reasoning) |
| `cli_display_error` | Red bold "Error:" prefix |
| `cli_display_status` | Blue dim status line |
| `cli_display_thinking` | "thinking..." yellow dim indicator |
| `display_print_info` | Dim info line |
| `display_print_success` | Green ✓ success |
| `display_print_warning` | Yellow ⚠ warning |
| `display_print_error` | Red ✗ error |
| `display_print_error_rich` | Error + suggestion text |

---

## 6. Interactive Widgets

### Curses Widget Library (`lib/libcurses_widget/`)

Port of Python `curses_ui.py` providing keyboard-navigable interactive menus
for tool/skill/settings configuration.

**Available widgets:**

| Widget | Purpose | Input Keys |
|--------|---------|-----------|
| `cw_checklist` | Multi-select with checkboxes | ↑↓ navigate, Space toggle, Enter confirm, / search, ESC/q cancel |
| `cw_radiolist` | Single-select radio list | Same, with fuzzy search support |
| `cw_picker` | Single-select with cancel row | Same, Enter to select, ESC/q to cancel |
| `cw_confirm` | Yes/No dialog | ↑↓ to switch, Enter to confirm, ESC to cancel |
| `cw_fallback_*` | Numbered text fallbacks | Always available when curses isn't |

**Search:** Press `/` to open fuzzy search. Type a query — items are filtered
by multi-token subsequence matching with camelCase boundary detection.
ESC clears search and restores full list.

### Line Editor (`lib/liblineedit/`)

Described in Section 3 above. Full readline-like editing with emacs and vi
keybindings, history search, tab completion, type-ahead.

---

## 7. Agent Loop

### Data Flow

```
User input
  → agent_run_conversation    [src/agent/conversation_loop.c]
    → build_prompt            [src/agent/prompt_builder.c]
    → provider.chat           [src/agent/provider_openai.c etc.]
    → stream response           [src/agent/stream_diag.c]
    → tool call? ──YES──
    │              → tool_executor.c
    │              → each tool's handler
    │              → tool_result back to messages
    │              → loop continues
    └─ NO: return final response
```

### Provider Routing

```
provider config (yaml)
  → provider_metadata.c        (resolve provider name)
  → provider_select          (pick by availability, fallback)
  → provider_openai.c          (OpenAI API)
  → provider_anthropic.c       (Anthropic API)
  → provider_google.c          (Google AI API)
  → provider_codex_responses.c (GitHub Copilot)
  → provider_custom.c          (Custom OpenAI-compatible)
```

### Tool Dispatch

```
tool_call received from LLM
  → tool_executor.c
    → registry.c
      → match by function name
      → call handler (e.g. tools/terminal.c, tools/file.c, etc.)
      → result back via tool_result.c
```

---

## 8. TUI Subsystem (ncurses)

### Architecture

```
tui_entry_run               [src/cli/tui_entry.c]
  ├─ tui_fullscreen_init    [src/cli/tui_fullscreen.c]
  │   ├─ ncurses initscr
  │   ├─ theme init
  │   └─ subsystem init (eventpub, layout, render)
  ├─ main event loop
  │   ├─ tui_eventpub_fetch   [src/cli/tui_eventpub.c]
  │   ├─ tui_layout_update    [src/cli/tui_layout.c]
  │   ├─ tui_render_draw      [src/cli/tui_render.c]
  │   └─ agent_step
  └─ tui_fullscreen_shutdown
```

### Communication

The TUI subsystem runs a JSON-RPC transport layer over websocket for
async communication:
```
tui_json_rpc.c  ──→  tui_transport.c  ──→  tui_websocket.c  ──→ gateway
```

### Slash Commands in TUI

```
tui_slash_worker.c
  └─ tui_slash_worker_run
       └─ commands_resolve  (same dispatch as CLI)
            └─ cmd_xxx      (same handlers)
```

---

## 9. Gateway Integration

### CLI-to-Gateway Bridge

```
CLI (cli.c)                     Gateway (gateway/)
  ├─ agent_loop                ├─ server.c (main loop)
  ├─ slash commands              ├─ run.c (platform dispatch)
  ├─ display output              ├─ session.c (session management)
  └─ config/paths                ├─ platforms/ (telegram, discord, etc.)
                                  └─ stream_events.c (event dispatch)
```

### Startup Modes

- **CLI mode**: `./slermes` — interactive line-editor interface
- **CLI with one-shot**: `./slermes "prompt"` — execute and exit
- **TUI mode**: `./slermes --tui` — ncurses full-screen interface
- **Gateway mode**: `./slermes gateway start` — daemon mode
- **Server mode**: `./slermes mcp serve` — MCP stdio transport

---

## 10. QoL Features Summary

### Input Experience
- [x] Arrow key navigation (↑↓ history, ←→ cursor)
- [x] Tab completion for 93 slash commands
- [x] Ctrl+R reverse history search
- [x] Multi-line input with continuation prompt
- [x] Type-ahead buffering during LLM thinking
- [x] Vi mode (normal, insert, visual)
- [x] Emacs keybindings (kill/yank, transpose, word movement)
- [x] History persistence across sessions

### Display
- [x] ANSI 16-color support
- [x] Bold, dim, italic styles
- [x] Spinner animation during processing
- [x] Status bar (model, mode, session info)
- [x] Error formatting (rich error + suggestion)
- [x] Success/warning/info color convention

### Navigation
- [x] 93 slash commands across 14 categories
- [x] Command aliases (/n → /new, /c → /clear)
- [x] Command completion with tab
- [x] Help system (/help, /commands)

### Interactive Widgets
- [x] Multi-select checklists (curses + text fallback)
- [x] Single-select radio lists (curses + text fallback)
- [x] Confirmation dialogs (curses + text fallback)
- [x] Fuzzy search in picker menus
- [x] Numbered text fallback when no curses

### Session Management
- [x] New/clear/undo/retry/compress/branch
- [x] Session save/load/resume
- [x] Session search with FTS5
- [x] Session export (JSON/markdown)
- [x] Snapshot/rollback system
- [x] Token usage tracking

### Configuration
- [x] Model management (list, show, set, providers)
- [x] Provider auth status
- [x] Secret management (list, get, sync)
- [x] Profile management
- [x] Skin/theme switching
- [x] Reasoning level control
- [x] Voice mode (enable/disable/config)
- [x] Fast mode toggle
- [x] YOLO mode toggle

### Background Operations
- [x] Background prompts (`/background`)
- [x] Queue system (`/queue`)
- [x] Process management (`/stop`)
- [x] Gateway management (`/gateway start|stop|restart`)
- [x] Cron job management (`/cron list|status`)

### Advanced
- [x] MCP server management
- [x] Kanban board interaction
- [x] Plugin management
- [x] Skills hub (search, show, list, sync)
- [x] Browser tool connection (CDP)
- [x] Webhook subscriptions
- [x] Dashboard launch
- [x] Doctor diagnostics
- [x] Shell completions generation (bash/zsh/fish)
- [x] File attachment (/image, /paste)

---

## 11. File Layout

```
slermes/
├── slermes                    ← compiled binary
├── src/
│   ├── cli/
│   │   ├── cli.c              ← interactive CLI loop
│   │   ├── commands.c         ← 93 slash commands
│   │   ├── config.c           ← config loading
│   │   ├── paths.c            ← path resolution
│   │   ├── display.c          ← display helpers
│   │   ├── display_core.c     ← ANSI display core
│   │   ├── main.c             ← entry point
│   │   ├── doctor.c           ← diagnostics
│   │   ├── setup_wizard.c     ← interactive setup
│   │   ├── cli_gaps.c         ← Python module mapping
│   │   ├── tui_*.c            ← ncurses TUI subsystem
│   ├── agent/
│   │   ├── agent_gaps.c       ← agent Python module mapping
│   │   └── ...                 ← agent subsystems
│   ├── tools/
│   │   ├── ...                 ← tool implementations
│   │   └── tool_search.c      ← BM25 tool search
│   ├── gateway/
│   │   ├── ...                 ← gateway server
│   │   └── platforms/*.c      ← platform adapters
│   └── cron/
│       └── cron_cli.c         ← cron CLI commands
├── lib/
│   ├── liblineedit/           ← readline-like line editor
│   ├── libcurses_widget/      ← interactive curses widgets
│   ├── libansi/               ← ANSI escape code helpers
│   ├── libfuzzymatch/         ← fuzzy text matching
│   ├── libncurses/            ← bundled ncurses (for tui build)
│   └── ...                    ← 60+ other libraries
└── include/
    └── hermes*.h              ← public API headers
```

---

## 12. Python Bridge Dependencies

### Auto-Install

The C binary works fully standalone — no Python required. For features that
delegate to Python SDKs (cloud sandboxes, TTS/STT providers, gateway plugins),
install the bridge dependencies:

```bash
# Three install paths, same result:
make python-deps           # via Makefile target
slermes /deps              # from inside the running binary
./setup-slermes.sh         # interactive setup (asks confirmation)
```

All three commands read `requirements-bridge.txt` (25+ packages organized
by category: inference SDKs, cloud sandboxes, provider-specific SDKs, CLI
tools, gateway plugins).

### What Needs Python Bridge Mode

| Feature | Python Package | C Equivalent |
|---------|--------------|-------------|
| OpenAI inference | `openai` | `provider_openai.c` (libhttp) |
| Anthropic inference | `anthropic` | `provider_anthropic.c` (libhttp) |
| Google OAuth | `google-auth-oauthlib` | `google_oauth.c` (direct OAuth2) |
| Azure AD auth | `azure-identity` | `provider_azure.c` (API-key auth) |
| Docker sandbox | `docker` | `terminal.c` (popen to Docker CLI) |
| SSH sandbox | `paramiko` | `terminal.c` (ssh/scp via popen) |
| Modal sandbox | `modal` | `terminal.c` (local backend) |
| Mistral inference | `mistralai` | `provider_mistral.c` (libhttp) |
| AWS Bedrock | `boto3` | `provider_bedrock.c` (libhttp) |
| Gateway plugins | `wechatpy`, `dingtalk-sdk`, etc. | `src/gateway/platforms/*.c` |

### See Also

- `THIRD_PARTY.md` — full catalog of all N/A modules with installation guidance
- `requirements-bridge.txt` — pip requirements file
- `commands.c` `/deps` command handler
- `Makefile` `python-deps` target

---

## 13. File Layout

# With ncurses TUI
make -j$(nproc) tui

# Static binary
make static

# Address sanitizer
make asan

# Run test suite
make test
```
