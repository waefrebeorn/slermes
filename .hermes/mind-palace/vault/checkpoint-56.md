<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Checkpoint 56 — AG25 looks_like_codex_intermediate_ack ported

**Battleship:** v82→v83

## What was done

### AG25: looks_like_codex_intermediate_ack

- New function in `src/agent/agent_message_sanitize.c:752-849`
- Declared in `include/hermes_agent.h:231-236`
- Port of Python `agent/agent_runtime_helpers.py:looks_like_codex_intermediate_ack()`
- Detects planning/ack messages that should continue instead of ending the turn
- Logic:
  1. Return false if any tool-role message is present
  2. Strip think blocks, lowercase, check length ≤ 1200
  3. Check for future ack patterns: i'll, i will, let me, i can do that, i can help with that
  4. Check action markers: look into, inspect, scan, check, review, explore, read, etc.
  5. Check workspace markers: directory, repo, codebase, project, files, path, etc.
  6. Return true if assistant mentions action AND (user or assistant targets workspace)

### Build/Test
- Clean compile, 0 errors (pre-existing warning only)
- 4/4 tests passing

### Classification Changes
- AG25 agent_runtime_helpers: 7/24 → 8/24 functions ported (29% → 33%)