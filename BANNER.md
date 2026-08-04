# SLERMES PRECISION WORKFLOW

## Core Principle

<!-- PARITY:AUTO -->
**v669 phase — PORT:** the C11 binary is the deliverable — faithful, oracle-verified, usable standalone across operating systems. Live parity counts live in the PARITY:AUTO sentinel blocks owned by `make parity-walkway`; never hand-edit a count.

**Upstream sync checkpoint:** 1,210 ahead / 0 behind upstream/main (last merge 2026-08-04 (upstream fetched)). The repo is up to date with upstream.

<!-- /PARITY:AUTO -->

Slermes is a C11 translation of the Hermes Python agent, maintained as a
**fork** of `NousResearch/hermes-agent` at `waefrebeorn/slermes`. The
forking model means slermes diverges from upstream Python — it does NOT
share git history with the upstream repo. The divergent count is the
snapshot timer of the last sync point.

## Fork Sync Workflow (canonical, v669)

This is the **exact workflow** used every time upstream gets new updates —
pull the hermes code, slap slermes back on, stamp the banner checkpoint:

```bash
# STEP 1 — PULL the hermes (Python) code into OUR fork (parent = Python quarry)
cd /home/wubu/hermes-agent-dev
git fetch upstream main
git merge upstream/main --no-edit            # conflicts: upstream wins (quarry)
git rev-list --count HEAD..upstream/main      # must be 0
# ⚠️ Do NOT push the parent — its origin/main is the same physical GitHub repo
#    as slermes (fork-object-sharing hazard). Keep the quarry sync local.

# STEP 2 — SLAP SLERMES BACK ON: upstream/main becomes an ancestor, C11 tree kept
cd /home/wubu/hermes-agent-dev/slermes
git branch -f backup-pre-forkfix HEAD         # safety net
git fetch upstream main
git merge -s ours upstream/main --no-edit     # behind=0, tree byte-identical
git rev-list --count HEAD..upstream/main      # must be 0

# STEP 3 — BANNER CHECKPOINT + PUSH
make parity-walkway    # stamps "X ahead / 0 behind … up to date with upstream"
git add BANNER.md ROADMAP.md docs/parity-summary.md scripts/gen_parity_walkway.py .hermes/
git commit -m "vNNN: behind=0 reconciliation — fork-base merge, banner up to date"
git push origin main
```

Verify: `gh api repos/waefrebeorn/slermes/compare/NousResearch:main...main` →
`behind_by: 0`. The banner says "up to date" only when behind==0 (the generator
stamps that phrase automatically; never hand-edit it).

## The Divergent Count

The divergent count between `origin/main` (our fork) and `upstream/main`
(the Hermes Python quarry) is the **snapshot timer** of the last sync.

- **0 commits behind upstream/main** — the goal state after every sync.
  Achieved via the canonical fork-sync flow above (`-s ours` fork-base
  merge makes upstream/main an ancestor; our C11 tree stays byte-identical).
  GitHub then reports `behind_by: 0` and the banner says "up to date".
- **N commits ahead of upstream/main** — our C11 work on top of the
  Python quarry. This is expected and normal fork state.

If the behind-count is ever non-zero, the sync was not completed — re-run
the canonical 3-step flow (it is idempotent; the `-s ours` merge returns
behind to 0 and the banner re-stamps).

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