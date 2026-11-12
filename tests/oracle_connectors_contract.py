#!/usr/bin/env python3
"""Oracle for tools/connectors/contract.py::resolves and ::allowed.

The module imports only stdlib, so exec the LIVE module whole: that also runs
the import-time loop that copies the mcp edges onto plugin/skill rows, which a
bare AST extraction of the TRANSITIONS literal would miss. Drive it with
kind/state strings (the enums are str-Enums, so TargetState.connected ==
"connected"). The harness passes NULL kind/actor as profile:null -> we call
with None.
"""
import enum
import json
import sys
import typing

SRC = "C:/Users/eman5/hermes-agent/tools/connectors/contract.py"


def main() -> int:
    ns = {
        "__name__": "contract_live",
        "Enum": enum.Enum,
        "Dict": typing.Dict,
        "Optional": typing.Optional,
        "Tuple": typing.Tuple,
    }
    exec(compile(open(SRC, encoding="utf-8").read(), SRC, "exec"), ns)
    resolves = ns["resolves"]
    allowed = ns["allowed"]

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE connectors_contract: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        tag = c.get("tag")
        if tag == "res":
            total += 1
            want = resolves(c["kind"], c["state"])
            if bool(want) != bool(c["out"]):
                mism += 1
                print(f"MISMATCH {c.get('case')} kind={c['kind']!r} state={c['state']!r}: "
                      f"C={c['out']} PY={want}")
        elif tag == "alw":
            total += 1
            want = allowed(c["kind"], c["current"], c["to"])
            if want != c["out"]:
                mism += 1
                print(f"MISMATCH {c.get('case')} kind={c['kind']!r} "
                      f"current={c['current']!r} to={c['to']!r}: "
                      f"C={c['out']!r} PY={want!r}")
    print(f"ORACLE connectors_contract: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
