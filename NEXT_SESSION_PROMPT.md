# Slermes v551 Session Prompt — RESIDUAL FACADE SWEEP + MONOLITH SPLIT
Branch: main (v550 HEAD bf8cce32ff pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo — NOT inside slermes/)
Date anchor: 2026-07-09

## Context (what v550 delivered)
v550 continued the NEW EDICT (no fake-success stubs; read Python, implement
real C or honestly demote). Fixed 3 genuine façades:
- `port_browser_supervisor.c`: `respond_to_dialog` + `evaluate_runtime` were
  fakes (logged, returned ok:true with no CDP call / '[Runtime evaluation
  result]'). Now send the REAL Page.handleJavaScriptDialog / Runtime.evaluate
  commands via the tree's existing CDP client (browser_cdp_tool__cdp_call).
- NEW `include/port_tools_browser_cdp_tool.h` — focused module header declaring
  the CDP client API (kills the implicit-declaration debt; supervisor calls it
  cleanly, no god header).
- `port_web_tools.c`: `web_extract_tool` was a FRAUD (success:true + fake
  '[Content extracted via ...]' content). Now returns honest per-URL
  success:false error + overall success:false. Python calls a remote provider
  API (firecrawl/tavily/exa) — C-unimplementable at call time without a real
  extract client; faking success was the violation.
Build: clean (0 errors). mission8: 36/0/35. Scanner unchanged (these are
infra/SDK-class, not in PORTED/REAL_GAP movement).

## Residual-façade sweep — remaining backlog (from v550 grep)
80 banned-phrase hits across 233 port_*.c. MOST ARE BENIGN (config/template
"placeholder" terminology, secret-value placeholder detection, comments about
unrelated Python core stubs). The GENUINE placeholder/stub returns needing
per-function Python adjudication (real gap vs implementable):
- src/cli/port_tools_read_terminal_tool.c:24 — "return a placeholder JSON"
- src/cli/port_main_na.c:48 — "create a placeholder" version
- src/cli/port_agent_plugin_llm.c:33 — "return a placeholder"
- src/cli/port_agent_copilot_acp_client.c:65 — "return a placeholder indicating the subprocess"
- src/cli/port_tools_environments_managed_modal.c:66,77,152 — "exec-placeholder"/"sandbox-placeholder"
- src/cli/port_tools_environments_modal_utils.c:155 — "exec-placeholder"
- src/cli/port_tools_video_generation_tool.c:14,16,221 — "Stub: provider resolution not ported yet" / result placeholder
- src/cli/port_tools_yuanbao_tools.c:19 — "Stub: adapter resolution not ported yet"
- src/tools/port_cronjob_tools.c:904,908,974 — "cronjob stub: dispatch not fully ported" / "execute_job_now: C port stub"
- src/tools/port_kanban_tools.c:156,164 — "stub that logs the attempt"
- src/tools/port_image_generation_tool.c:1412 — "deprecated stub" returning error (mostly honest)
- src/tools/port_process_registry.c:460 — "Not implemented for Windows in this port" (platform gap)

## The Mission (v551)
Per-function adjudication — for EACH genuine candidate above: read the real
Python, decide HONESTLY:
- Real work fallible in C with an available C backend/capability → implement
  for real (like the browser CDP fix).
- Genuinely C-unimplementable at call time (remote cloud/SDK, live browser
  session C doesn't reach, platform gap) → return an honest error / NULL
  (like the v547 SDK-getter → NULL truthful returns), NOT fake success.
- Delete any no-op that only hermes_log/(void)arg with no Python-equivalent
  work.

ALSO continue the refactor-first monolith split (v550's other half):
extract cohesive concerns from PENDING monoliths (port_file_operations.c
residual, port_browser_supervisor.c ~600, port_web_tools.c ~450,
port_skills_sync.c ~1600, port_image_generation_tool.c ~1400,
port_cronjob_tools.c ~1000, port_send_message_tool.c ~480) into self-contained
opaque-struct C11 modules. No god headers, no void* passthrough.

## Hard rules (unchanged + reinforced)
- Opaque struct in .h, private fields in .c. NO hermes.h god header in
  port_*.c. NO void* passthrough. NO "In a real implementation" / "not fully
  implemented" / placeholder-success comments — implement the actual function
  or return an honest error.
- Every public function gets a `/* PoP: c @ module.py:_py */` annotation.
- Reuse: factor shared logic into static helpers (don't duplicate).
- Per closure: read REAL Python, implement real C, ORACLE-verify (C == LIVE
  Python, 0 mismatches) where a live-equivalent exists. If the project's C lib
  is lenient where Python is strict, delegate to python3 for the strict check
  rather than ship a fidelity gap.
- make slermes 0 errors. bash tests/run_mission8_tests.sh → 36/35.
- v543–v546 oracle-verified leaf closures stay untouched.

## Done-when for v551
- Every genuine placeholder/stub return in the backlog above is adjudicated:
  implemented for real OR honestly demoted (no fake-success remains).
- At least 1 more PENDING monolith has a cohesive concern extracted into a
  self-contained module.
- Build clean, mission8 green, residual-façade grep shows 0 genuine fakes.

## Prestige (session end)
Update BANNER.md + docs/parity-summary.md + append v551 to STATE.md +
write NEXT_SESSION_PROMPT.md (v551→v552) + commit + push.
Report: facades fixed/demoted, modules extracted, build/test/oracle, what's left.

## Copy-paste ready
Slermes v551: adjudicate the residual-façade backlog (read each Python, implement
real C or honestly demote — no fake success) AND continue refactor-first
monolith split into self-contained opaque-struct C11 modules; build 0 errors,
36/35 mission8, no god headers, no void* passthrough.
