# 📋 Next-Session Prompt (copy-paste ready)
# Slermes v555 Session Prompt -- MONOLITH SPLIT CONTINUATION (browser/web/skills)
Branch: main (v554 HEAD pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo -- NOT inside slermes/)
Date anchor: 2026-07-09

## Context (what v554 delivered)
v554 finished the port_file_operations.c split and closed more hidden
faithful-port divergences:
- **NEW src/tools/file_pagination_ops.{h,c}** — extracted normalize_read/
  search_pagination, is_line_oriented_newline_error, pattern_has_regex_newline,
  maybe_warn_line_oriented_newline_pattern. Monolith now holds thin delegates.
- **Faithful normalize_read_pagination** — Python clamps offset≥1, limit∈[1,2000];
  old C used offset≥0, cap 10000, and wrongly swapped default_limit for any
  non-positive limit. Fixed.
- **Faithful normalize_search_pagination** — offset≥0 (not ≥1), no upper cap.
- **Faithful is_line_oriented_newline_error** — exact pair check (literal "\n"
  is not allowed AND --multiline); old C matched loose keywords -> false positives.
- **Faithful pattern_has_regex_newline** — odd-backslash \n regex; old C matched
  bare $/^ (false positives) AND had an off-by-one in the backslash count that
  inverted odd/even. Fixed both.
- **Faithful maybe_warn_*** — only warns when total_count==0 AND regex-newline
  AND (no error OR line-oriented error); clears error + sets specific warning.
  Return type corrected to json_t*.
- **cron oracle flakiness fixed** (pre-existing, surfaced in v554 regression):
  check_invisible_unicode names whichever codepoint Python's invisible-char SET
  iterates first (PYTHONHASHSEED-dependent) -> nondeterministic. Oracle now
  asserts the behavior contract ("did it block?") for that fn. C was correct.
- New oracle tests/t_port_file_pagination_ops.c + sta_oracle_*: **22/0**.
- Build clean; mission8 36/0/35.

### Hard-won lessons (carry forward)
1. Implicit declaration in oracle HARNESS (missing header include -> assumed int
   return vs real bool/_Bool) corrupts results. Always #include the declaring
   header.
2. Oracle maps that coerce a divergence to "match" (e.g. None->"lf") HIDE bugs.
   Fix the C; never patch the oracle to agree. Exception: when Python's result is
   itself nondeterministic (set iteration), assert the BEHAVIOR CONTRACT, not an
   exact value (AGENTS.md: behavior contracts over snapshots) — and note it.
3. Rebuild the SPECIFIC .o you changed before relinking the oracle harness; a
   stale .o produces phantom mismatches. The run scripts in /tmp rebuild the .o
   first for exactly this reason.

## v555 Mission (continue; do NOT regress the oracle gates)
port_file_operations.c is now ~fully split (only patch_v4a [abstract in Python,
no-op passthrough is acceptable], _exec/_has_command [POSIX primitives, leave],
and thin delegates remain). Move to the next monoliths:
- src/tools/port_browser_supervisor.c  (already has CDP-client + 3 real fns from
  v550; extract the dialog/respond + runtime-eval + the remaining helpers into a
  focused module, keep the CDP-client header from v550)
- src/tools/port_web_tools.c  (web_extract honest-demote done v550; extract the
  remaining real helpers)
- src/tools/port_skills_sync.c
- src/tools/port_image_generation_tool.c
- src/tools/port_send_message_tool.c

For EACH monolith you touch:
1. Identify a cohesive, oracle-verifiable concern (pure fns first, then I/O fns
   with temp-file / fixture harnesses).
2. Extract into src/tools/<name>.{h,c} with opaque-or-stateless API, focused
   includes, NO hermes.h god header, NO void* passthrough, a /* PoP: c @
   module.py:_py */ on every public fn.
3. Factor shared logic into static helpers; delete the dead duplicate cluster
   from the monolith (thin delegates where the symbol is still referenced).
4. Register the new .o in build/objects.mk (TOOLS_OBJ).
5. Write tests/t_port_<name>.c + tests/sta_oracle_<name>.py proving C == LIVE
   Python (0 mismatches). Pin PYTHONHASHSEED=0 where set-order matters. Run
   against the REAL parent-repo Python. The oracle must encode Python's EXACT
   behavior; when C disagrees, FIX THE C.
6. `make slermes` 0 errors; `bash tests/run_mission8_tests.sh` -> 36 passed / 0 failed.

## Hard rules (unchanged)
- Opaque struct in .h, private fields in .c. NO hermes.h god header in port_*.c.
  NO void* passthrough. NO placeholder-success comments; implement or honestly
  demote (return honest error/NULL/claimed:false) — never fake success.
- Every public fn gets a /* PoP: c @ module.py:_py */ annotation.
- Reuse: factor shared logic into static helpers.
- Oracle-verify C == LIVE Python before declaring done.

## Verification checklist before you push
- make slermes 0 errors
- bash tests/run_mission8_tests.sh -> 36 passed / 0 failed / 35 skipped
- All oracles 0 mismatches: file_text_ops (23/0), cron (19/0), file_ops_lint
  (11/0), file_fs_ops (18/0), file_pagination_ops (22/0), plumber fuzz (1611/0)
- git commit + push origin main; record in STATE.md + BANNER.md
