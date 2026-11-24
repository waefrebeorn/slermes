#!/usr/bin/env python3
"""Differential oracle for gateway/stream_consumer_fences.py (w18_streamcons_75ce67)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


if "gateway" not in sys.modules:
    _mk("gateway")

SRC = Q + "/gateway/stream_consumer_fences.py"
spec = importlib.util.spec_from_file_location("w18_streamcons_75ce67_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compat(v):
    if isinstance(v, tuple):
        return [compat(x) for x in v]
    return v


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
        if func == "escape":
            py = live.escape_code_fences_for_display(r.get("text"))
        elif func == "open_span":
            py = live.open_code_fence_span(r.get("text"), r.get("upto", 0))
        elif func == "ensure_closed":
            py = live.ensure_closed_code_fences(r.get("text"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    py = compat(py)
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_streamcons_75ce67: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
