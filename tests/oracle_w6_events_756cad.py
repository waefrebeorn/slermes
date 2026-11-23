#!/usr/bin/env python3
"""Differential oracle for acp_adapter/events.py (gate w6_events_756cad).

Loads the LIVE module by path under its package name with a parent-package
bootstrap: heavy ACP schema / client and the relative `.tools` import are
stubbed via sys.modules registration. The pure todo->plan translation and FIFO
upgrade are then replayed against the live functions.
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


schema = _mod("acp.schema")
for _n in ("AgentPlanUpdate", "PlanEntry"):
    setattr(schema, _n, type(_n, (_Attr,), {}))
_mod("acp", Client=type("Client", (object,), {}),
     update_agent_message_text=lambda *a, **k: None,
     update_agent_thought_text=lambda *a, **k: None)


def _json_loads_maybe(value):
    if not isinstance(value, str):
        return value
    try:
        return json.loads(value)
    except Exception:
        pass
    try:
        return json.JSONDecoder().raw_decode(value.lstrip())[0]
    except Exception:
        return None


# Parent package: stops the real acp_adapter/__init__ from running and lets the
# relative `.tools` import resolve to our stub.
pkg = _mod("acp_adapter")
pkg.__path__ = [Q + "/acp_adapter"]
_mod("acp_adapter.tools",
     _json_loads_maybe=_json_loads_maybe,
     build_tool_abandoned=lambda *a, **k: None,
     build_tool_complete=lambda *a, **k: None,
     build_tool_start=lambda *a, **k: None,
     coerce_tool_args=lambda *a, **k: {},
     make_tool_call_id=lambda *a, **k: "tc-stub")

SRC = Q + "/acp_adapter/events.py"
spec = importlib.util.spec_from_file_location("acp_adapter.events", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["acp_adapter.events"] = live
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
    if f == "plan_status":
        raw = r.get("status")
        rs = str(raw or "pending").strip()
        return live._PLAN_STATUS.get(rs, "pending")
    if f == "plan_update":
        pu = live._build_plan_update_from_todo_result(r.get("result"))
        if pu is None:
            return None
        return {"session_update": pu.session_update,
                "entries": [{"content": e.content, "priority": e.priority, "status": e.status}
                            for e in pu.entries]}
    if f == "upgrade_queue":
        ids = dict(r.get("ids") or {})
        q = live._upgrade_queue(ids, r.get("name", ""))
        return list(q) if q is not None else None
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
print("ORACLE w6_events_756cad: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
