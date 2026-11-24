# Held-out differential fuzzing (parent-side audit)

Each wave gate ships a fixture whose cases were written by the same agent that
wrote the C port, so the fixture tests the port on inputs its author already had
in mind. The C-to-Rust re-testing study (arXiv 2611.23839) measured the cost of
exactly that gap: of 11 released translations, all 11 shipped defects, 160 in
total, and for 81 of the 86 defective translations the shipped check "never
compared the programs on an input that exposes the defect". Their harnesses
caught only 20%-91% of planted bugs.

`scripts/gate_fuzz.py` is the answer to that for this repo: it perturbs the
gate's own fixture into inputs the author never wrote, then runs the same
differential contract.

```
python scripts/gate_fuzz.py --plan docs/wave32-plan.json --per-case 6 --jobs 4
python scripts/gate_fuzz.py w32_backup_1d0d4e_p1 --plan docs/wave32-plan.json --per-case 10
```

## How it works

1. Read `tests/fixtures/<gate>.jsonl`, keeping `func`, `case`, `seams` verbatim.
2. Emit K variants per case: numeric neighbours (`n±1`, `0`, `-1`), string
   shapes (`""`, `s+"x"`, `s[:-1]`, upper, doubled), list truncation/reversal/
   duplication, dict key removal/addition, and same-type value transplants from
   other cases of the same gate.
3. Strip `out`, feed the file to a harness compiled **from the port source**
   (`src/<module>.c`, never the cached `.o` — auditing a stale object was how a
   fresh fix kept "failing"), so the C side computes its own answer.
4. Pipe the filled file to the gate's oracle: it computes the live Python answer
   from the same line and compares. `py=… c=…` lines are real divergences;
   `live-error=…` lines are inputs the live code itself rejects (noise, counted
   separately) — reported but not counted.
5. A run that compares fewer than `--min-cases` lines is a VACUOUS failure, not
   a pass.

## Rules the tool enforces (each learned the hard way)

- **Never perturb a discriminator key** (`*_type`, `kind`). Turning
  `value_type: "int"` into `"intx"` makes the oracle model a *different Python
  object*, so the "divergence" is the harness talking to itself. Excluding
  those keys dropped `w32_hostedroom_be7d8d_p1` from 21 to 4 findings.
- **Perturb within a JSON type.** Swapping a dict for a string tests the seam,
  not the port, and the live side simply raises.
- **Rebuild the harness every run.** `.testbin/harnesses/<gate>.exe` and
  `.testbin/o_<gate>.exe` are link-cache artifacts; either can predate the edit
  under test.
- **Forward slashes for MSYS.** A raw `C:\Users\...` path handed to
  `bash -lc "cd <path>"` loses its separators.

## Defects this audit found in wave 32 (all fixed)

| gate | symptom | cause | fix |
|---|---|---|---|
| `w32_pluginvali_b2daaa_p2` | `py='mgr.None' c='mgr.'` | `_render`/`_dotted` dropped a null segment instead of printing Python's `None` | `pv_text()` renders missing/null segments as the literal `None` |
| `w32_subagents_a171ba_p2` | `py={sql,params} c=None` on a 1-element `found` row | `json_len(found) < 2` was treated as a refusal | index slot 1 as NULL; only a non-array is a refusal |
| `w32_tools_f39c80_p1` | `detail: null` vs `""` | probe without a `detail` key: `dict.get` gives None, the port defaulted to `""` | copy the node, or `json_null()` |
| `w32_backup_1d0d4e_p1` | uppercase `.BAK` survivor dropped | `glob` is case-insensitive on Windows; `strncmp` is not | `bp_starts_ci` / `bp_ends_ci` for the prune arm only |

## Known debt (still diverging under fuzz)

Sweep of 2026-11-24 (`--per-case 6`, wave 32 plan). These gates pass their
authored fixtures but diverge on held-out inputs; each needs a port fix before
the wave contract treats fuzz as blocking:

- `w32_audio_a418b2_p1` (8), `w32_audio_a418b2_p2` (3) — `speak`/`transcribe`
  argument shaping.
- `w32_backup_1d0d4e_p1` (2), `p2` (2) — `uses_wal` header sniffing, `verify`
  message ordering (size gate vs open failure).
- `w32_chatws_cd49ef_p1` (6) — `console_run` frame sequence when a frame is not
  a JSON object.
- `w32_hostedroom_be7d8d_p1` (4), `p2` (2) — remaining budget/validation arms.
- `w32_tools_f39c80_p1` (1), `p2` (3) — `toolset_config` defaults.
- `w32_methodscon_ce0f5d_p1/p2` — VACUOUS: the oracle skips every fuzz line
  (func name set differs), so the gate has no held-out coverage at all yet.

## Integration with landing

`scripts/gate_fuzz.py` exits 1 on any real divergence, so it can gate a landing
directly once this debt is cleared:

```
python scripts/gate_fuzz.py --plan docs/wave<N>-plan.json --per-case 6 --jobs 4 \
  && bash scripts/land_wave.sh <N>
```

Until then it runs as an audit and its findings are fixed in follow-up commits —
gates are green on their own fixtures, and every number quoted above is a
re-measured run of the command shown.
