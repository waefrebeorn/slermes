# S14 #5: CLI Arg Parsing Parity — Methodology Comparison

**Methodology:** Function-level diff of Python `cli.py` `process_command()` (11K LOC dispatch, ~890 lines in the dispatch chain) + `hermes_cli/commands.py` COMMAND_REGISTRY (1.8K LOC, 70 CommandDef entries) vs C `src/cli/commands.c` (6.7K LOC, 90 commands in static table + supporting config/show/doctor functions).

## Summary

**Verdict: PORTED (~85%)** — core dispatch table, alias resolution, config management, session commands all present. Missing ~15% in UX polish (Rich formatting, skill bundle routing, plugin commands, quick commands, tips, destructive confirmation dialogs).

## Command Registry Structure

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Registry type | `list[CommandDef]` dataclass | `static command_def_t[]` struct array | ✅ Equivalent |
| Aliases | `aliases=("bg",)` tuple | `alias` field per entry (e.g. `"/bg"`) | ✅ PORTED |
| Description | `description` field | `desc` field | ✅ PORTED |
| Category | `category` field | `category` field per entry | ✅ PORTED (struct + data) |
| Args hint | `args_hint="<prompt>"` | `args_hint` field per entry | ✅ PORTED (struct + data) |
| Subcommands tuple | `subcommands=("list","add")` | `subcommands` comma-separated field | ✅ PORTED (CL03) |
| cli_only / gateway_only | Boolean flags on each CommandDef | No flag (all commands available everywhere) | ❌ REAL GAP |
| gateway_config_gate | Config dotpath override for gateway | Not present | ❌ REAL GAP |
| Derived lookups | `_COMMAND_LOOKUP` dict built at import | `commands_resolve()` scans array at call | ✅ Equivalent |
| COMMANDS_BY_CATEGORY | Categorized dict for help | `/help` groups by category dynamically | ✅ PORTED (CL06) |
| SUBCOMMANDS lookup | `/cmd -> [sub1, sub2]` for tab-complete | Displayed in /help detail view | ✅ PORTED (CL03) |

## Dispatch Mechanism

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Entry point | `process_command()` method on HermesCLI | `commands_resolve()` -> `handlers` array | ✅ PORTED |
| Alias resolution | `resolve_command()` from central registry | `commands_resolve()` checks name + alias | ✅ PORTED |
| Prefix matching | Fuzzy prefix match with ambiguity resolution | Prefix match with no ambiguity reporting | ⚠️ PARTIAL |
| Case handling | Lowercase for dispatch, original for args | No lowercasing (exact match only) | ⚠️ PARTIAL |
| Skill bundle routing | `/bundle-name` routes to bundle loader | C has `/bundles list` only | ❌ REAL GAP |
| Skill command routing | `/skill-name` routes to skill loader | C has `/skills search/list/show` only | ❌ REAL GAP |
| Plugin command routing | Plugin handlers registered in dispatch | C has no plugin command routing | ❌ REAL GAP |
| Quick commands | User-defined exec/alias from config.yaml | Not implemented | ❌ REAL GAP |
| Return value | `bool` (True=continue, False=exit) | void handler (cmd_exit calls exit()) | ⚠️ PARTIAL |

## Command Count

| Metric | Python | C | Status |
|--------|--------|---|--------|
| Built-in slash commands | 70 | 90 | C has MORE |
| Aliases defined | ~25 | ~15 | ⚠️ PARTIAL |
| Handlers with subcommands | 15 (via subcommands tuple) | Varies per handler | ⚠️ PARTIAL |
| Gateway-only commands | 7 (start, topic, approve, deny, sethome, commands, restart, platform) | All commands available everywhere | ❌ REAL GAP |

## Missing Python Commands in C

| Command | Python | C Equivalent | Status |
|---------|--------|-------------|--------|
| `/undo` | Removes last exchange with confirmation | `cmd_undo` exists at commands.c | ✅ PORTED |
| `/start` | Acknowledge platform start pings (gateway-only) | Not implemented | ❌ REAL GAP |
| `/topic` | Telegram DM topic sessions (gateway-only) | C's is system topic/personality | ⚠️ Semantic mismatch |
| `/approve` / `/deny` | Gateway command approval | ✅ PORTED | ✅ PORTED |
| `/handoff` | Handoff to messaging platform | ✅ cmd_handoff exists | ✅ PORTED |
| `/codex-runtime` | Toggle Codex app-server runtime | Not implemented (niche) | ❌ REAL GAP |
| `/gquota` | Google Gemini quota check | Not implemented (niche) | ❌ REAL GAP |
| `/indicator` | TUI busy-indicator style | Not implemented | ❌ REAL GAP |
| `/quick-commands` | User-defined from config | Not implemented | ❌ REAL GAP |

