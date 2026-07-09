# Slermes v550 Session Prompt — REFACTOR-FIRST MONOLITH SPLIT (continued)
Branch: main (v549 HEAD pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo — NOT inside slermes/)
Date anchor: 2026-07-09

## Context (what v549 delivered)
v549 applied the NEW EDICT: stop dumping closures into monoliths. Demonstrated
the pattern by extracting the file-lint concern out of `port_file_operations.c`
(a 650+ line PENDING monolith w/ god header) into a self-contained module:
- `src/tools/file_lint.h` / `src/tools/file_lint.c` — opaque `file_lint_t`,
  minimal includes (libjson + hermes_logger + POSIX), ONE shared
  `lint_via_python()` helper reused by yaml/toml/python linters; JSON via strict
  `json_parse`.
- 4 linters re-implemented as REAL C (faithful to `tools/file_operations.py:
  _lint_*_inproc`); YAML/TOML/Python delegate to python3 running the SAME stdlib
  call (project's standalone libyaml/libtoml are intentionally lenient and do
  NOT reproduce PyYAML's strict safe_load — a lenient C lint would be a fidelity
  gap).
- Monolith `port_file_operations.c` shrank 707 → 591 lines; lint code + yaml/
  toml/wait includes removed.
- `lib/libtoml/toml.o` added to LIB_OBJ (was only a .a, never linked).
Build: clean (0 errors). Oracle (t_port_file_lint + sta_oracle_file_lint):
**20 fixtures, 0 mismatches** vs LIVE Python. mission8: 36/0/35.
Scanner: PORTED 4,881 / REAL_GAP 4,802 / PARTIAL 48 / STUB 0 / N/A 0 unchanged
(the 4 linters are NA_SDK infra — never in PORTED/REAL_GAP).

## The Mission (v550) — systematic refactor-first
Per the slermes-monolithic-refactor skill, BEFORE adding new functions to any
port_*.c > 400 lines, REFACTOR FIRST. Continue extracting cohesive concerns from
the PENDING monoliths into self-contained, opaque-struct, minimal-include, C11
modules. Priority targets (from the skill):
- `port_file_operations.c` (591, was 650) — remaining concerns to split:
  read/write paths, pagination helpers (`file_ops_normalize_*_pagination`,
  already PoP'd — verify they're real, not façades), BOM/encoding helpers,
  diff helpers. Each becomes its own module or joins a sibling.
- `port_browser_supervisor.c` (~500, "In a real implementation" stub @155) — FIX
  the stub by implementing it for real, or extract the real parts.
- `port_web_tools.c` (~450, "In a real implementation" @409) — same.
- `port_skills_sync.c` (~1600), `port_image_generation_tool.c` (~1400),
  `port_send_message_tool.c` (~480), `port_cronjob_tools.c` (~1000) — split by
  concern into focused modules.

## Hard rules (unchanged + reinforced)
- Opaque struct in .h, private fields in .c. NO `hermes.h` god header in port_*.c.
  NO `void*` passthrough params. NO "In a real implementation" / "not fully
  implemented" comments (implement the actual function).
- Every public function gets a `/* PoP: c @ module.py:_py */` annotation.
- Reuse: factor shared logic into static helpers (don't duplicate).
- For each closure: read the REAL Python, implement real C, ORACLE-verify
  (tests/t_port_*.c + tests/sta_oracle_*.py asserting C == LIVE Python, 0
  mismatches). Faithful beats clever: if the project's C lib is lenient where
  Python is strict (libyaml/libtoml), delegate to python3 for the strict check
  rather than ship a fidelity gap.
- `make slermes` 0 errors. `bash tests/run_mission8_tests.sh` → 36/35.
- v543–v546 oracle-verified leaf closures stay untouched.

## Done-when for v550
- At least 2 more PENDING monoliths have a cohesive concern extracted into a
  self-contained module (opaque struct, minimal includes, oracle-verified).
- Any "In a real implementation" stub found is implemented for real (not moved).
- 0 new god-header includes introduced; 0 void* passthrough params.
- Build clean, mission8 green, oracle 0 mismatches for new modules.

## Prestige (session end)
Update BANNER.md + docs/parity-summary.md + append v550 to STATE.md +
write NEXT_SESSION_PROMPT.md (v550→v551) + commit + push.
Report: modules extracted, stubs fixed, build/test/oracle, what's left.

## Copy-paste ready
Slermes v550: continue refactor-first — extract cohesive concerns from PENDING
monoliths (port_file_operations.c residual, port_browser_supervisor.c,
port_web_tools.c, port_skills_sync.c, etc.) into self-contained opaque-struct
C11 modules; implement any "In a real implementation" stubs for real; oracle-
verify each; build 0 errors, 36/35 mission8. No god headers, no void* passthrough.
