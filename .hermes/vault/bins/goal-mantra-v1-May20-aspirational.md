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
