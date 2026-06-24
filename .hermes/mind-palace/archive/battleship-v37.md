# Battleship v37 — Active Gap Map

**All gaps from DA sweep May 31 included. FIXED items removed from active gaps but noted.**  
**Total active gaps: 53** (was 51 in v34, now 53 with UX/discovery gaps, minus 6 fixed this session)

**⚠ METHODOLOGY NOTE: Every PORTED claim below is FILE-LIST PARITY, not FUNCTIONAL PARITY.**
We count compiled .o files, not verified user workflows. See Sector S14-15.
**S14 progress: 2/10 assessed** (agent loop + tool dispatch). Before trusting any "PORTED" claim, read the actual C implementation against the Python source.

---

## Sector S0a: Setup & First-Run Experience (NEW — REAL GAPS discovered in DA sweep)

| # | Gap | Status | Evidence | Priority |
|---|------|--------|----------|----------|
| 1 | Interactive setup wizard (`hermes setup` equivalent) | REAL GAP | No `cmd_setup`. Python has ~2000 lines in hermes_cli/setup.py with provider/model/deps/wizard | P0 |
| 2 | .env bootstrap API key wizard | REAL GAP | `cmd_secrets` backend exists but no guided key-entry flow. Python wizard asks for keys, validates them, writes .env | P1 |
| 3 | First-run onboarding / welcome | PARTIAL | Cold start — `>` prompt with no context. Python shows welcome banner, tips, /help suggestions | P2 |
| 4 | `hermes doctor` depth | PARTIAL | `cmd_doctor` exists but only shows config/env/keys status. Python doctor has connectivity checks, auth validation, system diagnostics | P1 |
| 5 | `hermes debug` (upload debug bundle) | PARTIAL | `cmd_debug` exists but only prints system info to stdout. Python uploads to debug service and returns shareable links | P2 |
| 6 | `hermes uninstall` | REAL GAP | Python has hermes_cli/uninstall.py with config backup, binary removal, env cleanup | P2 |
| 7 | `hermes backup/restore` | REAL GAP | Python has hermes_cli/backup.py for config/session backup. Not ported | P3 |

---

## Sector S0b: Install & Distribution Scripts (NEW — partially fixed May 31)

| # | Gap | Status | Evidence | Priority |
|---|------|--------|----------|----------|
| 1 | Setup shell script | FIXED May 31 | Created `setup-slermes.sh` — auto-detects 7 OS types, installs deps, compiles, symlinks, creates .env | DONE |
| 2 | Windows .bat installer | FIXED May 31 | Created `install.bat` — WSL-based build, binary copy, .cmd wrapper | DONE |
| 3 | Termux pkg detection in Makefile | PARTIAL | `setup-slermes.sh` handles Termux. Makefile has no Termux detection — relies on generic Linux path. Termux needs `-I` for termux-specific headers | P2 |
| 4 | Nix flake / shell.nix | REAL GAP | No shell.nix or flake.nix for NixOS users. Python has flake.nix + nix/ directory | P3 |
| 5 | Homebrew formula | REAL GAP | No brew formula for macOS. Python Hermes has brew deployment | P3 |
| 6 | Docker image publishing (GHCR/DockerHub) | REAL GAP | Python has docker/ directory with compose files. No CI workflow to publish Docker image | P2 |

---

## Sector S1: Agent & Conversation Loop

| # | Gap | Status | Evidence | Priority |
|---|------|--------|----------|----------|
| 1 | Memory system depth | PARTIAL | lib/libdb/sqlite3.o present, `/memory` command exists. Python memory providers (honcho, mem0, supermemory) not ported — only SQLite backend | P1 |
| 2 | Volatile prompt tier (per-turn memory snapshot) | REAL GAP | `system_prompt_build_volatile()` exists in system_prompt.c but NEVER called from production. The entire per-turn memory/user-profile/ext-memory injection tier is infrastructure that exists but is inert. Python injects memory snapshot every turn via agent loop | P1 |
| 3 | Onboarding UI wiring | PARTIAL | `src/agent/onboarding.c` exists. Need to verify if wired into agent loop or dead code | P2 |
| 4 | Shell hook security allowlist | REAL GAP | `allowlist_check`, `record`, `build_payload`, `spec_matches_tool` in shell_hooks.c all `__attribute__((unused))`. Shell command allowlist security exists but never runs | P1 |
| 5 | Provider guidance flags | REAL GAP | `is_google_family`, `is_alibaba` in provider_metadata.c declared but never set. LLM has no way to know which provider family is active for provider-specific formatting | P1 |
| 6 | Discord bot header builder | REAL GAP | `build_bot_header()` in discord.c — appears to build Discord interaction response headers but never called | P2 |
| 7 | Anthropic fast mode | REAL GAP | `supports_fast_mode()` in provider_anthropic.c exists, returns false. Fast mode not integrated into agent loop | P2 |
| 8 | Checkpoint/rollback UI | PORTED | `/rollback` works. No gaps | P3 |
| 9 | Session CRUD | PORTED | `/sessions`, `/load`, `/session-export` all work | P3 |
| 10 | Skill command injection | PORTED | `skill_commands.c` wired, `/skills` works | P3 |
| 11 | Tool guardrails | PORTED | `tool_guardrails.c` exists and wired | P3 |

