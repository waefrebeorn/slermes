#!/usr/bin/env python3
"""Differential oracle for hermes_cli/__init__.py (w7_init_51a22b gate)."""
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


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m


# The lazy __getattr__ reads the install stamp through these local imports;
# inject the fixture's base_version so the oracle is self-contained.
STATE = {"base": None}
stub("hermes_cli", __path__=[Q+"/hermes_cli"])
stub("hermes_cli.steward", read_install_stamp=lambda root: {"baseVersion": STATE["base"]})
stub("pm")
stub("pm.paths", repo_root=lambda: "root")

LIVE_NAME = "w7_init_51a22b_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/__init__.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

PUBLIC = [n for n in dir(live) if not n.startswith("__")]


def compute(r):
    f = r.get("func")
    if f == "doc":
        return live.__doc__
    if f == "release_date":
        return live.__release_date__
    if f == "public_names":
        return PUBLIC
    if f == "has_symbol":
        return r.get("name", "") in PUBLIC
    if f == "getattr":
        STATE["base"] = r.get("base_version")
        try:
            return live.__getattr__(r.get("name", ""))
        except AttributeError:
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
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH init %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_init_51a22b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
