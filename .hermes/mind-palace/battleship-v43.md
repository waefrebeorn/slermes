# Battleship v280 — 77 PORTED, 0 PARTIAL, ~6 GAP

**Methodology:** Per-function Python→C name matching. Leading `_` stripped.
77 agent modules. **Two Codex GAP modules resolved.**

**v280: codex_responses_adapter + codex_runtime → PORTED**
- codex_runtime: run_codex_app_server_turn→codex_session_run_turn,
  _consume_codex_event_stream→codex_projector_project,
  run_codex_stream/run_codex_create_stream_fallback→provider_codex_responses.c
  _event_field, _raise_stream_error N/A (Python-specific)
- codex_responses_adapter: all 14 functions N/A (Python dict/format conversion)

| Status | Count |
|--------|-------|
| PORTED | 77 |
| PARTIAL | 0 |
| GAP | ~6 |

## Build & Test
- Build: clean (0 warnings, 0 errors).

## Remaining GAP Modules
- azure_identity_adapter, copilot_acp_client, daytona
