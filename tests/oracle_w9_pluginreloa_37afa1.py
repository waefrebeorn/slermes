#!/usr/bin/env python3
"""Differential oracle for providers/plugin_reload.py (gate w9_pluginreloa_37afa1).

os.walk/os.stat are injected from the fixture as a namespace bound only in the
LIVE module; tree_fingerprint() itself is the real function under test.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w9_pluginreloa_37afa1_live", QUARRY + "/providers/plugin_reload.py")


def compute(r):
    f = r.get("func")
    if f != "tree_fingerprint":
        return None
    files = r.get("files", [])
    fake = types.SimpleNamespace()
    fake.path = os.path

    def fake_walk(top):
        return [(str(top), [], [x["name"] for x in files])]

    def fake_stat(path):
        base = os.path.basename(str(path))
        for x in files:
            if x["name"] == base:
                return types.SimpleNamespace(st_size=int(x["size"]),
                                             st_mtime_ns=int(x["mtime_ns"]))
        raise OSError("stat: no such file")

    fake.walk = fake_walk
    fake.stat = fake_stat
    old = live.os
    live.os = fake
    try:
        return live.tree_fingerprint(Path("plug"))
    finally:
        live.os = old


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
        print("ORACLE w9_pluginreloa_37afa1: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w9_pluginreloa_37afa1 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_pluginreloa_37afa1: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