---

## Sector S2: Build System Portability (6 fixed May 31, 5 remaining)

**FIXED this session:**
- Cross-compiler detection (auto-detect clang/gcc/cc)
- pkg-config for OpenSSL with manual fallback
- musl/Alpine support (`-ldl -lpthread` omitted, `__MUSL__` defined)
- macOS detection (brew openssl path, `__APPLE__` defined)
- asan/coverage targets now use portable LDFLAGS
- Dockerfile fixed (slermes binary name, correct paths, hermes symlink)

**REMAINING:**

| # | Gap | Status | Evidence | Priority |
|---|------|--------|----------|----------|
| 1 | macOS code-level compatibility | PARTIAL | `src/tools/terminal.c` has 5x `#ifdef __linux__` blocks with no `#elif __APPLE__` handler. `src/api_server.c:936` hardcodes `__linux__`. macOS compile would fail on these paths | P1 |
| 2 | Static linking target (`make static`) | REAL GAP | No `--static` build target. Docker uses dynamic libssl. For truly portable single-binary distribution, need `make static` with bundled OpenSSL | P2 |
| 3 | Build artifact cleaning | PARTIAL | `make clean` removes .o files but `lib/*.a` archive files remain. Build artifacts accumulate | P2 |
| 4 | Parallel compilation safety | PARTIAL | Massive single `slermes` target — no intermediate phony targets for recompilation granularity. `$(LIB_OBJ)` rebuilds every time | P3 |
| 5 | Pre-commit-hook integration | PARTIAL | `.pre-commit-config.yaml` exists but needs verification it runs `make check` | P3 |

---

## Sector S3: CLI Commands

| # | Gap | Status | Priority |
|---|------|--------|----------|
| 1 | codex-runtime toggle | MINOR — gateway-only feature, not relevant to C | P3 |
| 2 | gquota (Google Gemini quota) | MINOR — niche feature, not worth porting | P3 |

**Verdict: 70 Python commands → 70+ C commands (including extras). PORTED ≥95%. No critical gaps.**

---

## Sector S4: Gateway Platforms

All 19 Python gateway platforms ported to C:

| # | Platform | Status | Notes |
|---|----------|--------|-------|
| 1 | Telegram | PORTED | `telegram.o` + `telegram_network.o` |
| 2 | Discord | PORTED | `discord.o` — `build_bot_header()` dead but rest works |
| 3 | Webhook | PORTED | `webhook.o` |
| 4 | Slack | PORTED | `slack.o` |
| 5 | Matrix | PORTED | `matrix.o` |
| 6 | Mattermost | PORTED | `mattermost.o` |
| 7 | WhatsApp | PORTED | `whatsapp.o` |
| 8 | Email | PORTED | `email.o` |
| 9 | Signal | PORTED | `signal.o` |
| 10 | Home Assistant | PORTED | `homeassistant.o` |
| 11 | SMS | PORTED | `sms.o` |
| 12 | Feishu | PORTED | `feishu.o` |
| 13 | WeCom | PORTED | `wecom.o` + `wecom_callback.o` |
| 14 | DingTalk | PORTED | `dingtalk.o` |
| 15 | QQ Bot | PORTED | `qqbot.o` |
| 16 | BlueBubbles | PORTED | `bluebubbles.o` |
| 17 | MS Graph (Microsoft Teams) | PORTED | `msgraph_webhook.o` |
| 18 | WeChat / Weixin | PORTED | `weixin.o` |
| 19 | Yuanbao (元宝) | PORTED | `yuanbao.o` |

**Gateway server:** `src/gateway/server.o` + `helpers.o` — runtime, config, delivery, memory monitoring. All ported.

**Verdict: PORTED 100%.**

---

## Sector S5: Tool Depth (PARTIAL — 4 gaps)

