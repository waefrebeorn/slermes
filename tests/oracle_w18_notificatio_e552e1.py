#!/usr/bin/env python3
"""Differential oracle for gate w18_notificatio_e552e1 (tui_gateway/notification_park.py)."""
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

_tg = types.ModuleType("tui_gateway")
_tg.__path__ = [Q + "/tui_gateway"]
sys.modules["tui_gateway"] = _tg

NAME = "w18_notificatio_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/tui_gateway/notification_park.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w18_notificatio_e552e1"


def compute(r):
    func = r.get("func")
    inp = r.get("in") or {}
    evt = inp.get("evt") or {}
    if func == "parkable":
        return bool(live._parkable(evt))
    if func == "is_progress":
        return bool(live._is_progress(evt))
    if func == "key":
        return live._key(evt)
    if func == "admit_locked":
        live._parked = [(float(row[0]), row[1]) for row in (inp.get("state") or [])]
        admitted, displaced = live._admit_locked(evt, inp.get("key", ""))
        return {"admitted": bool(admitted), "displaced": displaced}
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
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
