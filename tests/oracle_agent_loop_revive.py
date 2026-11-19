#!/usr/bin/env python3
"""Differential oracle for the agent_gaps.c / conversation_loop_gaps.c /
scheduler_gaps.c revival ports.

Reads the harness JSONL on stdin ({func,in,out}), replays the SAME scripted
seam results through the LIVE quarry Python (AST-extracted defs), and compares
ordered trace + ret + state + raise. Fail-closed; catches BaseException per
case (SystemExit included). Summary: ORACLE agent_loop_revive: N cases, M mismatches.

Gate:
  bash tests/run_one_oracle.sh agent_loop_revive oracle_agent_loop_revive.py "" \
       "src/agent/agent_gaps.o src/agent/conversation_loop_gaps.o src/cron/scheduler_gaps.o lib/libjson/json.o"
"""
import ast
import json
import logging
import math
import os
import re
import sys
import threading
import time
import types
from pathlib import Path
from typing import Any, Callable, Optional

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home_al"))

import __future__
_ANN_FLAG = __future__.annotations.compiler_flag
SN = types.SimpleNamespace
MISSING = object()

# func -> (module, live fn name)
TARGETS = {
    "resolve_stream_max_lifetime": ("agent/stream_max_lifetime.py", "resolve_stream_max_lifetime"),
    "exceeded": ("agent/stream_max_lifetime.py", "exceeded"),
    "reserved_session_id": ("agent/runtime_session_child.py", "reserved_session_id"),
    "_maybe_grow_local_window": ("agent/conversation_loop.py", "_maybe_grow_local_window"),
    "_midturn_request_pressure_tokens": ("agent/conversation_loop.py", "_midturn_request_pressure_tokens"),
    "_pressure_with_real_floor": ("agent/conversation_loop.py", "_pressure_with_real_floor"),
    "_moa_client_consumes_prepared_request": ("agent/conversation_loop.py", "_moa_client_consumes_prepared_request"),
    "_is_interpreter_shutdown_error": ("agent/conversation_loop.py", "_is_interpreter_shutdown_error"),
    "_target_mirror_eligible": ("cron/scheduler_delivery.py", "_target_mirror_eligible"),
    "_should_yield_tick_to_fresh_gateway": ("cron/scheduler.py", "_should_yield_tick_to_fresh_gateway"),
    "_inactivity_watchdog_loop": ("cron/scheduler_liveness.py", "_inactivity_watchdog_loop"),
    "_detect_gateway_code_skew": ("cron/scheduler.py", "_detect_gateway_code_skew"),
}
MODULE_WANTED = {}
for _f, (_m, _fn) in TARGETS.items():
    MODULE_WANTED.setdefault(_m, set()).add(_fn)


def canon(x):
    if isinstance(x, bool):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canon(v) for v in x]
    if isinstance(x, Path):
        return x.as_posix()
    return x


def seed_basics(ns):
    ns.update(dict(Any=Any, Callable=Callable, Optional=Optional, math=math, time=time,
                   logging=logging, threading=threading, sys=sys, os=os, re=re, json=json))


def extract(module_rel):
    """AST-extract ONLY the wanted live defs (none of the targets read module
    constants; every collaborator is seamed). Registers a real module in
    sys.modules so py3.14 annotation/dataclass machinery is happy."""
    src = (QUARRY / module_rel).read_text(encoding="utf-8")
    tree = ast.parse(src)
    name = "allive_" + re.sub(r"\W", "_", module_rel)
    mod = types.ModuleType(name)
    mod.__file__ = str(QUARRY / module_rel)
    sys.modules[name] = mod
    ns = mod.__dict__
    seed_basics(ns)
    wanted = MODULE_WANTED[module_rel]

    def ex(node):
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), mod.__file__, "exec",
                         flags=_ANN_FLAG, dont_inherit=True), ns)
        except Exception:
            pass

    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted:
            ex(node)
        elif isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in wanted:
                    sub.decorator_list = []   # bare method (strip @property etc.)
                    ex(sub)
    return ns


_BUILTIN_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
                "GeneratorExit": GeneratorExit, "ValueError": ValueError, "TypeError": TypeError,
                "RuntimeError": RuntimeError, "AttributeError": AttributeError, "KeyError": KeyError,
                "ImportError": ImportError, "OSError": OSError, "TimeoutError": TimeoutError,
                "Exception": Exception}


def build_exc(kind, message, bases=None):
    if kind in _BUILTIN_EXC and not bases:
        return _BUILTIN_EXC[kind](message)
    base_classes = tuple(_BUILTIN_EXC.get(b, Exception) for b in (bases or ["Exception"]))
    cls = type(kind, base_classes, {})
    return cls(message)


