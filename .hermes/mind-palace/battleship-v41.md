# Battleship v278 — 72 PORTED, 0 PARTIAL, ~11 GAP

**Methodology:** Per-function Python→C name matching. Leading `_` stripped.
77 agent modules. **auxiliary_client promoted from GAP to PORTED.**

**v278: auxiliary_client overhaul**
- 37 new C functions (provider aliases, model predicates, header builders,
  URL helpers, error classification, health tracking, vision helpers, content extraction)
- 41 PoP annotations covering all portable Python functions
- N/A annotations for Python-specific SDK wrappers, async adapters, config I/O
- 63 new C unit tests — all pass

| Status | Count |
|--------|-------|
| PORTED | 72 |
| PARTIAL | 0 |
| GAP | ~11 |

## Build & Test
- Build: clean (0 warnings, 0 errors).
- Tests: 63/63 auxiliary_client + all lib tests pass.

## GAP Modules (next priority)
- azure_identity_adapter, codex_responses_adapter, codex_runtime
- browser_provider, browser_registry, copilot_acp_client, daytona
- conversation_compression (partially covered in llm_client.c)
