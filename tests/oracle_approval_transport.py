#!/usr/bin/env python3
"""Oracle for hermes_cli/approval_transport.py::_validate_decision.

Ast-extract the LIVE `_validate_decision` and drive it with fake `value` /
`request` objects (SimpleNamespace), feeding the harness fields. value_null is
mapped to value=None; kind "error" drives a TypeError-free path. The result
string (or None) is compared to the C output.
"""
import ast
import json
import sys
import types

sys.stderr.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/approval_transport.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"dataclass": __import__("dataclasses").dataclass}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_validate_decision":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.ClassDef) and n.name == "ApprovalDecision":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    # the live fn calls logger.warning(...) and isinstance(value, ApprovalDecision)
    ns["logger"] = types.SimpleNamespace(warning=lambda *a, **k: None)
    FN = ns["_validate_decision"]
    ApprovalDecision = ns["ApprovalDecision"]

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
        print("ORACLE approval_transport: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "atr":
            continue
        total += 1
        request = types.SimpleNamespace(
            request_id=c["rid"], digest=c["rdig"], allowed_choices=c["allowed"])
        if c["value_null"]:
            value = None
        else:
            value = ApprovalDecision(c["reqid"], c["digest"], c["choice"])
        # kind "error" -> the live fn checks `kind == "error"` before touching value
        want = FN(c["kind"], value, c["completed"], c["deadline"], request)
        if want != c["out"]:
            mism += 1
            print(f"MISMATCH {c.get('case')} kind={c['kind']!r} vnull={c['value_null']}"
                  f" reqid={c['reqid']!r} digest={c['digest']!r} choice={c['choice']!r}\n"
                  f"  PY={want!r}\n  C ={c['out']!r}")
    print(f"ORACLE approval_transport: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
