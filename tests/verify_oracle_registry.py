#!/usr/bin/env python3
"""Gate: every entry in tests/oracle/registry.json must point at real files.

The registry is the labelling system for the port work -- each port names a
harness, an oracle and optionally a fixture. Entries drift when a harness is
renamed without the registry following, and a stale entry silently drops a port
out of the run. This gate fails on any entry whose harness or oracle does not
exist.

Run scripts/audit_oracle_registry.py for the report (it also proposes unique
basename resolutions and summarises which naming conventions are in use).
"""
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REG = REPO / "tests" / "oracle" / "registry.json"


def main() -> int:
    if not REG.exists():
        print(f"registry gate: FAIL, {REG.relative_to(REPO)} is missing")
        return 1

    reg = json.loads(REG.read_text(encoding="utf-8"))
    ports = reg.get("ports")
    if not isinstance(ports, dict) or not ports:
        print("registry gate: FAIL, no ports object")
        return 1

    broken = []
    declared = 0
    for name, ent in ports.items():
        # A port may declare itself incomplete (harness/oracle not written yet).
        # That is tracked debt, not a dangling reference; the gate still fails on
        # any dangling reference that was NOT declared.
        if ent.get("status") == "incomplete":
            if ent.get("incomplete_reason"):
                declared += 1
                continue
            broken.append((name, "status", "incomplete without a reason"))
            continue
        for field in ("harness", "oracle", "fixtures", "runner", "framework"):
            val = ent.get(field)
            if not val:
                continue
            if not (REPO / val).exists():
                broken.append((name, field, val))

    print(f"registry gate: {len(ports)} ports, "
          f"{len(ports) - len({b[0] for b in broken}) - declared} complete, "
          f"{declared} declared incomplete, "
          f"{len(broken)} dangling references")
    for name, field, val in broken[:20]:
        print(f"    DANGLING  {name:34} {field:9} {val}")
    if len(broken) > 20:
        print(f"    ... and {len(broken) - 20} more "
              f"(run scripts/audit_oracle_registry.py)")

    print("registry gate:", "PASS" if not broken else "FAIL")
    return 0 if not broken else 1


if __name__ == "__main__":
    sys.exit(main())
