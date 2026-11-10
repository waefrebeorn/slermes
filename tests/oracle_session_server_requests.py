#!/usr/bin/env python3
"""Oracle for session_server_requests::is_response_frame.
The harness emits {"in": <json frame>,"ret": bool}; we ast-extract the LIVE
is_response_frame and compare on the same input."""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/gateway/session_server_requests.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "is_response_frame":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    FN = ns["is_response_frame"]

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
        print("ORACLE session_server_requests: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        frame = json.loads(c["in"])  # emitted JSON text -> the Python frame object
        want = bool(FN(frame))
        if c.get("ret") != want:
            mism += 1
            print(f"MISMATCH {total} C={c.get('ret')!r} PY={want!r} frame={c['in']}")
    print(f"ORACLE session_server_requests: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
