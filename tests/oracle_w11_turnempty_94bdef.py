#!/usr/bin/env python3
"""Differential oracle for agent/turn_empty_response.py (gate w11_turnempty_94bdef).

Imports the LIVE module with stubbed imports; the three pure helpers are
compared without touching the agent loop or any provider.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name); m.__path__ = []; sys.modules[name] = m
    return m

agent_pkg = _pkg("agent")

def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    setattr(agent_pkg, name.rsplit(".", 1)[-1], m)
    return m

_stub("agent.empty_response_guard", DEFAULT_EMPTY_RETRY_BUDGET=3,
      record_empty_attempt=lambda *a, **k: None,
      empty_retry_budget=lambda *a, **k: 3,
      deterministic_empty=lambda *a, **k: False,
      streak_cost_usd=lambda *a, **k: None)
_stub("agent.message_metadata", append_message=lambda *a, **k: None)
_stub("agent.message_sanitization", _sanitize_surrogates=lambda x: x)
_stub("agent.turn_context_compaction", _refund_api_call=lambda a, n: n)
_stub("agent.turn_recovery", interruptible_backoff_sleep=lambda *a, **k: None)

SRC = QUARRY + "/agent/turn_empty_response.py"
spec = importlib.util.spec_from_file_location("w11_turnempty_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_turnempty_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "latest_display":
        return live._latest_user_display_kind(r.get("messages") or [])
    if func == "machinery_quiet":
        return live._notification_turn_without_tool_round(r.get("messages") or [])
    if func == "retry_hint":
        agent = SimpleNamespace(_empty_response_retry_hint=r.get("hint"))
        msgs = json.loads(json.dumps(r.get("api_messages") or []))
        suffix = live.add_empty_response_retry_hint(agent, msgs)
        return {"suffix": suffix, "messages": msgs}
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
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_turnempty_94bdef: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_turnempty_94bdef: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
