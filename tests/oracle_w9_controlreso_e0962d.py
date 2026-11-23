#!/usr/bin/env python3
"""Differential oracle for gateway/control_resolve.py (gate w9_controlreso_e0962d)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
import gateway  # parent-package bootstrap

SRC = QUARRY + "/gateway/control_resolve.py"
NAME = "control_resolve_live_w9_controlreso"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "plain":
        return bool(live._plain_spelling(r.get("home", "")))
    if func == "requested":
        try:
            home = live._requested_home(r.get("request") or {})
            return {"ok": True, "home": home}
        except PermissionError:
            return {"ok": False, "home": None}
    if func == "constants":
        return {"verb": live.RESOLVE_VERB, "protocol": live.RESOLVE_PROTOCOL}
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_controlreso_e0962d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