| # | Gap | Status | Evidence | Priority |
|---|------|--------|----------|----------|
| 1 | MCP config/management UI | PARTIAL | `lib/libmcp/` exists, `cmd_reload_mcp` works, MCP server runtime hooks present. Python has `hermes setup mcp` with server catalog, `mcp-config`, `mcp-startup`, `mcp-catalog`. C has no server search/install UI | P1 |
| 2 | Plugin install/remove flow | PARTIAL | `lib/libplugin/` + `cmd_plugins` exist. Plugins loadable via `.so` files. Python has full lifecycle (install from URL, enable, disable, remove, list). C has list/show only | P1 |
| 3 | Browser tool depth (CDP) | UNCERTAIN | `lib/libbrowser/` exists, `cmd_browser` works. Needs function-level comparison against Python browser tool (CDP Chromium with screenshot, click, type, navigation, accessibility tree) | P2 |
| 4 | Kanban board (multi-agent) | PARTIAL | `cmd_kanban` exists. Python kanban has full task lifecycle, assignees, heartbeat, decomposition. C kanban needs comparison | P1 |

---

## Sector S6: Test Coverage

| # | Metric | Value | Status |
|---|--------|-------|--------|
| 1 | Passing tests | 339 | ✅ PORTED |
| 2 | Failing tests | 0 | ✅ |
| 3 | Skipped tests | 13 | ✅ |
| 4 | Test runner | bash test_runner.sh | ✅ |
| 5 | CI integration | Makefile `check` target | PARTIAL — no GitHub Actions workflow |

---

## Sector S7: Fuzz Coverage (X10)

| # | Count | Target | Status |
|---|-------|--------|--------|
| 1 | 51/62 | Fuzz functions implemented | 82% — 11 remaining |
| 2 | 179 | Active fuzz assertions | Across 36+4 categories |
| 3 | 11 | Remaining functions | libproc, libplugin, libhttp parsing, clipboard, device, user activity, app state, file system, screenshot, OCR, audio |

---

## Sector S8: Benchmark Parity (X11)

**TARGET ACHIEVED 100%. All 30 benchmarks, 102 assertions, suite stable.**

---

## Sector S9: Dead Code (S11 — 8 functions)

| # | Function | File | Status | Impact |
|---|----------|------|--------|--------|
| 1 | `allowlist_check()` | src/agent/shell_hooks.c | `__attribute__((unused))` — command allowlist exists but never runs | SECURITY — shell hook security is inert |
| 2 | `record()` | src/agent/shell_hooks.c | `__attribute__((unused))` — shell hook recording never runs | SECURITY |
| 3 | `build_payload()` | src/agent/shell_hooks.c | `__attribute__((unused))` — shell hook payload builder never runs | SECURITY |
| 4 | `spec_matches_tool()` | src/agent/shell_hooks.c | `__attribute__((unused))` — tool spec matcher never runs | SECURITY |
| 5 | `build_bot_header()` | src/gateway/platforms/discord.c | Discord interaction builder exists but never called | UX — discord responses may be bare |
| 6 | `supports_fast_mode()` | src/agent/provider_anthropic.c | Stub returns false. Fast mode never integrated | FEATURE |
| 7 | `system_prompt_build_volatile()` | src/agent/system_prompt.c | Memory snapshot tier — infrastructure written but production never calls it | FEATURE — overlaps S1 |
| 8 | `is_google_family`, `is_alibaba` | src/agent/provider_metadata.c | Provider-family flags declared but never set | FEATURE — guidance hints inert |

---

## Sector S10: LOC Ratio

| Metric | C (slermes) | Python (hermes) | Ratio |
|--------|-------------|-----------------|-------|
| Agent + tools source | ~62K LOC | ~125K LOC | 49% |
| Documentation | ~3K (md) | ~50K (website) | 6% |
| Libraries | 65 modules, ~50K LOC | ~200 packages | 30% |
| Total | ~112K LOC | ~800K LOC | 14% |

**Note:** Most Python code is wrappers, imports, config, and docstrings. C consolidation is normal. 49% for core agent+tools is healthy.

---

## Sector S11: Test Infrastructure

| # | Gap | Status | Priority |
|---|------|--------|----------|
| 1 | GitHub Actions CI | REAL GAP | No .github/workflows/ directory. Tests only run manually via `test_runner.sh` | P2 |
| 2 | Code coverage CI gate | PARTIAL | `make coverage` exists but no PR gate | P3 |
| 3 | ASan CI | PARTIAL | `make asan` exists but no CI integration | P3 |

---

## Sector S12: End-to-End Verification — #1 and #8 CONFIRMED WORKING

