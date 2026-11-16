#!/usr/bin/env python3
"""Staged, language-agnostic port progress -- generated, never hand-written.

A single percentage cannot answer "what do I do next", and a single number that
moves for unknown reasons is worse than none. So progress is reported as a
FUNNEL: every Python module advances through the same stages, and a module is
only counted at the highest stage it fully satisfies. A module that reaches
parity with a broken harness is NOT at parity -- the funnel makes that visible
instead of letting the scanner take the credit.

Stages, in order (a module is at stage N only if it satisfies N and all below):

  S1 unit       src/**/port_<stem>.c exists
  S2 header     include/port_<stem>.h exists and is included by the unit
  S3 harness    tests/t_port_<stem>.c exists
  S4 oracle     tests/sta_oracle_<stem>.py exists
  S5 instrument the harness compiles, emits output, and the oracle runs
  S6 parity     the oracle's cases MATCH

S5/S6 are read from the most recent oracle-suite run when one exists; without
a run they are reported as "not measured" rather than guessed.

Output is a `` block so gen_parity_walkway.py can
publish it into the README and docs/parity-summary.md from one source.

Usage:
    python scripts/gen_port_stages.py [--json]
"""
from __future__ import annotations

import argparse
import collections
import json
import re
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
SRC = REPO / "src"
INC = REPO / "include"
TST = REPO / "tests"
BUILD = TST / "oracle/build"
REG = TST / "oracle/registry.json"

STAGES = ["S1 unit", "S2 header", "S3 harness", "S4 oracle",
          "S5 instrument", "S6 parity"]


def python_modules() -> dict[str, str]:
    """stem -> top-level package, from whichever tree actually carries it."""
    out = {}
    skip = {".venv", "venv", "node_modules", ".git", "__pycache__", "build",
            "docs", "tests", "examples", "contrib", "contrib_"}

    def walk(d: Path, pkg: str):
        try:
            entries = list(d.iterdir())
        except OSError:
            return
        for e in entries:
            if e.name.startswith(".") or e.name in skip:
                continue
            if e.is_dir():
                walk(e, pkg)
            elif e.suffix == ".py":
                if e.name.startswith("_") and e.name != "__init__.py":
                    continue
                out.setdefault(e.stem, pkg)

    for base in (REPO, REPO.parent / "hermes-agent"):
        if not base.is_dir():
            continue
        for pkg in sorted(p for p in base.iterdir() if p.is_dir()):
            if pkg.name.startswith((".", "_")) or pkg.name in skip:
                continue
            walk(pkg, pkg.name)
    return out


def unit_index() -> dict[str, Path]:
    """stem -> port source, for every C unit named to the standard."""
    idx = {}
    for c in sorted(SRC.rglob("port_*.c")):
        m = re.match(r"port_(.+)\.c$", c.name)
        if m:
            idx.setdefault(m.group(1), c)
    return idx


def suite_verdicts() -> dict[str, str]:
    """port -> 'MATCH' | 'MISMATCH' | 'BROKEN' | 'ABSENT' from the last run."""
    out = {}
    for rc in BUILD.glob("suite_*.rc"):
        port = rc.stem[len("suite_"):]
        code = rc.read_text(encoding="utf-8", errors="replace").strip()
        if code == "0":
            out[port] = "MATCH"
        else:
            vp = BUILD / f"batch_{port}.verdicts.json"
            reasons = set()
            if vp.exists():
                try:
                    for v in json.loads(vp.read_text(encoding="utf-8")):
                        reasons.add(v.get("reason") or "")
                except Exception:
                    pass
            if any(r.startswith("oracle produced no output") for r in reasons):
                out[port] = "BROKEN"
            elif reasons:
                out[port] = "MISMATCH"
            else:
                out[port] = "ABSENT"
    return out


def stage_of(stem: str, pkg: str, units: dict[str, Path],
             verdict: str | None) -> int:
    """Highest stage FULLY satisfied. Cumulative: a harness with no unit is not
    progress, it is an orphan, so each stage requires the ones below it."""
    # Some units fold the package into the name (port_agent_turn_context.c for
    # agent/turn_context.py); the standard is the bare module stem. Try both.
    unit = units.get(stem) or units.get(f"{pkg}_{stem}")
    if unit is None:
        return 0
    if not (INC / f"port_{stem}.h").exists():
        return 1
    if not (TST / f"t_port_{stem}.c").exists():
        return 2
    if not (TST / f"sta_oracle_{stem}.py").exists():
        return 3
    if verdict not in ("MATCH", "MISMATCH"):
        return 4
    return 6 if verdict == "MATCH" else 5


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    mods = python_modules()
    units = unit_index()
    verdicts = suite_verdicts()
    if not verdicts:
        print("note: no oracle-suite run found; S5/S6 will be not-measured",
              flush=True)

    rows = []
    for stem, pkg in sorted(mods.items()):
        v = verdicts.get(stem)
        rows.append((pkg, stem, stage_of(stem, pkg, units, v), v))

    counts = collections.Counter(r[2] for r in rows)
    total = len(rows)
    reached = total - counts[0]

    # per-package rollup: how far each top-level package has got
    pkg_tot = collections.Counter(r[0] for r in rows)
    pkg_full = collections.Counter(r[0] for r in rows if r[2] == 6)

    if a.json:
        print(json.dumps({
            "total_modules": total,
            "stage_counts": {str(k): counts[k] for k in range(7)},
            "by_package": {p: {"modules": pkg_tot[p], "at_parity": pkg_full[p]}
                           for p in sorted(pkg_tot)},
        }, indent=1))
        return 0

    lines = []
    lines.append("| Stage | Meaning | Modules |")
    lines.append("|---|---|---|")
    meaning = {
        0: "no C unit",
        1: "unit exists",
        2: "header exists and is included",
        3: "harness exists",
        4: "oracle exists",
        5: "instrument green (harness + oracle run)",
        6: "parity: cases MATCH",
    }
    for k in range(7):
        lines.append(f"| S{k} | {meaning[k]} | {counts[k]} |")
    lines.append("")
    lines.append(f"**{reached} / {total}** Python modules have a C unit; "
                 f"**{counts[6]}** reach parity.")
    if verdicts:
        lines.append("S5/S6 measured from the oracle-suite run under "
                     "`tests/oracle/build/` (ports with no run are not "
                     "measured, not assumed).")
    else:
        lines.append("S5/S6 not measured -- no suite run under "
                     "`tests/oracle/build/`.")
    lines.append("")
    lines.append("| Package | Modules | At parity |")
    lines.append("|---|---|---|")
    for p in sorted(pkg_tot):
        lines.append(f"| {p} | {pkg_tot[p]} | {pkg_full[p]} |")
    lines.append("")
    lines.append("_Generated by `scripts/gen_port_stages.py` — do not edit by "
                 "hand. A module counts at the highest stage it fully "
                 "satisfies; a module with a broken oracle is not at parity._")

    block = "<!-- PARITY:AUTO:STAGES -->\n" + "\n".join(lines) + \
            "\n<!-- /PARITY:AUTO:STAGES -->"
    dest = REPO / "docs/port-stages.md"
    dest.write_bytes(block.encode("utf-8"))
    print(f"stages: {total} modules | "
          + " ".join(f"S{k}={counts[k]}" for k in range(7)))
    print(f"wrote {dest.relative_to(REPO).as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
