# Slermes v622 Session Prompt — RESUME PUSH + CONTINUE GAP CLOSURE

Branch: main (v621 HEAD 68f30e842c recovered; v620 = f0c7ebf3955)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo — NOT inside slermes/)
Date anchor: 2026-07-23

## Context (what v621 delivered / recovered)
The prior auto-pilot session committed real work but crashed BEFORE it could
(a) renumber 49 placeholder `vXXX`/`v572` commit messages, (b) refresh
BANNER/STATE/parity-summary, and (c) push. v622's recovery step:

- Restored byte-identical history via `git filter-repo` `--commit-callback`,
  renumbering the 49 mislabeled commits to **v573..v621** (monotonic; the 5
  descriptive intermediate commits like `port_web_server_auth:` / `pop
  annotations:` were kept verbatim).
- The 49 renumbered commits cover: god-header purge (hermes.h umbrella off all
  sources), web_server.py / weixin.py heavy pure-helper ports, batch PoP
  annotation closure (hundreds of REAL_GAP/ PARTIAL solved by annotation
  matching), and 14 local oracle-verified REAL_GAP closures (v608..v621:
  model_normalize, provider_catalog, skills_tool, dashboard_auth/cookies,
  kanban_diagnostics, moa_config, azure_detect, fallback_cmd, session_recap,
  middleware, lazy_deps, curses_ui, security_audit, doctor) — net 75 RG closed.
- Live parity (this session): **PORTED 6,434 / REAL_GAP 3,299 / PARTIAL 0**
  (total 9,733). Build: clean. mission8: 36/0/35.
- Docs synced (BANNER, parity-summary, STATE v621 checkpoint). NO push was
  done — see PUSH below (the fork `origin` remote is missing from .git/config;
  only `upstream`=NousResearch/hermes-agent is configured, and local `main`
  diverges from real upstream/main by 14k/17k commits — a fork branch, NOT to
  be force-pushed to upstream/main).

## PUSH (blocked on user decision — do NOT push to upstream/main)
On resume, confirm the fork remote with the user:
- Preferred: re-add `origin` = github.com/waefrebeorn/hermes-agent.git and push
  `main` there (the slermes fork's canonical push target).
- NEVER `git push upstream main` (would corrupt upstream).
- If upstream is the only option, push to a dedicated `slermes` branch only.
- Safety: `backup-pre-recovery` branch holds the pre-filter-repo HEAD.

## Continue gap closure (v622+)
With the recovery merged, resume the honest REAL_GAP closure doctrine:
- Reuse the oracle harness (tests/oracle/ + run_one_oracle.sh); add fixtures
  under tests/oracle/fixtures/<port>/, never scatter inputs in /tmp.
- Pick the next dense PURE-helper Python modules (no IO/network/DB coupling)
  and port faithfully, ORACLE-verify (C == LIVE Python, 0 mismatches).
- For each close: promote existing static helper to non-static + `/* PoP: c_func
  @ module.py:_py_func */`, or read REAL Python and implement real C; never
  fake success.
- Reject façades: no "not implemented" placeholders, no void* passthrough, no
  god headers in port_*.c.

## Hard rules (unchanged)
- make slermes 0 errors. run_mission8_tests.sh → 36/0/35.
- Live scanner is truth: `make parity-walkway` → "Live: PORTED ... REAL_GAP ...
  PARTIAL ...". Sum the `real_gaps` key (PLURAL) — summing `real_gap` yields 0.
- No god header; opaque struct in .h, private fields in .c.
- Every public function gets a PoP annotation. Reuse, don't duplicate.

## Done-when for v622
- Push target resolved + renumbered history pushed to the fork (NOT upstream/main).
- ≥1 more honest REAL_GAP batch closed (oracle-verified) and docs updated.
- Build clean, mission8 green, parity summary regenerated to live numbers.

## Prestige (session end)
Update BANNER.md + docs/parity-summary.md + append v622 to STATE.md + write
NEXT_SESSION_PROMPT.md (v622→v623) + commit + push.
Report: push target used, gaps closed, build/test/oracle, remaining RG.

## Copy-paste ready
Slermes v622: resolve fork-push target (never upstream/main), push the
renumbered v573..v621 history, then resume honest oracle-verified REAL_GAP
closure on dense pure Python helpers; build 0 errors, 36/0/35 mission8, no
god headers, no fake success.
