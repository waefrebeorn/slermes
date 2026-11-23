#!/usr/bin/env python3
"""Differential oracle for tui_gateway/user_messages.py (gate w13_usermessag_547811).

Imports the LIVE module directly (it only depends on typing) and compares each
harness `out` against the live function result.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tui_gateway/user_messages.py"
spec = importlib.util.spec_from_file_location("um_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["um_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    surface = r.get("surface")
    rec = r.get("recoverable", True)
    if f == "identity":
        return live._identity(surface)
    if f == "title":
        return live.turn_error_title(surface)
    if f == "hint":
        return live.turn_error_hint(surface, rec)
    if f == "text":
        return live.turn_error_text(r.get("error", ""), surface, recoverable=rec)
    if f == "busy":
        return live.busy_message(r.get("command", ""), r.get("compressing", False))
    if f == "agent_init":
        return live.agent_init_failed_message(r.get("exc", ""))
    if f == "resume_failed":
        return live.resume_failed_message(r.get("exc", ""))
    if f == "still_starting":
        return live.AGENT_STILL_STARTING
    if f == "build_abandoned":
        return live.AGENT_BUILD_ABANDONED
    if f == "missing_for_turn":
        return live.AGENT_MISSING_FOR_TURN
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
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_usermessag_547811: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
