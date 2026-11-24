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
| fixture < 5 cases | **73** | legacy `cn_*` single-case gates; cannot pin a difference |
| no harness | **3** | oracle without `tests/t_port_<gate>.c` |

Held-out fuzz (the only measure that can disagree with the author):

| wave | clean | carrying findings | per-case | date |
|---|---|---|---|---|
| 27 | 39 | 36 | 5 | 2026-11-24 |
| 34 | 5 | 10 | 5 | 2026-11-24 |

Waves 25, 26 and 28-33 have **never** been swept held-out. That is the largest
single unknown in this document, and it is a gate on any claim about the whole
tree: the waves that were swept each lost between a quarter and two thirds of
their gates to real divergences, so an unswept wave should be assumed to carry
the same rate until measured.

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
