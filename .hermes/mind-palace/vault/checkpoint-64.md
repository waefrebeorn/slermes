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
