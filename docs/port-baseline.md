# Port baseline — the standard, measured

Declared 2026-11-24. Every number below comes from `scripts/port_audit.py` and
`scripts/gate_fuzz.py` run against the tree on this date. Nothing here is carried
forward from an earlier session, and nothing is derived by subtracting one
session's number from another's.

```bash
python scripts/port_audit.py                 # sources + gate inventory
python scripts/gate_fuzz.py --plan docs/waveNN-plan.json --per-case 5
```

## What counts as ported

A module counts as ported only when all three hold:

1. its `src/**/*.c` header carries **no** "port pending" admission;
2. it has at least one **behaviour** `PoP:` line — a `PoP:` naming a symbol that
   actually decides something, not a `*_pending` placeholder;
3. its gate has **>= 5 authored cases** and **0 held-out mismatches** when the
   fuzzer drives the port with inputs the author never wrote.

Anything else is one of: *pending* (header says so), *untriaged* (no claim at
all), *vacuous* (gate cannot pin a difference), or *failing* (held-out
mismatches). None of those are progress, and none of them may be reported as
such.

## Baseline

Sources (`src/**/*.c`):

| class | count | meaning |
|---|---|---|
| ported (behaviour PoP, no pending header) | **1877** | 8066 behaviour PoP lines |
| pending header | **571** | admits the behaviour port has not happened |
| placeholder `*_pending` PoP lines | **543** | symbols that decide nothing |
| no claim (neither ported nor pending) | **2365** | untriaged — the real backlog |

Gates (`tests/oracle_*.py`):

| class | count | meaning |
|---|---|---|
| >= 5 cases | **2295** | 112,210 authored cases total |
| no fixture | **146** | oracle exists, nothing ever drives the harness |
| fixture < 5 cases | **1** | cannot pin a difference |
| constants-only surface | **72** | one case carries the whole table — size is the shape, not a gap |
| no harness | **3** | oracle without `tests/t_port_<gate>.c` |

Held-out fuzz (the only measure that can disagree with the author):

| wave | clean | carrying findings | per-case | date |
|---|---|---|---|---|
| wave | clean | carrying |
|---|---|---|
| 25 | 41 | 16 |
| 26 | 43 | 17 |
| 27 | 43 | 32 |
| 28 | 10 | 22 |
| 29 | 6 | 24 |
| 30 | 7 | 17 |
| 31 | 14 | 9 |
| 32 | 10 | 11 |
| 33 | 9 | 10 |
| 34 | 5 | 10 |
| 35 | 1 | 0 |

**357 gates measured, 189 clean (53%), 168 carrying at least one real divergence.**
Latest clean sweep, 2026-11-24, --per-case 5, after the day's fixes: eleven gates
went from carrying to clean and one new gate landed.

## Why the previous counts were wrong

Three failure modes, each caught in this project:

- **Annotation counted as behaviour.** 543 `*_pending` PoP lines were being
  summed with real ones, so the "ported function" total moved without any
  decision being implemented.
- **Arithmetic standing in for measurement.** "Debt 33 of 75" was the last
  sweep minus gates fixed after it started. When re-swept, wave 27 was 36 of 75.
- **A green gate that only held alone.** `w34_sessions_67685b` passed standalone
  and failed inside a `--plan` batch, because another oracle's pydantic stand-in
  leaked into the shared interpreter. A gate whose verdict depends on run order
  is not a gate; `tests/.isolated_gates` exists for exactly this, and the batch
  verdict must equal the standalone verdict before a wave may land.

## Correction after classification (same day, same tree)

Two gap classes were mis-labelled by name alone, and the corrected split changes
what the standard can claim:

- **The 146 "no fixture" gates are 145 stdin-driven oracles plus one oddity.** They
  run, but they have no `tests/fixtures/<gate>.jsonl`, and `scripts/gate_fuzz.py`
  perturbs *that file* — so **145 gates cannot be held-out fuzzed at all**. They
  are not passing; they are unmeasured. This is a bigger hole than the 36 known
  wave-27 divergences, because those at least have a verdict.
- **The 2365 "untriaged" sources split 525 / 1840.** 525 sit at the same relative
  path as an upstream `.py` (`src/agent/api_error_summary.c` <- 
  `agent/api_error_summary.py`): real module ports that never claimed credit, so
  neither the PoP count nor the pending count sees them. The other 1840 have no
  upstream module — helpers, generated data, libs — and are not ports at all.

So the backlog order changes: the 145 un-fuzzable gates and the 525 unclaimed
module ports come first, because a gate nobody can perturb is indistinguishable
from no gate.

**And 72 of the 73 "vacuous" fixtures are not gaps at all.** They are
`{"func": "constants"}` gates: the single case compares the entire constant table,
so one case is the whole surface and more cases would test the same bytes twice.
`scripts/port_audit.py` now separates `constants`-only fixtures from genuinely
under-sized ones — 72 by shape, 1 real.

## Open gaps (tackled in order)

1. **Untriaged sources (2365).** Classify each: real port missing its PoP line,
   pure I/O seam, or genuine backlog. The first group is cheap and mechanical;
   the second needs a header note; the third is work, not paperwork.
2. **No-fixture gates (146).** Decide per gate: legacy duplicate (delete the
   oracle), or real gate missing its fixture (write one with >= 5 cases).
3. **Vacuous gates (73).** Expand to >= 5 cases or fold into a neighbouring
   gate; a single case proves the pipeline runs, not that the decision matches.
4. **Un-swept waves (25, 26, 28-33).** Sweep each at `--per-case 5` and record
   the real split here before any of those waves is touched again.
5. **Wave 34.** 10 of 15 gates carry findings; it lands only at zero.

## The audit tool itself was wrong three times

Before trusting any number above, read this: the measuring instrument failed in
three ways during the 2026-11-24 sweep, and each failure made the tree look better
than it was.

1. **A truncated wave reported as a finished wave.** Printing a mismatch line hit
   `UnicodeEncodeError` on the console code page and killed the whole run mid-wave:
   wave 27 reported 31 gates instead of 75, wave 28 reported 1 instead of 32. The
   totals looked cleaner because two thirds of the gates were never measured.
   `gate_fuzz`/`gate_batch` now reconfigure stdout to UTF-8 with `errors="replace"`.
2. **Oracle output was decoded with the console code page.** A correct U+2026 from a
   harness came back as U+FFFD and was reported as a divergence:
   `w25_asyncdeleg_9f3438` showed 11 findings of which 10 were mojibake. The decode
   is now explicit UTF-8, and the gate's real count was 1 — now 0.
3. **Perturbing a documented seam.** `w28_configenv_e9e22f_p2` reported 8 findings
   because the fuzzer rewrote `cfg.custom_providers`, a field the port documents as
   not modelled. `tests/fuzz_seams/<gate>.txt` now lists keys to leave alone, either
   bare or as `func:key`, plus `func:*` for a whole function the port cannot answer.

A tool that invents findings is worse than no tool, because it spends the fixing
budget on ghosts and hides the real ones in the noise.

## Wave 35

`agent/checkpoints/undo_hint` was a header-only stub whose PoP line still counted:
`STATUS: C HOME CREATED — behavior port PENDING`, body `(void)0`. Its timestamp
normaliser (`_epoch`) and the `files[:8]` name shape are now ported, with 30
authored cases and 150 held-out inputs at 0 mismatches. The git walk and the i18n
catalogue stay seams, and the port answers the join plus the overflow count rather
than inventing a translated sentence.
