#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_custody.py (gate w12_updatecust_05309c)."""
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

live = load("w12_updatecust_05309c_live", QUARRY + "/hermes_cli/update_custody.py")

def compute(r):
    f = r.get("func")
    if f == "git_subcommand":
        return live.git_subcommand(r.get("args", []))
    if f == "is_local_mutator":
        return live.is_local_mutator(r.get("args", []))
    if f == "git_argv":
        dv = r.get("devnull", os.devnull)
        old = live._MUTATOR_CONFIG
        live._MUTATOR_CONFIG = ("-c", "core.fsmonitor=false", "-c", "credential.helper=",
                                "-c", "core.hooksPath=%s" % dv)
        try:
            return live.git_argv(r.get("git_cmd", []), r.get("args", []))
        finally:
            live._MUTATOR_CONFIG = old
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
        print("ORACLE w12_updatecust_05309c: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w12_updatecust_05309c %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w12_updatecust_05309c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
