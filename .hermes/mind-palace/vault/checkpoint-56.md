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