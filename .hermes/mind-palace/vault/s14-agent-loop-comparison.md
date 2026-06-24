# S14 #1 — Agent Loop Function-Level Comparison

**Python:** `agent/conversation_loop.py` `run_conversation()` — ~4300 lines (core loop ~2000)
**C:** `src/agent/agent_loop.c` `agent_run_conversation()` — ~920 lines (core loop)

## Core Loop Structure — PORTED

Both implement the same fundamental flow:

1. User message → system prompt assembly → LLM call
2. Tool dispatch (parallel via pthreads in C, sequential in Python)
3. Tool results appended → loop back to LLM
4. Retry with exponential backoff (1s, 2s, 4s... 16s)
5. Fallback model/providers on failure
6. Interrupt handling (SIGINT in C, flag in Python)
7. Context compression (adaptive threshold, anti-thrashing cooldowns)
8. Budget/cost/token tracking
9. Stream callback support
10. Security approval + tool guardrails

## Advanced Features — PARTIAL/MISSING

| # | Feature | Python | C | Status |
|---|---------|--------|---|--------|
| 1 | Plugin context injection | pre_llm_call hook returns `context` appended to user message | Hook fires JSON event only — no mechanism for plugins to inject content | REAL GAP |
| 2 | Anthropic prompt caching | `apply_anthropic_cache_control()` with TTL | `system_cached` flag only, no cache_control in API messages | REAL GAP |
| 3 | Tool call argument repair | `_sanitize_tool_call_arguments()`, `_repair_tool_call_arguments()` | Passes raw args from LLM | REAL GAP |
| 4 | Message role alternation repair | `_repair_message_sequence()` catches tool→user/user→user wedges | None | REAL GAP |
| 5 | Thinking-only turn detection | `_drop_thinking_only_and_merge_users()` for Anthropic compat | None | REAL GAP |
| 6 | External memory providers | `_memory_manager.prefetch_all()` (honcho, mem0, supermemory) | SQLite memory only | REAL GAP |
| 7 | Codex app-server runtime | `_run_codex_app_server_turn()` | None | REAL GAP |
| 8 | Rate limit guard (Nous Portal) | `nous_rate_guard` skips API call when rate-limited | None | REAL GAP |
| 9 | Truncated response handling | Length continuation, truncated tool_calls recovery | None | REAL GAP |
| 10 | Trajectory saving | `save_trajectories` flag → JSONL files | None | REAL GAP |
| 11 | Streaming spinner / thinking UI | KawaiiSpinner faces, thinking_callback for TUI | `fprintf(stderr, "...")` only | PARTIAL |
| 12 | Surrogate character sanitization | `_sanitize_messages_surrogates()` on api_messages | `sanitize_surrogates()` on user_message only | PARTIAL |

## Classification

- **Core loop: PORTED (85%)** — all essential flow is there
- **Advanced features: PARTIAL (50%)** — 10 missing production features
- **Overall: PARTIAL (~70%)**
- **Runtime verification: REAL GAP (0%)** — never executed end-to-end

## Evidence Files

- Python: `/home/wubu/.hermes/hermes-agent/agent/conversation_loop.py:351-2900` — run_conversation()
- C: `/home/wubu/hermes-agent-dev/slermes/src/agent/agent_loop.c:820-1739` — agent_run_conversation()
- C stream support: `/home/wubu/hermes-agent-dev/slermes/src/agent/llm_client.c` — llm_chat_completion_stream()
- Plugin hooks: `/home/wubu/hermes-agent-dev/slermes/src/agent/agent_loop.c:1160-1170,1360-1373,1497-1504,1591-1598`
