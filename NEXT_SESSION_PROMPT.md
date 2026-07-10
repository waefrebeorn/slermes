# 📋 Next-Session Prompt (copy-paste ready)
# Slermes v556 Session Prompt -- MONOLITH SPLIT CONTINUATION (web/skills/image/send)
Branch: main (v555 HEAD b384e2e66c pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo -- NOT inside slermes/)
Date anchor: 2026-07-10

## Context (what v555 delivered)
v555 finished the port_browser_supervisor.c redaction split and closed hidden
faithful-port divergences against LIVE agent/redact.py:
- **NEW src/tools/browser_redact.{h,c}** — faithful POSIX-ERE port of
  agent.redact.redact_sensitive_text + redact_cdp_url (vendor prefixes partial
  mask; Authorization/x-api-key/DB-url/private-key full mask; telegram; bare-token
  URL; JWT partial; CDP-URL query-param + user:pass@). The old C fns were crude
  memset-to-* fakes covering only token=/user:pass@ — a silent fidelity gap;
  bodies removed, replaced by PoP delegates to the new module.
- **NEW src/tools/browser_supervisor_redact.{h,c}** — backend delegating to
  browser_redact.
- **NEW tests/t_port_browser_redact.c + sta_oracle_browser_redact.py: 22/0.**
- Push protection: fixtures use scanner-safe FAKE token shapes; no real secrets.

## Verification gates (ALL green at end v555 — do NOT regress)
- make slermes: clean, 0 errors (slermes 41M with whisper).
- bash tests/run_mission8_tests.sh -> 36 passed / 0 failed / 35 skipped.
- Oracles 0 mismatches: file_text_ops (23/0), cron (19/0), file_ops_lint (11/0),
  file_fs_ops (18/0), file_pagination_ops (22/0), browser_redact (22/0),
  plumber fuzz (1611/0).
- 0 STUB / 0 N/A.

## v556 Mission (continue the monolith-split discipline; do NOT regress gates)
port_file_operations.c is fully split (v552-v554) and browser redaction is split
(v555). Next monoliths to extract focused, oracle-verified modules from:
- src/tools/port_web_tools.c  (web_extract honest-demote done v550; extract the
  remaining real helpers — web_search sanitize, url-normalize, fetch headers, etc.)
- src/tools/port_skills_sync.c  (skill metadata, dedup, slugify — pure string/regex
  cluster; the v547 audit found these have NO C consumer, so read Python first:
  if genuinely C-unimplementable / no consumer, honestly demote rather than build
  speculative parallel infra — AGENTS.md bans that)
- src/tools/port_image_generation_tool.c  (provider dispatch is SDK-coupled; the
  pure prompt/seed normalizers may be extractable — verify per-function)
- src/tools/port_send_message_tool.c  (recipient/body shaping helpers; extract the
  pure ones)

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

## Hard-won lessons (carry forward)
1. Implicit declaration in oracle HARNESS (missing header include -> assumed int
   return vs real bool/_Bool) corrupts results. Always #include the declaring
   header in the harness.
2. Oracle maps that coerce a divergence to "match" HIDE bugs. Fix the C; never
   patch the oracle to agree. Exception: when Python's result is itself
   nondeterministic (set iteration / fresh-process vs continuing-process state in
   the instrumented agent.redact), assert the BEHAVIOR CONTRACT, not an exact
   value, and note it.
3. Rebuild the SPECIFIC .o you changed before relinking the oracle harness; a
   stale .o produces phantom mismatches.
4. POSIX-ERE GOTCHAS (bit v555 hard — record so v556 doesn't relearn):
   - `(?:...)` NON-capturing groups FAIL to compile under this glibc (regcomp
     error 13). Use capturing `(...)` and count groups, OR use the redact_subst()
     helper (src/tools/browser_redact.c) which takes a template with \1..\n
     backrefs + a \M masked-group sentinel — mirrors Python's re.sub lambdas and
     eliminates manual group-index counting.
   - Negated classes containing a POSIX class FAIL: `[^[:space:]]` and
     `[^ \t]`-style negated-with-whitespace error 13. Use literal `[^ \t\"']`
     (add a non-whitespace char so the negated class is accepted).
   - No lookbehind/lookahead: emulate `(?<!...)`/`(?!...)` boundaries with a
     manual char check.
5. COMMIT-PROTECTION: GitHub push protection blocks real-looking secret prefixes
   (sk-, ghp_, AKIA, xox, AIza) even inside «redacted:...» wrappers in test
   fixtures. Use scanner-safe FAKE shapes that still match the C regex branch
   (e.g. sk-ZZZ...(≥10 but <32 chars), ghp_ZZZ...(≥10 but <36), AIzaZZ...(30),
   slackplaceholder-...) — never commit a real token.

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
  (11/0), file_fs_ops (18/0), file_pagination_ops (22/0), browser_redact (22/0),
  plumber fuzz (1611/0)
- git commit + push origin main; record in STATE.md + BANNER.md
