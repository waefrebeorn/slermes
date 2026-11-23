#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/models_profile_cache.py."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/models_profile_cache.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

spec = importlib.util.spec_from_file_location("profilecache_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["profilecache_live"] = live
spec.loader.exec_module(live)


class _Module:
    pass


def compute(r):
    if r.get("func") != "run":
        return None
    live._SLOTS_BY_HOME.clear()
    mod = _Module()
    for k, v in (r.get("initial") or {}).items():
        setattr(mod, k, v)
    results = []
    for op in r.get("ops") or []:
        override = op.get("override")
        home = op.get("home", "")
        live.get_hermes_home_override = lambda o=override: o
        live.hermes_home_key = lambda h=home: h
        if op["op"] == "set":
            live.profile_slot_set(mod, op["attr"], op.get("value"))
        elif op["op"] == "get":
            results.append(live.profile_slot_get(mod, op["attr"], op.get("default")))
    return results


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
        print(f"ORACLE profilecache: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH profilecache case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE profilecache: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
