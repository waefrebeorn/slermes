# Watercup Fleet gap_sentinel Audit — False-Green Gate Quantification

**Date:** 2026-11-19 · **Mode:** read-only devil's-advocate audit (no gate/harness/build edits)
**Scope:** `tests/watercup/gates/` — all 22,603 gates · verdicts from `tests/watercup/build/results.jsonl` (1,685 rows)
**Companion data:** `docs/dev/watercup-gap-sentinel-audit-fixqueue.json` (full per-gate fix queue)

## TL;DR

- **13,303 of 22,603 gates (58.86%) are `gap_sentinel` constant-return harnesses** that never call the port.
- **1,754 of them are stamped `classification: PORTED`** in gate.json — the false-green layer. 28.15% of all PORTED claims are backed by a harness that proves nothing.
- **781 of those 1,754 have a PoP-mapped C port that already exists** — the harness claims "No C port exists" while the port sits in `src/`, uncalled and unverified.
- **Cup-level false-green VERIFIED from sentinel harnesses: 0** in the current results — all 58 VERIFIED verdicts come from real differential harnesses. The cup engine catches sentinels at runtime (they MISMATCH/BROKEN against the live twin). The false-green is the **classification claim**, not the cup verdict.
- `scripts/watercup_truth.py` reports **1,626 false greens** (claimed PORTED, not VERIFIED); **639** of those run on sentinel harnesses, **987** on real harnesses that failed.
- The terminal_hints finding is **confirmed and worse than reported**: all 10 gates are sentinels, but all 10 functions **are already ported** in `src/tools/terminal_hints.c` (`thint_*`). The gates are currently labeled REAL_GAP (not PORTED), so they mask ported code as unported while proving nothing about it.

## 1. Fleet composition (22,603 gates)

| gate.json classification | prefix | gates | gap_sentinel harnesses | real harnesses |
|---|---|---:|---:|---:|
| PORTED | `port__` | 6,230 | **1,754** | 4,476 |
| REAL_GAP | `real__` | 11,511 | 11,511 | 0 |
| PARTIAL | `part__` | 38 | **38** | 0 |
| (none/legacy) | plain | 4,824 | 0 | 4,824 |
| **total** | | **22,603** | **13,303** | **9,300** |

Kinds: 22,596 `cup`, 7 `unit`. Every REAL_GAP gate is a sentinel (honest placeholder); every PARTIAL gate is a sentinel (claim exceeds proof); legacy gates are extern-linked or inline traced copies, none constant fakes.

## 2. Anatomy of a gap_sentinel harness

Two generator flavors, both constant:

- `static const char *gap_sentinel(const char *input) { (void)input; return "<fn_name>"; }` — **12,161 gates**
- `static int gap_sentinel(const char *input) { (void)input; return 0; }` + `snprintf(buf, …, "%s", "<fn_name>")` — **1,142 gates**

`main` loops the inputs, calls only `gap_sentinel`, and emits `WCUP_BEGIN/VALSTR(constant)/FINALIZE`. The cup is identical for every input. Verified across the 781 PoP-mapped PORTED sentinels: **zero call sites of the port symbol** (53 harnesses mention the port name in comments only; the single regex "call" hit was the C keyword `if` from a corrupt PoP entry, not a port call). No extern declarations, no `src/` files in `extra_src` (only `lib/libjson/json.c`).

The runner docstring's claim — "a stub that returns a constant cannot reproduce the real trace, so the cup never matches" — holds **only because the twin is live**. The sentinel is caught at runtime, but nothing stops gate.json from claiming PORTED before/without that run.

## 3. False-green quantification

### 3a. Classification layer (the real false-green)

`fix_all_gates.py` / `fix_ported_gates.py` stamp `classification=PORTED` on **harness existence**, not equivalence (documented in `watercup_truth.py`). Result:

- **1,754 PORTED-stamped gates whose harness is a constant sentinel.**
  - **781** have a PoP-indexed port (`pop_index.json`, `c_file` exists on disk) → port exists, never called, never verified. Worst tier.
  - **973** have no PoP mapping → PORTED claim with neither a mapped port nor a real harness.
  - Verdict coverage in current results.jsonl: **549 MISMATCH, 90 BROKEN, 0 VERIFIED, 1,115 never run**. The 1,115 unrun gates carry a PORTED claim with zero evidence of any kind.

