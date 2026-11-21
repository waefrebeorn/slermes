<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Oracle debug queue (measured states + triage notes)

Dispatch as debugger slots free. Repro pattern:
`/c/msys64/usr/bin/bash -lc 'source /c/Users/eman5/slermes/build/winlibs-env.sh; cd /c/Users/eman5/slermes; timeout 500 bash tests/run_one_oracle.sh <name> oracle_<name>.py "" "<objs>"'`
Default objs for self-contained harnesses: `lib/libjson/json.o`.
Harnesses that link (not include) the port need `src/cli/port_<name>.o` added.

## Open targets

1. **main_desktop_flow 170/45** — objs: src/cli/port_main_desktop_flow.o + libjson.
   Clusters: _desktop_launch_env 18, _launch_bundled_desktop 10,
   _check_desktop_skip_build 4, _packaged_node_pty_missing 3,
   _diagnose_esbuild_ignore_scripts 3, _run_desktop_one_shot 2,
   _packaged_desktop_launch_command 2, _windows_user_runnable_pe_machines 1.
   First root cause: print-literal trailing newlines — live print("...package\n")
   carries \n INSIDE the text arg (plus end="\n"); C literals stripped it.
   Agent reported 43/48 strings byte-identical — the 5 drifters are suspects.
   Log: C:/Users/eman5/doc_mdf_dbg.log.

2. **gateway_migrate_flow rc127 = STATUS_HEAP_CORRUPTION (0xC0000374)** —
   DIAGNOSED: correct objs link fine; harness emits 167/170 lines then dies
   (NTSTATUS via PowerShell). Same pattern as gateway_agent: adoption
   double-free planted mid-fixture, surfaces at teardown. Use poison-heap
   shim (.testbin/dbg_alloc2.c, compile shim TU separately WITHOUT -D
   macros, harness WITH -Dmalloc=dbg_malloc etc.) + fixture-prefix bisect.
   Suspect: json_append/json_set adoption of borrowed objects in the
   migration compensator paths (rollback_migration, apply_migration).

3. **mcp_oauth_flow rc139** — segfault. Self-contained harness (libjson only).
   Poison-heap shim bisect; agent noted a fixed use-after-free in
   acquire_refresh_fence — look for siblings (lname/stack-copy pattern).

