#!/usr/bin/env python3
"""Oracle for agent/conversation_compression_scaffolding.py (gate convscaf).

The three live helpers are compared directly on the fixture message dicts.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)

spec = importlib.util.spec_from_file_location(
    "agent.conversation_compression_scaffolding",
    QUARRY + "/agent/conversation_compression_scaffolding.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.conversation_compression_scaffolding"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    message = r.get("message")
    if func == "text":
        py = live._message_text(message)
    elif func == "flag":
        py = bool(live.has_scaffolding_flag(message))
    elif func == "scaffold":
        py = bool(live.is_user_role_scaffolding(message))
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE convscaf: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
