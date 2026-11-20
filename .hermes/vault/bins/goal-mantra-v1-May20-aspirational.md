<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
