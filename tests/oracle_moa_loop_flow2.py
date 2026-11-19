#!/usr/bin/env python3
"""Oracle for src/agent/moa_loop_flow2.c — drives the LIVE agent/moa_loop.py
UNCLAIMED-remainder FLOW defs + leaves under IDENTICAL scripted seams.

The 20 targets (14 module functions, 3 nested closures, 1 facade method,
2 MoAClient properties) are AST-extracted in source order; module
Assign/AnnAssign constants are seeded (plain-Name targets only), the real
_RefAccounting dataclass is extracted, and every node is compiled with the
PEP 563 future flag so annotations stay strings. The exec namespace is a
registered module (py3.14 dataclasses needs sys.modules[__name__]).

All claimed siblings (_agent_cache_opts flow1 port, build_moa_facade stub
claim) and every foreign-module import are replaced by recording stubs
consuming the SAME "io" scripts the C harness feeds its universal seam:
absent/null -> None on every call; {"seq": [...]} -> per-call sequence (None
when exhausted); any other value -> reused. Each call records {ev, args, ret}
(ret = RAW scripted value, deep-copied at call time) in call order. Ported
functions compose REAL (internal composition).

Value model: attribute doubles {"__attr__": ...} materialize to
SimpleNamespace; callable doubles {"__call__": name} materialize to tagged
recorder functions; the _reference_liveness_hook closure projects to
{"__closure__": "_touch_on_progress", "touch", "label"}; _RefAccounting
instances normalize to tagged dicts; the preset-cache tuple key projects to
compact JSON "[stamp,name]". Scripted raises are REAL exception instances
(builtins by name, synthetic Exception subclasses otherwise); a scripted
exception propagating out is reported with its spec VERBATIM (identity
match), an unscripted live raise with {type, str(e)}. BaseException-only
kinds (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError) pass
through every `except Exception` / suppress(Exception) site exactly as live.

Input: stdin JSON lines from t_port_moa_loop_flow2:
    {"func": ..., "in": {...}, "out": {"ret","raise","trace","state"}}
Summary: ORACLE moa_loop_flow2: N cases, M mismatches.
Fail-closed: exit 1 on any mismatch, on a setup PYERR, or when zero cases ran.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import dataclasses
import functools
import json
import os
import re
import sys
import threading
import types
import __future__
from dataclasses import dataclass, field, replace, KW_ONLY
from typing import Any

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "moa_loop.py")

MODULE_TARGETS = {
    "_redact_reference_text", "_redact_message_content",
    "_redact_reference_outputs", "_redact_trace_messages",
    "_redact_trace_accounting", "_moa_privacy_mode", "_slot_label",
    "peel_reference_guidance", "_attach_reference_guidance",
    "_reference_liveness_hook", "_touch_fanout_progress",
    "_slot_request_headers", "bind_moa_runtime", "_resolve_preset_cached",
}
NESTED_TARGETS = {"_touch_on_progress", "collect", "_moa_reference_relay"}
NESTED_PARENTS = {"_reference_liveness_hook", "_run_references_parallel",
                  "build_moa_facade"}
METHOD_TARGETS = {"_plan_aggregator_cache"}
PROPERTY_TARGETS = {"last_aggregator_route", "last_aggregator_attempt"}
WANTED = MODULE_TARGETS | NESTED_TARGETS | METHOD_TARGETS | PROPERTY_TARGETS
# Claimed siblings seamed wherever a ported flow reaches them.
MODULE_SEAMS = {"_agent_cache_opts", "build_moa_facade"}
# Foreign in-function imports -> sys.modules recording stubs.
IMPORT_SEAMS = {
    "agent.redact": {"redact_sensitive_text"},
    "hermes_cli.moa_config": {"coerce_privacy_filter", "resolve_moa_preset"},
    "hermes_cli.config": {"get_config_path", "load_config"},
    "hermes_cli.config_backend": {"config_cache_version_or_none"},
    "agent.safe_worker_policy": {"worker_config_snapshot"},
    "agent.auxiliary_client": {"_normalize_aux_provider"},
    "agent.provider_route_hooks": {"route_answer"},
    "providers": {"get_provider_profile"},
    "agent.agent_runtime_helpers": {"plan_cache_sections_for_destination"},
}

CUR = {"in": {}, "trace": [], "counts": {}, "scripted": [], "state_out": None}


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


def make_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "CancelledError":
        return asyncio.CancelledError(msg)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        return b(msg)
    return type(cls, (Exception,), {})(msg)


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


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

ACCT_FIELDS = ("usage", "cost_usd", "cost_status", "cost_source", "messages",
               "output", "model", "provider", "temperature")


def norm_ret(x):
    if isinstance(x, types.SimpleNamespace):
        return {"__attr__": {k: norm_ret(v) for k, v in vars(x).items()}}
    if callable(x) and hasattr(x, "_moa2_seam"):
        return {"__call__": x._moa2_seam}
    if isinstance(x, types.FunctionType) and x.__name__ == "_touch_on_progress":
        cells = dict(zip(x.__code__.co_freevars,
                         (c.cell_contents for c in (x.__closure__ or ()))))
        return {"__closure__": "_touch_on_progress",
                "touch": norm_ret(cells.get("touch")),
                "label": norm_ret(cells.get("label"))}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        if type(x).__name__ == "_RefAccounting":
            d = {"_type": "_RefAccounting"}
            for f in ACCT_FIELDS:
                d[f] = norm_ret(getattr(x, f))
            return d
        return norm_ret(dataclasses.asdict(x))
    if isinstance(x, tuple):
        return [norm_ret(v) for v in x]
    if isinstance(x, list):
        return [norm_ret(v) for v in x]
    if isinstance(x, dict):
        return {k: norm_ret(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, BaseException):
        return str(x)
    return x


def recorder(name):
    def f(*a, **k):
        return stub(name)(*a, **k)
    f._moa2_seam = name
    return f


def mat_value(j):
    if isinstance(j, dict):
        if set(j.keys()) == {"__call__"} and isinstance(j["__call__"], str):
            return recorder(j["__call__"])
        if "__attr__" in j and isinstance(j["__attr__"], dict):
            return types.SimpleNamespace(
                **{k: mat_value(v) for k, v in j["__attr__"].items()})
    return j


def materialize_acct(j):
    if isinstance(j, dict) and j.get("_type") == "_RefAccounting":
        return _RefAccounting(
            usage=j.get("usage"), cost_usd=j.get("cost_usd"),
            cost_status=j.get("cost_status"), cost_source=j.get("cost_source"),
            messages=j.get("messages"), output=j.get("output"),
            model=j.get("model"), provider=j.get("provider"),
            temperature=j.get("temperature"))
    return j


# ── per-seam argument projection (identical field sets to the C args) ──

ARGMAP = {
    "redact_sensitive_text": lambda a, k: {
        "text": a[0], "force": k.get("force"), "code_file": k.get("code_file")},
    "coerce_privacy_filter": lambda a, k: {"value": a[0]},
    "worker_config_snapshot": lambda a, k: {},
    "get_config_path": lambda a, k: {},
    "config_cache_version_or_none": lambda a, k: {"path": a[0]},
    "load_config": lambda a, k: {},
    "resolve_moa_preset": lambda a, k: {"moa_raw": a[0], "preset_name": a[1]},
    "_normalize_aux_provider": lambda a, k: {"provider": a[0]},
    "get_provider_profile": lambda a, k: {"provider": a[0]},
    "route_answer": lambda a, k: {
        "name": a[0], "provider": k.get("provider"),
        "base_url": k.get("base_url"), "model": k.get("model"),
        "user_initiated": k.get("user_initiated"), "scope": k.get("scope")},
    "touch": lambda a, k: {"message": a[0]},
    "progress_callback": lambda a, k: {
        "done": a[0], "total": a[1], "label": a[2]},
    "future.result": lambda a, k: {"idx": a[0]},
    "_agent_cache_opts": lambda a, k: {"agent": a[0]},
    "plan_cache_sections_for_destination": lambda a, k: {
        "messages": a[0], "tools": a[1], "provider": k.get("provider"),
        "base_url": k.get("base_url"), "api_mode": k.get("api_mode"),
        "model": k.get("model"), "cache_disabled": k.get("cache_disabled"),
        "cache_ttl": k.get("cache_ttl"), "cache_ttl_auto": k.get("cache_ttl_auto"),
        "static_system_prefix": k.get("static_system_prefix")},
    "build_moa_facade": lambda a, k: {"agent": a[0], "preset_name": a[1]},
    "tool_progress_callback": lambda a, k: dict(
        {"event": a[0], "label": a[1], "text": a[2]}, **k),
}


def materialize_result(name, r):
    if name == "get_provider_profile":
        return mat_value(r)
    return r


def stub(name):
    def f(*a, **k):
        args = {kk: norm_ret(vv) for kk, vv in ARGMAP[name](a, k).items()
                if vv is not None}
        r = script_next(name)
        rec({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(r)})
        if isinstance(r, dict) and "__raise__" in r:
            e = make_exc(r)
            CUR["scripted"].append((e, r))
            raise e
        return materialize_result(name, r)
    return f


# ── extraction ──

def compile_node(node, path):
    return compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                   flags=__future__.annotations.compiler_flag)


def strip_decorators(node):
    n = copy.copy(node)
    n.decorator_list = []
    return n


def build_collect_driver(collect_node):
    """Synthetic enclosing scope for collect's `nonlocal completed`."""
    params = ["done", "_completed", "_total", "_futures", "_results",
              "_agent", "_progress_callback", "_reference_models", "_cell"]
    body = []
    for p in params[1:-1]:
        body.append(ast.Assign(targets=[ast.Name(id=p[1:], ctx=ast.Store())],
                               value=ast.Name(id=p, ctx=ast.Load()),
                               lineno=0, col_offset=0))
    body.append(collect_node)
    call = ast.Expr(value=ast.Call(func=ast.Name(id="collect", ctx=ast.Load()),
                                   args=[ast.Name(id="done", ctx=ast.Load())],
                                   keywords=[]))
    fin = ast.Assign(
        targets=[ast.Subscript(value=ast.Name(id="_cell", ctx=ast.Load()),
                               slice=ast.Constant(value="completed"),
                               ctx=ast.Store())],
        value=ast.Name(id="completed", ctx=ast.Load()))
    body.append(ast.Try(body=[call], handlers=[], orelse=[], finalbody=[fin]))
    body.append(ast.Return(value=ast.Name(id="completed", ctx=ast.Load())))
    driver = ast.FunctionDef(
        name="_moa2_collect_driver",
        args=ast.arguments(posonlyargs=[],
                           args=[ast.arg(arg=p, annotation=None) for p in params],
                           vararg=None, kwonlyargs=[], kw_defaults=[],
                           kwarg=None, defaults=[]),
        body=body, decorator_list=[], returns=None, type_comment=None)
    ast.fix_missing_locations(driver)
    return driver


