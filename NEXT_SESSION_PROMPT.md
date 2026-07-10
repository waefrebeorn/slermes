# 📋 Next-Session Prompt (copy-paste ready)
# Slermes v553 Session Prompt -- MONOLITH SPLIT CONTINUATION + ORACLE-DRIVEN FIDELITY
Branch: main (v552 HEAD pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo -- NOT inside slermes/)
Date anchor: 2026-07-09

## Context (what v552 delivered)
v552 addressed the 3 pre-existing HIGH devil's-advocate flags on v551's file-ops
cluster AND continued the monolith split:
- **`ft_normpath2` strcpy / unbounded memcpy** (`src/cli/port_file_tools_helpers.c`)
  → replaced with snprintf-bounded writes (real path-length overflow eliminated;
  `file_tools_is_blocked_device_path` results unchanged: /dev/zero=1, /proc/1/fd/0=1,
  home/x=0, /etc/passwd=0).
- **`file_ops_looks_like_linter_unusable` was a WRONG port** (ignored linter
  `base_cmd`, hard-coded two substrings). Faithfully ported
  `tools/file_operations.py:_looks_like_linter_unusable` (base_cmd-keyed
  `_LINTER_UNUSABLE_PATTERNS`: npx / rustfmt / go). **Extracted to its own
  focused module** `src/tools/file_ops_lint.{h,c}` — the v552 monolith split.
- **`file_ops_delete_path` missing write-deny guard** → added `is_write_denied(path)`
  (already ported in `src/agent/file_safety.c`); denied paths return false, matching
  Python's `delete_path → _is_write_denied` protection.
- New oracle `tests/t_port_file_ops_lint.c` + `sta_oracle_file_ops_lint.py`: **11/0**
  vs LIVE `tools/file_operations.py` + `agent.file_safety`.
- Build clean (0 errors); mission8 36/0/35.
- Regression gates hold: file_text_ops 23/0, cron 19/0, plumber fuzz 1611/0.

### Hard-won lesson from v552 (do not repeat)
A "C returns wrong results but identical isolated code is correct" symptom was
NOT a codegen bug — it was an **implicit function declaration in the TEST
HARNESS** (harness called `file_ops_looks_like_linter_unusable` without
including `file_ops_lint.h`, so the compiler assumed `int` return while the
real fn returns `bool`/`_Bool` 1-byte → calling-convention mismatch corrupted the
return value). ALWAYS `#include` the module header that declares the function
under test. Before assuming a codegen/UB bug, confirm the harness declares the
fn correctly and build with the project's exact CFLAGS (`-O2 -g ...`).

## v553 Mission (continue; do NOT regress the oracle gates)
Keep shedding monoliths. PENDING monoliths (line counts at v552 start):
- src/tools/port_browser_supervisor.c
- src/tools/port_web_tools.c
- src/tools/port_skills_sync.c
- src/tools/port_image_generation_tool.c
- src/tools/port_send_message_tool.c
- src/tools/port_file_operations.c — already shrunk (file_text_ops + file_ops_lint
  splits). Next cohesive cluster to extract: the FS read/write/patch ops
  (read_file_raw, delete_path, patch_replace, patch_v4a, is_likely_binary,
  is_image, detect_file_line_ending, file_has_bom) into `file_fs_ops.{h,c}`.

For EACH monolith you touch:
1. Identify a cohesive, oracle-verifiable concern (pure functions first).
2. Extract into `src/tools/<name>.{h,c}` with opaque-or-stateless API, focused
   includes, NO hermes.h god header, NO void* passthrough, a /* PoP: c @
   module.py:_py */ on every public fn.
3. Factor shared logic into static helpers; delete the dead duplicate cluster
   from the monolith (leave thin delegates where the public symbol is still
   referenced elsewhere).
4. Register the new .o in build/objects.mk (TOOLS_OBJ).
5. Write tests/t_port_<name>.c + tests/sta_oracle_<name>.py proving
   C == LIVE Python (0 mismatches). Pin PYTHONHASHSEED=0 for set-order fns.
   Run against the REAL parent-repo Python.
6. `make slermes` 0 errors; `bash tests/run_mission8_tests.sh` -> 36 passed / 0 failed.

## Hard rules (unchanged + reinforced)
- Opaque struct in .h, private fields in .c. NO hermes.h god header in port_*.c.
  NO void* passthrough. NO "In a real implementation" / placeholder-success
  comments — implement the actual function or return an honest error.
- Every public function gets a /* PoP: c @ module.py:_py */ annotation.
- Reuse: factor shared logic into static helpers (don't duplicate).
- Oracle-verify C == LIVE Python before declaring done. The plumber fuzz +
  devil_advocate + plumber_deep_dive tools are the project's DA gate — run them.
- If a function is genuinely unimplementable in C (remote API, no real client),
  honestly demote (return honest error/NULL/claimed:false) — never fake success.

## Verification checklist before you push
- make slermes 0 errors
- bash tests/run_mission8_tests.sh -> 36 passed / 0 failed / 35 skipped
- All oracles 0 mismatches: file_text_ops (23/0), cron (19/0), file_ops_lint (11/0),
  plumber fuzz (1611/0)
- python3 tests/plumber_deep_dive.py and tests/devil_advocate.py clean on new modules
- git commit + push origin main; record in STATE.md + BANNER.md
