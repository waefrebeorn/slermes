#!/usr/bin/env python3
"""Differential oracle for session_rules/__init__.py (w8_init_8bc62e gate)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Parent-package bootstrap: spec_from_file_location on a package __init__ needs
# a real package object with __path__ already registered in sys.modules.
parent = types.ModuleType("session_rules")
parent.__path__ = [Q + "/session_rules"]
sys.modules["session_rules"] = parent

LIVE_NAME = "w8_init_8bc62e_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/session_rules/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

PUBLIC = [n for n in dir(live) if not n.startswith("__")]


def compute(r):
    f = r.get("func")
    if f == "doc":
        return live.__doc__
    if f == "public_names":
        return PUBLIC
    if f == "has_symbol":
        return r.get("name", "") in PUBLIC
    if f == "is_package":
        return hasattr(live, "__path__")
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
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH init %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_init_8bc62e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