def extract():
    global _RefAccounting
    modname = "moa_loop_flow2_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": modname,
        "contextlib": contextlib, "functools": functools, "json": json,
        "re": re, "threading": threading,
        "dataclass": dataclass, "field": field, "replace": replace,
        "KW_ONLY": KW_ONLY, "Any": Any,
        "SimpleNamespace": types.SimpleNamespace,
        "logging": types.SimpleNamespace(getLogger=lambda name: SilentLogger()),
        "logger": SilentLogger(),
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
    collect_node = None
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_RefAccounting":
            exec(compile_node(node, SRC), ns)
            _RefAccounting = ns["_RefAccounting"]
        elif isinstance(node, ast.FunctionDef):
            if node.name in MODULE_TARGETS:
                exec(compile_node(node, SRC), ns)
                real[node.name] = ns[node.name]
            if node.name in NESTED_PARENTS:
                for sub in ast.walk(node):
                    if (isinstance(sub, ast.FunctionDef)
                            and sub.name in NESTED_TARGETS and sub is not node):
                        if sub.name == "collect":
                            collect_node = sub
                        else:
                            exec(compile_node(strip_decorators(sub), SRC), ns)
                            real[sub.name] = ns[sub.name]
        elif isinstance(node, ast.ClassDef) and node.name == "MoAChatCompletions":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in METHOD_TARGETS:
                    exec(compile_node(strip_decorators(sub), SRC), ns)
                    real[sub.name] = ns[sub.name]
        elif isinstance(node, ast.ClassDef) and node.name == "MoAClient":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in PROPERTY_TARGETS:
                    exec(compile_node(strip_decorators(sub), SRC), ns)
                    real[sub.name] = ns[sub.name]
    if collect_node is None:
        raise RuntimeError("collect closure not found")
    exec(compile_node(build_collect_driver(collect_node), SRC), ns)
    real["_collect_driver"] = ns["_moa2_collect_driver"]
    # collect's `nonlocal completed` cannot compile standalone; the driver IS
    # its callable form (setup_case binds it under "collect" for the audit).
    real["collect"] = ns["_moa2_collect_driver"]

    missing = WANTED - set(real)
    if missing:
        raise RuntimeError("extraction missed: %s" % sorted(missing))
    for const in ("_MOA_EMAIL_RE", "_MOA_PHONE_RE", "_RELAY_EVENTS"):
        if const not in ns:
            raise RuntimeError("constant not seeded: %s" % const)
    return ns, real


