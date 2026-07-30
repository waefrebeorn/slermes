# Triple DA Audit — v290 — The Reckoning

## What happened

The old model was wrong. "86 PORTED, 0 PARTIAL, 0 GAP" was a fiction — we were
counting module-level function PoP annotations in agent/tools/lib, but the
REAL system has massive unfilled subsystems:

### DA1: Architecture Layer Audit

| Subsystem | Python (lines) | C (lines) | Real Parity |
|-----------|---------------|-----------|-------------|
| Agent core | 71,042 | 47,017 | 66% |
| Tools | 75,165 | 40,473 | 54% |
| CLI (hermes_cli+) | 140,467 | 23,967 | **17%** |
| Gateway | 80,057 | 19,098 | **24%** |
| ACP Adapter | 5,167 | 2,354 | 46% |
| Cron | 3,566 | 2,299 | 64% |
| TUI Gateway | 9,916 | 0 | **0%** |
| Desktop app | EXISTS | 0 | **0%** |
| Web server | 10,080 | 0 (in CLI) | **0%** |
| Plugin system | 3000+ | 0 | **0%** |
| Skills (built-in) | 5000+ | 0 | **0%** |
| Dashboard auth | 2500+ | 0 | **0%** |
| Tests | 100,000+ | minimal | **<5%** |

### DA2: Feature Depth Audit

Each subsystem has features the C version lacks:

**CLI (17% parity):**
- Setup wizard: Python 6,351 lines (hermes_cli/config.py + subcommands/setup.py) vs C's config.c (~huge but not 1:1)
- Auth system: 7,706 lines (oauth, copilot, dingtalk, dashboard auth) — C has basic token storage
- Web server: 10,080 lines — no C equivalent
- Backup: 1,041 lines — no C backup
- Voice: 846 lines — C has voice_mode.c but thin
- Curses UI: 872 lines — C has lib/libtui but different architecture
- Tools config: 3,917 lines — C has tool_config.c but thin
- Curator: 598 lines — C has curator.c but feature-incomplete
- Doctor: 2,189 lines — C has no diagnostic tool
- Debug: 829 lines — C has no debug module
- Clipboard: 494 lines — C has no clipboard

**Gateway (24% parity):**
- Many platforms present but at shallow depth
- Missing: google_chat (500+ lines), irc (400), line (300), ntfy (593), simplex (300)
- Session management: Python gateway/server.py + session.py is rich
- TUI gateway bridge: 9,916 lines — C has nothing equivalent

**Agent (66% parity):**
- Closest to parity but still missing:
  - Pre-LLM call plugin hooks
  - Full credits lifecycle (seeding, background thread)
  - Streaming context scrubber
  - Some message repair/sanitization edge cases

### DA3: User Experience Parity

The C version is functional but lacks:
1. **Setup wizard** — Python's interactive setup is rich; C's is bare-minimum
2. **Dashboard** — Python has a web dashboard with auth; C has nothing
3. **Desktop client** — Python has electron/desktop app; C has nothing
4. **Plugin discovery** — Python auto-discovers plugins; C doesn't
5. **Diagnostics** — Python has `hermes doctor`; C has no equivalent
6. **Auth flows** — Python handles OAuth PKCE flows (Google, GitHub); C is basic
7. **Skill ecosystem** — Python loads skills from config; C has basic skill loading

## Verdict

The 86 PORTED count is valid for FUNCTION-LEVEL MODULE parity in agent/tools,
but the SYSTEM-LEVEL parity is much lower:
- Agent: ~66%
- Tools: ~54%  
- Gateway: ~24%
- CLI: ~17%
- Everything else: ~0-5%

**We've been looking at the wrong level of abstraction.**
