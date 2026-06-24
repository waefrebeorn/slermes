# S14 #2 — Tool Dispatch Function-Level Comparison

**Python:** `model_tools.py` `handle_function_call()` (~240 lines) + `tools/registry.py` `ToolRegistry` (~440 lines)
**C:** `src/tools/registry.c` `registry_dispatch()` (~45 lines) + full module (~605 lines)

## Core Dispatch — PORTED

Both find a tool by name and call its handler:
1. ✅ Tool lookup in a thread-safe registry
2. ✅ Handler invocation with args JSON + task_id
3. ✅ Thread safety (mutex/RLock)
4. ✅ Generation counter for cache invalidation
5. ✅ Tool registration with name, description, schema, handler, toolset
6. ✅ Fuzzy name repair (registry_repair_tool_name)
7. ✅ Per-tool timeout support
8. ✅ Error JSON format on failure

## Full Dispatch Pipeline — PARTIAL

| # | Feature | Python | C | Status |
|---|---------|--------|---|--------|
| 1 | Type coercion (args "42"→42) | `coerce_tool_args()` | None | REAL GAP |
| 2 | Tool Search bridge | `tool_search`, `tool_describe`, `tool_call` | None | REAL GAP |
| 3 | Agent loop tool redirect | Catches tools that must be handled by loop | None | REAL GAP |
| 4 | Plugin pre_tool_call hook | Block check returns error on block | `hook_invoke("pre_tool_call",...)` fires event, no return value checked | PARTIAL |
| 5 | ACP edit approval | `maybe_require_edit_approval()` | None | REAL GAP |
| 6 | Plugin post_tool_call hook | Injects duration_ms, tool_name, result | `hook_invoke("post_tool_call",...)` fires event only | PARTIAL |
| 7 | Transform tool result hook | Plugin can replace result string | None | REAL GAP |
| 8 | Async handler support | `_run_async()` bridge | None (sync only) | REAL GAP |
| 9 | Toolset availability check | `check_fn()` per toolset, 30s cache | No check_fn concept | REAL GAP |
| 10 | Shadow detection | Rejects cross-toolset name collisions | Silently returns false | PARTIAL |
| 11 | deregister | MCP dynamic tool removal | None | REAL GAP |
| 12 | Result size limiting | `max_result_size_chars` enforcement | None | REAL GAP |
| 13 | Latency tracking | `time.monotonic()` → duration_ms | None | REAL GAP |
| 14 | Error sanitization | `_sanitize_tool_error()` strips framing/cdata | `tool_error_sanitize` (weak sym, test only) | REAL GAP |
| 15 | `tool_error/tool_result` helpers | `tool_error(msg)`, `tool_result(data)` | None in C (Python port exists as `tool_result.c`) | REAL GAP |
| 16 | Rich query API | get_schema, get_emoji, get_toolset_for_tool, is_toolset_available, etc. | registry_get_name, registry_get_toolset, registry_get_timeout only | PARTIAL |

## Classification

- **Core dispatch (find + call handler): PORTED (90%)**
- **Full dispatch pipeline: PARTIAL (~50%)**
- **Overall: PARTIAL (~60%)**
- **Runtime verification: REAL GAP (0%)** — never tested with real LLM response containing tool_calls

## Evidence Files
- Python dispatch: `/home/wubu/.hermes/hermes-agent/model_tools.py:802-1039` — handle_function_call()
- Python registry: `/home/wubu/.hermes/hermes-agent/tools/registry.py:151-589` — ToolRegistry
- C dispatch: `/home/wubu/hermes-agent-dev/slermes/src/tools/registry.c:342-386` — registry_dispatch()
- C registry: `/home/wubu/hermes-agent-dev/slermes/src/tools/registry.c:58-110` — registration
- C tool_error: `/home/wubu/hermes-agent-dev/slermes/src/tools/tool_result.c` — tool_error/tool_result helpers exist but may not be wired
