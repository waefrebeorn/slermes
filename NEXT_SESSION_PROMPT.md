# 📋 Next-Session Prompt (copy-paste ready)
# Slermes v552 Session Prompt -- MONOLITH SPLIT CONTINUATION + ORACLE-DRIVEN FIDELITY
Branch: main (v551 HEAD pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo -- NOT inside slermes/)
Date anchor: 2026-07-09

## Context (what v551 delivered)
v551 ran the residual-façade adjudication (the v550 ~12 backlog) AND continued
the refactor-first monolith split:
- **Residual façades:** read REAL Python for each of the ~12 candidates; 11/12
  honestly demoted (no fake success — returned honest error/NULL/claimed:false);
  1 retained as a genuine platform gap (process_registry Windows branch).
  0 genuine fakes remain (banned-phrase grep: 15 benign anti-placeholder /
  secret-detection hits only).
- **Monolith split x2 (oracle-verified):** `cron_prompt_sanitize.{h,c}` and
  `file_text_ops.{h,c}` extracted as self-contained C11 modules. Each is
  verified C == LIVE Python (0 mismatches): cron oracle 19/0, file_text oracle
  23/0. The oracles caught and we fixed REAL bugs in both (ZWJ byte-offset
  decode, dangling pointer, CSI over-strip, detect_line_ending lone-CR,
  add_line_numbers gutter, escape_shell_arg quoting, parse_search_context_line
  regex, threat-matcher fidelity, GitHub-auth strip, frozenset iteration order).
- Build: clean (0 errors). mission8: 36 passed / 0 failed / 35 skipped.
- Scanner PORTED/REAL_GAP unchanged (demotions + splits are SDK/platform-class,
  classified NA_SDK by the scanner).

## v552 Mission (continue; do NOT regress the v551 oracle gates)
The codebase must keep shedding monoliths. Remaining PENDING monoliths
(line counts at v551 start):
- src/tools/port_browser_supervisor.c (~609)
- src/tools/port_web_tools.c (~451)
- src/tools/port_skills_sync.c (~1723)
- src/tools/port_image_generation_tool.c (~1508)
- src/tools/port_send_message_tool.c (~886)
- src/tools/port_file_operations.c — already shrunk by file_text_ops split
  (~30 -> ~19 fns); extract the next cohesive cluster (file read/write/patch
  ops: read_file_raw, delete_path, patch_replace, patch_v4a, is_likely_binary,
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
5. Write a tests/t_port_<name>.c + tests/sta_oracle_<name>.py that proves
   C == LIVE Python (0 mismatches). Pin PYTHONHASHSEED=0 for any set-order-
   dependent fn. Run `python3 <module>.py` against the REAL parent-repo Python.
6. `make slermes` 0 errors; `bash tests/run_mission8_tests.sh` -> 36 passed / 0 failed.

## Hard rules (unchanged + reinforced)
- Opaque struct in .h, private fields in .c. NO hermes.h god header in port_*.c.
  NO void* passthrough. NO "In a real implementation" / "not fully implemented"
  / placeholder-success comments — implement the actual function or return an
  honest error.
- Every public function gets a /* PoP: c @ module.py:_py */ annotation.
- Reuse: factor shared logic into static helpers (don't duplicate).
- Per closure: read REAL Python, implement real C, ORACLE-verify (C == LIVE
  Python, 0 mismatches). If the project's C lib is lenient where Python is
  strict, delegate to python3 for the strict check rather than ship a fidelity
  gap.
- The v551 oracles (tests/sta_oracle_cron_prompt_sanitize.py,
  tests/sta_oracle_file_text_ops.py) MUST keep passing (0 mismatches) after
  your changes — re-run them in CI before commit.
- make slermes 0 errors. bash tests/run_mission8_tests.sh -> 36/0.
- v543-v546 oracle-verified leaf closures stay untouched.

## Done-when for v552
- At least 2 more PENDING monoliths have a cohesive concern extracted into a
  self-contained module (oracle-verified, 0 mismatches).
- All new + existing oracles pass (0 mismatches).
- Build clean, mission8 green, residual-façade grep shows 0 genuine fakes.
- process_registry Windows branch: either implement the real Windows path or
  leave an honest platform-gap comment (no fake success).

## Prestige (session end)
Update BANNER.md + docs/parity-summary.md + append v552 to STATE.md +
write NEXT_SESSION_PROMPT.md (v552->v553) + commit + push.
Report: modules extracted, oracle results (cases/mismatches), build/test,
what's left.

## Copy-paste ready
Slermes v552: continue refactor-first monolith split into self-contained
opaque-struct/oracle-verified C11 modules (no god headers, no void*
passthrough, every fn PoP-annotated); keep build 0 errors, 36/0 mission8, and
the v551 oracles at 0 mismatches.

The prompt is also saved at NEXT_SESSION_PROMPT.md (repo) and
/home/wubu/NEXT_SESSION_PROMPT.md.
Goal status: NOT complete — the monolith-split continuation (4+ pending
monoliths) and the v552 oracle gates are explicitly carried to v552.
