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

**Wave 27 sweep — 36 of 75 gates (measured, post-fix).**

Measured 2026-11-24 with `--per-case 5 --jobs 4` after the buzz, discord,
google_chat, matrix and schema fixes: **39 clean / 36 carrying findings, no
harness crashes** (`EXIT=1` comes from the mismatches, every line reports `ok`).
Wave 34, swept the same way at `--per-case 5`: **5 clean / 10 carrying findings**,
which is why it stays unlanded despite 15/15 authored gates. Those are the
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

**A quarter of the first sweep's findings were the tool arguing with itself.**
Perturbing a key that restates another key's fact — `found` next to a `names`
list, `count` next to the rows — models a state the live code cannot produce: the
oracle derives the number, the harness passes the fixture's number, and the
"divergence" is the pair disagreeing. `gate_fuzz` now skips
`found/count/total/n/size/len/...`; `w27_schema_3eeff1_p2` went from 4 findings to
0 with no port change at all. Every count below is post-fix.

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

## What the field does, and what this harness borrowed

Checked against how the problem is handled elsewhere (2026-11-24):

- **Differential testing alone is known to be insufficient for translated code.**
  Kaizen (arXiv 2607.04058) opens by noting that translated programs "compile and
  pass developer-written tests" yet still diverge — which is exactly what the sweep
  found here: 168 of 357 gates were green on their authored suite and wrong on
  inputs the author did not write. Their fix is the one adopted next: **metamorphic
  fuzzing** (mutations that must not change the answer) plus **grammar-based input
  generation** for behavioural diversity.
- **Cross-language differential testing of JSON parsers** (ACM 10.1145/3634737.3657003)
  is the closest published analogue to this work: same input, two implementations,
  compare values. Their emphasis on *type-directed* input generation matches the
  per-key value pools here.
- **Shrinking** is the standard property-testing answer to unusable reports
  (Hypothesis: "reduce a failing input to a simpler input that still fails"). This
  harness now does the same, in `scripts/fuzz_shrink.py`.

### scripts/fuzz_shrink.py

    python scripts/fuzz_shrink.py --plan docs/waveNN-plan.json <gate> [--case id]

Reduces a mismatching held-out case to a minimal one with the same disagreement:
drop optional keys, empty containers, zero numbers, truncate and case-fold strings,
and probe the values Python treats specially (`"0"`, `"None"`, `"false"`, `"[]"`,
`"{}"`). Candidates are evaluated in batches — one harness run, one oracle run per
round — because paying interpreter startup per probe timed out on a 200-case gate.

Why it matters: the minimal case *is* the specification. `sdp="0"` with an absent
`result` turned a 300-byte random object into "the route accepts a falsy result as
an empty mapping", which was a two-line fix.

## A slow RUNNER is an unmeasured gate

`w33_portabilit_b54bb9` could not be scored: five of its 727 fuzz cases carry a
15.7 MB payload and the harness exceeded the timeout, so the gate reported
`harness TIMEOUT` and no cases.

**Final answer, measured.** Every harness set stdout to `_IONBF` and then echoed the
request back **one `putchar` at a time**. On a 5 MB title that is millions of
unbuffered writes. With a buffered stdout and a per-line `fflush`:

| same 5 MB case | before | after |
|---|---|---|
| `w33_portabilit_b54bb9` | 7.00 s | **0.38 s** (18x) |

and the gate now measures: **732 cases, 0 mismatches** (authored 157 PASS). Applied
to all 2211 harnesses.

**The port was never slow.** Its JSON primitives were measured at 0.024 s parse /
0.023 s dump / 0.004 s deep copy for the whole 15.7 MB document, and the 5 MB title
case splits 1-session vs 10-sessions to 7.00 s vs 6.91 s — identical, i.e. one linear
pass, no quadratic anywhere.

Two earlier diagnoses in this file were wrong and are recorded here so they are not
repeatable: `json_append`'s one-slot growth (implemented, measured at 50.66 s vs
49.18 s, reverted) and "per-field work in `_portability.c`". Both were plausible
stories fitted to a symptom whose cause was in the instrument.

**The lesson that generalises:** when a measurement says the port is 35x slow, time
the *runner* before the *port*. A differential harness that echoes its own input
per character cannot measure anything large.

## Open conflicts (recorded, not papered over)

* **`w25_gatewayrun_b1c386` / `serves_home`** — held-out case `named:true,
  verdict:null, served:[], home_name:""` answers `False` live and `ValueError` in the
  port. The oracle builds `Path("C:/w25gate/profiles") / ""`, which collapses to the
  parent directory, so the profile name it normalizes is the literal `"profiles"` —
  not the empty string the port is handed. The port only receives the *name*, so it
  cannot reproduce a path-collapse artefact without hard-coding the fixture's parent
  directory. Left open deliberately: the authored suite is 17/17, and inventing
  `"profiles"` inside the port would be a fake fix for a test-seam quirk.

* **`w28_repair_1f5388_p3` / `repair_flow`** — held-out cases with a `db_path` that does
  not exist (`"0"`) answer `{"error": "0 does not exist"}` live and `{"error": null}` in
  the port. The live code runs `Path(db_path).exists()` **before** any scenario branch,
  so existence is not a scenario name. Attempted fix: `stat()` the path first. Measured
  result **worse** -- authored 33 cases went from 0 to 5 mismatches and held-out from 16
  to 27 findings -- because the oracle and the harness run in different working
  directories, so a relative path means different things to each. Reverted. Fixing it
  honestly needs the harness to carry an `exists` seam (the fixture's `seams` object is
  empty today), which is a fixture change, not a port change.

* **`w28_adapter_ca7894_p1` / `safe_decode` (3 remaining)** — a payload containing a
  NUL byte (`b64 "AGK="` -> `\x00b`) comes back truncated, because a JSON string in
  this tree is a bare `char *` with no length. Python strings carry NULs fine. Fixing
  it means a length-carrying string representation in `libjson` (a `str_len` beside
  `str_val`, touching every accessor in the tree), which is a model change and not a
  triage fix. The other four findings on this gate -- charset labels resolved the way
  `codecs.lookup` does -- are fixed.

* **`w28_adapter_ca7894_p3` / `sender_accepted` (1 remaining)** — held-out case
  `sender:"None", env:{EMAIL_ALLOW_ALL_USERS:"true"}` answers `{accepted: False,
  log: "debug"}` live and `{accepted: True, log: ""}` in the port. The "None" text is
  a fixture sentinel whose live meaning is not yet pinned: `_is_automated_sender`
  matches none of `_NOREPLY_PATTERNS` for "none", so the rejection comes from
  somewhere else on the absent-sender path. A speculative patch (treat "None" as an
  absent sender) was built and measured: no change, 1 -> 1, so it was reverted rather
  than kept. The gate's other six findings on this adapter are fixed (7 -> 1).
