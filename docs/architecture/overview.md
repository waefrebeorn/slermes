# Architecture Overview

Slermes is a single-process C11 application organized into subsystems that mirror the Python Hermes Agent.

## Directory Layout

```
slermes/
├── src/
│   ├── cli/          — CLI frontend, commands, config, display, TUI
│   ├── agent/        — Core agent loop, LLM client, provider adapters
│   ├── tools/        — Tool implementations (terminal, file, web, browser, etc.)
│   ├── gateway/      — Messaging gateway (telegram, discord, slack, signal...)
│   │   └── platforms/  — 14+ messaging platform adapters
│   ├── pet/          — Petdex mascot system (6 files)
│   ├── acp/          — Agent Communication Protocol
│   ├── cron/         — Scheduled task runner
│   ├── provider/     — OAuth/token exchange providers
│   ├── skills/       — Skills markdown parser
│   └── plugins/      — Plugin loader
├── include/          — 127 header files
├── lib/              — 73 internal libraries
│   ├── libjson/      — JSON parser/generator
│   ├── libhttp/      — HTTP client
│   ├── libtui/       — Terminal UI framework
│   ├── libdb/        — SQLite wrapper
│   ├── libyaml/      — YAML parser
│   ├── libproc/      — Process management
│   └── ...           — 66 more
├── tests/            — Python test suite
└── docs/             — Documentation
```

## Core Loop

```
User Input → CLI/TUI/Gateway → Agent Loop → LLM Call → Tool Execution → Response
```

1. **Input**: CLI, TUI, or messaging gateway receives user message
2. **Agent Loop**: Builds context, calls LLM, handles streaming
3. **Tool Execution**: Intercepts tool calls, runs C11 tool handlers
4. **Response**: Sends response back through same path

## Key Subsystems

### CLI (`src/cli/`)
- 95 slash command handlers in `commands.c`
- Config system (YAML-based, `config.c`)
- TUI display system (`display.c`, `display_core.c`)
- Doctor/diagnostics (`doctor.c`)

### Agent (`src/agent/`)
- Agent loop (`agent_loop.c`)
- LLM provider layer (`provider.c`, `provider_openai.c`, etc.)
- Context management (`context.c`, `context_engine.c`)
- System prompt building (`system_prompt.c`)

### Tools (`src/tools/`)
- 40+ tool handlers
- Terminal, file, web search, browser, image gen, voice, etc.
- Each tool has a handler function registered in `registry.c`

### Gateway (`src/gateway/`)
- 14+ messaging platforms in C
- Telegram, Discord, Slack, Signal, WhatsApp, Matrix, etc.
- Session management, delivery, streaming

### Pet System (`src/pet/`)
- 6 C11 files, 1 header
- Manifest fetching, pet store, rendering, animation state
- Kitty terminal protocol support

## Data Flow

```
config.yaml → hermes_config_t (config.c)
                    ↓
agent_state_t → agent_loop.c → llm_client.c → provider_*.c
                    ↓                    ↓
              tool_executor.c     provider_openai.c
                    ↓                    ↓
              tools/*.c           HTTP (libhttp)
```

See [Directory Layout](directory-layout.md) for the complete file map.
