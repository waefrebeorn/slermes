#!/usr/bin/env python3
"""Differential oracle for gate w14_inlinetool_874779 (agent/inline_tool_executors.py)."""
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")
_pkg("tools")
_pkg("persistence")
_mod("agent.prompt_cache_scope", conversation_id=lambda a: None)
_mod("tools.arg_coercion", coerce_tool_args=lambda n, a: a)
_mod("tools.arg_validation", call_with_arg_checks=lambda e, a, f: f(a))
_mod("tools.registry", registry=types.SimpleNamespace(
    get_entry=lambda n: None, dispatch=lambda *a, **k: None))
_mt = _mod("model_tools", _LEGACY_TOOL_ALIASES=frozenset(), _AGENT_LOOP_TOOLS=frozenset())
_mod("tools.connectors", is_connector_name=lambda n: False)
_mod("tools.tool_search", is_bridge_tool=lambda n: False)

NAME = "w14_inlinetool_874779_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "inline_tool_executors.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _agent(r):
    a = types.SimpleNamespace()
    for key in ("session_id", "_current_turn_id", "_current_api_request_id"):
        setattr(a, key, r.get(key, ""))
    return a


def compute(r):
    f = r.get("func")
    if f == "tool_hook_ids":
        sys.modules["agent.prompt_cache_scope"].conversation_id = lambda a: r.get("conversation_id")
        return live.tool_hook_ids(_agent(r), r.get("task_id", ""), r.get("tool_call_id"))
    if f == "is_context_engine_tool":
        a = types.SimpleNamespace(_context_engine_tool_names=r.get("names"))
        return live.is_context_engine_tool(a, r.get("function_name", ""))
    if f == "is_memory_provider_tool":
        names = r.get("tool_names") or []
        mm = types.SimpleNamespace(has_tool=lambda n: n in names) if r.get("has_manager", False) else None
        a = types.SimpleNamespace(_memory_manager=mm)
        return live.is_memory_provider_tool(a, r.get("function_name", ""))
    if f == "pre_registry_route":
        _mt._LEGACY_TOOL_ALIASES = frozenset(r.get("legacy_aliases") or [])
        _mt._AGENT_LOOP_TOOLS = frozenset(r.get("agent_loop_tools") or [])
        cn = set(r.get("connector_names") or [])
        bn = set(r.get("bridge_names") or [])
        sys.modules["tools.connectors"].is_connector_name = lambda n: n in cn
        sys.modules["tools.tool_search"].is_bridge_tool = lambda n: n in bn
        if r.get("has_agent", False):
            ce = r.get("context_engine_names") or []
            mp = r.get("memory_provider_names") or []
            a = types.SimpleNamespace(
                _context_engine_tool_names=ce,
                _memory_manager=types.SimpleNamespace(has_tool=lambda n: n in mp))
        else:
            a = None
        return live.pre_registry_route(r.get("function_name", ""), a)
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        if not isinstance(r, dict) or "out" not in r:
            continue
        cases += 1
        py, got = compute(r), r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
    print("ORACLE w14_inlinetool_874779: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
