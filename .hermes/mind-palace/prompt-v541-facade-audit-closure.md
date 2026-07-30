# Next Session Prompt — v541: Façade Audit Closure (Real Ports Shipped)

## Context
You are continuing the Slermes C translation project. Current state:
- **Build: CLEAN** — `make slermes` = 0 errors, binary links (~41 MB)
- **Tests:** `bash tests/run_mission8_tests.sh` → 36 passed, 0 failed, 35 skipped
- **Façade audit COMPLETE** — all 18 audited files / 52 fake-looking "In a real implementation" stubs rewritten as REAL ports and committed individually. No façade comments remain in `src/cli/port_*.c`.
- **Scanner (live, this session):** 4,664 PORTED (47.9%), 5,067 REAL_GAP (52.1%), 9,731 total features
- Prior walkway claims of "8,688/8,688 100% PORTED" are stale/v398-era fiction; corrected to live scanner output above

## What shipped this session (v541)
Real ports (per-file commits):
1. **session_search** — SQLite FTS5 + opendir locate
2. **env_probe** — fork/exec subprocess probes (python/pip/PEP668)
3. **mcp_oauth_manager** — per-server cache, mtime disk-watch, 401 dedup, libmcp_oauth-backed providers
4. **network adapters** (telegram_network/qqbot/sms) — DoH libhttp, QQ bind-task/poll, Twilio HMAC-SHA1 + base64
5. **codex_runtime** — OpenAI Responses API SSE + chat/completions fallback + real `codex` exec + event-stream consumption + cost estimate
6. **google_oauth** — real OAuth2 token exchange + local HTTP callback server
7. **gemini_adapter** — real Generative Language API + OpenAI-format translation
8. **browser_provider** — real Browserbase API when configured, honest NULL otherwise
9. **tts_provider** — honest abstract (list_voices=0, synthesize/stream=NULL)
10. **blueprints** — real job persist + suggestion jsonl store
11. **daytona** — real local file copies + real run_bash exec
12. **voice_mode** — real `pm list packages` probe
13. **windows** — real pidfile + kill(pid,0) liveness
14. **helpers** — real pending-batch ring buffer
15. **browser_cdp_tool** — real CDP WebSocket calls (libwebsocket)
16. **tui_fullscreen** — free external skins + persist active model

## Residual (out of audit scope — optional follow-up)
7 non-audited files still contain `simulate` comments: antigravity_code_assist, shell_hooks, desktop_app_common, browser, port_send_message_tool, port_image_generation_tool, port_gateway_relay_ws_transport. If the next audit pass targets these, treat each `simulate` as a candidate stub.

## Next missions (when ready)
- **Desktop/Web Parity** — 111 features mapped, ~99 missing (4% complete). Largest remaining REAL_GAP.
- **Gap Blitz** — continue closing the 5,067 REAL_GAP features (real C logic, no scaffolding).
- **Triple Devil's Advocate** — re-verify each "PORTED" function is genuinely not a stub (the audit pattern that drove this v541 work).

## Guardrails
- NO fake-looking stubs ("In a real implementation", placeholder WAV/JSON, simulated responses).
- Use in-tree libs: libdb (sqlite+FTS5), libjson, libhttp, libwebsocket, libcrypto, libbase64, libmcp_oauth.
- Commit per ported file; keep build clean; keep 36/36 green.
- SLERMES_HOME (not HERMES_HOME) for the C port; default `.slermes`.
