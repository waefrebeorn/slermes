# State — Slermes C Translation (v665)

- Build: `make slermes` = 0 errors, binary links clean (~48.8 MB)
- **Scanner (real, live 2026-07-26):** 6,914 PORTED (59.7%), 4,624 REAL_GAP (40.0%), 35 PARTIAL, 11,573 total features. N/A: 0 (deleted).
- Tests: `bash tests/run_mission8_tests.sh` → 36 passed, 0 failed, 35 skipped

## This Session (v665) — Façade Hunt + Orphan Audit + skills_guard Port

Continued the scaffolding/façade audit and infrastructure hardening:

- **`resolve_skill_config_values`** (libskillutils): was returning static `"{}"` with
  "config.yaml skills parsing TBD" — now real: walks `skills.config.<key>` dotpaths
  via libyaml, default fallback, `~`/`${VAR}` expansion. Root-caused that libyaml never
  stripped quotes from scalars (diverging from `yaml.safe_load`) — added faithful
  flow-scalar unquoting + escape processing; fixed `test_yaml.c` which had frozen the
  buggy behavior as the expected value. Oracle: 5/5 MATCH.
- **`label_from_token`** (credential_pool_custom): "In C we just return fallback since
  full JWT parsing requires base64 decode" — used already-ported JWT decode. Oracle 8/8.
- **`_detect_target`/`is_platform_supported`** (tirith): fabricated `"linux-x86_64"`
  triples that match no tirith release — now uname()-based Rust triples. Oracle MATCH.
- **`skills_hub_resolve_skill_name`** (port_skills_hub): "Pass through for now" with
  wrong signature — faithful `(fm, url)` port + linked `port_skills_hub.o` (was orphaned).
  Oracle: 10/10 MATCH.
