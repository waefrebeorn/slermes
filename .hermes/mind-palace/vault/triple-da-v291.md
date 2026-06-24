# Triple DA — v291 — Real Feature-by-Feature Audit

**Methodology:** Read C source. Read Python source. Compare BEHAVIOR, not file counts.

## DA1: CLI Commands — ACTUAL Depth Audit

| Command | C exists? | C depth | Python depth | REAL gap? |
|---------|-----------|---------|-------------|-----------|
| auth | cmd_auth | getenv check + token store (token_exchange.c:1396L + google_oauth.c:776L) | Full OAuth PKCE (auth.py:7706L) | **PARTIAL** — C has Google OAuth + token store but lacks Copilot/DingTalk/Dashboard auth flows |
| setup | cmd_setup → config.c | Interactive wizard (~300L) | Full config management (config.py:6351L) | **PARTIAL** — C wizard works but has fewer options |
| doctor | cmd_doctor | Quick checks | Full diagnosis system (doctor.py:2189L) | **PARTIAL** — C has basic, not full |
| backup | cmd_backup | Basic backup | Migration tool (backup.py:1041L) | **PARTIAL** |
| webhook | cmd_webhook | Full | Full (webhook.py:298L) | PORTED |
| cron | cmd_cron | Full | Full (cron.py:366L) | PORTED |
| model | cmd_model | Full | Full (codex_models.py:206L) | PORTED |
| logs | cmd_logs | Full | Full | PORTED |
| profile | cmd_profile | Full | Full (profile:203L) | PORTED |
| gateway | cmd_gateway | Full | Full (gateway:256L) | PORTED |
| dashboard | — | **NONE** | Web server (web_server.py:10080L) | **REAL GAP** |
| login | — (part of auth) | Missing browser flow | Browser OAuth (login subcommand) | **REAL GAP** |
| gui | — | **NONE** | GUI launcher (gui.py:63L) | LOW |
| plugins | cmd_plugins | Listing only | Full plugin mgmt | **PARTIAL** |
| voice | cmd_voice | Basic | Full (voice.py:846L) | PARTIAL |

## DA2: Gateway — ACTUAL Platform Audit

C has 18 platform adapters. Python has 18 real adapters.
**Platform parity: ~95%** (both have telegram, discord, slack, signal, whatsapp, etc.)

**REAL gateway gaps:**
1. **Gateway restart/lifecycle** — Python GatewayRunner (run.py:1863 class) has restart, graceful shutdown, exit code management. C server.c has no lifecycle manager.
2. **Streaming consumer** — Python stream_consumer.py handles SSE/polling. C does streaming differently (batch_accumulate/flush in server.c).
3. **api_server gateway adapter** — Python's api_server.py (4304L) is a gateway platform. C's api_server.c is NOT wired as a gateway platform.

## DA3: Agent Integration — ACTUAL Depth Audit

**Function-level parity: 86/86 ✓** (this was correct — every Python function has a C match)

**Integration gaps:**
1. **Credits background seeding** — Python uses daemon thread (credits_tracker.py:seed_credits_at_session_start). C has no threading for this yet.
2. **Plugin hooks** — Python has pre_llm_call, pre_tool_call hooks via plugin system. C has plugin_ext.c (286L) but no equivalent hook chain.
3. **Streaming context scrubber** — Python has stream_scrubber.py. C has think_scrubber.c.

## VERDICT — What's Actually Missing

### HIGH priority (blocks user experience):
1. **Web dashboard** — 0% parity. Python's web_server.py (10K lines) has no C equivalent.
2. **Login/auth browser flows** — C has token storage + Google OAuth but no browser-based login for Copilot/DingTalk/Dashboard.
3. **Gateway api_server adapter** — C has the API server code but not wired as a platform.
4. **Gateway restart/lifecycle** — C gateway can't restart gracefully.

### MEDIUM priority (nice-to-have):
5. **Setup wizard depth** — C works but lacks multi-provider auto-detection.
6. **Doctor/diagnostics** — C has basic checks, not full system diagnosis.
7. **Voice** — C has basic voice mode, not full TTS pipeline.
8. **Plugin hooks** — C plugin system is minimal.

### LOW priority (architectural differences):
9. **TUI gateway bridge** — C TUI is built differently; doesn't need Python's bridge.
10. **Container boot** — Docker entrypoint logic, not needed in C binary.
11. **Desktop app** — C binary IS the app.
12. **Tests** — 100K Python test lines are pytest-specific; C has its own test infrastructure.
13. **Plugin SDK** — Python plugins are Python-specific by nature.
14. **Stripe/billing** — C calls same REST endpoints as Python.