class Rig:
    """Per-case recording rig driven by the io scripts."""

    def __init__(self, io, ns):
        self.io = io or {}
        self.ns = ns
        self.trace = []
        self.counters = {}

    def script(self, name):
        if name not in self.io:
            return MISSING
        s = self.io[name]
        if isinstance(s, dict) and "__seq__" in s:
            seq = s["__seq__"]
            k = self.counters.get(name, 0)
            self.counters[name] = k + 1
            return seq[min(k, len(seq) - 1)] if seq else None
        return s

    def rec(self, name, args, materialize=lambda v: v):
        value = self.script(name)
        if value is MISSING:
            value = {"__raise__": "MissingScript", "message": name}
        self.trace.append({"ev": name, "args": canon(args), "ret": canon(value)})
        if isinstance(value, dict) and "__raise__" in value:
            raise build_exc(value["__raise__"], value.get("message", ""))
        return materialize(value)

    def stub(self, name, params=(), materialize=lambda v: v):
        def f(*a, **k):
            args = dict(zip(params, a))
            args.update(k)
            return self.rec(name, args, materialize)
        return f


def logger_double(rig):
    lg = SN()
    lg.debug = lambda fmt, *a, exc_info=False: rig.rec(
        "logger_debug", {"fmt": fmt, "args": [str(x) for x in a], "exc_info": exc_info})
    lg.info = lambda *a, **k: None
    lg.warning = lambda *a, **k: None
    return lg


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


