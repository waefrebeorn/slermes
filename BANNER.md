# SLERMES PRECISION WORKFLOW

## Core Principle

<!-- PARITY:AUTO -->
**v668 phase — PORT:** the C11 binary is the deliverable — faithful, oracle-verified, usable standalone across operating systems. Live parity counts live in the PARITY:AUTO sentinel blocks owned by `make parity-walkway`; never hand-edit a count.

**Upstream sync checkpoint:** 1,206 ahead / 0 behind upstream/main (last merge 2026-08-04 (upstream fetched)). The repo is up to date with upstream.

<!-- /PARITY:AUTO -->

Slermes is a C11 translation of the Hermes Python agent, maintained as a
**fork** of `NousResearch/hermes-agent` at `waefrebeorn/slermes`. The
forking model means slermes diverges from upstream Python — it does NOT
share git history with the upstream repo. The divergent count is the
snapshot timer of the last sync point.

## Stash → Pull → Fix → Pop Workflow

This is the **exact workflow** used every time upstream gets new updates:

```bash
# 1. STASH — save our local work before pulling
git stash

# 2. PULL — fetch upstream changes (rebase or merge)
git fetch upstream main
git rebase upstream/main    # or: git merge upstream/main

# 3. FIX — resolve any conflicts, re-run tests
make clean && make
bash tests/oracle/run_oracles.sh --check

# 4. POP — restore our stashed work on top
git stash pop

# 5. VERIFY — confirm everything still works
make clean && make
bash tests/oracle/run_oracles.sh --check

# 6. PUSH — push to our fork
git push origin main
```

## The Divergent Count

The divergent count between `origin/main` (our fork) and `upstream/main`
(the Hermes Python quarry) is the **snapshot timer** of the last sync.

- **0 commits ahead of origin/main, 0 commits behind** — our fork is
  perfectly synced with our own pushed state
- **N commits behind upstream/main** — this is expected and represents
  the delta between the Python quarry and our C11 port since the last
  pull. This is NOT a defect — it's the measure of work remaining.

After a stash→pull→pop cycle, the diverging count resets to reflect the
new upstream state. The count going down over time means we're closing
the gap.

## Key Files

- `BANNER.md` — project banner showing build status and parity numbers
- `tests/oracle/registry.json` — agnostic port→harness mapping
- `tests/oracle/run_oracles.sh` — project-wide oracle runner
- `tests/oracle/runners/run_oracle.sh` — per-port oracle runner
- `tests/oracle/results.jsonl` — stale detection log (regression alerts)
- `tests/oracle/build/` — oracle artifacts (never /tmp)

## Verification

```bash
# After push, verify HEAD == origin/main (0 ahead, 0 behind our fork)
git log --oneline origin/main..HEAD  # should be 0
git log --oneline HEAD..origin/main  # should be 0
```

## Stale Detection

The oracle runner (`run_oracles.sh`) appends verdicts to
`tests/oracle/results.jsonl`. If a previously-passing port flips to
FAIL, it's flagged as a regression. This prevents silent stale checks.