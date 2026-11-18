# SLERMES PRECISION WORKFLOW

## Core Principle

<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

**Phase:** PORT — the C11 binary is the deliverable: faithful, oracle-verified,
usable standalone across operating systems. Closing REAL_GAPs is the path;
consumers (AGI-OS integration and friends) consume the binary, not the Python
tree. The mission covers the whole ecosystem — agent, tools, platforms,
plugins, and every third-party dependency — SLERMed from scratch in C11.

**Workflow:** the parity docs are fully programmatic. `make parity`
(scanner → watercup oracle → walkway generator) refreshes every PARITY:AUTO
block from live sources on every run. Never hand-edit the count block; the
"ahead/behind" sync figure is the staleness timer — when it is non-zero,
re-run the sync flow below.

## What Slermes is

Slermes is a C11 SLERM of the Hermes Python agent, maintained at
`waefrebeorn/slermes`. It is a ground-up reimplementation, not a wrapper: the
Python tree is the quarry (reference ground truth), and every feature is
rewritten in C11 and proven against the live Python by the oracle and
watercup gates.

- **Upstream** = `NousResearch/hermes-agent` (the Python quarry). The local
  quarry checkout on the dev box is `C:/Users/eman5/hermes-agent`; the
  scanner resolves the ground truth through the package links beside the
  repo. Never confuse the quarry with another Slermes source.
- **Port work lands on `integrate`** and pushes to `origin integrate`.
  `origin/main` carries the Python upstream history plus docs commits — do
  not push the port branch there.

## Sync workflow (canonical)

Pull the Hermes Python code into the quarry, then port the delta:

```bash
# STEP 1 — sync the quarry (Python ground truth)
cd C:/Users/eman5/hermes-agent
git fetch upstream main
git merge upstream/main --no-edit    # conflicts: upstream wins (it is the quarry)
# or use the repo's pull configuration:
bash C:/Users/eman5/slermes/scripts/sync-upstream.sh

# STEP 2 — re-scan: the new Python delta is now REAL_GAP work
cd C:/Users/eman5/slermes
make parity                          # scan -> watercup -> walkway, fail-closed

# STEP 3 — port the delta, one module at a time, on integrate
git checkout integrate
# ... implement + oracle + watercup per module, commit each bite ...
git push origin integrate
```

The C11 work is never pushed back to upstream. A reference helper moving or
disappearing in the quarry is the cause of most oracle drift — when an oracle
breaks after a sync, check the live Python first.

## Key Files

- `BANNER.md` — this workflow + the live parity sentinel
- `tests/slermes_parity_battleground.py` — the function-level scanner
- `scripts/parity_truth.py` — fail-closed wrapper; only writer of `live_parity_scan.json`
- `scripts/run_parity_workflow.py` — the `make parity` pipeline (scan → watercup → walkway)
- `scripts/gen_parity_walkway.py` — stamps every PARITY:AUTO block + regenerates `docs/real-gap-list.md`
- `tests/oracle/registry.json` — port → harness mapping for the diff oracle
- `tests/run_all_oracles.sh` — differential-oracle regression gate (authoritative DEPS table)
- `tests/run_one_oracle.sh` — single-port oracle runner
- `tests/watercup/wc_run.py` / `wc_run_parallel.py` — trace-cup deep gate (22,603 gates)
- `tests/verify_build_health.py` — compile + port-object + link gate
- `tests/oracle/results.jsonl` — stale-detection log (regression alerts)
- `tests/oracle/build/`, `tests/watercup/build/` — gate artifacts (never /tmp)

## Verification

```bash
# Build (Windows dev box; inside MSYS2 bash)
source build/winlibs-env.sh
make -j8 slermes USERPROFILE='C:\Users\<you>' LOCALAPPDATA='C:\Users\<you>\AppData\Local'

# Gates — read exit codes, never pipe through head/tail
python tests/verify_build_health.py --no-link
bash tests/run_all_oracles.sh            # ORACLE_JOBS=1 on MSYS2
bash tests/run_slv_tests.sh
make parity                              # refresh every derived number
```

A gate that did not run is not a pass. A harness that cannot execute is an
explicit skip with a reason, never a silent green.

## Stale Detection

The oracle runner appends verdicts to `tests/oracle/results.jsonl`. If a
previously-passing port flips to FAIL, it is flagged as a regression. The
watercup runner appends per-gate verdicts to `tests/watercup/build/results.jsonl`
(the parity pipeline truncates it first so a stale run cannot poison the
walkway stats). This prevents silent stale checks.
