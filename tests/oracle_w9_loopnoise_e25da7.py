#!/usr/bin/env python3
"""Differential oracle for tui_gateway/loop_noise.py (gate w9_loopnoise_e25da7)."""
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

SRC = Q + "/tui_gateway/loop_noise.py"
spec = importlib.util.spec_from_file_location("w9_loopnoise_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

EXCS = {
    "ConnectionResetError": ConnectionResetError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "BrokenPipeError": BrokenPipeError,
    "ValueError": ValueError,
    "RuntimeError": RuntimeError,
    "OSError": OSError,
    "ConnectionError": ConnectionError,
}


def compute(r):
    if r.get("func") != "is_benign":
        return None
    kind = r.get("exc_kind")
    ctx = {}
    if kind and kind != "none":
        ctx["exception"] = EXCS.get(kind, ValueError)("x")
    if r.get("callback") is not None:
        ctx["callback"] = r["callback"]
    if r.get("handle") is not None:
        ctx["handle"] = r["handle"]
    return live._is_benign_teardown(ctx)


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
print("ORACLE w9_loopnoise_e25da7: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
