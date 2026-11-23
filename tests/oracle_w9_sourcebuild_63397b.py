#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/source_build.py
(w9_sourcebuild_63397b gate). The live _npmrc_script_settings filters the
settings read by an injected pm.index_config.read_npmrc stand-in."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


pm = _mod("pm")
pm.__path__ = []
idx = _mod("pm.index_config")
idx.__path__ = []
CURRENT = {}


def _read_npmrc(npmrc, env):
    return dict(CURRENT)


idx.read_npmrc = _read_npmrc

SRC = Q + "/hermes_cli/source_build.py"
spec = importlib.util.spec_from_file_location("sourcebuild_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sourcebuild_w9_live"] = live
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
    func = r.get("func")
    if func != "npmrc":
        continue
    try:
        CURRENT = r.get("settings", {})
        py = live._npmrc_script_settings(None, {})
        py = [tuple(p) for p in py]
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    got_cmp = [tuple(p) for p in got] if isinstance(got, list) else got
    if py != got_cmp:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_sourcebuild_63397b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
