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

## Defects this audit found in wave 33 (all fixed)

| gate | symptom | cause | fix |
|---|---|---|---|
| `w33_telegramne_429779_p2` | `py=-2.5 c=15`, `py=1.25 c=-5` | `_connect_share` clamped a negative `paths_left` to 1 before dividing | divide by `paths_left` as-is; 0 → null (Python raises) |
| `w33_standalone_9f791b` | `py=7 c='7'` | `standalone_send`'s token resolution `str()`-ed a non-string token | pass the chosen value through unchanged |
| `w33_standalone_9f791b` | `text: None` where Python returns `[]`/`0`/`{}` | caption split only forwarded string text | copy the node |
| `w33_telegramne_429779_p1` | `py=['[::1]'] c=['::1']` | IPv6 bracketing applied only in the with-port arm | bracket in both arms |

## Known debt (still diverging under fuzz)

**P0 — a hang, not a wrong answer.** `w33_portabilit_b54bb9` stops responding on a
perturbed `validate_payload` case carrying a very long title (the harness hits the
120 s ceiling the audit enforces). An unbounded input must fail fast, not spin:
this is the first thing to fix, ahead of any count below.

Sweep of 2026-11-24 (`--per-case 5`, wave 33 plan). These gates pass their
authored fixtures but diverge on held-out inputs:

- `w33_sandboxcon_be5b73_p2` (10), `w33_adapter_eaa5eb_p2` (6),
  `w33_interactio_95c25f_p2` (6), `w33_pipeline_540ecf_p2` (6),
  `w33_configfree_3f224a` (5), `w33_servers_a4580e` (3),
  `w33_maintenanc_3e1b75_p1` (2).
- `w33_telegramne_429779_p2` (5 remaining) — all `urlunsplit` round-trip
  artefacts on malformed URLs (space in the authority, single-slash, scheme-less);
  documented as a seam in the port header. Valid Bot API URLs are identical.
- `w33_adapter_eaa5eb_p1` — the ORACLE times out (not the harness), so the gate
  has no held-out verdict at all yet.
- Earlier wave 32 debt is listed above and unchanged: audio, chatws, hostedroom,
  toolsets, plus two vacuous gates with no held-out coverage.

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
