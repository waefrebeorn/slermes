#!/usr/bin/env python3
"""Audit tests/oracle/registry.json against the files that actually exist.

The registry is the labelling system for the port work: each port names a
harness, an oracle and (sometimes) a fixture. Three naming conventions are in
use -- tests/t_port_<stem>.c, tests/t_port_port_<stem>.c and
tests/t_port_agent_<stem>.c -- and entries drift when a harness is renamed
without the registry following.

For every port this reports:
  * harness/oracle present or missing
  * the convention each side follows
  * when missing, a UNIQUE candidate elsewhere in tests/ (resolved, not guessed)

Usage:
  python scripts/audit_oracle_registry.py           # report
  python scripts/audit_oracle_registry.py --verbose # include the resolvable rows
"""
import argparse
import json
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
REG = REPO / "tests" / "oracle" / "registry.json"


def convention(path: str) -> str:
    """Which naming family a tests/ path belongs to."""
    if not path:
        return "-"
    name = Path(path).name
    if name.startswith("sta_oracle_"):
        return "sta_oracle_*"
    if name.startswith("oracle_"):
        return "oracle_*"
    if name.startswith("t_port_port_"):
        return "t_port_port_*"
    if name.startswith("t_port_agent_"):
        return "t_port_agent_*"
    if name.startswith("t_port_"):
        return "t_port_*"
    if name.startswith("oracle"):
        return "oracle(no underscore)"
    return "other"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    reg = json.loads(REG.read_text(encoding="utf-8"))
    ports = reg["ports"]

    # Every candidate file in tests/, indexed by basename and by suffix stem.
    by_base = {}
    for p in (REPO / "tests").rglob("*"):
        if p.suffix not in (".c", ".py", ".sh"):
            continue
        by_base.setdefault(p.name, []).append(p)

    rows = []
    for name, ent in ports.items():
        h = ent.get("harness") or ""
        o = ent.get("oracle") or ""
        h_ok = bool(h) and (REPO / h).exists()
        o_ok = bool(o) and (REPO / o).exists()
        rows.append((name, h, o, h_ok, o_ok))

    bad = [r for r in rows if not (r[3] and r[4])]
    print(f"registry ports: {len(rows)}   complete: {len(rows) - len(bad)}   "
          f"broken: {len(bad)}")

    by_conv = {}
    for name, h, o, h_ok, o_ok in rows:
        key = (convention(h), convention(o))
        by_conv[key] = by_conv.get(key, 0) + 1
    print("\nconvention pairs in use:")
    for (hc, oc), n in sorted(by_conv.items(), key=lambda kv: -kv[1]):
        print(f"  {n:4d}  harness={hc:18} oracle={oc}")

    # Resolve broken entries by basename anywhere under tests/.
    resolved = unresolved = 0
    print("\nbroken entries:")
    for name, h, o, h_ok, o_ok in bad:
        notes = []
        if not h_ok and h:
            hits = by_base.get(Path(h).name, [])
            if len(hits) == 1:
                notes.append(f"harness -> {hits[0].relative_to(REPO).as_posix()}")
                resolved += 1
            elif len(hits) > 1:
                notes.append(f"harness -> AMBIGUOUS ({len(hits)} candidates)")
                unresolved += 1
            else:
                notes.append("harness -> no candidate")
                unresolved += 1
        if not o_ok and o:
            hits = by_base.get(Path(o).name, [])
            if len(hits) == 1:
                notes.append(f"oracle -> {hits[0].relative_to(REPO).as_posix()}")
                resolved += 1
            elif len(hits) > 1:
                notes.append(f"oracle -> AMBIGUOUS ({len(hits)} candidates)")
                unresolved += 1
            else:
                notes.append("oracle -> no candidate")
                unresolved += 1
        if args.verbose or notes:
            print(f"  {name:34} {'; '.join(notes) if notes else '(empty entry)'}")

    print(f"\nresolvable by unique basename: {resolved}   "
          f"unresolved/ambiguous: {unresolved}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
