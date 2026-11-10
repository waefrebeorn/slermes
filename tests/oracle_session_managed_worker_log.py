#!/usr/bin/env python3
"""Oracle for gateway/session_managed_worker_log.py::worker_log_path.

Drives the LIVE function with profile strings (the harness passes `profile`
directly). We pass a real Path(str(profile)) exactly as the source does, so
is_absolute and join semantics are Python's own on this Windows box.
"""
import json
import sys
import ast
from pathlib import Path

SRC = "C:/Users/eman5/hermes-agent/gateway/session_managed_worker_log.py"


def main() -> int:
    ns = {}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name == "worker_log_path":
            mod = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(mod), SRC, "exec"), ns)
    ns["Path"] = Path
    FN = ns["worker_log_path"]
    # LOG_NAME is used via module global; ensure it resolves
    src = open(SRC, encoding="utf-8").read()
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) \
                and n.targets[0].id == "LOG_NAME":
            m2 = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(m2), SRC, "exec"), ns)

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
        print("ORACLE session_managed_worker_log: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "wlp":
            continue
        total += 1
        if c["profile"] is None:
            have = FN(None)
        else:
            have = FN(c["profile"])
        if isinstance(have, Path):
            want = str(have)
        else:
            want = have
        if want != c["out"]:
            mism += 1
            print(f"MISMATCH {c.get('case')} profile={c['profile']!r}: "
                  f"C={c['out']!r} PY={want!r}")
    print(f"ORACLE session_managed_worker_log: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
