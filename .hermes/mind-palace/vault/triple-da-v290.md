# Triple DA — v290 — Real Devil's Advocate Audit

**Not a line-count. A feature-by-feature, capability-by-capability audit.**

## Methodology
For each Python subsystem, I asked: "Does C implement equivalent functionality?"
— not "Does C have the same number of files/lines?"

## DA1: CLI — VERDICT: MOSTLY PARITY, FEW REAL GAPS

**Python has 30 subcommands. C has 85+ command handlers.**
Every Python subcommand except `version` has a C `cmd_*` equivalent.

| Python | C cmd_ exists? | Depth | Verdict |
|--------|---------------|-------|---------|
| auth | cmd_auth (3 hits) | Partial | C has basic auth (token store) but NOT multi-provider OAuth PKCE, Copilot auth, DingTalk auth |
| setup | cmd_setup (3 hits) | Good | C has interactive setup wizard in config.c |
| config | cmd_config (3 hits) | Good | C has full config r/w |
| cron | cmd_cron (3 hits) | Good | C has cron scheduler + CLI |
| backup | cmd_backup (3 hits) | Partial | C has basic backup, not the full migration tool |
| doctor | cmd_doctor (3 hits) | Partial | C has basic diagnostics, not the full `hermes doctor` |
| debug | cmd_debug (3 hits) | Partial | C has debug, not as rich |
| logs | cmd_logs (3 hits) | Good | C has log viewing |
| model | cmd_model (3 hits) | Good | C has model listing/selection |
| profile | cmd_profile (3 hits) | Good | C has profile management |
| status | cmd_status (3 hits) | Good | C has status display |
| uninstall | cmd_uninstall (3 hits) | Good | C has uninstall |
| update | cmd_update (3 hits) | Good | C has self-update |
| dashboard | - | **GAP** | Python has web dashboard server (10K lines); C has none |
| gui | - | **GAP** | Python GUI launcher; C has none |
| version | 0 hits | Good | Built-in, not a command |
| login/logout | cmd_auth covers | Good | Auth covers both |

**REAL CLI GAPS:**
1. **Web dashboard** — Python's 10K-line web_server.py has no C equivalent
2. **Copilot auth** / **DingTalk auth** — Python has specialized auth flows C doesn't

## DA2: Gateway — VERDICT: GOOD PLATFORM PARITY, MISSING RUNNER DEPTH

**Platform comparison:**

Python real adapters (18): telegram, discord, slack, signal, whatsapp, webhook, matrix, email, weixin, wecom, dingtalk, feishu, yuanbao, bluebubbles, sms, msgraph_webhook, wecom_callback, api_server

C adapters (18): telegram, discord, slack, signal, whatsapp, webhook, matrix, email, weixin, wecom, dingtalk, feishu, yuanbao, bluebubbles, sms, msgraph_webhook, wecom_callback, homeassistant, mattermost, qqbot

**C has MORE platforms than Python** (homeassistant, mattermost, qqbot are C-native).
**C is missing:** api_server gateway adapter

CORE MISSING FEATURES (in C gateway/server.c vs Python gateway/run.py):
- Gateway restart management (Python GatewayRunner.request_restart at run.py:4227)
- Streaming consumer adapters (Python stream_consumer.py)
- Auto-continue freshness window
- Session persistence (Python session.py:1416 lines)

**REAL GATEWAY GAPS:**
1. Gateway restart / lifecycle management
2. Streaming consumer (different architecture in C, may not need separate file)
3. Session persistence (C may handle differently)

## DA3: Agent — VERDICT: GOOD MODULE PARITY, MISSING INTEGRATION

**Function-level: 86/86 PORTED ✓**
But integration features missing:
1. Background credits seeding (Python uses threading)
2. Pre-LLM call plugin hooks
3. Full memory provider plugin chain (C has memory_provider.c but thin)
4. Streaming context scrubber

## UNIVERSE — WHAT C TRULY DOESN'T HAVE

| Feature | Python size | C status | REAL gap? |
|---------|------------|----------|-----------|
| Web dashboard | 10K lines | 0 | **YES** — C needs an api_server gateway adapter + dashboard |
| OAuth PKCE (multi-provider) | 7.7K lines | 1 provider | **PARTIAL** — C has Google OAuth but not Copilot/DingTalk/Dashboard |
| TUI gateway bridge | 9.9K lines | 0 | **NO** — C's TUI is architected differently (no Python gateway needed) |
| Payment/billing | ~1K lines | 0 | **NO** — Portal API handles this; C calls the same REST endpoints |
| Container boot | ~400 lines | 0 | **NO** — Docker entrypoint, not needed in C binary |
| Full plugin system | 30+ dirs | 2 files | **PARTIAL** — Python plugin SDK is Python-specific; C has native hooks |
| Gateway restart mgmt | ~1K lines | 0 | **YES** — missing gateway lifecycle in C |
| API server gateway adapter | ~4.3K lines | 0 | **YES** — C has api_server.c but NOT wired as a gateway platform |
