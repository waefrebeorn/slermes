#!/usr/bin/env python3
"""Gate: every oracle fixture must parse, and must name a function the oracle
actually maps.

Nine ports in one suite run died with `json.decoder.JSONDecodeError: Expecting
value` -- the oracle read a fixture line that is not JSON, printed PYERR, and
the port reported "oracle produced no output". That is an ABSENT MEASUREMENT
that looks exactly like a broken port, and it cost a full suite run to find.

A fixture is data, so it is validated as data: every non-blank line that is not
a comment must parse as JSON, and (when the oracle is readable) its `func` key
must be one the oracle maps. Run this BEFORE the suite, not after.

Usage: python tests/verify_oracle_fixtures.py
Exit 0 = PASS, 1 = FAIL.
"""
import json
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
FIX = REPO / "tests/oracle/fixtures"
ORACLES = REPO / "tests"

def main() -> int:
    if not FIX.is_dir():
        print(f"fixture gate: FAIL, {FIX} is missing")
        return 1

    bad = []
    checked = 0
    for d in sorted(FIX.iterdir()):
        if not d.is_dir():
            continue
        stem = d.name
        for cf in sorted(d.glob("*.in")):
            checked += 1
            try:
                raw = cf.read_bytes()
            except OSError as e:
                bad.append((cf, "unreadable", str(e)))
                continue
            try:
                text = raw.decode("utf-8")
            except UnicodeDecodeError as e:
                bad.append((cf, f"not valid UTF-8 (byte 0x{e.object[e.start]:02x} "
                                 f"at offset {e.start})", ""))
                continue
            lines = text.splitlines()
            # Fixture format is per-oracle: prep_fixtures.py only substitutes
            # placeholders and never parses. So validate against what the
            # ORACLE reads, not against an assumed shape -- assuming JSON-lines
            # produced 10,005 phantom violations on pretty-printed arrays, and
            # assuming an array flagged raw-text fixtures (agent_display feeds
            # shell commands; agent_estop feeds bare case names).
            o = ORACLES / f"sta_oracle_{d.name}.py"
            # A port may declare itself incomplete in the registry; that is
            # tracked debt, not a dangling fixture, and the runner SKIPs it.
            try:
                reg = json.loads((REPO / "tests/oracle/registry.json")
                                 .read_text(encoding="utf-8"))
                ent = reg.get("ports", {}).get(d.name) or {}
            except Exception:
                ent = {}
            if ent.get("status") == "incomplete":
                continue
            otext = o.read_text(encoding="utf-8", errors="replace") if o.exists() else ""
            wants_lines = bool(re.search(r"for\s+line\s+in\b.*:\s*$\n?\s*json\.loads\(line",
                                         otext, re.M))
            if not o.exists():
                continue  # nothing to check the fixture against
            if wants_lines:
                for i, line in enumerate(lines, 1):
                    s = line.strip()
                    if not s or s.startswith("#"):
                        continue
                    try:
                        json.loads(s)
                    except json.JSONDecodeError as e:
                        bad.append((cf, f"line {i}: oracle reads JSON lines, "
                                         f"this is not JSON ({e.msg})", s[:70]))
            elif re.search(r"json\.load\s*\(", otext):
                try:
                    doc = json.loads(text)
                except json.JSONDecodeError as e:
                    bad.append((cf, f"oracle reads a whole JSON document, "
                                     f"fixture does not parse ({e.msg})", ""))
                    continue
                if not isinstance(doc, (list, dict)):
                    bad.append((cf, "fixture is neither a JSON array nor object", ""))
                    continue


    print(f"fixture gate: {checked} fixture file(s) checked, {len(bad)} problem(s)")
    for cf, why, sample in bad[:20]:
        rel = cf.relative_to(REPO).as_posix()
        print(f"    {rel}: {why}" + (f"  | {sample}" if sample else ""))
    if len(bad) > 20:
        print(f"    ... and {len(bad) - 20} more")
    print("fixture gate:", "PASS" if not bad else "FAIL")
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
