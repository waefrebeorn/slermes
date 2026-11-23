#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/restart_notice.py
(gate w8_restartnoti_29f67a).

planned_restart_marker_path is compared directly. write_planned_restart_marker
is time/IO stubbed so the stamped payload can be compared deterministically.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path
from types import SimpleNamespace

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

STATE = {"default_home": "C:/default/home"}
CAP = {}

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

_mod("hermes_constants", get_process_hermes_home=lambda: Path(STATE["default_home"]))
def _capture(path, payload, **kwargs):
    CAP["path"] = str(path)
    CAP["payload"] = payload
_mod("utils", atomic_json_write=_capture)

spec = importlib.util.spec_from_file_location("restart_notice_live", Q + "/gateway/restart_notice.py")
live = importlib.util.module_from_spec(spec)
sys.modules["restart_notice_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "marker_path":
            STATE["default_home"] = r.get("default_home", "C:/default/home")
            h = r.get("home")
            py = str(live.planned_restart_marker_path(Path(h) if h is not None else None))
        elif func == "marker_payload":
            live.time = SimpleNamespace(time=lambda: r.get("now", 0.0))
            CAP.clear()
            live.write_planned_restart_marker(via_service=bool(r.get("via_service", False)),
                                              detached=bool(r.get("detached", False)))
            py = CAP["payload"]
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w8_restartnoti_29f67a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
