# Battleship v399 — REAL_GAP Elimination Battleground

**Build:** Clean · **Version:** v414

## State

| Classification | Count | % |
|----------------|-------|---|
| ✅ PORTED       | 2,609 | 28.9% |
| 🔴 REAL_GAP     | 6,426 | 71.1% |
| ⚠️ PARTIAL      | 0 | 0.0% |
| ⚪ N/A          | 0 | 0.0% |
| 🔴 STUB         | 0 | 0.0% |

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
