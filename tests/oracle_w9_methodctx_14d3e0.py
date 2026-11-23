#!/usr/bin/env python3
"""Differential oracle for tui_gateway/method_ctx.py (gate w9_methodctx_14d3e0)."""
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

SRC = Q + "/tui_gateway/method_ctx.py"
spec = importlib.util.spec_from_file_location("w9_methodctx_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "names":
        reg = live.HandlerRegistry()
        for n in r.get("methods", []):
            reg.method(n)(lambda: None)
        return sorted(reg.names())
    if func == "skip":
        name = r.get("name", "")
        skip = r.get("skip", [])
        return name.startswith("__") or (name in live._PLUMBING) or (name in skip)
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_methodctx_14d3e0: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
