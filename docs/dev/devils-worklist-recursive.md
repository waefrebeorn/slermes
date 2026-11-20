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

# Devil's-advocate RECURSIVE audit (auditing the auditor)

The first audit (`devils-worklist.md`) had blind spots. Found via online
research (mutation adequacy / oracle-gap literature, DIFUZZI differential
fuzzing, WFDB-x conformance porting) + ingenuity. All numbers measured.

## 1. Mutation adequacy of VERIFIED-GREEN oracles (the deepest check)

Research: coverage != adequacy; "executed but poorly checked" code survives
mutations (oracle gap). Mutated 4 green ports (13 mutants, single-occurrence
semantic edits, rebuild + re-run, source restored):

- ALL sanity mutants (early-return NULL) KILLED -> harnesses genuinely
  exercise the ports; no trace fabrication.
- gateway_flow: bump-threshold SURVIVED = EQUIVALENT mutant (float-repr
  precision loop 17->18; Python shortest-roundtrip never needs 18).
  alter-string SURVIVED = REAL HOLE: bool repr "False" (run_flow.c L82)
  never exercised at a compared position.
- credential_pool_flow: flip-eq0 SURVIVED = REAL HOLE: `if (digits == 0)`
  guard (L269) branch uncovered. alter-string SURVIVED = ARTIFACT (regex
  matched a string inside a COMMENT — mutation generator must skip comments).
- models_catalog_flow: flip-eq0 crashed the harness (rc139) = killed-by-crash.
- cch_flow: battery baseline LINK-FAIL (wrong minimal objs for this port —
  battery tooling needs per-port dep lists, not one-size libjson).

WORK: extend gateway_flow fixtures with bool-repr cases and
credential_pool_flow with digits==0 guard cases (kill the 2 real survivors);
harden the mutation generator (skip comments, flag equivalents, per-port
deps); make mutation adequacy a recurring gate for every green oracle.

## 2. Deep stub patterns the first audit's regex missed

First pass caught `return 0/NULL/""/false` only. Recursive pass added
empty-alloc returns (`strdup("")`, `json_object()`, `json_array()`,
`json_null()`, `calloc(...)`), echo-param returns, stub-call chains, #if 0:

- 1,570 candidates -> live AST behavior triple-check -> **1,220 unique
  GUILTY** (live does more), 24 faithful, 213 orphan tags.
- Largest cluster: **src/hermes_cli/web_server_wrappers.c — 331 guilty
  stubs in ONE file** (web dashboard/router surface).
- Total guilty stubs now: 503 (first pass) + 1,220 (deep) = 1,723.
- 8 revival batches PREPARED (scratch deep_ws_1..4.json 82/82/82/85,
  deep_rest_1..4.json ~208 each) — dispatch as slots free.

## 3. Zero-coverage claims

PoP-tagged functions in NO fixture, NO watercup gate (dir-name matched),
NO harness embedded table: raw 9,545 -> refined **1,837**. Claimed parity
with zero execution evidence. WORK: backfill >=5 cases/fn or strip tags;
new-port dispatch templates already require coverage — backfill is the gap.

## 4. DA-6: the scanner's universe was selection-biased (biggest find)

PYTHON_SOURCE_DIRS walked only agent/tools/gateway/cron/hermes_cli/
persistence/acp_adapter/hermes_platform. Modules entered the count only if
walked OR already PoP-claimed — so the LARGEST UNCLAIMED surfaces were
invisible: 627 quarry modules with >=5 defs absent, incl.
plugins/platforms/discord/adapter.py (391 defs), telegram (369), slack (329),
feishu (279), matrix (178), tui_gateway/* (2,392 defs total), pm (829),
session_rules (262), providers (111). plugins alone: 360 modules / 5,842 defs.

FIXED: added plugins, tui_gateway, providers, session_rules, pm,
hermes_agent, apps to PYTHON_SOURCE_DIRS. Honest re-scan running; the
denominator GROWS and the parity % DROPS — that is honesty, not regression.
The gap map must be regenerated from the new scan.

## 5. Smaller findings

- oracle_openai_routes_flow.py norm() has a str() fallback (type-collapse
  risk) — review against the projection contract.
- local_env_flow.jsonl: 43% of cases script raises — verify happy-path
  coverage is not drowned (DIFUZZI: "both failing" counts as agreement;
  raise-matches can mask dead happy paths). WORK: add per-oracle
  raise-parity accounting to the runner summary.
- 1 file with #if 0 block; 9 echo-param stubs (in the 1,220).
- Scanner py_func_count vs raw AST defs: -6.4% on sample (nested closures
  excluded — definitional, document it; not an error).

## Method notes (for the next recursive pass)

- Mutation battery: scratch/mutation_battery.py (restore-in-finally,
  one build at a time, unowned targets only).
- Deep triple-check: scratch/deep_triplecheck.py (live AST behavior score;
  trivial-return live fns are FAITHFUL, not lazy).
- Static recursive scan: scratch/recursive_audit.py.
- Research grounding: oracle-gap (coverage vs mutation score, NSF PAR
  10555509), Mu2 differential mutation testing (PAR 10444294), DIFUZZI
  (arXiv 2610.38337 — sim counts both-failing as agreement), WFDB-x
  conformance porting (arXiv 2611.01358 — randomized differential fuzzing
  beyond hand-written cases; every diff becomes a conformance case).
  NEXT level: coverage-guided differential fuzzing of the flow harnesses
  (random seam scripts) — hand-written fixtures are the current ceiling.
