#!/usr/bin/env python3
"""Differential oracle for agent/terminal_approval_batch.py (w17_terminalap_428119).

Replays three pure decisions against the LIVE module with injected contextvars
and fakes for the middleware imports: grouping of adjacent terminal calls, the
prepared-guard consumption decision, and the "preparing" flag.
"""
import contextvars
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


_pkg("agent")
_pkg("tools")

# module-level import: from tools.thread_context import propagate_context_to_thread
tc = types.ModuleType("tools.thread_context")
tc.propagate_context_to_thread = lambda fn: fn
sys.modules["tools.thread_context"] = tc

# in-function import: from tools.approval_context import _approval_tool_call_id / _get_approval_mode
ac = types.ModuleType("tools.approval_context")
ac._approval_tool_call_id = contextvars.ContextVar("w17_approval_call_id", default="")
MODE = [""]
ac._get_approval_mode = lambda: MODE[0]
sys.modules["tools.approval_context"] = ac

# in-function import: from agent.tool_executor import _parse_tool_call
te = types.ModuleType("agent.tool_executor")
def _parse_tool_call(agent, call, flatten_probe=False):
    return types.SimpleNamespace(name=call.get("name", ""),
                                 parse_error=call.get("parse_error"))
te._parse_tool_call = _parse_tool_call
sys.modules["agent.tool_executor"] = te

NAME = "w17_terminalap_428119_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "terminal_approval_batch.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def guard(r):
    slot_exists = bool(r.get("slot_exists", False))
    preparing = bool(r.get("preparing", False))
    failure_seen = bool(r.get("failure_seen", False))
    expected = r.get("expected_call_id", "")
    command = r.get("command", "")
    env_type = r.get("env_type", "")
    has_host = bool(r.get("has_host_access", False))
    decision = r.get("decision")
    MODE[0] = r.get("approval_mode", "")

    if slot_exists:
        parsed = types.SimpleNamespace(
            ref=lambda tid: types.SimpleNamespace(call_id=expected))
        batch = types.SimpleNamespace(failure_seen=failure_seen, task_id="t")
        slot = types.SimpleNamespace(
            preparing=preparing, decision=decision,
            guard_key=(r.get("guard_command", ""), r.get("guard_env_type", ""),
                       bool(r.get("guard_host_access", False))),
            batch=batch, parsed=parsed, check_cancelled=lambda: None)
    else:
        slot = None

    ts = live._slot.set(slot)
    ti = ac._approval_tool_call_id.set(r.get("ctx_call_id", ""))
    try:
        return live.consume_prepared_guard(command, env_type, has_host)
    finally:
        live._slot.reset(ts)
        ac._approval_tool_call_id.reset(ti)


def compute(r):
    func = r.get("func")
    if func == "runs":
        groups = list(live.terminal_approval_runs(object(), r.get("calls", [])))
        return [[c.get("tag", "") for c in g] for g in groups]
    if func == "guard":
        return guard(r)
    if func == "preparing":
        slot = (types.SimpleNamespace(preparing=bool(r.get("preparing", False)))
                if r.get("slot_exists", False) else None)
        tok = live._slot.set(slot)
        try:
            return live.preparing_terminal_approval()
        finally:
            live._slot.reset(tok)
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w17_terminalap_428119: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
