#!/usr/bin/env python3
"""Oracle for src/agent/tool_executor_flow.c — drives the LIVE
agent/tool_executor.py execution-decision FLOW defs + leaves under IDENTICAL
scripted seams.

The 18 targets (17 module functions + _WorkerStartOnce.advance) are
AST-extracted in source order; module Assign/AnnAssign constants are seeded
(plain-Name targets only, with _COMPRESSION_MARKER_PREFIX pre-seeded from
agent/compression_marker.py so the _PRUNED_TOOL_ARGUMENTS_MESSAGE f-string
resolves), the real _ToolCallRef / _ManagedToolResult / _ParsedCall dataclasses
and the real _BatchAbandoned / _ToolTimeoutResult / _ToolCancelledResult
classes are extracted, and every node is compiled with the PEP 563 future flag
so annotations stay strings. The exec namespace is a registered module
(py3.14 dataclasses needs sys.modules[__name__]).

All live sibling / hook / clock / callback I/O is replaced by recording stubs
consuming the SAME "io" scripts the C harness feeds its universal seam:
absent/null -> None on every call; {"seq": [...]} -> per-call sequence (None
when exhausted); any other value -> reused. Each call records {ev, args, ret}
(ret = RAW scripted value) in call order. In-function imports record an
import_<module> event through a PEP 562 module __getattr__; a scripted raise
on that event fails the import exactly as live. Three seams fire their
scripted callback after recording (gate.run -> _pre_tool_block,
begin_execution -> _begin_tool_execution, run_with_activity_heartbeat ->
execute), mirroring the harness.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace;
callable doubles {"__callable__": ...}; facade doubles {"__gate__"},
{"__future__"}, {"__budget__"}, {"__guardrails__"}; dataclasses / marker
strings normalize to tagged dicts on BOTH sides; frozenset[str] normalizes to
a sorted deduped array. A scripted BaseException (KeyboardInterrupt /
SystemExit / GeneratorExit / CancelledError / _BatchAbandoned) propagates past
`except Exception` guards exactly as live; the C port models the same class
check. Raise-spec msg carries str(e) (KeyError: "'error'").

State mutations made before a raise stay visible in the state window on both
sides (the C harness mutates its state copy in place).

Input: stdin JSON lines from t_port_tool_executor_flow:
    {"func": ..., "in": {...}, "out": {"ret","raise","trace","state"}}
Summary: ORACLE tool_executor_flow: N cases, M mismatches.
Fail-closed: exit 1 on any mismatch, on a PYERR, or when zero cases ran.
"""
import ast
import contextlib
import copy
import dataclasses
import json
import os
import sys
import types
import __future__
from dataclasses import dataclass
from typing import Any, Callable, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass   # non-reconfigurable streams (embedded interpreters) keep their defaults

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "tool_executor.py")
MARKER_SRC = os.path.join(QUARRY, "agent", "compression_marker.py")
CATALOG_SRC = os.path.join(QUARRY, "tools", "tool_search_catalog.py")
NAMES_SRC = os.path.join(QUARRY, "tools", "connectors", "gateway", "names.py")

MODULE_TARGETS = {
    "_tc_name", "_tool_progress_enabled", "_preview", "_start_order_gate_timeout",
    "_delegate_spinner_label", "_safe_callback", "_interrupt_worker_tids",
    "_blocked_tool_result", "_pre_tool_block", "_unwrap_tool_search_call",
    "_parse_tool_call", "_dispatch_authorized_once", "_poll_sequential_future",
    "_unfinished_tool_result", "_abandoned_sequential_result",
    "_persist_multimodal_text_parts", "_emit_tool_complete_and_risk",
}
METHOD_TARGETS = {"advance"}   # _WorkerStartOnce.advance
WANTED = MODULE_TARGETS | METHOD_TARGETS

