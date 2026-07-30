# Checkpoint 30 — MS01 browser_provider + MS02 browser_registry

## Gaps closed

### MS01 (browser_provider.py) — REAL GAP → PORTED
- `include/browser_provider.h` — BrowserProvider interface struct with function pointers:
  - `name`, `display_name`, `is_available()`, `create_session()`, `close_session()`, `emergency_cleanup()`
  - Backward-compat aliases: `is_configured()` → `is_available()`, `provider_name()` → `display_name`
  - `browser_session_t` struct with session metadata (session_name, bb_session_id, cdp_url, features, external_call_id)
- `src/agent/browser_provider.c` — Default implementations and `browser_session_free()`

### MS02 (browser_registry.py) — REAL GAP → PORTED
- `include/browser_registry.h` — Registry API: register, get, list, resolve, reset
- `src/agent/browser_registry.c` — Thread-safe registry with:
  - `browser_registry_register()` — register/re-register providers
  - `browser_registry_get()` — lookup by name
  - `browser_registry_list()` — list all providers
  - `browser_registry_resolve()` — active provider resolution matching Python logic:
    1. Explicit "local" → NULL (disable cloud mode)
    2. Explicit config wins (ignoring availability for precise error messages)
    3. Legacy preference walk: browser-use → browserbase (filtered by availability)
    4. Firecrawl only via explicit config (not in legacy walk)
  - `browser_registry_reset()` — test-only clear
- `Makefile` — Added `src/agent/browser_provider.o` and `src/agent/browser_registry.o` to AGENT_OBJ

## Build
- Clean compile, 0 errors
- 4/4 tests pass
