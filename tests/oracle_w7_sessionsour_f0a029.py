#!/usr/bin/env python3
"""Differential oracle for agent/session_source.py (gate w7_sessionsour_f0a029)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/session_source.py"
spec = importlib.util.spec_from_file_location("w7_sessionsour_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "create_surface":
        return live.create_surface(r.get("source", ""))
    if func == "session_source_for":
        env = {
            "HERMES_SESSION_SOURCE": r.get("source", ""),
            "HERMES_SINGLE_QUERY_SESSION": "1" if r.get("single_query") else "0",
            "HERMES_SESSION_SOURCE_EXPLICIT": "1" if r.get("explicit") else "0",
        }
        live.get_session_env = lambda key, default="": env.get(key, default)
        return live.session_source_for(r.get("platform"))
    if func == "is_ui_transport":
        return r.get("source", "") in live.UI_TRANSPORT_SOURCES
    if func == "is_cli_family":
        return r.get("source", "") in live.CLI_FAMILY_SOURCES
    if func == "is_daybreak":
        return r.get("source", "") in live.DAYBREAK_SOURCES
    if func == "is_attended":
        return r.get("source", "") in live.ATTENDED_SOURCES
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_sessionsour_f0a029: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
