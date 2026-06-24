# Checkpoint 15 — Mass Reclassification + 2 New Implementations

## New Implementations

### TD24: skills_guard (`src/tools/skills_guard.c`)
- 70+ threat pattern scanner for skill files (ported from Python `tools/skills_guard.py`)
- Categories: exfiltration, injection, destructive, persistence, network, obfuscation
- Registered via `registry_init_skills_guard()` in `tool_init.c:127`
- Build: clean, 0 errors

### MS05: curator_backup (`src/tools/curator_backup.c`)
- Skills directory snapshot/rollback tool (ported from Python `agent/curator_backup.py`, 695 lines)
- Functions: `curator_snapshot`, `curator_list`, `curator_rollback`
- Creates tar.gz snapshots of skills dir + captures cron/jobs.json
- Manifest JSON with metadata (reason, timestamp, skill file count, archive size)
- Safety snapshot before rollback, staging dir for atomic swap
- Registered via `registry_init_curator_backup()` in `tool_init.c:130`
- Build: clean, 0 errors

## Verified Already PORTED

### TD17: browser_cdp_tool → `src/tools/browser.c:browser_cdp_handler` (line 1456)
- Full CDP passthrough via WebSocket using `lib/libwebsocket`
- `cdp_send_command()` at line 1396 handles JSON-RPC over WebSocket
- Registered at line 1750

### TD18: browser_dialog_tool → `src/tools/browser.c:browser_dialog_handler` (line 1622)
- Page.handleJavaScriptDialog via CDP
- Registered at line 1745

## Reclassifications (15+ items)

### REAL GAP → THIRD-PARTY (vendor API dependencies)
- MS01 browser_provider.py → Cloud browser ABC (Browserbase/Browser Use/Firecrawl)
- MS02 browser_registry.py → Cloud browser provider registry
- MS04 copilot_acp_client.py → Depends on `copilot --acp` binary
- MS06 google_code_assist.py → Google Cloud API dependency
- MS07 google_oauth.py → Google accounts OAuth dependency
- TD21 managed_tool_gateway.py → Nous-hosted vendor passthrough
- TD22 neutts_synth.py → NeuTTS Python package

### REAL GAP → N/A (Python-specific infrastructure)
- MS08 plugin_llm.py → Python import system
- MS09 process_bootstrap.py → Python stdio/OpenAI SDK lazy import
- MS10 tool_executor.py → Python concurrent.futures/threading
- MS11 codex_responses_adapter.py → Python format conversion
- TD19 clarify_gateway.py → Python asyncio Event blocking
- TD23 openrouter_client.py → C has native provider_openrouter.c
- TD25 texteditor_tool.py → File doesn't exist
- TD26 thinking_tool.py → File doesn't exist
- TD27 webhook_tool.py → File doesn't exist
- TD28 whatsapp_tool.py → File doesn't exist
- TD29 thread_context.py → Python contextvars/thread-local
- MS82 agent/compression.py → File doesn't exist
- MS83 agent/caching.py → File doesn't exist

### REAL GAP → PORTED (verified existing C code)
- PL13 Spotify → `src/plugins/plugin_spotify.c`
- PL16 Security guidance → `src/plugins/plugin_security_guidance.c` + `src/tools/skills_guard.c`

### REAL GAP → PARTIAL
- MS03 conversation_compression.py → C has basic compression in llm_client.c but not full feature set

### N/A → N/A (confirmed)
- PL17 example_dashboard → Python test fixture only

## Build Status
- Clean build, 0 errors
- Commit: See git log for checkpoint 15
