# Checkpoint 20 — TR03 + TR08 PORTED

**Gaps closed: 2** (TR03, TR08)

## TR03: Codex App Server Session Management

**What:** Session adapter for codex app-server runtime. Owns one Codex thread per Hermes session. Drives turn/start, consumes streaming notifications, handles approval requests, translates cancellation.

**Python ref:** `agent/transports/codex_app_server_session.py` (846 lines)
**C impl:** `src/agent/codex_app_server_session.c` (~850 lines)

Key functions ported:
- `codex_session_new/free/close` — lifecycle
- `codex_session_ensure_started` — spawn subprocess, initialize handshake, thread/start with cross-fill thread.id/sessionId
- `codex_session_run_turn` — main turn loop with notification polling, server request draining, approval bridging (exec/apply_patch/permissions/MCP elicitation), post-tool quiet watchdog, deadline management, interrupt handling, OAuth failure classification
- `codex_session_request_interrupt` — idempotent interrupt signaling
- Pending file-change cache for apply_patch approval summaries
- `codex_turn_result_free` — result cleanup

**Also:** Added `codex_client_respond_error()` to `codex_app_server_client` for unknown server request rejection.

**Impact:** AL07 (Codex app-server runtime) now PORTED — all three layers complete (provider + session + projector).

## TR08: Hermes Tools MCP Server

**What:** Exposes curated subset of Hermes tools to codex subprocess via stdio MCP protocol. Codex registers it as normal MCP server.

**Python ref:** `agent/transports/hermes_tools_mcp_server.py` (233 lines)
**C impl:** `src/agent/hermes_tools_mcp_server.c` (~230 lines)

Exposed tools (22): web_search, web_extract, browser_navigate/click/type/press/snapshot/scroll/back/get_images/console/vision, vision_analyze, image_generate, skill_view, skills_list, text_to_speech, kanban_complete/block/comment/heartbeat/show/list/create/unblock/link

Protocol: newline-delimited JSON-RPC 2.0 over stdin/stdout. Handles initialize, tools/list, tools/call, ping, notifications/initialized. Dispatches via existing `registry_dispatch()`.

## Metrics

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| PORTED | ~146 | ~149 | +3 |
| PARTIAL | ~14 | ~13 | -1 |
| REAL GAP | ~39 | ~38 | -1 |
| Overall | ~71% | ~71% | — |
