# SLERMES — NEXT SESSION PROMPT (gap closure, maximum effort)

> Generated 2026-07-12 from a real full scan:
> `python3 tests/slermes_parity_battleground.py --json`. All numbers below are
> live scanner output, verified against the C tree (sampled gaps grepped —
> genuinely absent, not scanner artifacts). **Time is irrelevant. This is
> agentic work — it takes minutes. Never cite "time" as a reason to defer,
> phase, or skip a gap.** Close gaps. Prove each with the oracle harness.

## Ground truth (this scan)
- **9731** total Python features tracked across **484** modules.
- **4924 PORTED**, **75 PARTIAL**, **4732 REAL_GAP**, 0 STUB, 0 NA.
- **318 modules** have ≥1 REAL_GAP. **76 modules** are NEVER started (0 ported).

Sanity checks already done so you don't repeat them:
- Sampled gaps (`_lifespan`, `_require_token`, `get_anthropic_key`,
  `detect_zai_endpoint`, `_start_desktop_cron_ticker`) → **0 hits in `src/`**.
  These are genuine absences, not dead-hybrid PoP false-positives.
- All top gap modules' C files EXIST (`web_server.c`, `gateway/run`→ wired,
  etc.) — the modules are STARTED but under-ported, not missing scaffolding.

## REAL_GAP by subsystem (where the work is)
| gaps | modules | subsystem |
|-----:|--------:|-----------|
| 2401 | 170 | `hermes_cli/` |
| 1109 |  36 | `gateway/`   |
|  732 |  56 | `tools/`     |
|  368 |  49 | `agent/`     |
|  102 |   6 | `cron/`      |
|   20 |   1 | `cli.py`     |

## Recommended attack order (go heavy first, highest ROI first)

### Tier 1 — FINISH THE TAIL (≥50% ported, big absolute gap → fastest to 100%)
These modules are more than half done; closing them zeroes a whole module.
| % done | gaps | module |
|-------:|-----:|--------|
| 94% (288/308) | 20 | `cli.py` |
| 73% (67/92)   | 25 | `hermes_cli/config.py` |
| 66% (66/100)  | 34 | `tools/browser_tool.py` |
| 64% (65/102)  | 37 | `gateway/platforms/api_server.py` |
| 60% (45/75)   | 30 | `tools/approval.py` |
| 56% (22/39)   | 17 | `tools/checkpoint_manager.py` |
| 55% (31/56)   | 25 | `tools/terminal_tool.py` |
| 54% (25/46)   | 21 | `tools/skill_usage.py` |
| 53% (80/152)  | 72 | `agent/auxiliary_client.py` |
| 52% (90/172)  | 79 | `gateway/platforms/base.py` |
| 52% (29/56)   | 27 | `hermes_cli/setup.py` |
| 51% (19/37)   | 18 | `agent/learning_graph_render.py` |

### Tier 2 — PURE-LOGIC `tools/` (oracle-able, highest verification quality)
`tools/` gaps are mostly pure functions → write `tests/oracle/` fixtures and
prove C == LIVE Python byte-for-byte. Biggest:
`cua_backend.py` (43), `delegate_tool.py` (37), `file_tools.py` (35),
`browser_tool.py` (34), `browser_camofox.py` (31), `approval.py` (30),
`skill_manager_tool.py` (28), `memory_tool.py` (27), `registry.py` (26),
`terminal_tool.py` (25), `environments/base.py` (25), `tirith_security.py` (25).

### Tier 3 — THE BIG UNDER-PORTED HUBS (most absolute gaps)
`hermes_cli/web_server.py` (335, 12% done), `gateway/run.py` (272, 6%),
`hermes_cli/main.py` (176, 20%), `hermes_cli/auth.py` (150, 23%),
`gateway/platforms/yuanbao.py` (148, 31%), `hermes_cli/gateway.py` (142, 12%),
`hermes_cli/kanban_db.py` (124, 29%).

### Tier 4 — NEVER-STARTED modules (76 total; adjudicate NA vs REAL first)
Top: `gateway/slash_commands.py` (56), `hermes_cli/cli_commands_mixin.py` (43),
`agent/pet/generate/atlas.py` (33), `tools/read_extract.py` (15),
`agent/learning_mutations.py` (15), `hermes_cli/dashboard_auth/*` (routes 14,
cookies 14, middleware 8, token_auth 7). Some may be genuinely un-C-able
runtime deps — apply the DOCTRINE below before demoting ANY to NA.

## NON-NEGOTIABLE DOCTRINE (from USER.md + skills — do not violate)
1. **"Honest NA" demotion of a failable-in-C function is BANNED.** Rewriting
   from scratch in C is the point of the project. NA only for genuinely
   un-C-able external runtime deps (libwebkit, libcairo, asyncio loop, live
   cloud/gateway at call time). Everything else = REAL_GAP, implement it.
2. **No façade fakes.** No `"not fully implemented in C port"` + `return NULL`,
   no `"In a real implementation, this would…"` comment-façade, no `void*`
   passthrough, no `touch_json()`. Read the Python, implement the real behavior.
3. **Faithful, not lenient.** If a C lib is lenient where Python is strict,
   that's a FIDELITY GAP — delegate to `python3` for the strict check rather
   than ship a weaker one.
4. **Oracle-verify every closure.** `tests/oracle/fixtures/<name>/` +
   `tests/oracle/runners/run_oracle.sh <name>` → expect `N cases, 0 mismatches`.
   `0 cases` = LINK FAILURE (grep `undefined reference`), NOT a pass.
5. **Reuse the harness.** Don't scatter fixtures in `/tmp`; don't hand-roll a
   compile+diff loop. One shared harness under `tests/oracle/`.
6. **Audit-first on any "split" impulse.** Cohesive PoP ports (one Python
   module → one C file) are the correct boundary — do NOT fragment them
   (AGENTS.md). God headers / true second-concerns are the only split targets.

## Build/verify incantations (load-bearing)
- Makefile has NO header-dep tracking: `rm -f slermes <touched>.o && make slermes`.
- Register new port objects by APPENDING into `PHASE5_OBJ` in `build/objects.mk`
  (the `PHASE*_OBJ += $(..._PORT_REGEN)` form is DEAD — overwritten). Verify
  with `nm slermes | grep <symbol>`.
- PoP annotation must be single-line `/* PoP: c @ m.py:f */` OR a `/*` … ` * PoP:` block;
  a `/* PoP: …` with the desc on the next line and no closing `*/` matches
  NEITHER scanner regex → phantom REAL_GAP. Re-run the scanner after PoP edits.
- Re-run the scan at the end and report the REAL_GAP delta:
  `python3 tests/slermes_parity_battleground.py --json`.

## Definition of done for the session
- A batch of modules taken to **0 REAL_GAP**, each faithfully implemented
  (real behavior, no façade), oracle-verified (`0 mismatches`), build green
  (`make slermes`, 0 undefined refs), committed + pushed.
- Report the before/after REAL_GAP count and the exact modules zeroed.
