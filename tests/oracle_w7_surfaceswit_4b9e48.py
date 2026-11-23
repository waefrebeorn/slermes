#!/usr/bin/env python3
"""Differential oracle for agent/surface_switch.py (gate w7_surfaceswit_4b9e48)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/surface_switch.py"
spec = importlib.util.spec_from_file_location("w7_surfaceswit_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _norm(x):
    if isinstance(x, tuple): return [_norm(v) for v in x]
    if isinstance(x, list): return [_norm(v) for v in x]
    if isinstance(x, dict): return {str(k): _norm(v) for k, v in x.items()}
    return x

def compute(r):
    func = r.get("func")
    prompt = r.get("prompt", "")
    if func == "split_runtime_boundary":
        return _norm(live.split_runtime_boundary(prompt))
    if func == "runtime_host_value":
        return live.runtime_host_value(prompt, r.get("label", ""))
    if func == "identity_line_value":
        return live.identity_line_value(prompt, r.get("label", ""))
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
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_surfaceswit_4b9e48: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
