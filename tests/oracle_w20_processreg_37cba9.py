#!/usr/bin/env python3
"""Differential oracle for tools/process_registry_handoff.py (w20_processreg_37cba9).

handoff_process imports its three collaborators inside the function, so
tools.process_registry, tools.delegate_tool_registry and tools.terminal_tool are
stubbed here; the live function then runs for every harness line against a fake
spawn tree described by the fixture, and its return dict is compared to `out`.
"""
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


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _Registry:
    def __init__(self, spec):
        self.spec = spec

    def transfer_ownership(self, session_id, from_owner=None, to_owner=None, to_task_id=None,
                           to_session_key=None, note=None):
        if not self.spec.get("transfer_ok"):
            return None
        return types.SimpleNamespace(id=self.spec.get("transfer_session_id"),
                                     command=self.spec.get("transfer_command"))


REGISTRY = _Registry({})
SUBAGENTS = {}
_mk("tools")
_mk("tools.process_registry", process_registry=REGISTRY)
_mk("tools.delegate_tool_registry", _active_subagents=SUBAGENTS,
    _active_subagents_lock=__import__("threading").Lock())
_mk("tools.terminal_tool", _resolve_container_task_id=lambda owner: owner)

NAME = "w20_processreg_37cba9_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "tools", "process_registry_handoff.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class Parent:
    def __init__(self, current, session):
        if current is not None:
            self._current_task_id = current
        if session is not None:
            self.session_id = session


class Child:
    def __init__(self, parent, handed):
        alive = parent if parent is not None else None
        self._delegate_parent_ref = lambda: alive
        if handed is not None:
            self._handed_off_processes = list(handed)


def expected(r):
    f = r.get("func")
    if f == "max_handoffs_per_child":
        return int(live._MAX_HANDOFFS_PER_CHILD)
    if f != "handoff":
        raise AssertionError("unknown func %r" % (f,))
    world = r.get("world") if isinstance(r.get("world"), dict) else {}
    task_id = r.get("task_id") or ""
    parent = Parent(world.get("parent_current_task_id"),
                    world.get("parent_session_id"))
    child = Child(parent, [None] * int(world.get("handed_before") or 0))
    REGISTRY.spec = world
    SUBAGENTS.clear()
    if world.get("has_child"):
        SUBAGENTS[str(task_id)] = {"agent": child if world.get("parent_live") else Child(None, None)}
    return live.handoff_process(r.get("session_id") or "", r.get("args") or {}, task_id)


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
print("ORACLE w20_processreg_37cba9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
