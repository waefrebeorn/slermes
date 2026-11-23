#!/usr/bin/env python3
"""Differential oracle for pm/_marker_eval.py (noc_w5_markereval_a9d132.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/pm/_marker_eval.py"
sys.path.insert(0, QUARRY)
sys.path.insert(0, QUARRY + "/.venv/Lib/site-packages")

# Parent-package bootstrap so packaging / pm imports resolve.
if "pm" not in sys.modules:
    _pkg = types.ModuleType("pm")
    _pkg.__path__ = [QUARRY + "/pm"]
    sys.modules["pm"] = _pkg

spec = importlib.util.spec_from_file_location("markereval_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["markereval_live"] = live
spec.loader.exec_module(live)


def compute(r):
    env = dict(r.get("env") or {})
    return 1 if live.Marker(r["marker"]).evaluate(environment=env) else 0


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE markereval: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH markereval {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE markereval: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