def modstub(fullname, **attrs):
    parts = fullname.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        m = types.ModuleType(nm)
        m.__path__ = []
        sys.modules[nm] = m   # overwrite: a stale package keeps old attrs
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], m)
    for kk, vv in attrs.items():
        setattr(sys.modules[fullname], kk, vv)


def install_module_stubs():
    for fullname, names in IMPORT_SEAMS.items():
        modstub(fullname, **{n: stub(n) for n in names})


# ── Future double (result seam; identity-keyed futures dict) ──

class FutureDouble:
    def __init__(self, idx):
        self.idx = idx

    def result(self):
        return stub("future.result")(self.idx)


# ── per-case setup ──

def setup_case(ns, real, inp, target):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    CUR["scripted"] = []
    CUR["state_out"] = None
    # Every ported target runs REAL (internal composition); restore all of
    # them each case so a prior case's bindings cannot leak.
    for name in WANTED:
        ns[name] = real[name]
    for name in MODULE_SEAMS:
        ns[name] = stub(name)
    ns["_preset_cache_lock"] = NoopLock()
    ns["logger"] = SilentLogger()


def drive(ns, real, func, inp):
    """Returns (ret, state_dict); CUR["state_out"] holds partial state on raise."""
    st = inp.get("state") or {}

    def echo(extra=None):
        out = dict(st)
        if extra:
            out.update(extra)
        CUR["state_out"] = out
        return out

    if func == "_redact_reference_text":
        return ns[func](inp.get("text")), echo()
    if func == "_redact_message_content":
        return ns[func](inp.get("content")), echo()
    if func == "_redact_reference_outputs":
        return ns[func](inp.get("reference_outputs")), echo()
    if func == "_redact_trace_messages":
        return ns[func](inp.get("messages")), echo()
    if func == "_redact_trace_accounting":
        return ns[func](materialize_acct(inp.get("acct"))), echo()
    if func == "_moa_privacy_mode":
        return ns[func](inp.get("moa_raw")), echo()
    if func == "_slot_label":
        return ns[func](inp.get("slot")), echo()
    if func == "peel_reference_guidance":
        return ns[func](inp.get("messages"), inp.get("guidance")), echo()

    if func == "_attach_reference_guidance":
        msgs = inp.get("agg_messages")
        if isinstance(msgs, list):
            msgs = list(msgs)
        out = echo({"agg_messages": msgs})
        ns[func](msgs, inp.get("guidance"))
        return None, out

    if func == "_reference_liveness_hook":
        agent = mat_value(inp.get("agent"))
        return ns[func](agent, inp.get("label", "")), echo()

    if func == "_touch_on_progress":
        agent = types.SimpleNamespace(_touch_activity=mat_value(inp.get("touch")))
        hook = ns["_reference_liveness_hook"](agent, inp.get("label", ""))
        hook()
        return None, echo()

    if func == "_touch_fanout_progress":
        agent = mat_value(inp.get("agent"))
        ns[func](agent, inp.get("description"))
        return None, echo()

    if func == "_slot_request_headers":
        runtime = inp.get("runtime")
        agent = mat_value(inp.get("agent"))
        return ns[func](runtime, agent), echo()

    if func == "_moa_reference_relay":
        ns["agent"] = mat_value(inp.get("agent"))
        kwargs = inp.get("kwargs") or {}
        ns[func](inp.get("event", ""), **kwargs)
        return None, echo()

    if func in ("last_aggregator_route", "last_aggregator_attempt"):
        slf = mat_value(inp.get("self"))
        return real[func](slf), echo()

    if func == "_resolve_preset_cached":
        cache = {}
        for k, v in (st.get("preset_cache") or {}).items():
            stamp, name = json.loads(k)
            cache[(stamp, name)] = v
        ns["_preset_cache"] = cache
        out = echo({"preset_cache": cache})
        try:
            r = ns[func](inp.get("preset_name", ""))
        finally:
            out["preset_cache"] = {
                json.dumps(list(k), separators=(",", ":")): v
                for k, v in cache.items()}
        return r, out

    if func == "collect":
        done = inp.get("done")
        if isinstance(done, list):
            futs = [FutureDouble(d.get("idx", 0)) for d in done]
            futures = {f: f.idx for f in futs}
        else:
            futs = done   # non-iterable -> the live for-loop raises
            futures = {}
        results = list(st.get("results") or [])
        cell = {"completed": st.get("completed", 0)}
        out = echo({"results": results, "completed": cell["completed"]})
        agent = mat_value(inp.get("agent"))
        cb = mat_value(inp.get("progress_callback"))
        ref_models = inp.get("reference_models")
        total = inp.get("total", 0)
        try:
            completed = real["_collect_driver"](
                futs, cell["completed"], total, futures, results,
                agent, cb, ref_models, cell)
        finally:
            out["completed"] = cell["completed"]
        out["completed"] = completed
        return None, out

    if func == "_plan_aggregator_cache":
        slf = types.SimpleNamespace(**{k: mat_value(v) for k, v in st.items()})
        out = echo()
        m = types.MethodType(real[func], slf)
        r = m(inp.get("agg_messages"), inp.get("tools"), inp.get("guidance"),
              inp.get("agg_runtime"))
        return r, out

    if func == "bind_moa_runtime":
        agent = mat_value(inp.get("agent"))
        out = echo({"agent": agent})
        ns[func](agent, inp.get("preset_name"), inp.get("api_key"))
        return None, out

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


def exc_spec(e):
    for exc, spec in CUR["scripted"]:
        if e is exc:
            return {"__raise__": spec.get("__raise__", "Exception"),
                    "msg": spec.get("msg", "")}
    return {"__raise__": type(e).__name__, "msg": str(e)}


def main():
    ns, real = extract()
    install_module_stubs()
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
            ret, state = drive(ns, real, func, inp)
            want = {"ret": norm_ret(ret), "raise": None,
                    "state": norm_ret(state)}
        except BaseException as e:   # SystemExit / KeyboardInterrupt included
            st = CUR["state_out"]
            if st is None:
                st = inp.get("state") or {}
            want = {"ret": None, "raise": exc_spec(e),
                    "state": norm_ret(st)}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE moa_loop_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())