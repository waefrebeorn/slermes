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

# Directory Layout — Subsystem Map

The complete file list is the tree itself (`find src -name '*.c'` — ~2,089
files at the latest run); a hand-maintained per-file table goes stale faster
than it can be written. This map gives the verified anchors per subsystem.
The old per-file table (82-command registry, `src/cli/main.c` entry, …) was
wrong and is retracted; the facts below were checked against the tree.

## Entry & top-level units (`src/*.c`)

| File | Purpose |
|------|---------|
| `main.c` | **The entry point.** argv dispatch: CLI, gateway, cron, init, setup, doctor, update, completions, acp, mcp-serve, api-server; flags --version/--help/--json/--session/--profile/--eval/--tui |
| `api_server.c`, `api_server_http.c`, `api_server_sse.c`, `api_server_readonly.c` | OpenAI-compatible REST API server |
| `app_state.c`, `app_desktop.c` | application state / desktop shell |
| `desktop_*.c` | SDL2 desktop GUI (gui, input, models, profiles, pty, sessions, settings) |
| `chat_composer.c`, `chat_render.c`, `chat_view.c` | chat rendering |
| `web_dashboard.c`, `web_app.c` | web dashboard |
| `infrastructure_stubs.c` | dependency stubs for unported callees — **never** stub a function that has a real port (wrong-shape facades here are binary-wide FAPs) |

## CLI (`src/cli/`, `src/hermes_cli/`)

| Anchor | Purpose |
|------|---------|
| `src/hermes_cli/partial_compress.c` | **The live slash-command registry** `COMMANDS[]` — 110 commands (historical filename; header reads "commands.c"). Count: `grep -c '{\.name="/'` |
| `src/cli/cli.c` | CLI interactive loop |
| `src/cli/paths.c` | SLERMES_HOME / profile path resolution |
| `src/cli/cli_cmd_*.c` | command handler families (session, config, gateway, security, memory, …) |
| `src/cli/tui_*.c` | ncurses TUI units (Linux target; dead end on Windows — Lua is the direction) |
| `src/hermes_cli/command_registry.c` | secondary registry metadata (not the dispatch table) |

## Agent (`src/agent/`)

| Anchor | Purpose |
|------|---------|
| `agent_loop.c`, `conversation_loop.c` | conversation loop + tool dispatch/retry |
| `turn_finalizer.c` | post-loop finalization |
| `prompt_builder.c`, `system_prompt.c` | prompt assembly |
| `context_compressor*.c`, `context.c` | context management |
| `provider_*.c` (18 units) | LLM provider adapters |
| `llm_client.c`, `auxiliary_client*.c` | LLM transport |
| `credential_pool*.c`, `port_agent_secret_scope.c` | credentials + profile secret scope |
| `hermes_state/` | session/state persistence |

## Tools (`src/tools/`)

| Anchor | Purpose |
|------|---------|
| `registry.c` | `registry_dispatch` — validation (`validate_args_json`), idempotency cache, retry classification |
| `tool_idempotency.c`, `tool_result_classification.c` | safe-retry primitives (watercup-gated) |
| 357 `.c` files total | terminal, file, web, browser, vision, voice, code exec, MCP, … |

## Gateway (`src/gateway/`)

| Anchor | Purpose |
|------|---------|
| `run.c`, `gateway_runner.c`, `gateway_runtime.c` | gateway runtime |
| `platforms/` (60 `.c`) | telegram, discord, slack, signal, whatsapp, matrix, feishu, wecom, weixin, yuanbao, qqbot, dingtalk, email, sms, webhook, homeassistant, mattermost, bluebubbles, msgraph_webhook, api_server, … |
| `relay/` | relay-connector runtime |
| `session.c`, `session_context.c`, `gw_session.c` | session management |
| `profile_routing*`, `authz_mixin.c` | multi-profile routing + authz |

## Other subsystems

| Dir | Purpose |
|------|---------|
| `src/cron/` | scheduler (`scheduler.c`, `cron_jobs_time.c`, …) |
| `src/pet/` | Petdex mascots (8 `.c`: manifest, store, render, state, commands, prompts, constants, atlas) |
| `src/plugins/` | native C plugins `plugin_*.c` → `.so` |
| `src/acp/`, `src/acp_adapter/` | Agent Communication Protocol |
| `src/provider/` | OAuth/token exchange |
| `src/skills/` | skills markdown parser |
| `src/pm/` | dependency-environment paths (`port_pm_environments.c`) |
| `src/lv/` | slerm-Lua interpreter (`slv_*`, upstream Wua) |
| `src/lua/` | Lua terminal host (`slv_ui.c`) |
| `src/ports/` | legacy port twins — per-platform linkage audit (`scripts/link_dead_ports.py`) |
| `src/compat/` + `include/compat/` | POSIX shims for the Windows build |

## Support trees

| Dir | Purpose |
|------|---------|
| `include/` | 708 headers: `port_<stem>.h` unit headers, `hermes_*.h`, `compat/` shims |
| `lib/` | 79 libraries — ours (libjson, libhttp, libyaml, libdb, libhash, libregex, liblineedit, libtui, libomap, …) + vendored SLERM targets (whisper_cpp, sqlite amalgamation, ncurses) |
| `build/` | config.mk (profiles), objects.mk (cupboards), rules.mk, targets.mk, test.mk, winlibs-env.sh, msys2-env.sh |
| `tests/` | `t_port_*.c` harnesses + `sta_oracle_*.py`/`oracle_*.py` references, `oracle/` registry+fixtures, `watercup/` (22,603 gates), gates, mission8 suites |
| `scripts/` | parity pipeline (`run_parity_workflow.py`, `parity_truth.py`, `gen_parity_walkway.py`), audits, generators |
| `.hermes/` | project navigation: battleship, state, mantra, mind-palace, museum |
