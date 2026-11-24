#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_session_switch.py (gate w17_clisession_e88af5).

Both helpers are side-effect-only. The oracle supplies fake cli/agent objects
that record calls and returns a JSON summary that the C port computes from the
equivalent injected state description.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


tools = _mod("tools")
tools.__path__ = []
_mod("tools.todo_tool", TodoStore=lambda: "NEW")

SRC = Q + "/hermes_cli/cli_session_switch.py"
spec = importlib.util.spec_from_file_location("w17_clisession_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_clisession_live"] = live
spec.loader.exec_module(live)


class Agent(object):
    pass


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
    has_agent = bool(r.get("has_agent", False))
    try:
        if func == "end_session":
            calls = []
            agent = Agent()
            agent._flush_messages_to_session_db = lambda hist, conversation_history=None: calls.append("flush")
            db = types.SimpleNamespace(
                end_session=lambda sid, reason: calls.append(("end", sid, reason)))
            cli = types.SimpleNamespace(agent=(agent if has_agent else None),
                                        conversation_history=[0] * int(r.get("history_len", 0)),
                                        _session_db=db, session_id=r.get("session_id", ""))
            live._end_current_session(cli, r.get("reason", ""))
            end_call = next((a for a in calls if isinstance(a, tuple)), None)
            py = {"flush": "flush" in calls,
                  "end": [end_call[1], end_call[2]] if end_call else None}
        elif func == "sync_agent":
            calls = []
            mm_calls = []
            agent = Agent()
            if r.get("has_last_flushed", False):
                agent._last_flushed_db_idx = -1
            if r.get("has_todo", False):
                agent._todo_store = "OLD"
            if r.get("has_invalidate", False):
                agent._invalidate_system_prompt = lambda: calls.append("invalidated")
            if r.get("has_mm", False):
                if r.get("mm_present", False):
                    agent._memory_manager = types.SimpleNamespace(
                        on_session_switch=lambda sid, **kw: mm_calls.append((sid, kw)))
                else:
                    agent._memory_manager = None
            agent.reset_session_state = lambda: calls.append("reset")
            cli = types.SimpleNamespace(agent=(agent if has_agent else None),
                                        conversation_history=[0] * int(r.get("history_len", 0)))
            live._sync_agent_to_session(cli, r.get("session_id", ""),
                                        parent_session_id=r.get("parent_session_id", ""),
                                        reason=r.get("reason", ""))
            if not has_agent:
                py = {"noop": True, "session_id": None, "reset": False,
                      "last_flushed": None, "invalidated": False, "mm": None,
                      "todo_replaced": False}
            else:
                mm = None
                if mm_calls:
                    sid, kw = mm_calls[0]
                    mm = [sid, kw["parent_session_id"], kw["reason"]]
                py = {"noop": False,
                      "session_id": agent.session_id,
                      "reset": "reset" in calls,
                      "last_flushed": getattr(agent, "_last_flushed_db_idx", None),
                      "invalidated": "invalidated" in calls,
                      "mm": mm,
                      "todo_replaced": getattr(agent, "_todo_store", "OLD") == "NEW"}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_clisession_e88af5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
