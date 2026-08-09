# Battleship v671 — REAL_GAP Elimination Battleground

**Build:** Clean · 

## State

> Live counts are owned by `make parity-walkway` — the table below is the
> authoritative sentinel (do not hand-edit).

<!-- PARITY:AUTO -->
| PORTED  | 13,330 / 14,045 (94.9%) |
| REAL_GAP| 710 (5.1%) — no N/A |
| PARTIAL | 5 (0.0%) |
| BOOTLEG | 3 (recursive_false_gap_hunter.py) |

**Phase (v671):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 1,309 ahead / 853 behind upstream/main (last merge 2026-08-09 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-08-09T07:38:25Z from live scanner `tests/slermes_parity_battleground.py` — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->

**Rewriting from scratch in C is the point.** Stubs = REAL_GAP.

## What Changed (v399)
- All STUB classifications in scanner changed to REAL_GAP
- 3 sites in `classify_feature`: `classification="STUB"` → `"REAL_GAP"`, severity `MEDIUM` → `HIGH`
- Rationale: project goal is C rewrite, not stub generation. A stub that logs+returns NULL is not an implementation.

## Missions

> Mission counts below are computed from the live scanner (2026-08-09, v671) —
> per-directory REAL_GAP totals. Port files are the C homes for each mission.

### M1: CLI Core (283 REAL_GAPs, CLI + hermes_cli dirs)
**Priority: HIGHEST** — User-facing CLI commands
**Port files:** src/cli/hermes_cli_*.c, src/cli/port_cli.c
**Top modules:** hermes_cli/web_routers/profiles.py (15), sessions.py (14), cron.py (13),
sqlite_safe_read.py (13), skills.py (12), tools.py (12)

### M2: Gateway Platforms (79 REAL_GAPs, gateway dir)
**Priority: HIGH** — Platform adapters
**Port files:** src/cli/port_gateway_*.c, src/gateway/

### M3: Tools (246 REAL_GAPs, tools dir)
**Priority: MEDIUM** — Tool implementations
**Port files:** src/cli/port_tools_*.c, src/tools/
**Top modules:** tools/process_registry.py (32), cua_backend.py (14), doctor.py (14)

### M4: Agent (114 REAL_GAPs, agent dir)
**Priority: MEDIUM** — Agent runtime
**Port files:** src/cli/port_agent_*.c

### M5: Cron (19 REAL_GAPs, cron dir)
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