## Commands Unique to C (no Python equivalent)

| C Command | Purpose | Status |
|-----------|---------|--------|
| `/tools-verify` | Verify all expected tools registered | ✅ C-specific utility |
| `/skills-hub` | Skills hub search/show/list/sync | ✅ C-specific utility |
| `/secrets` | Manage secrets list/get/sync/status | ✅ C-specific utility |
| `/auth` | Provider auth status | ✅ C-specific utility |
| `/webhook` | Manage webhook subscriptions | ✅ C-specific utility |
| `/memory` | Memory setup status/providers/setup | ✅ C-specific utility |
| `/completions` | Generate shell completions | ✅ C-specific utility |
| `/dump` | Dump system debug info | ✅ C-specific utility |
| `/send` | Send a message to a target | ✅ C-specific utility |
| `/key` | Manage API keys list/set/show/unset | ✅ C-specific utility |

Some of these have Python equivalents in different subsystems (`list_completions`, `hermes doctor` equivalent, etc.) but not as dedicated slash commands.

## UX Features (REAL GAPS in C)

| Feature | Python | C | Gap |
|---------|--------|---|-----|
| Destructive confirmation | `_confirm_destructive_slash()` for clear/new/undo | No confirmation dialog | ❌ REAL GAP |
| Pending session title | Deferred to first message | Not implemented | ❌ REAL GAP |
| Pending resume state | Tracks pending resume sessions | Not implemented | ❌ REAL GAP |
| Random tips | `get_random_tip()` shown on new session | Not implemented | ❌ REAL GAP |
| Welcome banner after /clear | Full Rich banner + tips | Static text banner | ⚠️ PARTIAL |
| Skin engine | Data-driven from config key `display.skin` | `/skin` lists/show/changes but no data-driven per-command theming | ⚠️ PARTIAL |
| History formatting | Rich-formatted with colors | Plain text dump | ⚠️ PARTIAL |
| Plugin error routing | Routes to plugin handler in dispatch | No plugin dispatch | ❌ REAL GAP |
| Prefix ambiguity resolution | "Ambiguous command: did you mean X, Y?" | Returns first match silently | ⚠️ PARTIAL |

## Strengths of C Implementation

1. **More commands (90 vs 70)** — includes utilities not in Python (key management, completions, secrets, auth, webhook, memory, debug dump)
2. **Multi-alias support** — `/resume` via `aliases` field in struct
3. **Config validation** — `/doctor` has 5 subcommands with full system diagnostics
4. **Plugin management** — `/plugins install <path>` and `/plugins remove <name>` (source-level install)
5. **Session search/export** — dedicated `/session-search` and `/session-export` commands
6. **Log viewing** — `/logs` with error/gateway filtering
7. **Send command** — `/send [target] <message>` for cross-platform messaging

## Verdict

**PORTED (~92%)** — core dispatch infrastructure is solid. Category, args_hint, subcommands fields added and populated. Help display groups by category and shows subcommands. Missing features are UX polish (Rich formatting, tips, confirmation dialogs, skin data-driven theming) and dynamic routing (skill bundles, plugin commands, quick commands) plus cli_only/gateway_only flags.

**Key gaps (worth implementing if pursuing full parity):**
1. Destructive confirmation for `/clear`, `/new`, `/undo`, `/reset`
2. cli_only / gateway_only command flags with dispatch filtering
3. Quick commands from config (user-defined exec/alias)
4. Welcome banner with tips on new session
5. Plugin command routing in dispatch
6. Skill bundle routing

**Evidence:** Python `cli.py:8391-8895` (process_command dispatch), `hermes_cli/commands.py:64-223` (COMMAND_REGISTRY). C `src/cli/commands.c:134-235` (COMMANDS static table), `commands.c:242-260` (commands_resolve dispatch).
