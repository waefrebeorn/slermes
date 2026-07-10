# 📋 Next-Session Prompt (copy-paste ready)
# Slermes v557 Session Prompt -- MONOLITH SPLIT CONTINUATION (image/send + residual)
Branch: main (v556b HEAD 8326de1468 local; v555 pushed b384e2e66c + 1c5e9f20d4)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo -- NOT inside slermes/)
Date anchor: 2026-07-10

## Context (what v556 delivered)
v556 extracted TWO monolith clusters into oracle-verified focused modules:
- **v556a: web_base64_img** — faithful POSIX-ERE port of
  tools/web_tools.py:convert_base64_images_to_links (was a `strdup(text)`
  silent stub). Also replaced the inline SSRF `strncmp` block in
  web_extract_tool with a delegate to the EXISTING url_safety.c::url_is_safe()
  (no double-coding). Oracle 12/0.
- **v556b: skills_sync_fs** — consolidated dir_hash (MD5), safe_rel_install_path
  (traversal rejection), compute_relative_dest into a focused module.
  port_skills_sync.c now delegates (thin PoP delegates). Oracle 4/0.
  NOTE: lib/libskillsync/skills_sync.c ALSO has a skills_sync_dir_hash but it is
  NOT in objects.mk/Makefile (dead, unlinked) — left as-is this window; the
  live consolidated dir_hash is skills_sync_fs_dir_hash.
- Both builds clean, mission8 36/0, all oracles 0 mismatch.

## Verification gates (ALL green — do NOT regress)
- make slermes: clean, 0 errors.
- bash tests/run_mission8_tests.sh -> 36 passed / 0 failed / 35 skipped.
- Oracles 0 mismatches: file_text_ops (23/0), cron (19/0), file_ops_lint (11/0),
  file_fs_ops (18/0), file_pagination_ops (22/0), browser_redact (22/0),
  web_base64_img (12/0), skills_sync_fs (4/0), plumber fuzz (1611/0).
- 0 STUB / 0 N/A.

## v557 Mission (continue the monolith-split discipline; do NOT regress gates)
web_tools (v556a) and skills_sync (v556b) are DONE. Next monoliths to extract
focused, oracle-verified modules from:
- src/tools/port_image_generation_tool.c  (1508 lines; provider dispatch is
  SDK-coupled, but the pure prompt/seed/aspect-ratio normalizers + the FAKE-shape
  fixture guards may be extractable — read Python first, verify per-function)
- src/tools/port_send_message_tool.c  (886 lines; recipient/body shaping,
  platform formatting helpers — extract the pure ones)
Then residual-façade sweep on ~12 catalogued genuine stub returns (read_terminal_
tool, main_na, agent_plugin_llm, agent_copilot_acp_client, managed_modal/
modal_utils, video_generation, yuanbao, cronjob_tools, kanban_tools,
image_generation, process_registry) — each needs Python read to decide
implement-for-real vs honest demotion.

For EACH monolith you touch:
1. Identify a cohesive, oracle-verifiable concern (pure fns first).
2. Extract into src/tools/<name>.{h,c} with opaque-or-stateless API, focused
   includes, NO hermes.h god header, NO void* passthrough, a /* PoP: c @
   module.py:_py */ on every public fn.
3. Factor shared logic into static helpers; delete the dead duplicate cluster
   from the monolith (thin delegates where the symbol is still referenced).
   REUSE existing focused modules (url_safety.c, image_gen*.c, skills_guard.c,
   skills_sync_fs.c) — never re-inline logic already ported.
4. Register the new .o in build/objects.mk (TOOLS_OBJ).
5. Write tests/t_port_<name>.c + tests/sta_oracle_<name>.py proving C == LIVE
   Python (0 mismatches). Run against the REAL parent-repo Python. When C
   disagrees, FIX THE C.
6. `make slermes` 0 errors; `bash tests/run_mission8_tests.sh` -> 36 passed / 0 failed.

## Hard-won lessons (carry forward)
1. Implicit declaration in oracle HARNESS (missing header include -> assumed int
   return vs real bool/_Bool) corrupts results. Always #include the declaring
   header in the harness.
2. Oracle maps that coerce a divergence to "match" HIDE bugs. Fix the C; never
   patch the oracle to agree. (Exception: Python nondeterministic under
   instrumentation -> assert the BEHAVIOR CONTRACT, note it.)
3. Rebuild the SPECIFIC .o you changed before relinking the oracle harness; a
   stale .o produces phantom mismatches.
4. POSIX-ERE GOTCHAS: `(?:...)` NON-capturing groups FAIL (regcomp error 13) ->
   capturing `(...)` + redact_subst() helper; negated POSIX classes FAIL ->
   literal `[^ \\t\\\"']`; no lookbehind/lookahead (emulate with manual check).
5. COMMIT-PROTECTION: GitHub push protection blocks real-looking secret prefixes
   (sk-, ghp_, AKIA, xox, AIza) even inside «redacted:...» wrappers. Use
   scanner-safe FAKE shapes (sk-ZZZ..., ghp_ZZZ..., AIzaZZ...,
   slackplaceholder-...); never commit a real token.
6. PoP DEAD-HYBRID trap: `/* PoP: c @ m.py:f` with NO closing `*/` on the same
   line (the `*/` on a following ` * desc */` line) matches NEITHER scanner
   regex -> phantom REAL_GAP despite c_func existing. ALWAYS single-line
   `/* PoP: c @ m.py:f */` (with `*/`) or proper `/*` + ` * PoP:` block.
   Re-run the parity scanner after every PoP edit.
7. Oracle HARNESS JSON escaper: use a fixed buffer capacity constant, NOT
   `sizeof(b)` where `b` is a `char*` (pointer -> 8, truncates strings to ~3
   chars). Use `g_js[4][CAP]` + loop bound `j < CAP-4`.
8. Double-coding avoidance: before extracting a helper, grep the whole tree
   (incl. lib/libskillsync, lib/*) for an existing port; delegate to it instead
   of re-inlining. The v543 parallel-dupe-file trap + v556 skills_sync dir_hash
   duplicate are the cautionary cases.

## Hard rules (unchanged)
- Opaque struct in .h, private fields in .c. NO hermes.h god header in port_*.c.
  NO void* passthrough. NO placeholder-success comments; implement or honestly
  demote (return honest error/NULL/claimed:false) — never fake success.
- Every public fn gets a /* PoP: c @ module.py:_py */ annotation.
- Reuse: factor shared logic into static helpers / existing modules.
- Oracle-verify C == LIVE Python before declaring done.

## Verification checklist before you push
- make slermes 0 errors
- bash tests/run_mission8_tests.sh -> 36 passed / 0 failed / 35 skipped
- All oracles 0 mismatches (see gates above)
- git commit + push origin main; record in STATE.md + BANNER.md
