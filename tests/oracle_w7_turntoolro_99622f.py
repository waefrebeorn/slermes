#!/usr/bin/env python3
"""Differential oracle for agent/turn_tool_round.py (gate w7_turntoolro_99622f).

Exercises the pure classification decisions of stage_tool_call_message against the
LIVE module constants and the LIVE _STALE_MARKER_RE pattern read from the quarry.
"""
import importlib.util
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("agent")
_stub("agent.message_metadata", append_message=lambda *a, **k: None)
_stub("agent.message_sanitization", normalize_provider_tool_call_ids=lambda *a, **k: None)
_stub("session_rules")
_stub("session_rules.turns", coalesce_tool_call_id=lambda tc: "")
_stub("agent.turn_preflight", compress_after_tool_results=lambda *a, **k: None)
_stub("agent.turn_tool_validation", validate_tool_calls=lambda *a, **k: None)

LIVE_NAME = "w7_turntoolro_99622f_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/agent/turn_tool_round.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

# Read the LIVE regex pattern from the quarry source (no hardcoded copy).
src = open(Q + "/agent/conversation_loop.py", encoding="utf-8").read()
m = re.search(r'_STALE_MARKER_RE\s*=\s*re\.compile\((r?"[^"\n]+")\)', src)
assert m, "LIVE _STALE_MARKER_RE not found"
stale_re = re.compile(eval(m.group(1)))

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "tool_round_facts":
        continue
    names = r.get("tool_names", [])
    content = r.get("content", "")
    py = {
        "all_housekeeping": all(n in live._HOUSEKEEPING_TOOLS for n in names),
        "line_is_reply": all(n in live._LINE_IS_REPLY_TOOLS for n in names),
        "drop_marker": bool(names and stale_re.fullmatch(content.strip())),
    }
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH tool_round_facts args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_turntoolro_99622f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
