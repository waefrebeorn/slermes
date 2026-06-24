# Checkpoint 58 — Name Parity DA Sweep (7 renames)

## What Was Done
1. **Triple DA sweep** — verified ALL battleship PORTED claims against source
2. **7 C functions renamed** to match Python names exactly (name parity):
   - `extract_rate_limit_reset` → `extract_api_error_context` (llm_client.c:32,1039,1554)
   - `provider_context_cache_save` → `save_context_length` (provider_metadata.c:1634)
   - `provider_context_cache_get` → `get_cached_context_length` (provider_metadata.c:1692)
   - `model_grok_supports_reasoning_effort` → `grok_supports_reasoning_effort` (provider_metadata.c:1138)
   - `account_usage_fetch` → `fetch_account_usage` (tools/account_usage.c:408)
   - `account_usage_render` → `render_account_usage_lines` (tools/account_usage.c:438)
   - `credential_sanitize_payload` → `sanitize_borrowed_credential_payload` (credential_pool.c:441)
   - `context_ref_parse` → `parse_context_references` (context_references.c:369,761)
3. Updated 2 header files: `include/provider_metadata.h`, `include/credential_pool.h`,
   `include/hermes_context_refs.h`, `include/hermes_account_usage.h`
4. Verified all 8 header declarations match new names

## Files Changed
- `src/agent/llm_client.c` — 3 renames (static + call + definition)
- `src/agent/provider_metadata.c` — 3 renames (save_context_length, get_cached_context_length, grok_supports_reasoning_effort)
- `include/provider_metadata.h` — 3 header declarations
- `src/tools/account_usage.c` — 2 renames (fetch_account_usage, render_account_usage_lines)
- `include/hermes_account_usage.h` — 2 header declarations
- `src/agent/credential_pool.c` — 1 rename (sanitize_borrowed_credential_payload)
- `include/credential_pool.h` — 1 header declaration
- `src/agent/context_references.c` — 2 renames (definition + internal call site)
- `include/hermes_context_refs.h` — 1 header declaration

## Build/Test
- Build: clean compile, 0 errors
- Tests: 4/4 passed

## New Directive Applied
- ALL GAPS ARE VALID — no more N/A items
- Name parity 1:1 is immediate priority
- Third-party plugins are valid implementation targets