# Upstream migration checkpoint — 2026-11-18 (pluggable-DB day)

## Trigger
Nous announcement (teknium, Discord): Hermes' DB architecture is now
**pluggable** (future PostgreSQL and other drivers for scaled variants).
Warning: `main` channel users should back up their DB/hermes before
updating — "a bit riskier than usual". User directive: kill the fleet,
pull the quarry, retool — checkpoint day to migrate forward through the
upstream rename/reorganization of the state DB.

## What moved
- Quarry: `944799abc0` → `3371c03df6` (**+13,473 commits**, ff-only).
- **228 Python renames**, 150 deletions, 2,280 additions.
- New `persistence/` package (169 .py files): the pluggable-DB architecture
  (`persistence/sqlite/_state/*`, `admin/*`, `authority/*`, `core`,
  `conversation`, `gateway`, `housekeeping`, `workers`). The old root
  `hermes_state_*.py` modules moved into `persistence/sqlite/_state/` and
  `admin/`; `hermes_state.py` remains at the root.
- `tools/checkpoint_*` → `agent/checkpoints/*`; `tools/cronjob_*` → `cron/*`;
  `tools/send_message_*` → `gateway/outbound/*`;
  `hermes_cli/session_lost_and_found.py` → `persistence/sqlite/admin/`.
- **`utils.py` was SPLIT** into a `utils/` package (`env.py`, `files.py`,
  `urls.py`) whose `__init__.py` re-exports the public surface — every name
  the oracles/twins import from `utils` is still importable (verified
  programmatically against all 33 used names). Private helpers
  (`_hostname_of`) live in `utils.urls` now.
- New runtime surfaces: `acp_adapter/`, `hermes_platform/`.
- Below-threshold moves (git rename detection missed them): e.g.
  `tools/send_message_senders.py` → `gateway/outbound/senders.py` — fixed
  by hand in the oracle rebind list.

## Retool applied (scripts/migrate_upstream_renames.py)
| Surface | Changes |
|---|---|
| PoP annotations (src/include/lib) | 238 claims in 45 files → new module paths |
| sta_oracle imports | 9 import lines + dotted string refs (rebind lists, twins) |
| watercup gate.json `py_module` | 194 gates |
| watercup gate directory names | 96 dirs (real__/port__ prefixes re-keyed) |
| scripts/pyport_map.json | 11 path refs |
| battleground scanner | +persistence, +acp_adapter, +hermes_platform source dirs |
| home-dir junctions | ~/persistence, ~/acp_adapter, ~/hermes_platform → quarry |

The rename map is reproducible: `git diff --diff-filter=R --name-status
944799abc0..3371c03df6 -- '*.py'` in the quarry (saved under the session
scratch checkpoint dir).

## Backups taken before the pull
- Pre-migration watercup verdicts: scratch/checkpoint-2026-11-18/
  watercup-results-pre-migration.jsonl (23 streamed verdicts).
- Quarry pre-state recoverable at `944799abc0` (git).
- slermes checkpoint commits on main before the pull.

## Consequences (measured, not guessed — see sentinels after make parity)
- The 169-file `persistence/` package is NEW gap surface: the C
  `src/agent/hermes_state/*` ports now correspond to
  `persistence/sqlite/_state/*`; the driver abstraction (core/, authority/)
  is unported territory. This is the next porting front.
- Content-changed functions (renames with similarity < 100%) may lose
  parity credit until the C catches up — the scanner reports this honestly.
- Fleet was killed for the pull (user-authorized); restart it AFTER the
  retool so gates target the new quarry.
