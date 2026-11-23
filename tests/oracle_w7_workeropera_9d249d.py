#!/usr/bin/env python3
"""Differential oracle for pm/worker_operations.py (gate w7_workeropera_9d249d)."""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import pm  # noqa: F401  (parent-package bootstrap)
spec = importlib.util.spec_from_file_location("pm.worker_operations", Q + "/pm/worker_operations.py")
live = importlib.util.module_from_spec(spec)
sys.modules["pm.worker_operations"] = live
spec.loader.exec_module(live)


def render(op):
    if op is None:
        return None
    return {
        "module": op.module,
        "packages": list(op.packages) if op.packages is not None else None,
        "bootstrap": op.bootstrap,
    }


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "names":
            py = sorted(live.OPERATIONS)
        elif func == "lookup":
            py = render(live.OPERATIONS.get(r.get("name", "")))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_workeropera_9d249d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
