# Slermes ↔ Hermes Parity Ledger (honest, whole-project)
Generated 2026-11-07 — whole production tree, every top-level `def`, except indirect-scan tooling.

## Method
- **PORTED-faithful**: Python fn whose C port carries a `/* PoP: <cfn> @ <module.py>:<pyfn> */` annotation naming THIS module+fn (exact, plus basename-resolved).
- **PORTED-cname**: no PoP, but an identically-named C def exists somewhere (mostly name collisions — NOT counted as ported).
- **REAL_GAP**: no C counterpart found.
- Scope: production `hermes_cli/ agent/ tools/ gateway/ cron/`. Excludes `tests/` (7,508 files), plugins, evals, scripts, optional-skills.

## Totals
- Total top-level Python defs: **36545**
- **PORTED-faithful: 3132 (8%)**
- PORTED-cname (ambiguous): 763 (2%)
- **REAL_GAP: 32650 (89%)**

## By area
| area | defs | faithful | % | cname | gap |
|---|---|---:|---:|---:|---:|
| hermes_cli | 15012 | 1091 | 7% | 269 | 13652 |
| agent | 7440 | 850 | 11% | 270 | 6320 |
| tools | 6593 | 585 | 8% | 115 | 5893 |
| gateway | 6662 | 499 | 7% | 104 | 6059 |
| cron | 838 | 107 | 12% | 5 | 726 |

## Why this differs from STATE.md (66.1% PORTED at v621)
STATE.md's 9,733-fn scoped scanner counted functions across a narrower/module-overlapping set and did
not require a per-function PoP identity — it matched on module/file presence and counted the C unported
margin unit by unit. The identity-led scan above is stricter: a Python def is PORTED only when the C
port explicitly claims that exact module+function (`PoP ... @ module.py:fn`). By that standard the
project is at **~8–9% production parity**, and the oracle suite (81 harnesses, ~4,100 cases) covers
only a fraction of the faithful slice. The two numbers are measuring different things; the ledger here
is the conservative, decision-ready one.