#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profile_cmd.py (w8_profilecmd_e127ec gate)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

LIVE_NAME = "w8_profilecmd_e127ec_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/profile_cmd.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

ACTION_KEYS = sorted(k for k in live.PROFILE_ACTIONS if k is not None)


def compute(r):
    f = r.get("func")
    if f == "confirm":
        return r.get("answer", "").strip().lower() in {"y", "yes"}
    if f == "is_active":
        name = r.get("name", "")
        active = r.get("active", "")
        default = bool(r.get("is_default", False))
        return name == active or (active == "default" and default)
    if f == "action_keys":
        return ACTION_KEYS
    if f == "has_action":
        return r.get("action", "") in ACTION_KEYS
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
        print("MISMATCH profilecmd %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_profilecmd_e127ec: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
