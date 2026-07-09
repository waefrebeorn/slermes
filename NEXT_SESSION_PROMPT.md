# Slermes v549 Session Prompt — POST-v548 FACADE ERADICATION
Branch: main (v548 HEAD pushed to origin/main)
Working dir: /home/wubu/hermes-agent-dev/slermes
Python sources: /home/wubu/hermes-agent-dev/  (parent repo — NOT inside slermes/)
Date anchor: 2026-07-09

## Context (what v548 delivered)
v548 ERADICATED **95 additional fraudulent/dormant façades** on top of v547's 110:
- 53 residual façades (`return <const>` / void no-op) — deleted fake `PoP:` + body.
- 25 no-return `(void)arg` bodies — deleted (honest REAL_GAP).
- 17 thin-wrapper frauds (Python did real work, C returned canned/const) — deleted.
- 37 thin wrappers + 3 SDK-getters + stateless scrubber reset RETAINED as honest.
Build: clean (0 errors). run_mission8_tests.sh: 36 passed / 0 failed / 35 skipped.
Live scanner end-v548: PORTED 4,881 (50.2%), REAL_GAP 4,802 (49.3%), PARTIAL 48, STUB 0, N/A 0.
Verified: **0 `PoP:` comments remain** for any eradicated name.

Tooling left in tree (reusable, reproducible):
- tests/v548_detect.py — C-aware body extractor + shape classifier
- tests/v548_adjudicate_all.py, tests/v548_delete_facades.py,
  tests/v548_delete_noret.py, tests/v548_delete_thin.py
- tests/.v548_facade_real.json / .v548_noret_real.json / .v548_thin_real.json
  (per-function Python-body reads + honest verdicts)

## The Mission (v549)
The residual façade sweep is DONE (0 façades remain). v549 should now drive
PORTED toward genuine closures by IMPLEMENTING real gaps in C — NOT by
manufacturing more annotations. Honest frontier:

1. **Genuine-gap closures (preferred):** pick REAL_GAP functions whose Python is
   pure-local or has a real C equivalent (config read via config.c, file IO via
   file_ops, JSON via hermes_json, rate-limit via libratelimit, etc.) and
   IMPLEMENT them for real in C. Each must get a `tests/t_port_*.c` +
   `tests/sta_oracle_*.py` asserting C == LIVE Python, 0 mismatches.
   Good starter modules (high genuine-closure density, low network coupling):
   - tools/port_file_operations.c (densify_matches was just demoted — re-implement
     for real instead of faking), _lint_*_inproc (re-implement real parsers).
   - cli/port_scale_to_zero_helpers.c, cli/port_config_helpers.c.
   - agent/port_agent_* helpers that are pure-compute.

2. **Honest-limitation façades (26 retained):** only implement real semantics if a
   live caller REQUIRES them (e.g. `_get_anthropic_sdk` returning NULL blocks a
   code path). Otherwise leave as NULL/0 — they are truthful. Do NOT invent SDK
   shims in C.

3. **Residual sweep for NEW façades:** re-run tests/v548_detect.py after any
   change; if any `FACADE_SHAPE`/`NORET_SHAPE` reappears, eradicate per v547 method.
   Also sweep NON-PoP functions that silently hardcode const (no annotation but
   same fraud pattern) — these are the next hiding spot.

4. **Never:** void* passthrough, placeholder comments, "In a real impl…" prose,
   swap a façade for another façade, or auto-demote a genuine port.

## Hard rules (unchanged)
- make slermes 0 errors. bash tests/run_mission8_tests.sh -> 36 passed / 35 skip.
- PoP MUST immediately precede the real C fn. Extend existing port_X.c (no dupes).
- v543–v546 oracle-verified leaf closures are REAL — do NOT touch.

## Done-when for v549
- N genuine gaps implemented for real (with oracle tests), N reported.
- PORTED increases; REAL_GAP reflects ONLY honest gaps.
- 0 new façades; 0 stubs; 0 N/A.

## Prestige (session end)
Update BANNER.md + docs/parity-summary.md + append v549 to STATE.md +
write NEXT_SESSION_PROMPT.md (v549->v550) + commit + push.
Report: closures implemented, build/test, parity delta, what's left.

## Copy-paste ready
Slermes v549: implement genuine REAL_GAP closures in C (file ops, config,
rate-limit, pure-compute helpers) with oracle tests; eradicate any new façades;
drive PORTED up honestly. Build 0 errors, 36/35 mission8. No stubs/NA.
