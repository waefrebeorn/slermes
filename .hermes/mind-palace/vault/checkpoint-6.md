# Checkpoint 6 — Vault Entry

**Gaps closed:** 3 (SE06, GW15, SK07)

## SE06: Handoff System — PORTED
- `src/cli/commands.c:3388-3618` — Full request/claim/complete flow
- `handoff_write_request()` — creates JSON handoff files in ~/.hermes/handoffs/
- `handoff_read_dir()` — scans handoff directory, parses entries
- `cmd_handoff()` — subcommand dispatch: request, claim, complete, status, list
- `handoff_entry_t` and `list_t` types for entry management
- Fixed duplicate struct definitions and forward declaration ordering

## GW15: Session Sources LRU Cache — PORTED
- `src/gateway/server.c:1143-1260` — LRU cache (512 entries)
- `source_cache_get()` — LRU lookup with MRU promotion on hit
- `source_cache_put()` — insert/update with LRU eviction at capacity
- `gw_session_get_source()` — public API: cache-first, falls back to session pool
- Wired into `gw_session_set_source()` — populates cache on source set
- `gateway_state_t.source_cache[512]` with mutex, count, max fields
- Initialized in gateway startup
- Mirrors Python `gateway/run.py` `_session_sources` OrderedDict pattern

## SK07: Skill Config Injection — PORTED
- `src/agent/skill_commands.c:58-220` — Config var extraction and injection
- `skill_extract_config_vars_from_file()` — parses metadata.hermes.config from SKILL.md frontmatter using line-by-line parsing (indentation-tracking state machine for metadata→hermes→config nesting)
- `skill_config_inject()` — resolves skills.config.* from config.yaml using yaml_get_string()
- Path value expansion: `~` → HOME, `${VAR}` → getenv(VAR) — matches Python expanduser/expandvars
- `skill_cmd_entry_t.config_vars[8]` — stores key, description, default_val, resolved value
- Called from `scan_one_skills_dir()` after frontmatter parsing; inject called after all dirs scanned
- Mirrors Python `agent/skill_utils.py` `extract_skill_config_vars()` + `resolve_skill_config_values()`

## DA Issues Found and Fixed
- **SK07 missing expanduser/expandvars:** Initial implementation didn't expand `~` and `${ENV}` in path values. Fixed to match Python behavior.

## Files Modified (9 total)
- `src/cli/commands.c` — SE06 handoff system
- `src/gateway/server.c` — GW15 LRU cache
- `include/hermes_gateway.h` — source_cache struct, gw_session_get_source() declaration
- `src/agent/skill_commands.c` — SK07 config injection
- `include/hermes_skill_commands.h` — config_vars[8] on skill_cmd_entry_t
- `battleship-v40.md` — SE06/GW15/SK07 → PORTED, checkpoint 6
- `state.md` — checkpoint 6, 3 new completions
- `plan.md` — checkpoint 6
- `vault/checkpoint-6.md` — this file