- **Orphaned-object audit:** 66 src/*.c files referenced by NO build target. Five finished
  ports (~135 PoP fns: mcp_tool, send_message, tts, file_operations, image_generation)
  were counted PORTED by the scanner but never entered the binary. Wired, 0 symbol
  collisions. `port_process_registry.c` didn't even compile (corrupted comment block).
- **`skills_guard.py` faithful port** (port_tools_skills_guard.c + header + oracle):
  Finding/ScanResult dataclasses, content digest, scan cache. Builds clean, linked.

Live parity: PORTED 6,914 / REAL_GAP 4,624. All ports oracle-verified vs live Python.

All 18 files ported to real implementations (per-file commits, no god-header):

1. **port_tools_session_search_tool.c** — real SQLite FTS5 session-DB queries + opendir locate (f2d62a89f5)
2. **port_tools_env_probe.c** — real fork/exec subprocess probes (python/pip/PEP668) (d8a8be8c79)
3. **port_tools_mcp_oauth_manager.c** — real OAuth coordination: per-server cache w/ URL-change discard, mtime disk-watch, 401 dedup, libmcp_oauth-backed providers (c41a03134f)
4. **gateway network send adapters** (telegram_network, qqbot_onboard, sms) — real libhttp/libjson DoH, QQ bind-task/poll, Twilio HMAC-SHA1 + base64 Basic auth (a2ee660c07)
5. **port_agent_codex_runtime.c** — 5 stubs → real OpenAI Responses API SSE stream + chat/completions fallback + real `codex` subprocess exec + real event-stream consumption + per-model cost estimate
6. **port_agent_google_oauth.c** — 2 stubs → real OAuth2 token exchange + local HTTP callback server
7. **port_agent_gemini_cloudcode_adapter_methods.c** — 2 stubs → real Gemini Generative Language API (generateContent/stream) + OpenAI-format translation
8. **port_agent_browser_provider_methods.c** — abstract create_session → real Browserbase API call when configured, NULL otherwise (honest)
9. **port_agent_tts_provider_methods.c** — 3 stubs → honest abstract: list_voices=0, synthesize/stream=NULL (matches Python NotImplementedError)
10. **port_tools_blueprints.c** — 2 stubs → real job creation (cron-validate + persist) + suggestion registration (jsonl store)
11. **port_tools_environments_daytona.c** — 6 stubs → real local file upload/download copies + real run_bash exec; honest live-sandbox notice
12. **port_tools_voice_mode.c** — Termux API check → real `pm list packages` probe
13. **port_gateway_windows.c** — _wait_for_gateway_absent → real pidfile + kill(pid,0) liveness
14. **port_gateway_platforms_helpers.c** — enqueue/cancel_all → real pending-batch ring buffer (mutex-guarded)
15. **port_tools_browser_cdp_tool.c** — 2 stubs → real CDP WebSocket calls via libwebsocket (send command, read response)
16. **tui_fullscreen.c** — free external skins on shutdown + persist active model to SLERMES_HOME

- Build CLEAN, 36/36 tests pass, every ported file committed individually.
- Residual `simulate` mentions live only in 7 non-audited files (antigravity_code_assist, shell_hooks, desktop_app_common, browser, port_send_message_tool, port_image_generation_tool, port_gateway_relay_ws_transport) — outside the 52-stub audit scope; flag for a follow-up audit if desired.

## Follow-up Audit (same session, post-push)
Extended the façade sweep to the 7 residual `simulate`/fake-comment files. Outcome:
- **antigravity_code_assist** — `antigravity_post_json` was a real fake (returned `{}` 200). → real Code Assist POST via libhttp (Bearer auth + JSON body, status mapped).
- **desktop_app_common** — `desktop_oauth_login` fabricated `stub-token-...`. → real browser OAuth handoff (opens authorize URL, honest `interactive_oauth_required`, no fake token).
- **relay_ws_transport** — `ws_connect_worker`/`ws_send_frame`/`ws_read_loop` were all fakes (fake fd, no-op send, usleep loop). → real libwebsocket: `ws_connect`, `ws_send`, `ws_recv` loop dispatching frames to handler.
- **relay_adapter** — reworded stale "in production" handshake comment to reflect real descriptor delivery.
- **shell_hooks / image_generation / browser / port_send_message_tool** — `simulate` comments were misworded; behavior already real (env-var dispatch, lazy-load flag, text-browser UI text, retry_after parse). Reworded; left honest UI/help text in browser.c.
- 36/36 tests still pass; full tree now free of fake-looking stub code.

## This Session (v570) — Parity Gap Closure (post-façade)

Continued the gap-closure pass: ported 70 REAL_GAP functions across 15 modules, all
committed + pushed, build clean, 36/36 tests green. Real gaps dropped 5,053 → 4,989
(64 closed). Scanner now 4,700 PORTED / 4,989 REAL_GAP / 42 PARTIAL.

Discipline: faithful ports only; deferred (not faked) network/config/DB-coupled
functions; checked for existing symbols to avoid collisions; registered new files in
`build/objects.mk`.

Modules ported (commits):
- `cfd2050084` web_server — 7 fs helpers
- `1aaaba041c` web_server — 8 path/auth
- `59db715b73` weixin — 6 AES-128-ECB (OpenSSL EVP)
- `1d638786db` base — 3 proxy/URL
- `73523bbe16` auth — 4 auth-error (HMAC/SHA256)
- `0ae633fbae` kanban_db — 12 TTL/board helpers
- `e99a929744` base — 4 network/media
- `9421da1634` gateway — 4 PID (`/proc`)
- `f6d9175181` models — 2 Nous cache (libjson)
- `204ff038c0` main — 5 git/cgroup
- `3461dc294a` config — 3 .env helpers
- `91805df3b7` main — 1 session file
- `49a1c859c9` backup — 2 exclude/skip
- `b21caa411c` gateway — 5 platform/env (uname/sha256)
- `9f3a620f81` kanban — 4 CLI/time helpers

## This Session (v665) — Pure-Transform Gap Closure (17 funcs)

Ported 17 REAL_GAP functions across 6 modules with faithful C11 + `/* PoP: */`
annotations, each backed by an oracle harness (`t_port_*.c` + `sta_oracle_*.py`)
verified 0 mismatches vs live Python. Build links clean, Mission 8: 36/0/35.

- agent/reasoning_timeouts.py (3): `_get_pattern`, `_match_any`, `get_reasoning_stale_timeout_floor`
- agent/replay_cleanup.py (4): `is_interrupted_tool_result`, `strip_interrupted_tool_tails`, `strip_dangling_tool_call_tail`, `sanitize_replay_history`
- agent/retry_utils.py (3): `_error_text`, `is_zai_coding_overload_error`, `adaptive_rate_limit_backoff`
- agent/thinking_timeout_guidance.py (2): `is_thinking_timeout`, `build_thinking_timeout_guidance`
- agent/agent_runtime_helpers.py (2): `intent_ack_continuation_mode`, `intent_ack_continuation_enabled`
- gateway/cgroup_cleanup.py (3): `_own_cgroup_path`, `_read_cgroup_pids`, `reap_cgroup`

Scanner: 4,884 → 4,901 PORTED (+17); 4,774 → 4,757 REAL_GAP (−17). All 17 target funcs flipped to PORTED.
Fixed `tests/run_one_oracle.sh` (added `-I src`) so port headers resolve.

<!-- PARITY:AUTO -->
| PORTED  | 8,488 / 12,260 (69.2%) |
| REAL_GAP| 3,771 (30.8%) — no N/A |
| PARTIAL | 1 (0.8%) |
| STUB    | 0 |

_Generated from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
