# Slermes Gap Closure Plan

**Date:** 2026-07-05  
**State:** 4,116 / 9,731 ported (42.3%) — 5,615 unported  
**Build:** `make` passes, scanner: 0 PARTIAL, 0 STUB, 0 REAL_GAP  

## The Problem

The parity scanner marks 5,615 features as `). The edict says:

> "rewriting from scratch in C is the point of the project — anything that falls under that is reclassified as work REAL_GAP"

These are NOT truly N/A. They are Python features that haven't been ported to C yet. Every one is either:

| Type | Count | Meaning |
|------|-------|---------|
| **NA_SDK** | 2,857 | Python library dependency — needs C rewrite |
| **NA_CLI** | 2,434 | Python CLI infrastructure — low priority, C binary already handles this |
| **NA_ASYNC** | 305 | Python async/await — needs C callback/thread model |
| **NA_ABC** | 17 | Python abstract base classes — needs C struct + function ptrs |
| **NA_CONFIG_IO** | 2 | Config file I/O — already ported elsewhere |

## Priority Tiers

### Tier 1 — Tools Layer (Highest Value) ≈ 1,200 features

These are tool-level modules with straightforward C-portable logic:
environment lookups, JSON parsing, string manipulation, HTTP calls.

| Module | NA | Priority | Reason |
|--------|----|----------|--------|
| `tools/skills_hub.py` | 163 | **P0** | Git-based skill install/update/list → libgit or popen |
| `tools/mcp_tool.py` | 87 | **P0** | MCP protocol client → HTTP/JSON |
| `tools/tts_tool.py` | 63 | **P1** | TTS API calls → HTTP POST |
| `tools/browser_tool.py` | 56 | **P1** | Browser tool helpers — 44% done, more to go |
| `tools/computer_use/cua_backend.py` | 55 | **P1** | Computer use backends |
| `tools/file_operations.py` | 47 | **P1** | File operations |
| `tools/delegate_tool.py` | 42 | **P1** | Task delegation → subprocess/IPC |
| `tools/file_tools.py` | 41 | **P1** | File tools |
| `tools/terminal_tool.py` | 37 | **P1** | Terminal tool helpers |
| `tools/browser_camofox.py` | 33 | **P1** | Camofox browser driver |
| `tools/memory_tool.py` | 28 | **P2** | Memory tool helpers |
| `tools/tirith_security.py` | 26 | **P2** | Security policy engine |
| `tools/vision_tools.py` | 22 | **P2** | Vision analysis API |
| `tools/lazy_deps.py` | 21 | **P2** | Lazy dependency loader |
| `tools/fuzzy_match.py` | 21 | **P2** | Fuzzy string matching |
| `tools/skill_manager_tool.py` | 30 | **P2** | Skill management |
| `tools/registry.py` | 26 | **P2** | Tool registry |
| `tools/environments/base.py` | 25 | **P2** | Environment base class |
| `tools/environments/local.py` | 24 | **P2** | Local env execution |
| `tools/discord_tool.py` | 19 | **P3** | Discord integration |
| `tools/send_message_tool.py` | 11 | **P3** | Message sending |
| `tools/web_tools.py` | 1 | **P3** | ~100% done, 1 remaining |

### Tier 2 — Gateway Layer ≈ 800 features

Platform adapters, session management, streaming.

| Module | NA | Priority | Reason |
|--------|----|----------|--------|
| `gateway/run.py` | 273 | **P1** | Gateway lifecycle, error handling, event loop |
| `gateway/platforms/yuanbao.py` | 166 | **P2** | Protocol serialization |
| `gateway/platforms/base.py` | 102 | **P1** | Platform adapter base (79 SDK + 23 async) |
| `gateway/platforms/qqbot/adapter.py` | 75 | **P2** | QQ Bot adapter |
| `gateway/platforms/weixin.py` | 73 | **P2** | WeChat adapter |
| `gateway/slash_commands.py` | 56 | **P2** | Slash command parser |
| `gateway/status.py` | 56 | **P2** | Gateway status reporting |
| `gateway/platforms/yuanbao_proto.py` | 45 | **P2** | Protocol definition |
| `gateway/platforms/api_server.py` | 37 | **P2** | API server platform |
| `gateway/stream_consumer.py` | 37 | **P2** | Stream consumption |
| `gateway/session.py` | 32 | **P2** | Session management |
| `gateway/pairing.py` | 24 | **P3** | Gateway pairing |

### Tier 3 — Agent/Core Layer ≈ 500 features

| Module | NA | Priority | Reason |
|--------|----|----------|--------|
| `agent/auxiliary_client.py` | 73 | **P1** | Auxiliary LLM client |
| `agent/learning_graph_render.py` | 36 | **P2** | Learning graph rendering |
| `agent/pet/generate/atlas.py` | 33 | **P2** | Pet sprite atlas generation |
| `agent/memory_manager.py` | 30 | **P2** | Memory management |
| `agent/verification_evidence.py` | 24 | **P2** | Verification evidence |
| `agent/pet/render.py` | 22 | **P2** | Pet terminal rendering |
| `tools/skills_hub.py` | 21 | **P2** | Skill hub syncing |
| `tools/computer_use/tool.py` | 20 | **P2** | Computer use tool |

### Tier 4 — CLI Layer (Lowest Priority) ≈ 2,400 features

These are Python CLI modules that already work through the C binary.  
**Keep as NA unless they block a real feature.**

| Module | NA | Note |
|--------|----|------|
| `hermes_cli/main.py` | 194 | CLI entry point |
| `hermes_cli/auth.py` | 184 | API key management |
| `hermes_cli/kanban_db.py` | 153 | Kanban database CLI |
| `hermes_cli/gateway.py` | 151 | Gateway management CLI |
| `hermes_cli/web_server.py` | 354 | Web server CLI (not→C HTTP server) |
| `hermes_cli/models.py` | 82 | Model configuration |
| `hermes_cli/plugins.py` | 58 | Plugin management |
| ... | ... | ... |

## FAP — the behavioral layer the scanner is blind to

`REAL_GAP`/`PARTIAL`/`STUB` count *missing or shaped-wrong* functions. They say
nothing about functions that **are** ported but **behave differently** from LIVE
Python. That class is a **FAP (Functional Alignment Problem)** — see
`docs/fap.md` for the canonical definition. In one sentence: *a C fn that is
statically "ported" but whose runtime output diverges from LIVE Python, found only
by running the oracle harness.* Do not call it "drift", "desync", "divergence", or
"defect" — call it a **FAP** so every doc, commit, and review means the same
thing.

The function-level scanner can report `PORTED 11,500+ / REAL_GAP 0 / PARTIAL 0`
and the build can be green while **real FAPs still exist** — e.g. a C
provider-auth table with different membership than Python's `PROVIDER_REGISTRY`,
or C json serialization that differs in key order from Python's `json.dumps`.
Those are invisible to the scanner and only surface when the oracle harness runs
(`bash tests/oracle/run_oracles.sh` → any `cases: MISMATCH` is a FAP).

**Closure rule:** a port is not "done" until its oracle reports `MATCH`. If a
port has an oracle harness (`tests/t_port_<name>.c` + `tests/sta_oracle_<name>.py`)
it MUST be registered in `tests/oracle/registry.json`; an unregistered port never
runs, so its FAPs stay silent.

## Methodology

### For each module:
1. **Read the Python source** — understand every function's intent
2. **Create `port_<module>.c`** or extend existing port file
3. **Implement function-by-function** — no void* stubs, no "not fully implemented"
4. **Each function gets a PoP annotation**: `/* PoP: c_name @ module.py:python_name */`
5. **Self-contained includes** — no `hermes.h` (god header banned)
6. **Opaque structs** where the Python uses classes

### What counts as REAL_GAP:
- Any Python function that does data processing, string manipulation, JSON parse/generation, file I/O, environment variable lookup, HTTP calls, or CLI interaction
- **NOT** REAL_GAP: Python-specific runtime (asyncio event loop, import machinery, class definition) — but the *logic inside* async functions IS real C work

## Phase Plan

### Phase A — Tools Layer (next sprint)
1. `tools/skills_hub.py` → `port_skills_hub.c` — skill download/install/update
2. `tools/mcp_tool.py` → `port_mcp_tool.c` — MCP protocol client
3. `tools/tts_tool.py` → `port_tts_tool.c` — TTS API integration
4. `tools/file_operations.py` → `port_file_operations.c` — file ops
5. `tools/computer_use/cua_backend.py` → `port_cua_backend.c`

### Phase B — Gateway Layer
1. `gateway/run.py` — gateway lifecycle (event loop → C main loop)
2. `gateway/platforms/base.py` — platform adapters
3. `gateway/session.py` — session management

### Phase C — Agent Core
1. `agent/auxiliary_client.py` — auxiliary LLM calls
2. `agent/memory_manager.py` — memory persistence
3. `agent/pet/*` — pet generation/runtime