<!-- PARITY:AUTO -->
| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Slermes C — Project (May 22 DA v5)

## Mission
1:1 C translation of NousResearch/hermes-agent. Every CLI command, tool, gateway, TUI, GUI — identical form and function. slermes is the alias binary.

## Done
- ✅ 12 standalone libs
- ✅ Agent loop + LLM + streaming + context + titles
- ✅ 27 tools (18 core + 4 browser + 1 approval + provider framework)
- ✅ 16 CLI commands with registry dispatch
- ✅ 7 gateway platforms (Telegram/Discord/Slack/Matrix/Mattermost/Webhook/WhatsApp)
- ✅ Plugin system, skin engine, cron scheduler, ncurses TUI
- ✅ Security approval integrated into agent loop
- ✅ Abstract provider interface (OpenAI impl done, not wired)
- ✅ Config isolation ~/.slermes/, SLERMES_HOME env
- ✅ 21/21 tests passing, 0 parity issues
- ✅ 5 bugs fixed
- ✅ ROADMAP.md + mind palace + workflow scripts

## Constraint
No PRs to upstream until full 1:1 parity reached. C/ is additive only — never modifies Python files in ways that can't be stashed.

## P0 Next
1. Wire provider interface → llm_client.c
2. Browser click/type/scroll
3. CLI /reset /retry /compress /branch /snapshot
4. Security: URL safety, path traversal, Tirith
5. Gateway: multi-platform simultaneous

## P1 Next (after all P0)
Signal/Email/SMS/HA gateways, kanban(8), spotify(7), terminal backends, skills hub, transcription

## Full Plan
plans/roadmap-100-phases.md (170 phases)
