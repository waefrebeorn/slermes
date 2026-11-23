#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/features.py (gate w5_features_c0a63b).

pm/features.py imports json/pathlib/typing at load; pm.paths is stubbed in
sys.modules (parent-package bootstrap) for the features_path branch.
"""
import importlib.util, json, sys, types
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)

_pkg = types.ModuleType("pm"); _pkg.__path__ = [Q + "/pm"]
sys.modules["pm"] = _pkg
sys.modules["pm.paths"] = types.ModuleType("pm.paths")

spec = importlib.util.spec_from_file_location("live_features_w5", Q + "/pm/features.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_features_w5"] = live
spec.loader.exec_module(live)

def norm(p):
    return str(p).replace("\\", "/")

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
        if func == "payload":
            ex = r.get("extras", [])
            py = json.dumps({"schema": 1, "extras": sorted(set(ex))}, indent=2) + "\n"
        elif func == "read":
            try:
                data = json.loads(r.get("text", ""))
            except ValueError:
                py = None
            else:
                ex = data.get("extras")
                py = None if not isinstance(ex, list) else sorted({str(e) for e in ex})
        elif func == "optin":
            names = set(r.get("names", []))
            declared = set(r.get("declared", []))
            unknown = sorted(names - declared)
            if unknown:
                py = None   # ValueError in Python
            else:
                py = sorted(names)
        elif func == "path":
            py = norm(live.features_path(Path(r.get("base", ""))))
        else:
            continue
    except Exception as exc:
        print(f"MISMATCH {func} {r}: python raised {exc!r}", file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_features_c0a63b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
