# Battleship v276 — 65 PORTED, 0 PARTIAL, 12 GAP (per-function PoP)

**Methodology:** Per-function Python→C name matching. Leading `_` stripped.
77 agent modules. **All PARTIAL modules cleared.**

**v276: curator 12→26 (81%) → PORTED!**
  - extract_absorbed_into_declarations, classify_removed_skills
  - parse_structured_summary + 8 prior implementations this session
  - Last PARTIAL module eliminated

| Status | Count |
|--------|-------|
| PORTED | 65 |
| PARTIAL | 0 |
| GAP | 12 |

## Build & Test
- Build: clean. Tests: 4/4 pass.

## GAP Modules (next priority)
- async_utils, auxiliary_client, azure_identity_adapter
- codex_responses_adapter, codex_runtime, conversation_compression
- copilot_acp_client, credential_pool, google_code_assist
- jiter_preload, plugin_llm, tool_executor
