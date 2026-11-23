#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cpu_profile.py (gate w8_cpuprofile_dfa160).

Logic under test: resolve_profile_path(path).  The hermes home seam is stubbed
with the fixture's `home`, and HOME/USERPROFILE are set to the same value so
Path.expanduser() agrees with the C port.  The oracle compares the resolved
path in POSIX spelling (Path.as_posix()) with the C string.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Stubbed before loading; live resolve_profile_path imports it lazily.
_hc = _stub("hermes_constants", get_hermes_home=lambda: Path("C:/home/u"))

LIVE_NAME = "w8_cpuprofile_dfa160_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/cpu_profile.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "resolve_profile_path_ex":
        continue

    home = r.get("home", "C:/home/u")
    _hc.get_hermes_home = lambda h=home: Path(h)
    os.environ["HOME"] = home
    os.environ["USERPROFILE"] = home
    py = live.resolve_profile_path(r.get("path", "")).as_posix()

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH resolve_profile_path_ex %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_cpuprofile_dfa160: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