**CRITICAL UPDATE:** Items #1 and #8 have been verified working end-to-end on a live DeepSeek provider.

|| # | Smoke Test | Current Status | Evidence | Priority |
|---|-----------|----------------|----------|----------|
|| 1 | Full conversation: user message → LLM call → tool dispatch → reply | ✅ **CONFIRMED WORKING** | `slermes --json "Say hello"` → `{"response":"Hello"}`. `slermes --json "What is 2+2?"` → called `execute_code`, got `4`, replied `"2+2=4"`. Interactive mode also verified. See vault/achievements.md | P0 |
||| 2 | Gateway loop: HTTP webhook → agent loop → back out | ✅ **CONFIRMED WORKING** | `POST /webhook {"text":"hello world"}` → `{"status":"ok","response":"Hello world!..."}`. Uses HERMES_GATEWAY_PLATFORMS=webhook. Webhook port 9999. See vault/achievements.md | P0 |
|| 3 | Plugin loading: compile .so → place in plugins/ → /plugins list | NEVER RUN | lib/libplugin/ + cmd_plugins exist but no end-to-end plugin lifecycle test | P1 |
|| 4 | MCP integration: configure MCP server in YAML → tool call dispatched | NEVER RUN | lib/libmcp/ + cmd_reload_mcp exist but no MCP server has been connected | P1 |
|| 5 | Browser tool: CDP connection to Chromium | NEVER RUN | lib/libbrowser/ exists but no CDP endpoint has been contacted | P2 |
|| 6 | Kanban board: create → add tasks → retrieve | NEVER RUN | cmd_kanban exists but no kanban test exists in test_runner.sh | P2 |
|| 7 | Cron scheduler: schedule job → fire → deliver output | NEVER RUN | src/cron/*.o built but no timed job has ever been observed to fire | P2 |
|| 8 | Single provider HTTP request | ✅ **CONFIRMED WORKING** | Prerequisite for #1. DeepSeek provider responds to chat completions. `lib/libhttp/http.o` verified | P0 |

**Verdict: PARTIAL. The primary use case (conversation + gateway webhook) now works. 5/8 smoke tests still need verification.**

---

## Sector S13: Code Quality Baseline

| # | Concern | Current State | What's Missing | Priority |
|---|---------|---------------|----------------|----------|
| 1 | Memory safety | Builds with `-g -O2` | No valgrind/ASan run on anything but `--help`. Never run on agent loop or gateway | P0 |
| 2 | Null pointer safety | 0 compiler warnings | No static analysis (cppcheck, clang-tidy). C NULL checks need audit across all tool functions | P1 |
| 3 | Thread safety | `-lpthread` linked | No mutex audit across global state shared by agent loop, gateway, cron subsystems | P2 |
| 4 | Buffer overflow | `-Wno-stringop-truncation` is SUPPRESSED | That warning catches real truncation bugs. It's turned off. Truncation is happening silently | P1 |
| 5 | File descriptor leaks | Not checked | Gateway + HTTP + cron all open file descriptors. No FD tracking or watchdog | P2 |
| 6 | Credential exposure | .env support exists | No audit that secrets stay off argv, out of debug logs, out of system prompt context | P1 |
| 7 | Signed vs unsigned comparison | `-Wno-sign-compare` not set but many patterns visible | Implicit signed/unsigned casts in library code. Needs audit | P2 |
| 8 | Stack buffer sizes | `char buf[4096]` in many places | No audit that snprintf bounds match buffer sizes. Root cause of truncation bugs | P2 |
| 9 | Error path coverage | Most functions return int for error | No audit that every error path is actually checked by the caller | P2 |
| 10 | Fuzzer on JSON/YAML parsing | Fuzz harness exists | No fuzz corpus for malformed config/skill files that could crash parser | P2 |

---

## Sector S14: Python Source Comparison — The Methodology Gap

**The battleship's PORTED/PARTIAL/REAL GAP classification is itself PARTIAL** because it compares file counts, not function signatures.

| # | Methodology Gap | Current Approach | What We Should Do | Priority |
|---|----------------|-----------------|-------------------|----------|
|| 1 | Agent loop comparison | PARTIAL (~70%) | **ASSESSED.** Read `run_agent.py` `run_conversation()` vs C `agent_run_conversation()`. Core loop: PORTED (user msg → system prompt → LLM call → tool dispatch → loop). 10 advanced features are REAL GAPS: plugin context injection, Anthropic prompt caching, tool call arg repair, role alternation repair, thinking-only stripping, external memory, Codex runtime, rate limit guard, truncated response handling, trajectory saving. See `vault/s14-agent-loop-comparison.md`. Runtime verification: REAL GAP (0%) — never executed. | P0 |
|| 2 | Tool dispatch comparison | PARTIAL (~60%) | **ASSESSED.** Read `model_tools.py` `handle_function_call()` vs C `registry_dispatch()`. Core dispatch PORTED (find + call handler). 16 gaps: type coercion, Tool Search bridge, agent loop redirect, plugin hooks (pre/post/transform), ACP edit approval, async, toolset checks, shadow detection, deregister, result size limiting, latency tracking, error sanitization, unwired tool_error helpers, rich query API. See `vault/s14-tool-dispatch-comparison.md`. | P0 |
| 3 | Gateway protocol parity | Counted gateway .o files | Read Python `gateway/session.py` wire protocol. Verify C gateway speaks the same JSON-RPC format | P1 |
| 4 | Session storage parity | Counted `lib/libdb/db.o` | Read Python `hermes_state.py` SessionDB. Verify SQLite schema, FTS5 search, session CRUD match | P1 |
| 5 | CLI arg parsing parity | Counted `cmd_*` functions | Read `cli.py` `process_command()` argument parser. Verify our `const char *args` parsing matches Python's argparse-derived behavior | P1 |
| 6 | Skill system parity | Counted `skill_commands.o` | Read `agent/skill_commands.py`. Verify skill scanning, injection format, bundle resolution match | P2 |
| 7 | Provider adapter format parity | Counted `provider_*.o` | Read Python provider adapters. Verify request/response transformation, error normalization, streaming format match | P1 |
| 8 | Cron job parity | Counted `cron/*.o` | Read `cron/jobs.py` + `cron/scheduler.py`. Verify schedule parsing, job lifecycle, delivery format match | P2 |
| 9 | Memory subsystem parity | Counted `lib/libdb/sqlite3.o` | Read Python memory providers. Verify embedding, search, relevance scoring match | P2 |
| 10 | Redaction mechanism parity | Counted `redact.o` | Read Python `agent/redact.py`. Verify regex patterns, context matching, output sanitization match | P2 |

**Verdict: The methodology that generated this battleship is itself PARTIAL.** Every "PORTED" claim is based on counting what compiles, not what runs or what matches Python behavior.

---

## Sector S15: Dead Methodology

| # | What We're Measuring | What It Actually Tells Us | What It Hides |
|---|---------------------|--------------------------|---------------|
| 1 | .o file existence | The file compiles | NOTHING about whether the function works, matches Python behavior, or handles errors |
| 2 | Slash command name parity | The command handler exists | NOTHING about whether arg parsing, output format, or error messages match Python UX |
| 3 | Library .c file count | We wrote a file in a directory | NOTHING about whether the implementation handles edge cases Python handles |
| 4 | "Builds with 0 errors" | GCC didn't reject the AST | NOTHING about runtime correctness, memory safety, or logical correctness |
| 5 | Test count (339) | test_runner.sh exits 0 | We haven't read a single test to know unit vs integration vs smoke coverage |
| 6 | "FIXED" status for Makefile changes | The change was written | NOT tested on Alpine, macOS, Arch, Termux, NixOS |

---

## Priority Re-Order (honest this time)

| Tier | What | Why |
|------|------|-----|
| **P0-REAL** | Run ONE end-to-end conversation through the binary | Proves the primary use case works |
| **P0-REAL** | Run `make asan && test_runner.sh` | Proves memory safety of what we've built |
| **P0-REAL** | Read ONE Python agent loop function and diff against C | Validates the methodology itself |
| **P1** | Read ONE tool's Python/C implementation side-by-side | First real functional parity check |
| **P1** | Configure a real provider, send a message, see a reply | Proves the integration layer works |
| **P1** | Run valgrind on the binary | Proves no obvious memory corruption |
| **P1** | Remove `-Wno-stringop-truncation` and fix what breaks | Proves buffer safety |
| **P2** | E2E gateway test with webhook | Proves gateway subsystem |
| **P2** | Remaining 11 fuzz functions | Completes X10 |
| **P2** | Function-level Python/C diff of all 85 tools | Real battleship methodology |

---

**LEGEND**
- **PORTED (≥80%)** = functionally equivalent — **⚠ CLAIM IS UNVERIFIED See S14**
- **PARTIAL (20-80%)** = exists but lacks depth
- **REAL GAP (<20%)** = doesn't exist in any form
- **UNCERTAIN** = not verified against source
- **FIXED** = resolved in this session
