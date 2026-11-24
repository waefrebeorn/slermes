#!/usr/bin/env python3
"""Differential oracle for tools/connectors/dispatch.py (w20_dispatch_c9fbe4).

The live dispatch module is imported with its real partition/merge policy
(``tools.connectors.gateway.merge``) and real ``MAX_CALLS_PER_DISPATCH``, while
the gateway bridge, the local executor, the interrupt flag and the local-batch
rejection hook are scripted from each fixture line.  Compared values: the
partition of a batch, the three-key model payload of a single connector call
(plus the data-use names it records), and the assembled batch result including
the per-entry ``<id>#<position>`` call ids.
"""
import importlib.util
import json
import os
import sys
import types
from dataclasses import dataclass, field

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


STATE = {}


class _Hook:
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


def _record_data_use(names):
    STATE["data_use"] = list(names or ())


def _run_remote(planned, tool_call_id, **kw):
    STATE["run_remote_ids"] = [p.position for p in planned]
    STATE["run_remote_tool_call_id"] = tool_call_id
    return [dict(entry) for entry in STATE.get("entries", [])]


def _is_interrupted():
    n = STATE.get("probe", 0)
    STATE["probe"] = n + 1
    gate = STATE.get("interrupt_at")
    return gate is not None and n >= gate


def _handle_function_call(name, arguments, **kw):
    idx = STATE.get("call", 0)
    STATE["call"] = idx + 1
    payloads = STATE.get("payloads") or []
    payload = payloads[idx] if idx < len(payloads) else None
    if isinstance(payload, str):
        payload = payload.replace("$ID$", str(kw.get("tool_call_id")))
    STATE.setdefault("call_ids", []).append(kw.get("tool_call_id"))
    return payload


@dataclass
class Ids:
    tool_call_id: str | None = None
    session_key: str = "s"
    turn_id: str | None = None
    task_id: str | None = None


_mk("tools.connectors.data_use", record_data_use=_record_data_use)
_mk("tools.connectors.gateway.bridge", run_remote=_run_remote)
_mk("model_tools", handle_function_call=_handle_function_call,
    suppress_post_tool_call_hook=lambda value: _Hook(value))
_mk("tools.interrupt", is_interrupted=_is_interrupted)

NAME = "w20_dispatch_c9fbe4_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "tools", "connectors", "dispatch.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

from tools.connectors.gateway.merge import partition_calls  # noqa: E402  (real policy)


def _partition_obj(calls):
    part = partition_calls(calls)
    return {
        "local": [[pos, call] for pos, call in part.local],
        "remote": [{"position": p.position, "name": p.name, "connector": p.connector,
                    "tool": p.tool, "arguments": p.arguments} for p in part.remote],
        "errors": list(part.errors),
    }


def _reset(r):
    STATE.clear()
    STATE["call"] = 0
    STATE["probe"] = 0
    STATE["payloads"] = r.get("payloads") or []
    STATE["interrupt_at"] = r.get("interrupted_at")
    STATE["entries"] = r.get("entries") or []


def expected(r):
    f = r.get("func")
    calls = r.get("calls")
    if not isinstance(calls, list):
        calls = []
    if f == "max_calls":
        return live.MAX_CALLS_PER_DISPATCH
    if f == "partition":
        return _partition_obj(calls)
    if f == "connector_call":
        _reset(r)
        try:
            text = live.dispatch_connector_call(r.get("name"), r.get("arguments"),
                                                r.get("tool_call_id"), turn_id=r.get("turn_id"),
                                                task_id=r.get("task_id"))
        except Exception as exc:  # noqa: BLE001
            return {"status": "raised", "type": type(exc).__name__, "message": str(exc)}
        return {"result": json.loads(text), "data_use": list(STATE.get("data_use") or [])}
    if f == "connector_batch":
        _reset(r)
        ids = Ids(tool_call_id=r.get("ids", {}).get("tool_call_id") if isinstance(r.get("ids"), dict) else None)
        text = live.dispatch_connector_batch(
            calls, ids, user_task="u", enabled_tools=(), middleware_trace=[],
            enabled_toolsets=(), disabled_toolsets=())
        return json.loads(text)
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_dispatch_c9fbe4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
