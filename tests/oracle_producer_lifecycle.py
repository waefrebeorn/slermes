#!/usr/bin/env python3
"""Oracle for gateway/session_producer_lifecycle.py::_executed_outcome.

Harness (t_port_producer_lifecycle.c) emits per case:
  {family:"outcome", case, cap:<json>, out:<"success"|"cancelled"|"failure">}

Ast-extract the live _executed_outcome and replay on the emitted captured dict.
The C returns lowercase outcome strings that are the ProcessingOutcome enum
.values() (SUCCESS="success", CANCELLED="cancelled", FAILURE="failure").
"""
import ast
import json
import sys
import enum

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/session_producer_lifecycle.py"


def _make_enum():
    class ProcessingOutcome(enum.Enum):
        SUCCESS = "success"
        CANCELLED = "cancelled"
        FAILURE = "failure"
    return ProcessingOutcome


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ProcessingOutcome = _make_enum()
    ns = {"Any": object, "ProcessingOutcome": ProcessingOutcome}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name == "_executed_outcome"]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    f = ns["_executed_outcome"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        want = f(c.get("cap")).value
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH outcome case={c.get('case')} cap={c.get('cap')}\n"
                  f"  C={got} PY={want}")
    if total == 0:
        print("ORACLE producer_lifecycle: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE producer_lifecycle: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
