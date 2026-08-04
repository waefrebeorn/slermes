# Directory Layout — Complete File Map

## Source Files by Subsystem

### CLI Subsystem (`src/cli/`)

| File | Purpose |
|------|---------|
| `port_cli_command_registry.c` | 82 slash command handlers + dispatch table |
| `config.c` | YAML config load/save/diff/merge |
| `display.c` | Terminal output formatting |
| `display_core.c` | Core display routines |
| `main.c` | CLI entry point |
| `cli.c` | CLI initialization |
| `doctor.c` | System diagnostics |
| `setup_wizard.c` | First-run setup |
| `tui_json_rpc.c` | TUI JSON-RPC methods (92 registered) |
| `tui_fullscreen.c` | Fullscreen TUI mode |
| `paths.c` | Path resolution |
| `cli_gaps.c` | CLI compatibility stubs |

### Agent Subsystem (`src/agent/`)

| File | Purpose |
|------|---------|
| `agent_loop.c` | Main conversation loop |
| `agent_init.c` | Agent initialization |
| `agent_runtime_helpers.c` | Runtime helper functions |
| `provider.c` | LLM provider abstraction |
| `provider_openai.c` | OpenAI provider |
| `provider_anthropic.c` | Anthropic provider |
| `provider_google.c` | Google/Gemini provider |
| `provider_deepseek.c` | DeepSeek provider |
| `provider_xai.c` | xAI provider |
| `provider_openrouter.c` | OpenRouter provider |
| `provider_azure.c` | Azure OpenAI |
| `provider_bedrock.c` | AWS Bedrock |
| `provider_custom.c` | Custom endpoint provider |
| `llm_client.c` | Low-level LLM HTTP client |
| `context.c` | Context window management |
| `context_engine.c` | Context compression engine |
| `system_prompt.c` | System prompt builder |
| `tool_executor.c` | Tool call dispatch |
| `credential_pool.c` | API key credential pool |
| `conversation_loop.c` | Turn-by-turn conversation |
| `memory_manager.c` | Memory system |
| `memory_provider.c` | Memory provider abstraction |
| `skill_commands.c` | Skill integration |
| `onboarding.c` | First-run onboarding |

### Tool Subsystem (`src/tools/`)

| File | Purpose |
|------|---------|
| `registry.c` | Tool registration |
| `terminal.c` | Terminal/command execution |
| `file.c` | File read/write/search/patch |
| `web.c` | Web search |
| `browser.c` | Browser automation |
| `exec_code.c` | Code execution sandbox |
| `memory.c` | Memory tool |
| `todo.c` | Task list management |
| `process.c` | Process management |
| `cronjob.c` | Scheduled job management |
| `session_search.c` | Session search |
| `session_crud.c` | Session CRUD |
| `vision.c` | Image analysis |
| `delegate.c` | Subagent delegation |
| `voice_mode.c` | Voice I/O |
| `image_gen.c` | Image generation |
| `mcp_tool.c` | MCP tool integration |
| `kanban.c` | Kanban board |
| `transcribe.c` | Speech-to-text |
| `tts.c` | Text-to-speech |
| `video_gen.c` | Video generation |

### Gateway Subsystem (`src/gateway/`)

| File | Purpose |
|------|---------|
| `server.c` | Gateway server |
| `gateway_runtime.c` | Runtime management |
| `config.c` | Gateway configuration |
| `session.c` | Session tracking |
| `session_context.c` | Session context |
| `delivery.c` | Message delivery |
| `run.c` | Gateway main loop |
| `platforms/telegram.c` | Telegram adapter |
| `platforms/discord.c` | Discord adapter |
| `platforms/slack.c` | Slack adapter |
| `platforms/signal.c` | Signal adapter |
| `platforms/whatsapp.c` | WhatsApp adapter |
| `platforms/matrix.c` | Matrix adapter |
| `platforms/email.c` | Email adapter |
| `platforms/webhook.c` | Webhook adapter |
| `platforms/sms.c` | SMS (Twilio) adapter |
| `platforms/feishu.c` | Feishu/Lark adapter |
| `platforms/wecom.c` | WeCom adapter |
| `platforms/dingtalk.c` | DingTalk adapter |
| `platforms/qqbot.c` | QQ Bot adapter |
| `platforms/bluebubbles.c` | BlueBubbles (iMessage) adapter |
| `platforms/weixin.c` | Weixin adapter |
| `platforms/yuanbao.c` | Yuanbao adapter |
| `platforms/homeassistant.c` | Home Assistant adapter |

### Pet Subsystem (`src/pet/`)

| File | Purpose |
|------|---------|
| `pet_constants.c` | Scale, frame geometry, state aliases |
| `pet_state.c` | State machine derivation |
| `pet_manifest.c` | Manifest fetching + TTL cache |
| `pet_store.c` | Install/list/resolve/remove pets |
| `pet_render.c` | Terminal detection, frame count, kitty encoding |
| `pet_commands.c` | Global state, JSON builders, CLI dispatch |

### ACP Subsystem (`src/acp/`)

| File | Purpose |
|------|---------|
| `server.c` | ACP server |
| `events.c` | Event handling |
| `permissions.c` | Permission management |
| `resource.c` | Resource management |
| `edit_approval.c` | Edit approval flow |

### Cron Subsystem (`src/cron/`)

| File | Purpose |
|------|---------|
| `scheduler.c` | Cron scheduler |
| `jobs.c` | Job management |
| `cron_sqlite.c` | SQLite job store |
| `cron_cli.c` | Cron CLI commands |

## Include Files (`include/`)

127 header files. Key ones:

| Header | Purpose |
|--------|---------|
| `hermes.h` | Main project header, config types |
| `hermes_agent.h` | Agent state and API |
| `hermes_http.h` | HTTP client API |
| `pet.h` | Pet system API |
| `hermes_cli.h` | CLI subsystem |
| `hermes_gateway.h` | Gateway subsystem |
| `hermes_tools.h` | Tool registry |
| `hermes_json.h` | JSON API shim |
| `json.h` | libjson API |
| `hermes_logger.h` | Logging API |

## Library Files (`lib/`)

73 internal libraries:

| Library | Purpose |
|---------|---------|
| `libjson/` | JSON parser/generator |
| `libhttp/` | HTTP/HTTPS client |
| `libyaml/` | YAML parser |
| `libtui/` | Terminal UI framework |
| `libdb/` | SQLite wrapper |
| `libproc/` | Process spawn/management |
| `libcrypto/` | Crypto utilities |
| `libwebsocket/` | WebSocket client |
| `libmcp/` | Model Context Protocol |
| `libskin/` | Skinning/themming |
| `libcron/` | Cron expression parser |
| `libpath/` | Path utilities |
| `libdatetime/` | Date/time formatting |
| `libcsv/` | CSV parsing |
| `libhash/` | Hash tables |
| `libuuid/` | UUID generation |
| `libbase64/` | Base64 encode/decode |
| `libhtml/` | HTML sanitization |
| `libregex/` | Regex engine |
| `libglob/` | Glob pattern matching |
| `libsignal/` | Signal handling |
| `libansi/` | ANSI escape processing |
| `libtranscribe/` | Speech-to-text providers |
| `libplugin/` | Plugin loader |
| `libcurses_widget/` | ncurses widget toolkit |
