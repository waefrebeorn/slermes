#!/usr/bin/env python3
"""Differential oracle for toolset_distributions.py (gate w5_toolsetdist_3d9ebc).

random.random() and toolsets.validate_toolset() are injected per case: rolls
come from the fixture and a name is valid iff it is in the fixture's `valid`
array. Warning prints from the live sampler are captured so oracle stdout
stays clean.
"""
import contextlib
import importlib.util
import io
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

_valid_set = set()
ts = types.ModuleType("toolsets")
ts.validate_toolset = lambda name: name in _valid_set
sys.modules["toolsets"] = ts

spec = importlib.util.spec_from_file_location("w5_toolsetdist_live", Q + "/toolset_distributions.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_toolsetdist_live"] = live
spec.loader.exec_module(live)


def compute(r):
    global _valid_set
    f = r.get("func")
    if f == "get_distribution":
        return live.get_distribution(r.get("name", ""))
    if f == "validate_distribution":
        return live.validate_distribution(r.get("name", ""))
    if f == "entry_members":
        return live._entry_members(r.get("entry", ""))
    if f == "sample":
        _valid_set = set(r.get("valid") or [])
        live.validate_toolset = lambda name: name in _valid_set
        rolls = iter(r.get("rolls") or [])
        live.random.random = lambda: next(rolls)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            try:
                return live.sample_toolsets_from_distribution(r.get("name", ""))
            except ValueError:
                return None
    return None


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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH toolsetdist %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_toolsetdist_3d9ebc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
