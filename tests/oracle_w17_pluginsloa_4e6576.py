#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_load_guard.py (w17_pluginsloa_4e6576).

Builds a class chain (the MRO) from the fixture's per-class attribute-name
lists, applies the LIVE decorator, and reads back the names that carry
functools.wraps' __wrapped__ marker. Compares to the C name-selection result.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/plugins_load_guard.py"
spec = importlib.util.spec_from_file_location("w17_pluginsloa_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_pluginsloa_live"] = live
spec.loader.exec_module(live)


def guarded_names(sets):
    base = object
    for names in sets:
        attrs = {n: (lambda self, *a, **k: None) for n in names}
        base = type("M", (base,), attrs)
    live.ignore_registrars_after_abandoned_load(base)
    return sorted(n for n in dir(base) if hasattr(getattr(base, n, None), "__wrapped__"))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "guardable":
        continue
    py = guarded_names(r.get("sets", []))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_pluginsloa_4e6576: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
