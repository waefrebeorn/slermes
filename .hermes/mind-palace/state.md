# State — Slermes C Translation (v667, PARITY phase)

- Build: `make slermes` = 0 errors, binary links clean (~48.9 MB)
- **Scanner (real, live 2026-08-02):** 6,798 PORTED (55.4%), 5,222 REAL_GAP (42.6%), 240 PARTIAL — counts owned by `make parity-walkway` sentinel blocks
- **Phase philosophy (v667):** PARITY project — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. The AGI-OS integration consumes the compiled binary, not the Python tree. Every REAL_GAP closed is a real feature ported — no stubs, no N/A.
- Tests: `bash tests/run_mission8_tests.sh` → 77 passed, 0 failed, 0 skipped; hunter `--verify` → 0 missed / 7,514 exported symbols
- **Census truth (v666):** ~2,360 phantom PoP credits purged from the scanner — prior "8,806 PORTED" figures were inflated; the honest baseline is 6,446 PORTED / 5,574 REAL_GAP, now drained to 6,798 / 5,222 (v667).
- Desktop parity: 111 features mapped, ~99 missing (4% complete) — separate workstream

## This Session (v667) — Stub Sweep + Classifier Honesty (210 gaps closed so far)

- **Setter rule (classifier):** assignment to a symbol NOT defined in the port
  file is a legitimate setter, not a bootleg (mirrors getter rule) — 11 flips.
- **Bare-call rule (classifier):** bare `fn(...)` to a defined port function
  recurses; call to an undefined symbol = external code = real — 16 flips,
  all verified as real delegations.
- **~60-function stub sweep**, PoP-verified against Python: rate-limit
  tracker, credits_tracker, skill_utils, memory_manager, windows_ssh
  validators, registry, x_search, approval, mcp_stdio_watchdog, desktop_ui,
  sqlite_runtime, journey, gateway systemctl, nous_account, cron executions,
  weixin sync-buffer, dashboard routes/auth (is_token_route, _s256 PKCE via
  hash_sha256+base64url, SSO/session cookies), curator restore, mcp_picker
  rows, kanban integrity, launchd (reload log path, unloaded exit codes),
  update markers (real file ops), async_delegation (real sqlite DELETE +
  event delivery), process_registry is_completion_consumed (real
  consumed-set accessor), pet atlas slot bounds, weixin split (delegates to
  real splitter), status signatures, clipboard macOS fallbacks, bluebubbles
  api url, env helpers, credential pool, aux client, codex fingerprint.

Live parity: PORTED 6,698 / REAL_GAP 5,322 / PARTIAL 240. Every commit green
(Mission8 77/0, hunter VERIFY 0 missed).
- `ede4b86adf` batch 1 — +31 (20 stubs + 11 setter flips)
- `0d76eda554` batch 2 — +38 (22 stubs + 16 bare-call flips)
- `d2fad16d2e` batch 3 — +22 (22 stubs)
- `0bff81812f` batch 4 — +19 (19 stubs)

## This Session (v666) — Census Truth + 142-Gap Stub Sweep

The parity census was corrected and then drained:

- **Phantom PoP credits purged (scanner bug):** pop_patterns[1,2] (trailing-underscore
  captures) fired on EVERY `/* PoP: xxx_yyy @ ... */` annotation, creating phantom
  PopAnnotations with python_file='' that shadow-credited ~2,360 functions across
  unrelated modules (e.g. goals.py:state was "ported" by subscription
  build_subscription_state). Both phantom patterns deleted; find_pop_for_python now
  rejects module-less annotations when the caller knows its module. Honest census:
  PORTED 6,446 / REAL_GAP 5,574 (was falsely 8,806 / 3,453).
- **Getter bootleg false-positive fixed:** `return <bare static symbol>;` was flagged
  as a bootleg delegation — real getters (display labels, cache clears, streak
  resets) un-hid. ~19 ports recovered.
- **Memory-management real signals added:** free/memset/strdup are real work —
  destructor-class ports (raw config cache clear, approval session key reset) credit.
- **~120-function stub sweep** (PoP-verified against Python bodies): hermes_state
  session-management + gateway-routing clusters (16+6), lsp/servers spawner tier (12),
  lsp/eventlog (9), iron_proxy (8), bitwarden (6), onepassword (6), secret-source
  helpers, service-detection (systemd/launchd/s6), PE-header machine parse, real EVP
  SHA-256, SQLite WAL-reset version check, watchdog cluster, uninstall/gui logs,
  provider slug map, cron API, yuanbao MsgBody/properties, qqbot helpers, safe_float
  trio, modal mode coerce, token redaction, ISO timestamps, env getters, path
  helpers, cmd shims, and ~80 more small-to-medium ports.

Live parity: PORTED 6,588 / REAL_GAP 5,432 / PARTIAL 240. Every commit green
(Mission8 77/0, hunter VERIFY 0 missed).

*(prior-session record, kept for history — v665 façade/orphan session's port list:)*
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

## This Session (v666) — Pure-Transform Gap Closure (17 funcs)

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
| PORTED  | 6,798 / 12,260 (55.4%) |
| REAL_GAP| 5,222 (42.6%) — no N/A |
| PARTIAL | 240 (2.0%) |

**Phase (v667):** PARITY project — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

_Generated 2026-08-02T10:21:52Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
