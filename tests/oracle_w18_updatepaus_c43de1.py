#!/usr/bin/env python3
"""Oracle for gate w18_updatepaus_c43de1 (hermes_cli/update_pause_record.py).

Ast-extracts the three pure token helpers (_without, _has_work, _relaunch_keys)
from the live module. No I/O or process-liveness imports are needed.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/update_pause_record.py"

WANTED = ("_without", "_has_work", "_relaunch_keys")
src = open(SRC, encoding="utf-8").read()
NS = {}
for node in ast.parse(src).body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), NS)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        func = r.get("func")
        try:
            if func == "without":
                tok = json.loads(json.dumps(r.get("token") or {}))
                py = NS["_without"](tok, set(r.get("pids") or []))
            elif func == "has_work":
                py = NS["_has_work"](r.get("token") or {})
            elif func == "relaunch_keys":
                py = sorted(NS["_relaunch_keys"](r.get("token") or {}))
            else:
                continue
        except Exception as exc:
            print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
            continue
        cases += 1
        got = r.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
    print(f"ORACLE w18_updatepaus_c43de1: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