### 3b. Verdict layer (watercup_truth.py cross-tab)

Current `results.jsonl` is a **partial fleet**: 1,685 `port__` gates only (a `--port-only` run). Official `watercup_truth.py` output:

```
watercup truth: 1685 gate results
  VERIFIED      58
  MISMATCH     825
  BROKEN       802
claimed classifications: {'PORTED': 1684, 'legacy': 1}
false greens (claimed PORTED, not verified): 1626
```

Verdict × harness shape (joined against the fleet scan):

| verdict | real differential | gap_sentinel |
|---|---:|---:|
| VERIFIED | **58** | **0** |
| MISMATCH | 276 | 549 |
| BROKEN | 712 | 90 |

- **False-green VERIFIED from gap_sentinel fakes: 0.** All 58 VERIFIED verdicts are real differential harnesses (extern + port source linked, port symbol called).
- Of the 1,626 "claimed PORTED, not verified" false greens: 639 are sentinel harnesses, 987 are real harnesses that failed (build/link/twin errors or genuine divergence).
- Caveat: no full-fleet verdict file exists. The 0-fake-VERIFIED result applies to the current 1,685-row results; the 1,115 unrun PORTED sentinels and all 11,511 REAL_GAP sentinels have no verdicts at all.

## 4. Case study: tools/terminal_hints.py (the triggering finding)

All 10 gates (`real__tools_terminal_hints_py__*`, incl. `annotate_failure`) are gap_sentinel harnesses. Corrections/deepening of the original finding:

1. **Current gate.json classification is REAL_GAP, not PORTED**, and none of the 10 appear in results.jsonl (the run was `--port-only`). The PORTED/VERIFIED claim in the finding does not match the current gate state — it describes the claim-layer mechanism, not these gates' present labels.
2. **The sentinel comment "No C port exists" is factually false.** All 10 functions are defined in `src/tools/terminal_hints.c` (20 KB): `thint_annotate_failure`, `thint_annotate_masked_success`, `thint_first_token`, and the 7 `thint_hint_*` helpers.
3. 8 of 10 are PoP-indexed to that file; `annotate_masked_success` and `_first_token` lack PoP annotations despite being defined.
4. Net effect: ported code is masked as REAL_GAP, and the gates prove nothing about the existing port. If an existence-based stamp ever reclassifies them PORTED (as happened to 1,754 other gates), they become textbook false greens.

## 5. Additional defects found

- **2 malformed PORTED harnesses** (digit-leading extern symbols — invalid C, cannot compile): `port__hermes_cli_observability_shared_metrics_py___ensure_private_file` (`extern int 0600(...)`) and `port__tools_mcp_oauth_manager_py__invalidate_if_disk_changed` (`extern int 1(...)`). Generator emitted literal values as function names. No verdicts yet.
- **38 PARTIAL gates, all sentinels**; 23 PoP-mapped. PARTIAL claim with a harness that never touches the partial port.
- **8 corrupt PoP index entries** (`c_fn` parsed as C keywords `if`/`for` from misparsed annotations, e.g. `tools/yuanbao_tools.py:_get_active_adapter → c_fn 'if'` in `src/app_desktop.c`). 1 of the 781 P0 mappings rests on a corrupt entry (solid P0 ≥ 780).
- **301 legacy gates link no port via extern/extra_src** — inline "traced copy" harnesses (70 use bare prototypes, 49 carry extra_src, 2 `#include` a `.c`). Not constant fakes, but they verify a *copy* of the logic, not the port binary: drift risk. One (`browser_tool_format_timeout_error`) stubs an env helper (`browser_is_local_mode() = false`) — documented in its ladder, still a coverage hole.
- Twin integrity: scanned all 4,476 real PORTED twins for fake/no-live-import patterns — **0 fakes** (the single heuristic hit, `port__agent_account_usage_py__redeemed`, uses the legitimate `make_twin` shared runtime; its current verdict is MISMATCH).

## 6. Top offender modules

PORTED-stamped sentinel gates (1,754 total, 292 modules):

