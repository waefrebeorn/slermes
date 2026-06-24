# Task Plan — Remaining Work Breakdown

*Created end of Checkpoint 10. All remaining gaps deconstructed and prioritized.*

## P1 — Quick Wins (implement 1-2 per session, build significant)

### TD20: env_probe.py
- **What:** Environment probing tool — check what's installed, what Python/packages exist
- **Python ref:** `tools/env_probe.py` (file doesn't exist — was listed as gap but artifact)
- **Verdict:** File doesn't exist. Reclassify as **N/A** next session.

### TD23: openrouter_client.py
- **What:** OpenRouter HTTP client convenience wrapper for tool use
- **Python ref:** `tools/openrouter_client.py`
- **C status:** OpenRouter provider exists as `provider_openrouter.c`. This tool is a convenience wrapper.
- **Effort:** M — 1 session

### TD24: skills_guard.py
- **What:** Skill security guard — restrict which skills can be invoked by which agents
- **Python ref:** `tools/skills_guard.py` (1086 lines — complex)
- **Effort:** M — need to design C equivalent

### TD19: clarify_gateway.py
- **What:** Gateway-side clarify with blocking event queue + button UI + timeout
- **Python ref:** `tools/clarify_gateway.py` (278 lines)
- **C status:** `clarify.c` is CLI-only (uses fgets). Needs pthread condvar + gateway integration.
- **Effort:** M — 1-2 sessions

## P2 — Medium Efforts (one per session)

### AL07: Codex app-server runtime
- **What:** `_run_codex_app_server_turn()` in agent loop
- **Python ref:** `agent/codex_runtime.py`
- **C status:** Config field `codex_runtime` exists, no implementation
- **Effort:** M — need to understand codex protocol, add to agent_loop.c
- **Dependency:** Need to read Python codex_runtime.py first

### PL18: Gateway Platforms (5 adapters)
*C already has 20+ platform adapters natively. These 5 are Python-only plugins:*
- **google_chat** (~500 lines Python) — Google Chat webhook platform
- **irc** (~400 lines) — IRC protocol adapter
- **line** (~300 lines) — LINE messaging adapter
- **ntfy** (593 lines) — HTTP push streaming adapter
- **simplex** (~300 lines) — SimpleX messaging adapter

*Each adapter follows the same pattern:*
1. Register platform with gateway
2. Handle incoming messages (HTTP webhook or polling)
3. Route to gateway message handler
4. Send replies back via platform API

**Effort per adapter:** M (1 session each)
**Recommended order:** simplex → irc → google_chat → line → ntfy (simplest first)

### PL01: Memory providers
- **Current:** `plugin_honcho.c` exists, `plugin_file_memory.c` exists
- **Missing:** mem0, supermemory plugins
- **Effort:** S per provider

## P3 — Low Priority

### PR07: default_aux_model
- **What:** Single cheap model for all auxiliary tasks (vs per-task override)
- **Effort:** S — add config field + resolution logic
- **Priority:** Low — per-task works fine

### MS03: conversation_compression.py
- **What:** Different from context_compressor — this is per-conversation summarization
- **Effort:** M

### TD22: neutts_synth.py
- **What:** Neural TTS synthesis tool
- **Effort:** L — audio processing pipeline

## XL — Needs Architecture Design First

### PL14: Teams pipeline
- **6 Python files:** pipeline.py, runtime.py, models.py, subscriptions.py, meetings.py, store.py
- **Total:** ~2500+ lines Python
- **Design needed:** How should Teams integration work in C? Different from existing msgraph_webhook platform.
- **Breakdown:** Design first (1 session), then implement per-file (6+ sessions)

### GW16: api_server platform adapter
- **Python:** 4228 lines — full REST API platform adapter with sessions, runs, SSE, health checks
- **C status:** `api_server.c` exists as standalone HTTP server, NOT a gateway platform adapter
- **Gap:** Need to wire api_server.c as a gw_platform_t, add session management, SSE events, run lifecycle
- **Breakdown:** 
  1. Wire api_server.c into gw_platform_t (1 session)
  2. Add session CRUD endpoints (1 session)
  3. Add run lifecycle + SSE (1 session)
  4. Add health check + capabilities endpoints (1 session)

### MS01/MS02: browser provider
- **Python:** browser_provider.py + browser_registry.py
- **C status:** `browser.c` exists but unclear if it covers provider abstraction
- **Effort:** L — CDP protocol, session management, multiple browser backends

### MS04: copilot_acp_client
- **Python:** GitHub Copilot ACP client
- **Effort:** L — ACP protocol + GitHub API integration

## Session Template

Each session should:
1. Read task-plan.md → pick next P1 item
2. Read Python reference
3. Implement C equivalent
4. Build → test → verify
5. Update battleship + walkway
6. Doc checkpoint every 3+ gaps

## Doc Checkpoint Rules
- Trigger: 3+ gaps closed, subsystem done, or session boundary
- Not triggered: 1-2 trivial gaps, code-only refactors
- Batch all gaps since last checkpoint into ONE vault entry
- Version-bump checkpoint count once per batch
