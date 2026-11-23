#!/usr/bin/env python3
"""Differential oracle for pm/uv_cache_prune.py (gate w6_uvcachepru_3aea47).

Loads the LIVE module by path. The `dist_name` probe replays the exact
`([A-Za-z0-9_.]+?)-\\d[^-]*\\.dist-info` regex that the live source defines
inside prune_uv_cache_to_lock() (an inner helper, not importable), then calls
the live `_normalized` on the captured group -- the same two operations the C
port performs.
"""
import importlib.util
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/pm/uv_cache_prune.py"
spec = importlib.util.spec_from_file_location("w6_uvcachepru_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_uvcachepru_live"] = live
spec.loader.exec_module(live)

DIST_RE = re.compile(r"([A-Za-z0-9_.]+?)-\d[^-]*\.dist-info")


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r.get("func")
    if f == "normalized":
        name = r.get("name")
        return live._normalized(name) if isinstance(name, str) else None
    if f == "dist_name":
        markers = r.get("markers") or []
        for name in markers:
            m = DIST_RE.match(name) if isinstance(name, str) else None
            if m:
                return live._normalized(m.group(1))
        return None
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w6_uvcachepru_3aea47: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