| module | gates | | module | gates |
|---|---:|---|---|---:|
| cli.py | 241 | | hermes_cli/models.py | 39 |
| hermes_cli/kanban_db.py | 108 | | gateway/stream_consumer.py | 35 |
| hermes_cli/web_server.py | 65 | | gateway/platforms/base.py | 27 |
| hermes_cli/kanban.py | 57 | | gateway/run.py | 27 |
| hermes_cli/observability/relay_shared_metrics.py | 44 | | hermes_cli/main.py | 26 |
| hermes_cli/console_engine.py | 42 | | gateway/hosted_room_driver.py | 25 |
| hermes_cli/goals.py | 41 | | gateway/platforms/yuanbao_proto.py | 23 |

Worst tier — PoP-mapped port exists, sentinel harness, PORTED claim (781):

| module | gates | | module | gates |
|---|---:|---|---|---:|
| hermes_cli/kanban_db.py | 63 | | hermes_cli/main.py | 16 |
| hermes_cli/console_engine.py | 41 | | gateway/hosted_room_driver.py | 15 |
| hermes_cli/goals.py | 32 | | cli.py | 14 |
| hermes_cli/observability/shared_metrics.py | 22 | | tools/fuzzy_match.py | 14 |
| hermes_cli/projects_db.py | 19 | | hermes_cli/models.py | 13 |
| gateway/platforms/base.py | 18 | | tools/memory_tool.py | 13 |
| hermes_cli/heartbeat.py | 18 | | gateway/outbound/send.py | 12 |

All-fleet sentinel backlog (13,303, mostly honest REAL_GAP): gateway/run.py 530, hermes_cli/web_server.py 429, hermes_state.py 371, cli.py 310, hermes_cli/main.py 240, agent/auxiliary_client.py 220, …

## 7. Fix queue (prioritized)

Full per-gate lists in `watercup-gap-sentinel-audit-fixqueue.json`.

- **P0 — 781 gates: PORTED + sentinel + PoP-mapped port exists.** Regenerate real differential harnesses against the mapped `c_fn`/signature. `tests/watercup/fix_ported_gates.py` already does exactly this (reads the real C signature, emits extern+call harness) — run it over this set, then re-run the fleet subset. Until then these PORTED claims are unearned.
- **P1 — 973 gates: PORTED + sentinel, no PoP mapping.** Verify whether a port exists under a non-annotated name; if yes, add the PoP annotation and regenerate (→ P0 path); if no, reclassify REAL_GAP. Either way the PORTED stamp must go.
- **P2 — 10 terminal_hints gates.** Real harnesses against `thint_*` in `src/tools/terminal_hints.c`; add missing PoP annotations for `annotate_masked_success` and `_first_token`; reclassify from REAL_GAP once gated.
- **P3 — 38 PARTIAL sentinels** (23 PoP-mapped): real harnesses for the ported portion, or honest reclassification.
- **P4 — 2 malformed numeric-extern harnesses:** fix the generator's symbol derivation (it emitted `0600` and `1` as function names) and regenerate.
- **Backlog — 11,511 REAL_GAP sentinels:** honest placeholders, not false greens; this is the porting work itself. Prioritize by module traffic (table above).
- **Systemic:** make `classification=PORTED` contingent on a VERIFIED cup verdict (or at least a real harness that calls the port), never on harness existence. `watercup_truth.py` already computes the honest cross-tab — the walkway/claim consumers should read verdicts, not stamps. Also fix the 8 corrupt PoP entries and add a generator guard rejecting digit-leading/keyword symbols.

## 8. Method & caveats

- Pure-Python read-only scan of every `gate.json` + harness (`c_src` honored): gap_sentinel marker, extern/bare-prototype declarations, `extra_src` port files, static constant-return bodies, first-comment shape; joined last-wins with results.jsonl and `pop_index.json`; C-definition index over 2,267 `src/` files (40,189 defs) used only as a cross-check (suffix matching is noisy; PoP index is the authoritative mapping).
- "Real differential" = harness declares and calls a port symbol linked from `src/` (or legacy inline traced copy). The 2 malformed harnesses are counted in the 4,476 but cannot compile (sound real PORTED harnesses: 4,474).
- Verdict semantics per `watercup_truth.py`: VERIFIED = ok:true ∧ mismatches==0; MISMATCH = mismatches>0; BROKEN = ok:false ∧ mismatches==0.
- results.jsonl covers only 1,685 of 22,603 gates (port__ subset). No claim is made about verdicts of unrun gates; the 1,115 unrun PORTED sentinels are flagged as zero-evidence claims.
- No watercup source, gate, build/, registry.json, or libjson file was modified. Only this report and its companion JSON were written.