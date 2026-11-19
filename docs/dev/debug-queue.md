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
