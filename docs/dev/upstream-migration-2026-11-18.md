<!-- PARITY:AUTO -->
| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T17:36:51Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

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

## Oracle retool (the 17 post-migration suite failures, closed)
- **Interpreter**: the suite now prefers the quarry venv (`run_one_oracle.sh`
  picker) — the new `utils.files → hermes_yaml → ruamel` chain kills the bare
  hermes-tools python. `PYTHONIOENCODING=utf-8` pinned for oracle pipes.
- **AST seeders** (aux_task_builder, container_boot): seed module-level
  constants AND all private helpers, not a fixed list — upstream keeps
  adding them (`_INHERITABLE_CORE_BLOCKS`, `_strip_python_launcher`).
- **ccd**: the function gained `from hermes_cli.config import …`; the old
  non-package fake broke it → real modules under the venv.
- **utils.py source readers**: redirected per-def to `utils/env.py` /
  `utils/urls.py`; the `base_url_host_matches` stub transcribed from live.
- **hermes_state_keys**: runtime import split across
  `persistence.authority.runtime` + `persistence.sqlite.authority._runtime`.
- **cli_process_dock**: genuine upstream semantic change PORTED —
  `failed_start` removed from `_STATUS_BY_REASON`; failed start with no exit
  code now reads `done`.
- Path-string hazards: dotted-module rewrites must NOT touch file paths
  (one corruption repaired); subprocess loops must spawn
  `C:/msys64/usr/bin/bash.exe` explicitly — native `bash` is the WSL stub
  and returns UTF-16 error blobs (two false retest rounds before catching it).

## C tree mirror
`src/persistence/sqlite/_state/` (24 ports) → `src/persistence/sqlite/_state/`,
mirroring the quarry package. objects.mk, DEPS, 80 gate extra_src,
pyport_map, comment refs all rewritten; binary relinks green.

## More imported (link_dead_ports.py)
8 port objects linked into the binary (lsp_reporter/lsp_workspace,
portal_tags, prompt_caching, message_content, account_usage,
markdown_tables, memory_provider). Tool fixes: global-symbol-only collision
check (file-local statics falsely dropped ports) and prior-survivor
preservation across runs. ~134 port-style files still fail to COMPILE on
Windows (POSIX-only APIs: pthread_timedjoin_np, dirent d_type, …) — that is
the Windows porting backlog, measured not guessed.

## C tree mirror (reorganization to the new schema)
`scripts/reorg_c_mirror.py` moved **313 C files** so `src/` mirrors the
quarry's CURRENT package layout, driven by each file's PoP claims:
- `src/persistence/sqlite/_state/` → `src/persistence/sqlite/_state/` (exact
  mirror); admin ports → `persistence/sqlite/admin/`, state_repair likewise.
- `src/cli/port_*` hermes_cli claims → `src/hermes_cli/`; agent claims →
  `src/agent/`; tools claims → `src/tools/`; gateway platform claims →
  `src/gateway/platforms/` and `src/plugins/platforms/*` (upstream moved
  platforms under plugins).
- checkpoint ports → `src/agent/checkpoints/`, transports →
  `src/agent/transports/`, utils ports → `src/utils/`, cron → `src/cron/`,
  tui_gateway ports → `src/tui_gateway/`.
- 2,061 path references rewritten (objects.mk, DEPS, 22k gate.json
  extra_src/cflags, pyport_map, harness/oracle paths, C include forms);
  stale `.o` at old paths deleted (the FULL-link tier globs `src/**/*.o`);
  SRCINCS depth bumped 3 → 6 for the deeper mirror.
- **19 namesake collisions kept their old path** (two ports of one module,
  e.g. `src/hermes_cli/display.c` vs `src/agent/display.c`) — refs reverted,
  merge/dedupe is a follow-up cleanup. List: re-run the script (idempotent)
  to print them.
- Parallel porting agents' files were excluded from moves/rewrites.

## Backups taken before the pull
- Pre-migration watercup verdicts: scratch/checkpoint-2026-11-18/
  watercup-results-pre-migration.jsonl (23 streamed verdicts).
- Quarry pre-state recoverable at `944799abc0` (git).
- slermes checkpoint commits on main before the pull.

## Consequences (measured, not guessed — see sentinels after make parity)
- The 169-file `persistence/` package is NEW gap surface: the C
  `src/persistence/sqlite/_state/*` ports now correspond to
  `persistence/sqlite/_state/*`; the driver abstraction (core/, authority/)
  is unported territory. This is the next porting front.
- Content-changed functions (renames with similarity < 100%) may lose
  parity credit until the C catches up — the scanner reports this honestly.
- Fleet was killed for the pull (user-authorized); restart it AFTER the
  retool so gates target the new quarry.
