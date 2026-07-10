# 📋 Next-Session Prompt (copy-paste ready)
# Slermes v554 Session Prompt -- MONOLITH SPLIT CONTINUATION + FAITHFUL-FIDELITY
Branch: main (v553 HEAD pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo -- NOT inside slermes/)
Date anchor: 2026-07-09

## Context (what v553 delivered)
v553 continued the monolith split + closed a hidden faithful-port divergence:
- **NEW src/tools/file_fs_ops.{h,c}** — extracted the FS read/write/type cluster
  (read_file_raw, delete_path, python_delete, patch_replace, is_likely_binary,
  is_image, detect_file_line_ending, file_has_bom) out of port_file_operations.c.
  The monolith now holds thin delegates to file_fs_ops_* (no god header, focused
  includes, opaque struct stays private). file_fs_ops.o registered in objects.mk.
- **Faithful is_image** — now matches Python IMAGE_EXTENSIONS (added .ico).
- **Faithful is_likely_binary** — ported the real Python heuristic: ext in the
  full BINARY_EXTENSIONS set (tools/binary_extensions.py) OR >30% non-printable
  in first 1000 bytes. The old C only checked for a NUL byte (silent wrong
  result on binary-looking text).
- **Faithful detect_file_line_ending** — fixed a divergence the v551 oracle had
  PAPERED OVER: no-newline content (empty / single-line file) -> Python None ->
  "unknown"; old C returned "lf". Now "unknown". Updated the v551 oracle too.
- New oracle tests/t_port_file_fs_ops.c + sta_oracle_file_fs_ops.py: **18/0**.
- Build clean (0 errors/warnings on new modules); mission8 36/0/35.

### Hard-won lessons (do not repeat)
1. Implicit declaration in the oracle HARNESS (missing #include of the
   declaring header -> assumed int return vs real bool/_Bool) corrupts results
   and mimics a codegen bug. Always #include the module header under test.
2. Oracle maps that silently coerce a divergence to "match" (e.g. None->"lf")
   HIDE real bugs. The oracle must encode Python's EXACT behavior; when C
   disagrees, FIX THE C — don't patch the oracle to agree.

## v554 Mission (continue; do NOT regress the oracle gates)
Keep shedding monoliths. Remaining monoliths (line counts at v553 start):
- src/tools/port_browser_supervisor.c
- src/tools/port_web_tools.c
- src/tools/port_skills_sync.c
- src/tools/port_image_generation_tool.c
- src/tools/port_send_message_tool.c
- src/tools/port_file_operations.c — already shrunk (file_text_ops +
  file_ops_lint + file_fs_ops splits). Remaining cohesive cluster to extract:
  the pagination + command-exec + newline-error helpers (normalize_read/
  search_pagination, _exec, _has_command, is_line_oriented_newline_error,
  maybe_warn_line_oriented_newline_pattern, pattern_has_regex_newline) into
  file_pagination_ops.{h,c} / file_cmd_ops.{h,c}.

For EACH monolith you touch:
1. Identify a cohesive, oracle-verifiable concern (pure fns first, then FS/I-O
   fns with temp-file fixtures).
2. Extract into src/tools/<name>.{h,c} with opaque-or-stateless API, focused
   includes, NO hermes.h god header, NO void* passthrough, a /* PoP: c @
   module.py:_py */ on every public fn.
3. Factor shared logic into static helpers; delete the dead duplicate cluster
   from the monolith (leave thin delegates where the public symbol is still
   referenced elsewhere).
4. Register the new .o in build/objects.mk (TOOLS_OBJ).
5. Write tests/t_port_<name>.c + tests/sta_oracle_<name>.py proving
   C == LIVE Python (0 mismatches). Pin PYTHONHASHSEED=0 for set-order fns.
   Run against the REAL parent-repo Python. The oracle MUST encode Python's
   exact behavior — fix the C, never patch the oracle to agree.
6. `make slermes` 0 errors; `bash tests/run_mission8_tests.sh` -> 36 passed / 0 failed.

## Hard rules (unchanged + reinforced)
- Opaque struct in .h, private fields in .c. NO hermes.h god header in port_*.c.
  NO void* passthrough. NO "In a real implementation" / placeholder-success
  comments — implement the actual function or return an honest error.
- Every public function gets a /* PoP: c @ module.py:_py */ annotation.
- Reuse: factor shared logic into static helpers (don't duplicate).
- Oracle-verify C == LIVE Python before declaring done. Run the project's
  triple-DA + plumber fuzz where applicable.
- If a function is genuinely unimplementable in C (remote API, no real client),
  honestly demote (return honest error/NULL/claimed:false) — never fake success.

## Verification checklist before you push
- make slermes 0 errors
- bash tests/run_mission8_tests.sh -> 36 passed / 0 failed / 35 skipped
- All oracles 0 mismatches: file_text_ops (23/0), cron (19/0),
  file_ops_lint (11/0), file_fs_ops (18/0), plumber fuzz (1611/0)
- git commit + push origin main; record in STATE.md + BANNER.md
