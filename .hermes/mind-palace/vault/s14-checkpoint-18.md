# Checkpoint 18 — Codex Responses API Provider + Reclassifications

## New Code
**`src/agent/provider_codex_responses.c`** — OpenAI Responses API (Codex) provider:
- `codex_build_url()` → `/v1/responses` endpoint
- `codex_build_headers()` — Bearer auth
- `codex_build_request_body()` — Converts chat messages to Responses API `input` array, flat function tool schema, reasoning effort, `store=false`
- `codex_parse_response()` — Parses `output` array (message/function_call/reasoning items), extracts text content, tool calls, encrypted reasoning, finish_reason
- `codex_parse_stream_chunk()` — Parses SSE events (`response.output_text.delta`, `response.function_call_arguments.delta`, `response.output_item.done`, `response.completed`)
- `codex_free_response()` — Response cleanup

## Wiring
- **`include/hermes.h`**: Added `PROVIDER_CODEX` to enum, `extern PROVIDER_OPS_CODEX`, `char api_mode[32]` to `llm_config_t`
- **`src/agent/provider.c`**: Registered `PROVIDER_CODEX`, added `"codex_responses"` and `"codex"` name mappings
- **`src/agent/llm_client.c`**: Both `llm_chat_completion()` and `llm_chat_completion_stream()` detect `api_mode=="codex_responses"` and swap provider ops to `PROVIDER_OPS_CODEX`
- **`src/agent/agent_loop.c`**: Copies `cfg->provider_cfg.api_mode` to `state->llm.api_mode`
- **Makefile**: Added `src/agent/provider_codex_responses.o` to `AGENT_OBJ`

## Reclassifications
1. **TD17** `browser_cdp_tool.py` → ✅ PORTED (`browser.c:1455-1481`, `browser.c:1750-1753`)
2. **TD18** `browser_dialog_tool.py` → ✅ PORTED (`browser.c:1621-1650`, `browser.c:1745-1748`)
3. **PL18** gateway_platforms → N/A (Python plugin wrappers for platforms built-in to C gateway)
4. **AL07** Codex app-server runtime → ⚠️ PARTIAL (core Responses API provider done; missing: multimodal content conversion, encrypted reasoning replay, cross-issuer guard)
5. **MS11** `codex_responses_adapter.py` → ⚠️ PARTIAL (same as AL07)

## Build Status
✅ Clean compile, 0 warnings, binary links successfully

## Coverage
- **~89% PORTED** (134/151)
- **~5% PARTIAL** (8/151)
- **~2% REAL GAP** (3/151)
