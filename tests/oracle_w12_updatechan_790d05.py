#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_channel.py (gate w12_updatechan_790d05)."""
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

_pm = types.ModuleType("pm")
_pm.__path__ = []
sys.modules["pm"] = _pm
_pe = types.ModuleType("pm.environments")
_pe.install_key = lambda p: "deadbeef"
_pe.installs_root = lambda: None
sys.modules["pm.environments"] = _pe
_pp = types.ModuleType("pm.paths")
_pp.install_root = lambda: "."
sys.modules["pm.paths"] = _pp
_rc = types.ModuleType("hermes_cli.release_channels")
_rc.validate_name = lambda v: v
sys.modules["hermes_cli.release_channels"] = _rc

live = load("w12_updatechan_790d05_live", QUARRY + "/hermes_cli/update_channel.py")

def compute(r):
    f = r.get("func")
    if f == "is_canary_tag":
        return live.is_canary_tag(r.get("tag", ""))
    if f == "canary_timestamp":
        return live.canary_timestamp(r.get("tag", ""))
    if f == "canary_tag_for_date":
        try:
            return live.canary_tag_for_date(r.get("version", ""), r.get("date_utc", ""))
        except ValueError:
            return None
    if f == "install_records":
        return live._install_records(r.get("config"))
    if f == "package_channel":
        return live._package_channel(r.get("stamp") or {})
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
        print("ORACLE w12_updatechan_790d05: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w12_updatechan_790d05 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w12_updatechan_790d05: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
