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

**Wave 27 sweep — the biggest signal so far: 47 of 75 gates.** Those are the
largest modules (telegram 415 KB, slack 387 KB, discord 380 KB, matrix 177 KB,
messages, buzz, schema, whatsapp, google_chat, photon, openai_routes, kanban api)
and their authored fixtures passed 3129 cases with 0 mismatches; held-out inputs
still find divergence in 63% of their gates. Samples, each a different mechanism:

| gate | held-out divergence | reading |
|---|---|---|
| `w27_adapter_04abb5_p1` | `effective_port py=80 c=None` | missing scheme default |
| `w27_adapter_04abb5_p1` | `is_relay_media_url py=True c=False` | relay-URL predicate |
| `w27_adapter_1f12c3_p14` | `forum_thread_name py='New Post' c='New Postx'` | truncation arm |
| `w27_adapter_1f12c3_p14` | `response_encoding py='utf-8' c='gzip'` | header precedence |
| `w27_adapter_e9cb6b_p2` | `csv_set py=['3','a','b'] c=['3','3','a','a','b','b']` | no dedupe on a set |
| `w27_messages_87d5f2_p2` | C stripped a `<memory-context>` segment Python kept | filter rule |

**Decision: new waves pause here.** Landing more gates as "green" while 63% of the
largest wave has a known held-out divergence would make the gate count a claim the
repo cannot support. The next unit of work is triage, not porting:

1. classify each of the 47 as artefact (fuzz modelled an impossible input) or
   defect (port disagrees with Python on a reachable input);
2. fix the defects, re-run `gate_fuzz` per gate until it reports 0;
3. re-run the same sweep on waves 28-33 (their debt is listed below) until the
   whole tree reports `held-out fuzz: N gates, 0 bad`;
4. only then make `gate_fuzz` a blocking step in `land_wave.sh`, so a wave cannot
   land on authored fixtures alone again.

**P0 — measured, not a hang: a 100× constant in payload validation.**
`w33_portabilit_b54bb9`'s `validate_payload` processes a 1 MB import payload in
**14.5 s** and 500 KB in **6.5 s** — 2.2× time for 2× input, so it is linear, but
the throughput is ~70 KB/s where the live Python module does the same work in
tens of milliseconds. The audit first reported this as a hang because the 120 s
ceiling tripped while walking 780 generated cases in one process; run alone, the
worst case completes. Still a defect worth fixing (an import path that spends
minutes of CPU on a large-but-legal payload is a denial-of-service surface), just
a different one than "unbounded loop": profile the per-character work in
`pb_core_validate_session` (title/byte accounting) before touching correctness.

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
