#!/usr/bin/env python3
"""Differential oracle for hermes_cli/setup_quick.py (w8_setupquick_c8d6f5 gate)."""
import importlib.util
import json
import sys
import copy

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

LIVE_NAME = "w8_setupquick_c8d6f5_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/setup_quick.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "minimize_config":
        cfg = copy.deepcopy(r.get("config", {}))
        live._blank_slate_minimize_config(cfg)
        return cfg
    if f == "messaging_platform":
        name = r.get("name", "")
        match = next(((plat, emoji) for needle, plat, emoji in live._MESSAGING_PLATFORMS
                      if needle in name), None)
        return list(match) if match else None
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
        print("MISMATCH setupquick %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_setupquick_c8d6f5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