# Module siblings seamed when not the target (claimed ports + shared leaves).
MODULE_SEAMS = {
    "_canonical_tool_name", "_parse_tool_arguments", "_tool_search_scoped_names",
    "_context_pruned_argument_paths", "_begin_tool_execution",
    "_run_with_activity_heartbeat", "_emit_terminal_post_tool_call",
    "interrupt_skip_wording", "maybe_persist_tool_result",
    "_redact_tool_args_for_display", "tool_hook_ids", "_ra",
}

CUR = {"in": {}, "trace": [], "counts": {}}


def rec(ev):
    CUR["trace"].append(ev)


def script_next(name):
    io = CUR["in"].get("io") or {}
    s = io.get(name)
    if s is None:
        return None
    if isinstance(s, dict) and "seq" in s:
        seq = s["seq"]
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        return seq[i] if i < len(seq) else None
    return s


BUILTIN_EXC = {
    "Exception": Exception, "RuntimeError": RuntimeError, "ValueError": ValueError,
    "TypeError": TypeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "OSError": OSError, "TimeoutError": TimeoutError, "LookupError": LookupError,
    "CancelledError": __import__("asyncio").CancelledError, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "KeyError":
        # spec msg is str(e), e.g. "'error'"; KeyError(x) str() is repr(x).
        try:
            return KeyError(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return KeyError(msg)
    if cls == "_BatchAbandoned":
        return _BatchAbandoned(msg)
    if cls in BUILTIN_EXC:
        return BUILTIN_EXC[cls](msg)
    return type(cls, (Exception,), {})(msg)


class SilentLogger:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass


# ── value model: normalize (live -> tagged JSON) / materialize (JSON -> live) ──

class CallableDouble:
    """Inert truthy callable (agent.tool_progress_callback etc.)."""
    def __call__(self, *a, **k):
        raise AssertionError("inert callable double invoked")


class GateDouble:
    def run(self, callback):
        r = script_next("gate.run")
        rec({"ev": "gate.run", "args": {"callback": "<callable>"}, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return callback()

    def excluded_seconds(self):
        return stub("gate.excluded_seconds")()

    def begin_in_order(self, order, callback=None, *, tool_name=""):
        return stub("gate.begin_in_order")(order, callback, tool_name=tool_name)


class BeginExecDouble:
    def __call__(self, callback=None):
        r = script_next("begin_execution")
        args = {} if callback is None else {"callback": "<callable>"}
        rec({"ev": "begin_execution", "args": args, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        if callback is not None:
            return callback()
        return None


class FutureDouble:
    def result(self, timeout=None):
        return stub("future.result")(timeout=timeout)

    def done(self):
        return stub("future.done")()


class BudgetDouble:
    def resolve_threshold(self, tool_name):
        return stub("budget.resolve_threshold")(tool_name)


class GuardrailsDouble:
    def before_call(self, name, args):
        return stub("guardrails.before_call")(name, args)


def heartbeat_stub(agent, function_name, fn):
    r = script_next("run_with_activity_heartbeat")
    rec({"ev": "run_with_activity_heartbeat", "args": {"name": function_name}, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return fn()


class CMStub:
    def __init__(self, name, a, k):
        self.name = name
        proj = ARGMAP.get(name, lambda a, k: {})(a, k)
        args = {kk: norm_ret(vv) for kk, vv in proj.items() if vv is not None}
        self.r = script_next(name)
        rec({"ev": name, "args": args, "ret": self.r})
        if isinstance(self.r, dict) and "__raise__" in self.r:
            raise make_exc(self.r)

    def __enter__(self):
        return self.r

    def __exit__(self, *a):
        r = script_next(self.name + ".__exit__")
        rec({"ev": self.name + ".__exit__", "args": {}, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)   # an exit raise replaces the in-flight outcome
        return False


def cm_factory(name):
    return lambda *a, **k: CMStub(name, a, k)


def cb_proj(v):
    return "<callable>" if callable(v) else norm_ret(v)


def norm_ret(x):
    if isinstance(x, BaseException):
        return {"__raise__": type(x).__name__, "msg": str(x)}
    if isinstance(x, CallableDouble):
        return {"__callable__": True}
    if isinstance(x, GateDouble):
        return {"__gate__": True}
    if isinstance(x, FutureDouble):
        return {"__future__": True}
    if isinstance(x, BudgetDouble):
        return {"__budget__": True}
    if isinstance(x, GuardrailsDouble):
        return {"__guardrails__": True}
    if isinstance(x, BeginExecDouble):
        return {"__callable__": "begin_execution"}
    if callable(x) and hasattr(x, "__seam_name__"):
        return {"__callable__": x.__seam_name__}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        tn = type(x).__name__
        if tn in ("_ToolCallRef", "_ManagedToolResult", "_ParsedCall"):
            d = {"_type": tn}
            for f in dataclasses.fields(x):
                d[f.name] = norm_ret(getattr(x, f.name))
            return d
        return norm_ret(dataclasses.asdict(x))
    if type(x).__name__ in ("_ToolTimeoutResult", "_ToolCancelledResult"):
        return {"_type": type(x).__name__, "value": str(x)}
    if isinstance(x, types.SimpleNamespace):
        return {"__attr__": {k: norm_ret(v) for k, v in vars(x).items()}}
    if isinstance(x, frozenset):
        return sorted(norm_ret(v) for v in x)
    if isinstance(x, tuple):
        return [norm_ret(v) for v in x]
    if isinstance(x, list):
        return [norm_ret(v) for v in x]
    if isinstance(x, dict):
        return {k: norm_ret(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


def materialize(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            return types.SimpleNamespace(
                **{k: materialize(v) for k, v in j["__attr__"].items()})
        cb = j.get("__callable__")
        if cb is True:
            return CallableDouble()
        if isinstance(cb, str):
            if cb == "begin_execution":
                return BeginExecDouble()
            return stub(cb)
        if j.get("__gate__"):
            return GateDouble()
        if j.get("__future__"):
            return FutureDouble()
        if j.get("__budget__"):
            return BudgetDouble()
        if j.get("__guardrails__"):
            return GuardrailsDouble()
        t = j.get("_type")
        if t == "_ToolCallRef":
            return _ToolCallRef(
                name=j.get("name", ""), args=materialize(j.get("args")) or {},
                task_id=j.get("task_id", ""), call_id=j.get("call_id", ""),
                trace=materialize(j.get("trace")) or [])
        if t == "_ManagedToolResult":
            return _ManagedToolResult(
                result=materialize(j.get("result")),
                args=materialize(j.get("args")) or {},
                middleware_trace=materialize(j.get("middleware_trace")) or [],
                blocked=j.get("blocked", False), dispatched=j.get("dispatched", False),
                data_use=frozenset(j.get("data_use") or []))
        if t == "_ParsedCall":
            return _ParsedCall(
                tool_call=materialize(j.get("tool_call")), name=j.get("name", ""),
                args=materialize(j.get("args")) or {},
                middleware_trace=materialize(j.get("middleware_trace")) or [],
                parse_error=j.get("parse_error"), scope_block=j.get("scope_block"))
        if t in ("_ToolTimeoutResult", "_ToolCancelledResult"):
            return NS[t](j.get("value", ""))
        return {k: materialize(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize(v) for v in j]
    return j


# ── per-seam argument projection (identical field sets to the C args) ──

ARGMAP = {
    "callback": lambda a, k: {"args": list(a), "kwargs": dict(k)},
    "logging.debug": lambda a, k: {"fmt": a[0], "args": [norm_ret(x) for x in a[1:]]},
    "_ra": lambda a, k: {},
    "_set_interrupt": lambda a, k: {"value": a[0], "tid": a[1], "reason": k.get("reason")},
    "guardrail_block_result": lambda a, k: {"decision": norm_ret(a[0])},
    "_emit_terminal_post_tool_call": lambda a, k: dict(
        {"function_name": k.get("function_name"), "function_args": k.get("function_args"),
         "result": norm_ret(k.get("result")), "effective_task_id": k.get("effective_task_id"),
         "tool_call_id": k.get("tool_call_id"), "middleware_trace": k.get("middleware_trace")},
        **{kk: norm_ret(vv) for kk, vv in k.items()
           if kk not in ("function_name", "function_args", "result", "effective_task_id",
                         "tool_call_id", "middleware_trace")}),
    "import_dispatch_pre_tool_call_hooks": lambda a, k: {},
    "tool_hook_ids": lambda a, k: {"task_id": a[1], "call_id": a[2]},
    "_dispatch_pre_tool_call_hooks": lambda a, k: dict(
        {"name": a[0], "args": a[1]},
        **{kk: norm_ret(vv) for kk, vv in k.items() if kk != "middleware_trace"},
        middleware_trace=k.get("middleware_trace")),
    "import_tool_search": lambda a, k: {},
    "resolve_underlying_call": lambda a, k: {"function_args": a[0]},
    "_tool_search_scoped_names": lambda a, k: {"agent": norm_ret(a[0])},
    "scope_block_reason": lambda a, k: {"underlying": a[0]},
    "validate_deferred_call_args": lambda a, k: {"underlying": a[0], "underlying_args": a[1]},
    "_canonical_tool_name": lambda a, k: {"function_name": a[0]},
    "_parse_tool_arguments": lambda a, k: {"raw_arguments": a[0]},
    "_unwrap_tool_search_call": lambda a, k: {
        "function_name": a[1], "function_args": a[2],
        "flatten_probe": k.get("flatten_probe", False)},
    "import_hosted_tool_policy": lambda a, k: {},
    "hosted_tool_refusal": lambda a, k: {"name": a[0]},
    "_pre_tool_block": lambda a, k: {"agent": norm_ret(a[0]), "ref": norm_ret(a[1])},
    "_context_pruned_argument_paths": lambda a, k: {"name": a[0], "args": a[1]},
    "guardrails.before_call": lambda a, k: {"name": a[0], "args": a[1]},
    "_blocked_tool_result": lambda a, k: {
        "ref": norm_ret(a[1]), "block_body": k.get("block_body"),
        "block_error_type": k.get("block_error_type"),
        "guardrail_decision": norm_ret(k.get("guardrail_decision"))},
    "import_terminal_approval_batch": lambda a, k: {},
    "prepare_current_terminal": lambda a, k: {"ref": norm_ret(a[0])},
    "_begin_tool_execution": lambda a, k: {"ref": norm_ret(a[1]), "display_index": a[2]},
    "import_tool_progress_relay": lambda a, k: {},
    "import_data_use": lambda a, k: {},
    "import_mcp_elicitation_form": lambda a, k: {},
    "bound_tool_progress": lambda a, k: {"callback": cb_proj(a[0]), "name": a[1], "call_id": a[2]},
    "collect_data_use": lambda a, k: {},
    "bound_form_asker": lambda a, k: {"callback": cb_proj(a[0])},
    "execute": lambda a, k: {"args": a[0]},
    "gate.excluded_seconds": lambda a, k: {},
    "time.monotonic": lambda a, k: {},
    "future.result": lambda a, k: {"timeout": k.get("timeout")},
    "future.done": lambda a, k: {},
    "_touch_activity": lambda a, k: {"label": a[0]},
    "interrupt_skip_wording": lambda a, k: {"agent": norm_ret(a[0])},
    "maybe_persist_tool_result": lambda a, k: {
        "content": k.get("content"), "tool_name": k.get("tool_name"),
        "tool_use_id": k.get("tool_use_id"), "env": norm_ret(k.get("env")),
        "config": norm_ret(k.get("config"))},
    "budget.resolve_threshold": lambda a, k: {"tool_name": a[0]},
    "_redact_tool_args_for_display": lambda a, k: {"name": a[0], "args": a[1]},
    "_safe_callback": lambda a, k: {
        "callback": cb_proj(a[0]), "label": a[1],
        "args": [norm_ret(v) for v in a[2:]], "kwargs": dict(k)},
    "gate.begin_in_order": lambda a, k: {
        "order": a[0], "callback": cb_proj(a[1]) if len(a) > 1 else None,
        "tool_name": k.get("tool_name")},
}


def materialize_result(name, r):
    if name == "_tool_search_scoped_names":
        return frozenset(r) if isinstance(r, list) else r
    if name == "guardrails.before_call":
        return materialize(r)
    if name == "_ra":
        # run_agent module double: _set_interrupt is the recorded seam.
        return types.SimpleNamespace(_set_interrupt=stub("_set_interrupt"))
    return r


def stub(name):
    def f(*a, **k):
        proj = ARGMAP.get(name, lambda a, k: {})(a, k)
        args = {kk: norm_ret(vv) for kk, vv in proj.items() if vv is not None}
        r = script_next(name)
        rec({"ev": name, "args": args, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return materialize_result(name, r)
    f.__seam_name__ = name
    return f


# ── extraction ──

def compile_node(node, path):
    return compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                   flags=__future__.annotations.compiler_flag)


def extract_assign(path, name):
    tree = ast.parse(open(path, encoding="utf-8").read())
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            exec(compile_node(node, path), ns)
            return ns[name]
    raise RuntimeError("Assign not found: %s in %s" % (name, path))


def extract():
    global _ToolCallRef, _ManagedToolResult, _ParsedCall
    global _BatchAbandoned, _ToolTimeoutResult, _ToolCancelledResult, NS
    modname = "tool_executor_flow_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": modname,
        "json": json, "contextlib": contextlib, "copy": copy,
        "dataclass": dataclass, "Any": Any, "Optional": Optional, "Callable": Callable,
        "threading": types.SimpleNamespace(Lock=object, Event=object, Condition=object,
                                           current_thread=lambda: types.SimpleNamespace(ident=0)),
        "time": types.SimpleNamespace(time=stub("time.time"), monotonic=stub("time.monotonic")),
        "logging": types.SimpleNamespace(debug=stub("logging.debug"),
                                         getLogger=lambda name: SilentLogger()),
        "logger": SilentLogger(),
        "concurrent": types.SimpleNamespace(futures=types.SimpleNamespace(
            TimeoutError=TimeoutError, wait=stub("concurrent.futures.wait"))),
        "interrupt_skip_wording": stub("interrupt_skip_wording"),
        "_COMPRESSION_MARKER_PREFIX": extract_assign(MARKER_SRC, "_COMPRESSION_MARKER_PREFIX"),
    })

    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # Module constants: Assign AND AnnAssign, plain-Name targets only.
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(
                isinstance(t, ast.Name) for t in node.targets):
            try:
                exec(compile_node(node, SRC), ns)
            except Exception:
                pass
        elif (isinstance(node, ast.AnnAssign) and node.value is not None
              and isinstance(node.target, ast.Name)):
            try:
                exec(compile_node(node, SRC), ns)
            except Exception:
                pass
    ns["logger"] = SilentLogger()   # restore after the logger Assign seeding

    real = {}
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in (
                "_ToolCallRef", "_ManagedToolResult", "_ParsedCall",
                "_BatchAbandoned", "_ToolTimeoutResult", "_ToolCancelledResult"):
            exec(compile_node(node, SRC), ns)
        elif isinstance(node, ast.FunctionDef) and node.name in MODULE_TARGETS:
            exec(compile_node(node, SRC), ns)
            real[node.name] = ns[node.name]
        elif isinstance(node, ast.ClassDef) and node.name == "_WorkerStartOnce":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in METHOD_TARGETS:
                    sub = copy.copy(sub)
                    sub.decorator_list = []
                    exec(compile_node(sub, SRC), ns)
                    real[sub.name] = ns[sub.name]
    _ToolCallRef = ns["_ToolCallRef"]
    _ManagedToolResult = ns["_ManagedToolResult"]
    _ParsedCall = ns["_ParsedCall"]
    _BatchAbandoned = ns["_BatchAbandoned"]
    _ToolTimeoutResult = ns["_ToolTimeoutResult"]
    _ToolCancelledResult = ns["_ToolCancelledResult"]
    missing = WANTED - set(real)
    if missing:
        raise RuntimeError("extraction missed: %s" % sorted(missing))
    for const in ("_PRUNED_TOOL_ARGUMENTS_ERROR", "_PRUNED_TOOL_ARGUMENTS_MESSAGE",
                  "_START_ORDER_GATE_TIMEOUT_S", "_SEQUENTIAL_INTERRUPT_POLL_SECONDS",
                  "_NO_REASON"):
        if const not in ns:
            raise RuntimeError("constant not seeded: %s" % const)
    return ns, real


def modstub(fullname):
    parts = fullname.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        m = sys.modules.get(nm)
        if m is None:
            m = types.ModuleType(nm)
            m.__path__ = []
            sys.modules[nm] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], m)
    return sys.modules[fullname]


# `from X import Y` does TWO getattrs (the _handle_fromlist hasattr probe and
# IMPORT_FROM); cache the produced attribute after the first so the import
# seam records exactly once. setup_case clears the cache per case.
_IMPORT_SEAM_CACHE = []


def import_seam_module(fullname, imports):
    """imports: attr -> (import seam name, zero-arg factory). PEP 562 __getattr__
    records the import event (scripted raise fails the import) then returns the
    attr produced by the factory."""
    m = modstub(fullname)

    def getattr_(attr):
        if attr in imports:
            seam_name, factory = imports[attr]
            r = script_next(seam_name)
            rec({"ev": seam_name, "args": {}, "ret": r})
            if isinstance(r, dict) and "__raise__" in r:
                raise make_exc(r)
            value = factory()
            m.__dict__[attr] = value
            return value
        raise AttributeError(attr)

    m.__dict__["__getattr__"] = getattr_
    _IMPORT_SEAM_CACHE.append((m, list(imports)))
    return m


def install_module_stubs():
    tool_call_name = extract_assign(CATALOG_SRC, "TOOL_CALL_NAME")
    sentinel = extract_assign(NAMES_SRC, "CONNECTOR_BATCH_SENTINEL")
    ts = types.ModuleType("tools.tool_search")
    ts.TOOL_CALL_NAME = tool_call_name
    ts.CONNECTOR_BATCH_SENTINEL = sentinel
    ts.resolve_underlying_call = stub("resolve_underlying_call")
    ts.scope_block_reason = stub("scope_block_reason")
    ts.validate_deferred_call_args = stub("validate_deferred_call_args")
    sys.modules["tools.tool_search"] = ts
    tools_pkg = modstub("tools")
    # `from tools import tool_search` must record the import seam: do NOT set
    # the instance attribute up front; the package __getattr__ serves it (and
    # caches it for the IMPORT_FROM second getattr).
    def tools_getattr(attr):
        if attr == "tool_search":
            r = script_next("import_tool_search")
            rec({"ev": "import_tool_search", "args": {}, "ret": r})
            if isinstance(r, dict) and "__raise__" in r:
                raise make_exc(r)
            tools_pkg.__dict__["tool_search"] = ts
            return ts
        raise AttributeError(attr)
    tools_pkg.__dict__["__getattr__"] = tools_getattr
    _IMPORT_SEAM_CACHE.append((tools_pkg, ["tool_search"]))

    import_seam_module("tools.hosted_tool_policy", {
        "hosted_tool_refusal": ("import_hosted_tool_policy", lambda: stub("hosted_tool_refusal"))})
    import_seam_module("hermes_cli.plugins", {
        "_dispatch_pre_tool_call_hooks": ("import_dispatch_pre_tool_call_hooks",
                                          lambda: stub("_dispatch_pre_tool_call_hooks"))})
    import_seam_module("agent.terminal_approval_batch", {
        "prepare_current_terminal": ("import_terminal_approval_batch",
                                     lambda: stub("prepare_current_terminal"))})
    import_seam_module("agent.tool_progress_relay", {
        "bound_tool_progress": ("import_tool_progress_relay",
                                lambda: cm_factory("bound_tool_progress"))})
    import_seam_module("tools.connectors.data_use", {
        "collect_data_use": ("import_data_use", lambda: cm_factory("collect_data_use"))})
    import_seam_module("tools.mcp_elicitation_form", {
        "bound_form_asker": ("import_mcp_elicitation_form",
                             lambda: cm_factory("bound_form_asker"))})


# ── per-case setup ──

def setup_case(ns, real, inp, target):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    for m, attrs in _IMPORT_SEAM_CACHE:
        for a in attrs:
            m.__dict__.pop(a, None)
    for name in MODULE_TARGETS | MODULE_SEAMS:
        if name == target:
            ns[name] = real[name]
        elif name == "_run_with_activity_heartbeat":
            ns[name] = heartbeat_stub
        else:
            ns[name] = stub(name)


def attempt(fn):
    try:
        return fn(), None
    except BaseException as e:   # CancelledError / SystemExit / _BatchAbandoned included
        return None, {"__raise__": type(e).__name__, "msg": str(e)}


def drive(ns, real, func, inp):
    """Returns (ret, state_dict, raise_spec)."""
    st = inp.get("state") or {}

    if func == "_tc_name":
        ret, exc = attempt(lambda: ns[func](materialize(inp.get("tool_call"))))
        return ret, {}, exc

    if func == "_tool_progress_enabled":
        ret, exc = attempt(lambda: ns[func](materialize(inp.get("agent"))))
        return ret, {}, exc

    if func == "_preview":
        ret, exc = attempt(lambda: ns[func](inp.get("text"), inp.get("limit")))
        return ret, {}, exc

    if func == "_start_order_gate_timeout":
        ret, exc = attempt(lambda: ns[func](inp.get("batch_timeout")))
        return ret, {}, exc

    if func == "_delegate_spinner_label":
        ret, exc = attempt(lambda: ns[func](inp.get("function_args") or {}))
        return ret, {}, exc

    if func == "_safe_callback":
        cb = materialize(inp.get("callback"))
        args = inp.get("args") or []
        kwargs = inp.get("kwargs") or {}
        ret, exc = attempt(lambda: ns[func](cb, inp.get("label", ""), *args, **kwargs))
        return ret, {}, exc

    if func == "_interrupt_worker_tids":
        agent = materialize(inp.get("agent"))
        kw = {} if "reason" not in inp else {"reason": inp["reason"]}
        ret, exc = attempt(lambda: ns[func](agent, inp.get("tids") or [], **kw))
        return ret, {}, exc

    if func == "_blocked_tool_result":
        agent = materialize(inp.get("agent"))
        agent._guardrail_block_result = stub("guardrail_block_result")
        ref = materialize(inp.get("ref"))
        ret, exc = attempt(lambda: ns[func](
            agent, ref, block_body=materialize(inp.get("block_body")),
            block_error_type=inp.get("block_error_type", ""),
            guardrail_decision=materialize(inp.get("guardrail_decision"))))
        return ret, {}, exc

    if func == "_pre_tool_block":
        agent = materialize(inp.get("agent"))
        ref = materialize(inp.get("ref"))
        ret, exc = attempt(lambda: ns[func](agent, ref))
        return ret, {}, exc

    if func == "_unwrap_tool_search_call":
        agent = materialize(inp.get("agent"))
        ret, exc = attempt(lambda: ns[func](
            agent, inp.get("function_name"), inp.get("function_args") or {},
            flatten_probe=inp.get("flatten_probe", False)))
        return ret, {}, exc

    if func == "_parse_tool_call":
        agent = materialize(inp.get("agent"))
        tc = materialize(inp.get("tool_call"))
        ret, exc = attempt(lambda: ns[func](agent, tc, flatten_probe=inp.get("flatten_probe", False)))
        return ret, {}, exc

    if func == "_dispatch_authorized_once":
        agent = materialize(st.get("agent"))
        managed = materialize(st.get("state"))
        ref = materialize(st.get("ref"))
        execute = materialize(inp.get("execute"))
        begin = materialize(inp.get("begin_execution"))
        gate = materialize(inp.get("authorization_gate"))
        ret, exc = attempt(lambda: ns[func](
            agent, managed, ref, execute=execute, scope_block=inp.get("scope_block"),
            display_index=inp.get("display_index"), begin_execution=begin,
            authorization_gate=gate))
        state_out = {"agent": norm_ret(agent), "state": norm_ret(managed), "ref": norm_ret(ref)}
        return ret, state_out, exc

    if func == "_poll_sequential_future":
        agent = materialize(inp.get("agent"))
        agent._touch_activity = stub("_touch_activity")
        future = materialize(inp.get("future"))
        gate = materialize(inp.get("authorization_gate"))
        ret, exc = attempt(lambda: ns[func](
            agent, future, inp.get("function_name", ""), inp.get("deadline"),
            inp.get("started", 0.0), gate))
        return ret, {}, exc

    if func == "_unfinished_tool_result":
        agent = materialize(inp.get("agent"))
        ref = materialize(inp.get("ref"))
        ret, exc = attempt(lambda: ns[func](
            agent, ref, timed_out=inp.get("timed_out", False), timeout_s=inp.get("timeout_s")))
        return ret, {}, exc

    if func == "_abandoned_sequential_result":
        agent = materialize(inp.get("agent"))
        ref = materialize(inp.get("ref"))
        cls = ns[inp.get("result_cls", "_ToolTimeoutResult")]
        outcome = inp.get("outcome") or {}
        ret, exc = attempt(lambda: ns[func](agent, ref, inp.get("message", ""), cls, **outcome))
        return ret, {}, exc

    if func == "_persist_multimodal_text_parts":
        budget = materialize(inp.get("budget"))
        env = materialize(inp.get("env"))
        ret, exc = attempt(lambda: ns[func](
            inp.get("result") or {}, inp.get("tool_name", ""), inp.get("tool_call_id", ""),
            env, budget))
        return ret, {}, exc

    if func == "_emit_tool_complete_and_risk":
        agent = materialize(inp.get("agent"))
        ref = materialize(inp.get("ref"))
        ret, exc = attempt(lambda: ns[func](
            agent, ref, inp.get("result"), inp.get("risk_metadata"), inp.get("blocked", False)))
        return ret, {}, exc

    if func == "advance":
        slf = types.SimpleNamespace(**{k: materialize(v) for k, v in st.items()})
        m = types.MethodType(real["advance"], slf)
        cb = materialize(inp.get("callback"))
        ret, exc = attempt(lambda: m(cb))
        return ret, {k: norm_ret(v) for k, v in vars(slf).items()}, exc

    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()),
                              key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    try:
        ns, real = extract()
        install_module_stubs()
    except BaseException as e:
        print("PYERR extract: %s: %s" % (type(e).__name__, e))
        return 1
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        setup_case(ns, real, inp, func)
        try:
            ret, state, exc = drive(ns, real, func, inp)
        except BaseException as e:
            print("PYERR %s: %s: %s" % (func, type(e).__name__, e))
            mism += 1
            continue
        want = {"ret": norm_ret(ret), "raise": exc, "state": norm_ret(state),
                "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE tool_executor_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())