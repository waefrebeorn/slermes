#!/usr/bin/env python3
"""Port audit: one measured surface instead of ad-hoc counts.

Answers, for the whole tree at once:

  * which ``src/**/*.c`` files are still STUBS while claiming PoP credit
    ("port pending" marker + one or more ``PoP:`` lines) -- the annotation
    that says "ported" over a body that decides nothing;
  * which gates have no fixture at all, so nothing ever drives their harness
    with authored input;
  * which gates have a fixture too small to constrain anything (the "0 cases,
    0 mismatches" class: a single case proves the pipeline runs, not that the
    decision matches);
  * which oracles have no ``tests/t_port_<gate>.c`` harness.

Every count this prints is derived from the tree on this run: no stored
totals, no subtraction, nothing remembered from a previous session.

Usage::

    python scripts/port_audit.py            # summary
    python scripts/port_audit.py --json     # machine-readable
    python scripts/port_audit.py --write docs/port-baseline.json
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src"
TESTS = REPO / "tests"
FIXTURES = TESTS / "fixtures"

STUB_RE = re.compile(r"(STATUS:\s*PENDING|port[ _]pending|PORT_PENDING)", re.IGNORECASE)
POP_RE = re.compile(r"^\s*\*?\s*PoP:", re.MULTILINE)
# A placeholder PoP names a symbol that admits it decides nothing, or sits under a
# header that says the behaviour port is pending. Counting those as "ported" is
# how a gap count falls while the behaviour stays absent.
POP_PLACEHOLDER_RE = re.compile(r"^\s*\*?\s*PoP:.*_pending\b", re.MULTILINE)
# A function definition that returns a value: the port's whole point.
DEF_RE = re.compile(r"^\s*(?:static\s+)?(?:json_t|int|char|double|long|void|size_t)\s*\*?\s*\w+\s*\(", re.MULTILINE)

VACUOUS_CASES = 5  # below this a fixture cannot pin a decision table


def audit_sources() -> dict:
    stubs, honest_stubs, real = [], [], []
    for path in sorted(SRC.rglob("*.c")):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rel = path.relative_to(REPO).as_posix()
        stub = bool(STUB_RE.search(text))
        pops = len(POP_RE.findall(text))
        placeholders = len(POP_PLACEHOLDER_RE.findall(text))
        defs = len(DEF_RE.findall(text))
        row = {"file": rel, "pop": pops, "placeholder_pop": placeholders,
               "real_pop": pops - placeholders, "defs": defs, "bytes": len(text)}
        if stub:
            stubs.append(row)          # header says behaviour port is pending
        elif row["real_pop"]:
            real.append(row)           # PoP credit with no pending admission
        else:
            honest_stubs.append(row)   # no claim either way
    return {"stub_with_pop": stubs, "stub_no_pop": honest_stubs, "ported": real}


def audit_gates() -> dict:
    no_fixture, vacuous, no_harness, counted, orphan_fixtures = [], [], [], [], []
    oracle_names = set()
    for oracle in sorted(TESTS.glob("oracle_*.py")):
        gate = oracle.name[len("oracle_"):-len(".py")]
        oracle_names.add(gate)
        fixture = FIXTURES / f"{gate}.jsonl"
        harness = TESTS / f"t_port_{gate}.c"
        cases = 0
        if fixture.is_file():
            cases = sum(1 for line in fixture.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip())
        row = {"gate": gate, "cases": cases}
        if not fixture.is_file():
            no_fixture.append(row)
        elif cases < VACUOUS_CASES:
            vacuous.append(row)
        else:
            counted.append(row)
        if not harness.is_file():
            no_harness.append(row)
    for fixture in sorted(FIXTURES.glob("*.jsonl")):
        gate = fixture.name[:-len(".jsonl")]
        if gate not in oracle_names:
            orphan_fixtures.append({"gate": gate})
    return {
        "with_cases": counted,
        "no_fixture": no_fixture,
        "vacuous": vacuous,
        "no_harness": no_harness,
        "orphan_fixtures": orphan_fixtures,
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json", action="store_true", help="print the full report as JSON")
    ap.add_argument("--write", metavar="PATH", help="write the full report to PATH")
    ap.add_argument("--limit", type=int, default=12, help="rows to show per section (default 12)")
    args = ap.parse_args(argv)

    sources = audit_sources()
    gates = audit_gates()
    report = {
        "sources": {
            "ported_files": len(sources["ported"]),
            "ported_pop_lines": sum(f["real_pop"] for f in sources["ported"]),
            "placeholder_pop_lines": sum(f["placeholder_pop"] for f in sources["stub_with_pop"])
            + sum(f["placeholder_pop"] for f in sources["stub_no_pop"]),
            "stub_with_pop": len(sources["stub_with_pop"]),
            "stub_no_pop": len(sources["stub_no_pop"]),
        },
        "gates": {
            "with_cases": len(gates["with_cases"]),
            "no_fixture": len(gates["no_fixture"]),
            "vacuous": len(gates["vacuous"]),
            "no_harness": len(gates["no_harness"]),
            "orphan_fixtures": len(gates["orphan_fixtures"]),
            "authored_cases": sum(g["cases"] for g in gates["with_cases"]),
        },
        "detail": {"sources": sources, "gates": gates},
    }

    if args.write:
        Path(args.write).write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
        print(f"wrote {args.write}")
    if args.json:
        print(json.dumps(report, indent=1))
        return 0

    s, g = report["sources"], report["gates"]
    print("SOURCES")
    print(f"  ported files            {s['ported_files']}  ({s['ported_pop_lines']} behaviour PoP lines)")
    print(f"  pending headers         {s['stub_with_pop']}   <- header admits no behaviour port")
    print(f"  placeholder PoP lines   {s['placeholder_pop_lines']}   <- *_pending symbols, decide nothing")
    print(f"  no claim                {s['stub_no_pop']}")
    print("GATES")
    print(f"  with >= {VACUOUS_CASES} cases        {g['with_cases']}  ({g['authored_cases']} authored cases)")
    print(f"  no fixture              {g['no_fixture']}   <- nothing ever drives the harness")
    print(f"  fixture < {VACUOUS_CASES} cases       {g['vacuous']}   <- cannot pin a difference")
    print(f"  no harness              {g['no_harness']}")
    print(f"  orphan fixtures         {g['orphan_fixtures']}")

    for title, rows in (
        ("stub + PoP credit (first %d)" % args.limit, sources["stub_with_pop"][: args.limit]),
        ("no fixture (first %d)" % args.limit, gates["no_fixture"][: args.limit]),
        ("fixture < %d cases (first %d)" % (VACUOUS_CASES, args.limit), gates["vacuous"][: args.limit]),
    ):
        if not rows:
            continue
        print(f"\n{title}:")
        for row in rows:
            if "file" in row:
                print(f"  {row['file']}  ({row['pop']} PoP, {row['defs']} defs, {row['bytes']} B)")
            else:
                print(f"  {row['gate']}  ({row['cases']} cases)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
