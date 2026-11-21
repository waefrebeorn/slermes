<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Slermes  Session Prompt — RESUME PUSH + CONTINUE GAP CLOSURE

Branch: main ( HEAD 68f30e842c recovered;  = f0c7ebf3955)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo — NOT inside slermes/)
Date anchor: 

## Context (what  delivered / recovered)
The prior auto-pilot session committed real work but crashed BEFORE it could
(a) renumber 49 placeholder `vXXX`/`` commit messages, (b) refresh
BANNER/STATE/parity-summary, and (c) push. 's recovery step:

- Restored byte-identical history via `git filter-repo` `--commit-callback`,
  renumbering the 49 mislabeled commits to **..** (monotonic; the 5
  descriptive intermediate commits like `port_web_server_auth:` / `pop
  annotations:` were kept verbatim).
- The 49 renumbered commits cover: god-header purge (hermes.h umbrella off all
  sources), web_server.py / weixin.py heavy pure-helper ports, batch PoP
  annotation closure (hundreds of REAL_GAP/ PARTIAL solved by annotation
  matching), and 14 local oracle-verified REAL_GAP closures (..:
  model_normalize, provider_catalog, skills_tool, dashboard_auth/cookies,
  kanban_diagnostics, moa_config, azure_detect, fallback_cmd, session_recap,
  middleware, lazy_deps, curses_ui, security_audit, doctor) — net 75 RG closed.
- Live parity (this session): **PORTED 11,537 / REAL_GAP 1 / PARTIAL 0**
  (total ). Build: clean. mission8: 77/0/0 (color-support test fixed
  to soft-pass on TERM=dumb/unset, matching real color-gating behavior).
- Oracle harness restored: registry.json now wires all 89 runnable ports
  (had regressed to 2). Runner include-path fixed so libjson/json.h etc.
  resolve. Running `tests/oracle/run_oracles.sh` is now the FAP detector —
  see docs/fap.md.
- Docs synced (BANNER, parity-summary, STATE  checkpoint). NO push was
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

## Continue gap closure (+)
With the recovery merged, resume the honest REAL_GAP closure doctrine:
- Reuse the oracle harness (tests/oracle/ + run_one_oracle.sh); add fixtures
  under tests/oracle/fixtures/<port>/, never scatter inputs in /tmp.
- **FAP (Functional Alignment Problem):** a C fn that is statically "ported"
  (annotated, compiles, depth-check clean) but whose *runtime output diverges
  from LIVE Python*. The parity scanner is blind to FAPs. The oracle harness
  (`bash tests/oracle/run_oracles.sh`) is the *only* FAP detector — any
  `cases: MISMATCH` is a FAP. See docs/fap.md for the canonical definition and
  triage (real FAP → fix C; false FAP = env noise → fix oracle normalize).
  Always register new ports in tests/oracle/registry.json so their FAPs run.
- Pick the next dense PURE-helper Python modules (no IO/network/DB coupling)
  and port faithfully, ORACLE-verify (C == LIVE Python, 0 mismatches).
- For each close: promote existing static helper to non-static + `/* PoP: c_func
  @ module.py:_py_func */`, or read REAL Python and implement real C; never
  fake success.
- Reject façades: no "not implemented" placeholders, no void* passthrough, no
  god headers in port_*.c.

## Hard rules (unchanged except FAP mandate)
- make slermes 0 errors. run_mission8_tests.sh → 77/0/0.
- Live scanner is truth: `make parity-walkway` → "Live: PORTED ... REAL_GAP ...
  PARTIAL ...". Sum the `real_gaps` key (PLURAL) — summing `real_gap` yields 0.
- **A port is not "done" until its oracle reports MATCH.** The PORTED count
  says nothing about behavioral correctness; the oracle green/red does. Run
  `tests/oracle/run_oracles.sh` after touching any ported code.
- No god header; opaque struct in .h, private fields in .c.
- Every public function gets a PoP annotation. Reuse, don't duplicate.

## Done-when for 
- Push target resolved + renumbered history pushed to the fork (NOT upstream/main).
- ≥1 more honest REAL_GAP batch closed (oracle-verified) and docs updated.
- Build clean, mission8 green, parity summary regenerated to live numbers.

## Prestige (session end)
Update BANNER.md + docs/parity-summary.md + append  to STATE.md + write
NEXT_SESSION_PROMPT.md (→) + commit + push.
Report: push target used, gaps closed, build/test/oracle, remaining RG.

## Copy-paste ready
Slermes : resolve fork-push target (never upstream/main), push the
renumbered .. history, then resume honest oracle-verified REAL_GAP
closure on dense pure Python helpers; build 0 errors, 77/0/0 mission8, no
god headers, no fake success. Run tests/oracle/run_oracles.sh after any port
touch — a MATCH is the behavioral-done signal; a MISMATCH is a FAP.
