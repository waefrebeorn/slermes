<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T17:36:51Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Architecture Overview

Slermes is a single-process C11 application organized into subsystems that
mirror the Python Hermes Agent package tree. One native binary, zero Python
at runtime. (Counts below measured live; re-derive with `find`/`ls`
rather than trusting prose.)

## Directory Layout

```
slermes/
├── src/
│   ├── main.c        — entry point: argv dispatch (CLI/gateway/cron/acp/
│   │                   mcp-serve/api-server/setup/doctor/update/…)
│   ├── cli/          — CLI loop, slash commands, config, display, TUI units
│   ├── hermes_cli/   — ports of the hermes_cli Python package
│   ├── agent/        — agent loop, LLM client, provider adapters, context
│   ├── tools/        — tool implementations + registry.c (registry_dispatch)
│   ├── gateway/      — messaging gateway runtime + relay
│   │   └── platforms/  — platform adapters (60 .c files: telegram, discord,
│   │                     slack, signal, whatsapp, matrix, feishu, weixin, …)
│   ├── hermes_state/ — session/state persistence ports
│   ├── pet/          — Petdex mascot system (8 .c files)
│   ├── acp/ acp_adapter/ — Agent Communication Protocol
│   ├── cron/         — scheduled task runner
│   ├── provider/     — OAuth/token exchange
│   ├── skills/       — skills markdown parser
│   ├── plugins/      — native C plugins (plugin_*.c → .so)
│   ├── pm/           — dependency-environment path resolution
│   ├── lv/           — slerm-Lua interpreter (slv_*, upstream Wua)
│   ├── lua/          — Lua-driven terminal host (slv_ui.c)
│   ├── ports/        — legacy port twins (per-platform linkage audit)
│   └── *.c           — top-level units: api_server*, desktop_*, web_*,
│                       chat_*, app_state, battery, …
├── include/          — 708 headers: port_<stem>.h unit headers + compat/ shims
├── lib/              — 79 libraries: ours (libjson, libhttp, libyaml, libdb,
│                       libtui, liblineedit, libhash, libregex, …) + vendored
│                       third-party SLERM targets (whisper_cpp, sqlite, ncurses)
├── build/            — config.mk (profiles/flags), objects.mk (cupboards),
│                       rules.mk, targets.mk, winlibs-env.sh / msys2-env.sh
├── tests/            — oracles (sta_oracle_*.py + t_port_*.c), watercup/,
│                       gates, mission8 suites, fixtures
├── scripts/          — parity pipeline, audits, generators
└── docs/             — documentation (mirrors the subsystem layout)
```

`src/` holds ~2,089 `.c` files. The desktop GUI is SDL2-based
(`desktop_*.c`, `desktop-gui` target); the web server/API is
`api_server*.c` / `web_*.c`; the ncurses TUI units live in `src/cli/tui_*.c`
and are a dead end on Windows — the Lua substrate (`src/lv/` + `src/lua/`)
is the UI direction.

## Core Loop

```
User Input → CLI/TUI/Gateway → Agent Loop → LLM Call → Tool Execution → Response
```

1. **Input**: CLI, TUI, or messaging gateway receives the user message.
2. **Agent Loop** (`src/agent/agent_loop.c`, `conversation_loop.c`): builds
   context, calls the LLM, handles streaming.
3. **Tool Execution**: tool calls dispatch through `registry_dispatch`
   (`src/tools/registry.c`) to C11 handlers.
4. **Response**: returns through the same path; `turn_finalizer.c` handles
   post-loop finalization.

## Key Subsystems

### CLI (`src/cli/`, `src/hermes_cli/`)
- **110 slash commands** registered in `COMMANDS[]`
  (`src/hermes_cli/partial_compress.c` — historical filename; the header
  reads "commands.c — Slash command definitions"). Count it yourself:
  `grep -c '{\.name="/' src/hermes_cli/partial_compress.c`. See
  [cli/index.md](../cli/index.md).
- Config (YAML via libyaml), display, doctor/diagnostics units.

### Agent (`src/agent/`)
- Agent loop, LLM provider layer (`provider_*.c`, 18 provider units),
  context compression, prompt building, credential pool, memory.

### Tools (`src/tools/`)
- Handlers registered via `tools_init_all()`; `registry_dispatch` is the
  real dispatcher (input validation, idempotency cache, retry classification).

### Gateway (`src/gateway/`)
- 60 platform-adapter `.c` files + relay runtime, session management,
  delivery, streaming, profile routing.

### Pet System (`src/pet/`)
- 8 C11 files: manifest, store, render, state, commands, prompts, constants,
  atlas. Kitty terminal protocol support.

## Data Flow

```
config.yaml → hermes_config_t (config load)
                    ↓
agent_state_t → agent_loop.c → llm_client.c → provider_*.c
                    ↓                    ↓
              registry_dispatch    HTTP (libhttp + winsock on Windows)
                    ↓
              tools/*.c handlers
```

See [Directory Layout](directory-layout.md) for the file map and
[Build System](build-system.md) for targets and profiles.