def mk(x):
    """JSON -> object double: dict->SimpleNamespace, {"__call__":true}->callable."""
    if isinstance(x, dict):
        if set(x.keys()) == {"__call__"} and x["__call__"] is True:
            return lambda *a, **k: None
        return SN(**{k: mk(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mk(v) for v in x]
    return x


def unmkk(x):
    """object double -> JSON (inverse of mk) for seam-arg recording."""
    if isinstance(x, SN):
        return {k: unmkk(v) for k, v in vars(x).items()}
    if isinstance(x, (list, tuple)):
        return [unmkk(v) for v in x]
    if callable(x):
        return {"__call__": True}
    return x


# ── per-function drivers: return (ret, state_after, raise_spec) ──

def d_resolve(rig, ns, inp, state):
    stub_module("hermes_cli.config", load_config_readonly=rig.stub("load_config_readonly"))
    ns["logger"] = logger_double(rig)
    ret = ns["resolve_stream_max_lifetime"](inp.get("stale_timeout"))
    return canon(ret), state, None


def d_exceeded(rig, ns, inp, state):
    ns["time"] = SN(time=lambda: rig.rec("time_time", {}))
    slf = SN(cap=state.get("cap"), attempt=state.get("attempt"), started=state.get("started"))

    def after():
        return {"cap": canon(slf.cap), "attempt": canon(slf.attempt), "started": canon(slf.started)}
    try:
        ret = ns["exceeded"](slf, inp.get("attempt"))
    except BaseException as ex:
        return None, after(), {"kind": type(ex).__name__, "message": str(ex)}
    return canon(ret), after(), None


def d_reserved(rig, ns, inp, state):
    slf = SN(scope=state.get("scope"))
    try:
        ret = ns["reserved_session_id"](slf)
    except BaseException as ex:
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    return canon(ret), state, None


def d_grow(rig, ns, inp, state):
    stub_module("hermes_cli.local_runtime.growth",
                maybe_grow_window=rig.stub("maybe_grow_window", ("model",)))
    ns["logger"] = logger_double(rig)
    agent = mk(inp.get("agent", {}))
    comp = mk(inp.get("compressor", {}))
    try:
        ret = ns["_maybe_grow_local_window"](agent, comp, inp.get("request_tokens"))
    except BaseException as ex:
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    return canon(ret), state, None


def d_midturn(rig, ns, inp, state):
    def est(agent, api_messages, system_prompt="", tools=None):
        args = {"agent": unmkk(agent), "api_messages": canon(api_messages), "system_prompt": system_prompt}
        if tools:
            args["tools"] = unmkk(tools)
        return rig.rec("estimate_native_responses_preflight_tokens", args)
    stub_module("agent.codex_responses_adapter", estimate_native_responses_preflight_tokens=est)
    ns["_estimate_tools_tokens_rough"] = lambda tools: rig.rec("_estimate_tools_tokens_rough", {"tools": unmkk(tools)})
    ns["logger"] = logger_double(rig)
    agent = mk(inp.get("agent", {}))
    try:
        ret = ns["_midturn_request_pressure_tokens"](agent, inp.get("api_messages"),
                                                     inp.get("effective_system"), inp.get("approx_tokens"))
    except BaseException as ex:
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    return canon(ret), state, None


def d_floor(rig, ns, inp, state):
    comp = mk(inp.get("compressor", {}))
    try:
        ret = ns["_pressure_with_real_floor"](comp, inp.get("rough_tokens"))
    except BaseException as ex:
        return None, state, {"kind": type(ex).__name__, "message": str(ex)}
    return canon(ret), state, None


def d_moa(rig, ns, inp, state):
    client = mk(inp.get("client"))
    ret = ns["_moa_client_consumes_prepared_request"](client)
    return canon(ret), state, None


def d_shutdown(rig, ns, inp, state):
    excspec = inp.get("exc")
    exc = None if excspec is None else build_exc(excspec.get("kind", "Exception"),
                                                 excspec.get("message", ""), excspec.get("bases"))

    def isd(e):
        return rig.rec("interpreter_shutting_down", {"exc": {"kind": type(e).__name__, "message": str(e)}})
    stub_module("tools.interpreter_shutdown", interpreter_shutting_down=isd)
    ret = ns["_is_interpreter_shutdown_error"](exc)
    return canon(ret), state, None


def d_mirror(rig, ns, inp, state):
    ns["_resolve_origin"] = rig.stub("_resolve_origin", ("job",))
    ns["_target_matches_origin"] = rig.stub("_target_matches_origin",
                                            ("origin", "platform_name", "chat_id", "thread_id"))
    ret = ns["_target_mirror_eligible"](inp.get("job"), inp.get("target"),
                                        global_mirror=inp.get("global_mirror"),
                                        origin_match=inp.get("origin_match"))
    return canon(ret), state, None


def d_yield(rig, ns, inp, state):
    ns["_detect_gateway_code_skew"] = rig.stub("_detect_gateway_code_skew")
    ns["_current_gateway_code_sha"] = rig.stub("_current_gateway_code_sha")
    ns["_get_hermes_home"] = rig.stub("_get_hermes_home")
    stub_module("cron.scheduler_ownership",
                owns_cron_tick_for=rig.stub("owns_cron_tick_for", ("home",)),
                live_gateway_ticking=rig.stub("live_gateway_ticking", ("home",)))
    ret = ns["_should_yield_tick_to_fresh_gateway"]()
    return canon(ret), state, None


def d_watchdog(rig, ns, inp, state):
    ns["AwakeIdleMeter"] = lambda: SN(measure=lambda idle_s: rig.rec("meter_measure", {"idle_s": idle_s}))
    stop = SN(wait=lambda timeout: rig.rec("stop_wait", {"timeout": timeout}))
    ret = ns["_inactivity_watchdog_loop"](
        get_idle_seconds=lambda: rig.rec("get_idle_seconds", {}),
        limit_s=inp.get("limit_s"), poll_s=inp.get("poll_s"), stop=stop,
        future_done=lambda: rig.rec("future_done", {}), meter=None)
    return canon(ret), state, None


def d_skew(rig, ns, inp, state):
    stub_module("gateway.code_skew",
                detect_code_skew=rig.stub("detect_code_skew",
                                          materialize=lambda v: tuple(v) if isinstance(v, list) else v))
    ret = ns["_detect_gateway_code_skew"]()
    return canon(ret), state, None


DRIVERS = {
    "resolve_stream_max_lifetime": d_resolve,
    "exceeded": d_exceeded,
    "reserved_session_id": d_reserved,
    "_maybe_grow_local_window": d_grow,
    "_midturn_request_pressure_tokens": d_midturn,
    "_pressure_with_real_floor": d_floor,
    "_moa_client_consumes_prepared_request": d_moa,
    "_is_interpreter_shutdown_error": d_shutdown,
    "_target_mirror_eligible": d_mirror,
    "_should_yield_tick_to_fresh_gateway": d_yield,
    "_inactivity_watchdog_loop": d_watchdog,
    "_detect_gateway_code_skew": d_skew,
}


def run_case(func, inp):
    module, fn = TARGETS[func]
    ns = extract(module)
    if fn not in ns:
        raise RuntimeError("live def %s not extracted from %s" % (fn, module))
    rig = Rig(inp.get("io"), ns)
    state = inp.get("state") or {}
    driver = DRIVERS[func]
    try:
        ret, state_after, raise_spec = driver(rig, ns, inp, state)
    except BaseException as ex:  # SystemExit included
        ret, state_after, raise_spec = None, state, {"kind": type(ex).__name__, "message": str(ex)}
    return {"ret": canon(ret), "trace": canon(rig.trace), "state": canon(state_after),
            "raise": canon(raise_spec)}


def main():
    data = sys.stdin.read()
    cases = []
    for line in data.split("\n"):
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception as ex:
            print("ORACLE agent_loop_revive: FATAL bad harness line: %s" % ex)
            sys.exit(2)
    if not cases:
        print("ORACLE agent_loop_revive: FATAL no cases (harness produced nothing)")
        sys.exit(2)
    mismatches = 0
    for c in cases:
        func = c.get("func")
        if func not in TARGETS:
            print("MISMATCH unknown func %r" % func)
            mismatches += 1
            continue
        c_out = c.get("out") or {}
        try:
            p_out = run_case(func, c.get("in") or {})
        except BaseException as ex:
            print("MISMATCH %s oracle-driver raised %s: %s" % (func, type(ex).__name__, ex))
            mismatches += 1
            continue
        if canon(c_out) != p_out:
            mismatches += 1
            print("MISMATCH %s" % func)
            print("  C   : %s" % json.dumps(canon(c_out), sort_keys=True)[:600])
            print("  live: %s" % json.dumps(p_out, sort_keys=True)[:600])
    print("ORACLE agent_loop_revive: %d cases, %d mismatches" % (len(cases), mismatches))
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()