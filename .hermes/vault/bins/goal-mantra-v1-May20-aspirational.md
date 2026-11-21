<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Goal Mantra — Hermes C Translation (Archived v1, May 20)
**⚠️ OUTDATED — Replaced by goal-mantra.md (May 25 HONEST)**

**Translate Hermes Agent Python → C, zero-dependency single binary.**

Phase 1: Foundation deps (JSON/YAML/HTTP/crypto/DB/display → C wrappers)
Phase 2: Agent core (loop + LLM client + CLI → C)
Phase 3: Tools (file/terminal/web/skills → C)
Phase 4: Gateway (server + platform adapters → C)
Phase 5: Advanced (cron/skills/edge → C)

**Rules:**
- No Python runtime dependency
- Single compiled binary
- Track alongside Python in same repo (C/ dir)
- Every pull: digest → implement → build → commit
- No time goals. Loop until done.
