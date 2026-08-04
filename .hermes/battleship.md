# Battleship v669 — REAL_GAP Elimination Battleground

**Build:** Clean · 

## State

> Live counts are owned by `make parity-walkway` — the table below is the
> authoritative sentinel (do not hand-edit).

<!-- PARITY:AUTO -->
| PORTED  | 12,085 / 14,045 (86.0%) |
| REAL_GAP| 1,958 (13.9%) — no N/A |
| PARTIAL | 2 (0.0%) |
| BOOTLEG | 7 (recursive_false_gap_hunter.py) |

**Phase (v669):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,211 ahead / 4 behind upstream/main (last merge 2026-08-03 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-04T05:30:26Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->

**Rewriting from scratch in C is the point.** Stubs = REAL_GAP.

## What Changed (v399)
- All STUB classifications in scanner changed to REAL_GAP
- 3 sites in `classify_feature`: `classification="STUB"` → `"REAL_GAP"`, severity `MEDIUM` → `HIGH`
- Rationale: project goal is C rewrite, not stub generation. A stub that logs+returns NULL is not an implementation.

## Missions

### M1: CLI Core (2,632 REAL_GAPs, 161 modules)
**Priority: HIGHEST** — User-facing CLI commands
**Port files:** src/cli/hermes_cli_*.c, src/cli/port_cli.c

### M2: Gateway Platforms (1,854 REAL_GAPs, 56 modules)
**Priority: HIGH** — Platform adapters
**Port files:** src/cli/port_gateway_*.c

### M3: Tools (1,491 REAL_GAPs, 95 modules)
**Priority: MEDIUM** — Tool implementations
**Port files:** src/cli/port_tools_*.c

### M4: Agent (363 REAL_GAPs, 59 modules)
**Priority: MEDIUM** — Agent runtime
**Port files:** src/cli/port_agent_*.c

### M5: Cron (86 REAL_GAPs, 6 modules)
**Priority: LOW** — Cron management
**Port files:** src/cli/port_cron_*.c

## Resolution Protocol
1. Pick next module from roadmap (lowest gap count first)
2. For each REAL_GAP function:
   a. Read Python source
   b. Implement real C logic (not hermes_log + return NULL)
   c. Build, test, commit
3. Scanner re-run: verify PORTED++ REAL_GAP--
4. Move to next module

No choices. No questions. Never stop between gaps.

Scanner: `tests/slermes_parity_battleground.py` — 9,035 features across 377 modules.
