<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Goal Mantra — Slermes C Translation

The sentinel above is the live scanner verdict, owned by `make parity`.
Never hand-type a parity count into this file — run the tools.

## The Mantra

1. **No stubs.** A function that logs + returns a default is a REAL_GAP, not
   a port. "For brevity", "scaffolding for later", "stub for extension" —
   all REAL_GAP.
2. **Form must equal function.** A PoP annotation on a fake body is worse
   than none.
3. **Rewrite from scratch in C is the point.** Anything in the upstream
   delta is work, never N/A. The whole ecosystem — agent, tools, platforms,
   plugins, and every third-party dependency — gets SLERMed. Nothing is
   "not doable"; we do it because we are doing it in C11.
4. **One shared oracle harness.** Fixtures in `tests/oracle/fixtures/`,
   generic runners `tests/run_one_oracle.sh` / `tests/run_all_oracles.sh` —
   never hand-roll a per-port validator that prints ok/MISMATCH prose.
5. **Reuse before add.** Grep `lib/ + src/ + include/` for an existing
   backend before writing a port file.

## The Loop

Read scanner → battleship → pick the next module by the right axis → grep
before writing → implement real C matching the Python architecture → add
PoP annotations (C name first) → prove with oracle + watercup → build →
scan → commit. One module at a time; finish it.

No choices. No questions. Never stop between gaps.

## Honest scoreboards

- Scanner (`make parity`) — feature-level PoP claims, a pointer not a verdict.
- `tests/parity_reality.py` — body-level reality; placeholders don't count.
- Oracle + watercup — the only behavioral proof. A port is parity-closed
  only when BOTH say MATCH.