4. **tail_revive1 LINK FAIL** — harness #includes 8 port .c files; agent's
   documented gate uses -ffunction-sections -fdata-sections -O1
   -Wl,--gc-sections (drops prompt_builder.c's unrelated externals).
   Runner default flags lack --gc-sections -> undefined refs. Fix: runner
   DEPS entry with the gc-sections link, or harness-side seam stubs for
   prompt_builder externals. Files: tests/t_port_tail_revive1.c.

5. **tail_revive2 222/1** — objs: 8 gap .o + libjson (see agent gate command
   in deleg_a88929f3 task-1 report). One mismatch — likely single literal
   or projection nit.

6. **tail_revive4** — slow compile (20 included TUs); needs timeout >= 900.
   Re-run in flight (proc_7244bc3d7c71, log doc_tr4_dbg2.log). State unknown.

7. **run_adapters_flow 138/66** — debugger DISPATCHED (deleg_0b088220 task-9).

8. **Revival remainders from wave 1** (debugger slots when free):
   plugin_revive 163/17 (load_directory_module flow depth 10,
   find_plugin_skill 3, resolve_pre_tool_block 2, remove/handler 1+1);
   cli_gaps_revive1 186/10; cli_gaps_revive2 211/51; gateway_revive 229/14.

## Mutation-adequacy follow-ups (recursive audit)

- gateway_flow: add bool-repr fixture cases (run_flow.c L82 "False" path
  never compared) — kills the alter-string survivor.
- credential_pool_flow: add digits==0 guard cases (L269) — kills flip-eq0.
- Mutation generator hardening: skip comments, flag equivalent mutants,
  per-port dep lists (cch_flow baseline link-failed on libjson-only).

## Infra

- DA-6 honest universe scan: proc_fadc01f06908 (explicit interpreter —
  bare `python` in persisted sessions = WindowsApps stub, silent death).
  When landed: regenerate docs/dev/parity-gap-map.md totals.
- Fleet retry run: doc_watercup7.log (was 66%); scripts/watercup_truth.py
  reports VERIFIED/MISMATCH/BROKEN vs claimed PORTED.

## 2026-11-20 wave 56+ open debug state (authoritative until verdicts land)

- **memory_manager_flow3** — RESOLVED in wave 57 (`fe321f4170`, wired).
  Was 59 cases / 2 mismatches on `initialize_for_agent` — a use-after-free
  (borrowed providers pointer copied after `json_free(r)`). Parent gate 59/0.
- **runtime_provider_flow2** — RESOLVED in wave 57 (`fe321f4170`, wired).
  Was 245 cases / 163 mismatches across ~41 defs; scoped down honestly to
  13 faithful defs, parent gate 65/0. Removed defs are real gaps again.
- Parent working set (committed): wave 72 wired 847->851, HEAD `17b3ef528b`;
  build health 2158/2158 clean, port gate PASS.
  Wave 60 RESOLVED: tui_server_flow3 honestly scoped from 62 over-scoped
  fns (374/287, 50 red) to the 12 faithful ones, parent gate 74/0, wired.
  The 50 removed fns are real gaps again. Scanner perf fix committed
  separately (`7fae0e9970`) — bounded return-type search, full scan now
  advances past the old quadratic phase.
  Wave 59 RESOLVED: acp_gateway_server_flow parent 126/0 (JSON object vs
  array-only accessor bugs + BaseException passthrough), api_server_runs_flow2
  parent 98/0 (borrowed-node double-free + `in` / exact exception contracts).
  Both wired. tui_server_flow3 crash fixed (3 double-frees) but 374/287
  mismatches across 50/62 fns from incompatible seam models — scope-down
  agent `sa-0-e2986bc8` (deleg_c4759951) is reducing it honestly.
  Full `make parity` scan hangs in parity_truth.py (>600s); docs were
  stamped with `gen_parity_walkway.py --from-scan live_parity_scan.json`.
- Six code-only port agents (deleg_2848c18e) own: tui_hosted_room_service_flow,
  executions_worker_flow, acp_gateway_server_flow, tui_server_flow3,
  api_server_runs_flow2, feishu_adapter_flow5. Parent gates + wires on landing.
  LANDED wave 58 (`64f1875b43`): only 3 were parent-green and are wired
  (804): tui_hosted_room_service_flow 151/0, executions_worker_flow 168/0,
  feishu_adapter_flow5 134/0. The other three were FALSE GREENS in child
  self-reports (shadow-diff / 1-case gate) and are NOT wired:
  - **acp_gateway_server_flow** — parent 126 cases, 8 mismatches
    (`_editor_id` alias cases, `_answer_permission`). Debugger `sa-0-4ebef4c3`.
  - **tui_server_flow3** — parent harness rc127 (child claimed a fake
    1-case green); 62 fns / 374 fixture cases must actually run.
    Debugger `sa-1-d1095c0e`.
  - **api_server_runs_flow2** — parent harness rc139; 14 fns / 98 cases.
    Debugger `sa-2-2776008c`.
  Repro for all: rebuild the port .o, then `ORACLE_REBUILD=1 bash
  tests/run_one_oracle.sh <name> oracle_<name>.py "" "lib/libjson/json.o"`
  with winlibs env sourced. Parent re-gates before any wire.

## 2026-11-20 adapter wave (deleg_b577dbff) — RESOLVED in wave 61

All three children claimed shadow 0 but failed the real parent gate; all
three were debugged to green in `deleg_8589e8fd` and parent-verified:
- **telegram_adapter_flow6** — 116/0; rc127 was a double-free on the
  isoformat-exception swallow path plus a flaky borrowed-pointer UAF.
- **discord_adapter_flow5** — 118/0; `da5_int` needed Python int()
  trailing-whitespace semantics.
- **slack_adapter_flow4** — 200/0; 4 borrowed-pointer double-frees plus
  a context arg-shape fix. Original 9 wave-50 functions byte-identical;
  all 16 additions kept. Already wired, source fixed in place.
Committed `60bfbbc467`; wired 810->812, build health 2119/2119 clean.
Next wave `deleg_20edbe62` (4 code-only agents) owns the verified
remainders: tui_hosted_room_service_flow2 (59 unclaimed),
executions_worker_flow2 (50), acp_gateway_server_flow2 (44),
matrix_adapter_flow3 (102). Parent gates them before any wire.
THAT WAVE landed; parent battery 4/4 RED again (child shadow claims all
false) — debuggers `deleg_aaf9d094` active, NONE wired:
- tui_hosted_room_service_flow2: 101 cases, 20 mismatches (`live_room_services`).
- executions_worker_flow2: oracle SELFTEST fault — touch_session_activity
  seeded a plain dict where live needs `.ts` attribute; oracle-side fix.
- acp_gateway_server_flow2: 266 cases, 261 mismatches — over-scoped 44-fn
  class port; honest scope-down required (flow1's 20 fns are committed,
  untouched).
- matrix_adapter_flow3: 126 cases, 31 mismatches (_handle_approval_reaction
  cluster). Parent re-gates before any wire.
RESOLVED in wave 62 (`7cc1503427`, all wired, parent-verified):
tui_hosted_room_service_flow2 101/0, executions_worker_flow2 351/0,
acp_gateway_server_flow2 187/0 (13 divergent fns honestly removed),
matrix_adapter_flow3 126/0. Next wave uses SELF-GATING port agents
(agent runs the real run_one_oracle.sh and reaches 0 before reporting)
to eliminate the repeated false-green debug round trip. Wave 63 wired
4 self-gated swaths (816->820, HEAD `5a5a94f61c`), parent gates
106/0, 141/0, 128/0, 137/0; build health 2127/2127.

## Wired credit is registration, not production code (re-confirmed)
Audit of objects.mk: 2091 referenced objects, 19 missing files, 694 under
2 KB. Sampled port objects (telegram_adapter_flow7.o 491 B,
memory_manager_flow3.o 489 B) contain only section symbols. This is the
ALREADY-DOCUMENTED expected shape for all-static flow ports (see
slermes-windows-build references/fleet-oracle-workflow.md): the harness
`#include`s the port `.c`, so the oracle gates verify real behavior, but
the production link gets an empty translation unit. "Wired" therefore
buys PoP credit, NOT production parity — report oracle-verified behavior,
not wired-object counts, as progress. The genuinely open question is the
19 objects referenced by objects.mk that are MISSING on disk (e.g.
src/wubuoxml/*.o) — audit those separately; a missing object is not the
same as the expected empty all-static one.
FOLLOW-UP: the 18 wubu "missing" were a regex artifact (real paths are
lib/libwubuoffice/src/...). The only genuinely unbuilt object,
src/agent/account_usage_nous_flow2.o, has a source and was built as a
real 43228-byte object with 26 text symbols; .o files are gitignored, so
make regenerates it. No repo defect there.
Wave 64 (`47372b2b92`) wired 4 self-gated swaths, parent gates
128/0, 122/0, 118/0, 105/0. Next wave `deleg_d05146fd` (4 self-gating
agents) owns freshly verified unclaimed sets: feishu_adapter_flow6 (175),
context_compressor_flow5 (65), whatsapp_adapter_flow2 (38),
session_policy_flow2 (31). THAT WAVE: parent 129/0, 116/0, 237/0,
282/0; wave 65 (`c15077de77`) wired 824->826 (feishu flow6 + context
compressor flow5 new; session_policy_flow2 extended in place; whatsapp
flow2 was a stale-brief no-op — its file existed but 38 OTHER funcs
remain unclaimed). Next wave `deleg_db0d6770` uses EXPLICIT unclaimed
name lists: extend whatsapp_adapter_flow2 (>=12 of 38), google_chat_flow2
(35), gateway_run_flow4 (24), context_compressor_flow6 (56).
THAT WAVE: parent 314/0, 132/0, 87/0, 119/0; wave 66 (`eda97ca4ed`)
wired 826->829. gateway_run_flow4.o is a real 14-symbol object.
Next wave `deleg_e983d189` (explicit names): feishu_adapter_flow7 (174),
matrix_adapter_flow6 (69), context_compressor_flow7 (38), and a second
whatsapp_adapter_flow2 (26 remaining). THAT WAVE: parent 138/0, 106/0,
98/0, 394/0; wave 67 (`af1e5c4dee`) wired 829->832.
Claim-parser fix: PoP names can be DOTTED (e.g. Class.method); derive on
the last segment or already-ported class methods look unclaimed. With the
fix gateway/run.py dropped from 24 to 0 unclaimed — a refused slot was
avoided. Next wave `deleg_7af52fe7` (accurate explicit names):
feishu_adapter_flow8 (121), matrix_adapter_flow7 (53),
context_compressor_flow8 (24), final whatsapp extension (14).
THAT WAVE: parent 125/0, 106/0, 107/0, 466/0; wave 68 (`b8cf9d2b2e`)
wired 832->835. whatsapp_adapter_flow2 is now effectively complete
(62 PoP, 466 cases). Next wave `deleg_b2b144da` (all NEW flows):
telegram_adapter_flow9 (214 unclaimed), discord_adapter_flow8 (168),
slack_adapter_flow7 (175), feishu_adapter_flow9 (105).
THAT WAVE: parent 132/0, 124/0, 99/0, 127/0; wave 69 (`2bea8d2223`)
wired 835->839. Continue with the next remaining flow numbers on each
adapter after re-deriving with the dotted-name-aware parser.
THAT WAVE: parent 114/0, 129/0, 110/0, 130/0; wave 70 (`59b5c3c6c0`)
wired 839->843. Keep advancing the per-adapter flow numbers.
THAT WAVE: parent 110/0, 98/0, 117/0, 106/0; wave 71 (`0d7fb69585`)
wired 843->847. Agent-discovered seeding pitfalls were written back into
slermes-windows-port/references/flow-oracle-seeding.md.
THAT WAVE: parent 140/0, 121/0, 102/0, 103/0; wave 72 (`17b3ef528b`)
wired 847->851. More seeding pitfalls written into flow-oracle-seeding.md
and c-port-pitfalls.md by the agents.
