# Checkpoint 16 — TD Sector Closed + Provider Fields + AL09

## TD Sector → FULLY PORTED

All 8 TD gaps verified against C source:

| TD | Feature | C Evidence |
|----|---------|------------|
| TD02 | Tool Search bridge | `registry_search()` line 919, `registry_describe()` line 939 |
| TD03 | Agent loop tool redirect | `agent_loop.c:1821-1837` intercepts finish/_finished |
| TD05 | ACP edit approval | `acp/edit_approval.c` + `acp_maybe_require_edit_approval()` line 1786 |
| TD08 | Async handler support | `tool_t.async` field at `hermes.h:111` |
| TD09 | Toolset availability check | `registry_set_toolset_check_fn()` line 868 |
| TD11 | Dynamic deregister | `registry_deregister()` line 898 |
| TD12 | Result size limiting | `agent_loop.c:1899-1910` |
| TD16 | Rich query API | search, describe, get_emoji, get_toolset, is_toolset_available |

## Provider Fields (PR04-PR07)

Added to `provider_t` struct (`provider.h`):
- `env_vars[8]` — alternative env var names for API key lookup (PR04)
- `auth_type` — 0=api_key, 1=oauth, 2=aws_sdk, 3=copilot, 4=bearer (PR05)
- `display_name[128]` — human-readable provider name (PR06)
- `signup_url[512]` — provider signup URL (PR06)
- `default_aux_model[128]` — cheap model for subtasks (PR07)

## AL09: Truncated Response Handling

Implemented at `agent_loop.c:1618-1643`:
- Detects `finish_reason == "length"` (LLM hit max_tokens mid-response)
- Pushes partial response as assistant message
- Appends user continuation prompt: "Continue your previous response exactly where you left off..."
- Loops back to LLM for continuation
- Merges partial + continuation in conversation history

## Build Status
- Clean build, 0 errors
- 3 new gaps closed, 8 verified PORTED, 4 provider fields added
