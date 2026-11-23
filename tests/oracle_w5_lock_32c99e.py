#!/usr/bin/env python3
"""Differential oracle for pm/lock.py (w5_lock_32c99e).

The pure seams are the {{store}} templating pair, Lockfile.artifacts validation
and _tool_rows history rows; all are called on the LIVE module.
"""
import importlib.util
import json
import sys
from pathlib import Path, PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/pm/lock.py"
spec = importlib.util.spec_from_file_location("pmlock_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pmlock_live"] = live
spec.loader.exec_module(live)
live.Path = PurePosixPath  # keep path spelling identical to the C port

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
    got = r.get("out")

    if func == "templatize":
        py = live._templatize(r.get("env", {}), PurePosixPath(r.get("root", "")))
    elif func == "resolve":
        py = live._resolve(r.get("env", {}), PurePosixPath(r.get("root", "")))
    elif func == "artifacts":
        lf = live.Lockfile.__new__(live.Lockfile)
        lf._packages = r.get("packages", {})
        try:
            py = lf.artifacts(r.get("name", ""), r.get("target", ""))
        except ValueError:
            py = None
    elif func == "tool_rows":
        py = list(live._tool_rows(r.get("fact")))
    else:
        continue

    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_lock_32c99e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
