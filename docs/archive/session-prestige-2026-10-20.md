# Session Prestige — 2026-10-20

## Status Table

| Metric | start | end |
|--------|-------|-----|
| Binary | 25.6MB, hollow stub cli_main | **42.2MB, real cli_main** |
| `nm slermes` T cli_main | stub (infrastructure_stubs.o) | **real (src/cli/cli.o)** |
| hermes_state linked | 169 | 169 |
| Mission 8 | 75/2 (2 CLI failures) | **77/0** |
| Hollow stubs replaced this session | — | 132 |
| Watercup sweep (verify_set 4537) | ~3108 verified | ~3530 verified, draining |

## What Was Done

### PHASE5_OBJ — the structural unlock (commit 411b1c230d)

`build/targets.mk`'s slermes link consumes `$(PHASE5_OBJ)`, which was
**referenced but never defined** in objects.mk — empty for the entire CLI_OBJ
era. The whole interactive-CLI family (src/cli/*, src/hermes_cli/*,
subcommands, agent runtime — 1000+ objects) compiled but never linked; the
binary ran a hollow stub `cli_main` that could not parse argv.

- `PHASE5_OBJ = $(CLI_OBJ)` defined in objects.mk (the documented
  subsystem-list composition convention).
- Wired into the live block: the src/cli family, `partial_compress.o` (home
  of the CLI globals), `lib/libpngdec/pngdec.o`, and the full wubuzip object
  set (inflate/huffman/crc/bit/block/fixed/canon/fixedcode/limitcode).
- `infrastructure_stubs.c`: 132 hollow stubs renamed `stub_shadowed_*` with
  matching `#define <orig> stub_shadowed_<orig>` lines — the TU stays
  self-consistent with original signatures, zero body surgery. Real ports
  win every collision by construction. Variable multidefs (`g_gw`,
  `g_session_token`) shadowed the same way.
- `plugin_honcho.c`: plugin boilerplate (plugin_init/plugin_meta_*/
  plugin_deps_*/plugin_get_interface/plugin_cleanup/plugin_configure)
  shadowed — the TU stays linked only for its non-boilerplate symbols;
  plugins register via dlopen in the real flow.
- cli.c/main.c/cli.h: extracted `print_usage`; `--help` prints usage
  (rc 0), unknown flags error out (rc 2) — argparse parity.

Proof: `make slermes` green; `nm` shows `T cli_main` (real),
`T hermes_config_load`, `T get_hermes_home`, `T agent_chat`,
`T commands_dispatch`; mission8 **77/0**; `./slermes --help` rc0,
`--invalid-flag` rc2, `--version` banner OK.

### Incident 19:44 — destructive oracle, containment (commit 2ec1c9939b)

A cron-tick oracle (api_server/skills twin) booted real gateway code with
the inherited **cwd = repo root**. Its skills scan walked `.` and a
refresh/purge flow **deleted 32 tracked root files** (Makefile, README,
LICENSE, .gitignore, BANNER, ...) and created fixture-named dirs
(hello/, world/, test123/ — 292k junk files inside, NULL/, state/).

- Restored: `git checkout HEAD -- <32 paths>`; junk dirs removed.
- Root cause fixed in `wc_run_parallel.run_single_gate`: every oracle now
  runs inside a fresh per-gate sandbox under TMPDIR (pid+gate namespaced),
  cwd restored in a finally, sandbox wiped before and after. Verified live:
  gate runs, repo `git status` clean, zero leftovers.
- Lesson recorded: cron ticks + oracle fleets must NEVER inherit ambient
  cwd; sandbox per gate.

### Sweep mechanics learned (hard way)

- Chunks run from MY shell (foreground or background tool) get SIGTERM'd
  (~3-115s, "Terminated"); **cron-context ticks run to completion** and bank
  +400 verdicts each. Manual supervision of ticks is useless — let the cron
  do it.
- `agg2.py` pipeline verified healthy: results.jsonl → verdicts →
  pending.json regen. Manual single-gate runs DO write and get consumed.
- The tick's `chunk rc=0` log line is unreliable when the chunk is killed;
  trust the snapshot line counts in /tmp/scratch/sweep_snap/ and the agg
  verdicts.

## Next-Session Queue

1. **Baseline re-verification is SELF-DRIVING**: after the machinery
   overhaul below, all stale harness-class verdicts (zero-mismatch fails)
   were reset from every results source; 1545 gates requeued. The cron tick
   (watercup-sweep-tick, every 20m, 400 gates/chunk) drains them with the
   fixed pipeline. Check `tail /tmp/scratch/sweep_tick.log` — when
   pending.json <= 1 the tick prints the final verdict.
2. **The real work**: ~2700 cupfail gates = the measured parity queue
   (real cup mismatches, mostly gateway/platforms + hermes_cli + tools).
   Attack subsystem-by-subsystem, heaviest first (port__hermes_cli_web_server
   94, port__hermes_cli_kanban_db 91, port__cli 86, auth 71, models 65,
   update_cmd 65, skills_hub 62...). START ONLY after the drain settles —
   porting while ticks re-verify the same sources flaps verdicts.
3. `fix_all_gates.py` and any script calling `wc_run.run_cup_gate`
   directly still runs with ambient cwd — apply the same sandbox wrapper
   before using it outside a controlled shell.
4. Post-commit hook `advice.ignoredHook` noise: hooks/post-commit exists but
   is not executable — chmod +x or remove.

## Session Addendum (evening): the sweep-machinery overhaul

After the doc above was written, three more stacked root causes were
found and fixed — the sweep is now fully operational end to end:

- **watercup.h WCUP_VALINT** (commit 32d2276c1f): emitted by an older
  harness generator, never defined; 4,011 harnesses failed to link on it.
  Defined to byte-match the python twin's valstr(str(result)).
- **Deep-closure link tier** (same commit): retry rounds 5→12, then a
  last-resort link with ALL main-build objects (src+lib, muldefs,
  full syslibs) — hermes_cli_goals_py__state went BUILD FAIL → 5 real
  measured cup mismatches.
- **Per-gate subprocess isolation** (commit 4565b50e57): strace caught a
  traced oracle child calling kill(0, SIGTERM) — one group-kill murdered
  the whole fleet at ~3s on any 250+ gate run (three cron ticks banked
  nothing). Each gate now runs `--single` in its own session + sandbox;
  the fleet survives and the killer gate errors alone. Proof: the
  always-dying pending[:250] slice completed 250 gates in 123s, +250
  banked; the next cron tick banked +650.
- **Quarry**: plugins/memory/config_schema.py restored from upstream
  NousResearch/hermes-agent (web_server.py:92 imports it unconditionally;
  its absence killed every web_server-transitive twin).
- **Honest verdict reset**: every zero-mismatch fail (stale harness
  errors) dropped from all results sources; buildfail/timeout went
  1180 → 0 and 1545 gates requeued for honest re-verification.
