#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_hooks.py (gate w7_auxiliaryho_d1a407).

Loads the LIVE module by path under its package name with a stubbed
``agent.relay_runtime.current_turn`` driven from the fixture. The other pure
helpers are called directly on the live module.
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


class _Attr:
    def __init__(self, **kw):
        self.__dict__.update(kw)


_TURN = [None]


def _current_turn():
    return _TURN[0]


pkg = _mod("agent")
pkg.__path__ = [Q + "/agent"]
_mod("agent.relay_runtime", current_turn=_current_turn)

SRC = Q + "/agent/auxiliary_hooks.py"
spec = importlib.util.spec_from_file_location("agent.auxiliary_hooks", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["agent.auxiliary_hooks"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r.get("func")
    if f == "constants":
        return {"PRE_AUXILIARY_CALL": live.PRE_AUXILIARY_CALL,
                "POST_AUXILIARY_CALL": live.POST_AUXILIARY_CALL}
    if f == "identity":
        t = r.get("turn")
        if t is None:
            _TURN[0] = None
        else:
            lease = _Attr(**(t.get("lease") or {}))
            _TURN[0] = _Attr(lease=lease, task_id=t.get("task_id"), turn_id=t.get("turn_id"))
        return live._parent_turn_identity()
    if f == "system_prompt":
        return live._system_prompt(r.get("messages"), r.get("kwargs") or {})
    if f == "first_choice":
        msg, fr = live._first_choice_message(r.get("response"))
        return {"message": msg, "finish_reason": fr}
    if f == "field":
        return live._field(r.get("obj"), r.get("name", ""))
    if f == "message_count":
        return len(r.get("messages") or r.get("input") or [])
    if f == "retry_counts":
        n = int(r.get("raw") or 0)
        return {"retry": n, "call": n + 1}
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
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w7_auxiliaryho_d1a407: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
