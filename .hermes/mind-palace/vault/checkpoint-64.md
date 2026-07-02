# Checkpoint 64 — AG25 anthropic_prompt_cache_policy ported (13/24)

## Analysis: Python `agent/agent_runtime_helpers.py:anthropic_prompt_cache_policy()` → C `anthropic_prompt_cache_policy()`

### What
Pure decision function that determines whether to apply Anthropic prompt caching and which layout to use (native Anthropic vs OpenAI-wire envelope). Takes provider, base_url, api_mode, model as strings and returns (should_cache, use_native_layout). Checks against known caching-capable providers: native Anthropic, OpenRouter Claude, Nous Portal, third-party Anthropic-compatible gateways, MiniMax, and Alibaba-family Qwen models.

### Evidence
- **Implementation:** `src/agent/prompt_caching.c:280-398` — `anthropic_prompt_cache_policy(provider, base_url, api_mode, model, &use_native_layout)`
- **Header:** `include/prompt_caching.h:129-146`
- Python original: `agent/agent_runtime_helpers.py:1153-1255` (106 lines)

### Impact
- **AG25:** 12/24→13/24 public functions ported (54%)
- **Name parity:** Matches Python `anthropic_prompt_cache_policy()` exactly

### Build & Test
- **Build:** Clean compile, 0 errors
- **Tests:** 4/4 pass
- **Battleship:** v91
