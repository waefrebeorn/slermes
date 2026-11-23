#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_handoff.py (gate w12_updatehand_b18a8a)."""
import importlib.util
import json
import os
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

live = load("w12_updatehand_b18a8a_live", QUARRY + "/hermes_cli/update_handoff.py")

def compute(r):
    f = r.get("func")
    if f == "is_post_swap_child":
        key = live.POST_SWAP_ENV
        val = r.get("env_value")
        had = key in os.environ
        prev = os.environ.get(key)
        if val is None:
            os.environ.pop(key, None)
        else:
            os.environ[key] = str(val)
        try:
            return live.is_post_swap_child()
        finally:
            if had:
                os.environ[key] = prev
            else:
                os.environ.pop(key, None)
    if f == "takeover_request":
        return live._takeover_request(r.get("payload", {}), r.get("argv_tail", []))
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
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w12_updatehand_b18a8a: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w12_updatehand_b18a8a %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w12_updatehand_b18a8a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
