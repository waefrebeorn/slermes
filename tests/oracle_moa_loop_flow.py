#!/usr/bin/env python3
"""Oracle for src/agent/moa_loop_flow.c — drives the LIVE agent/moa_loop.py
fan-out / aggregation FLOW defs + leaves under IDENTICAL scripted seams.

The 18 targets (14 module functions + 4 MoAChatCompletions methods) are
AST-extracted in source order; module Assign/AnnAssign constants are seeded
(plain-Name targets only), the real _RefAccounting dataclass and the real
CanonicalUsage (from agent/usage_pricing.py) are extracted, and every node is
compiled with the PEP 563 future flag so annotations stay strings. The exec
namespace is a registered module (py3.14 dataclasses needs sys.modules[__name__]).

All live model-call / sibling / clock I/O is replaced by recording stubs
consuming the SAME "io" scripts the C harness feeds its universal seam:
absent/null -> None on every call; {"seq": [...]} -> per-call sequence (None
when exhausted); any other value -> reused. Each call records {ev, args, ret}
(ret = RAW scripted value) in call order. The _run_references_parallel stub
fires one progress_callback per scripted "progress" entry (each invokes the
_emit seam, raises suppressed as the live collect() does) before returning the
materialized outputs.

Value model: CanonicalUsage / _RefAccounting instances normalize to tagged
dicts ({"_type": ...}) on BOTH sides; inputs materialize back to real instances
so isinstance gates and __add__ run with live semantics. A scripted
CancelledError (BaseException) propagates past `except Exception` guards exactly
as live; the C port models the same class check.

Input: stdin JSON lines from t_port_moa_loop_flow:
    {"func": ..., "in": {...}, "out": {"ret","raise","trace","state"}}
Summary: ORACLE moa_loop_flow: N cases, M mismatches.
Fail-closed: exit 1 on any mismatch, on a PYERR, or when zero cases ran.
"""
import ast
import asyncio
import contextlib
import copy
import dataclasses
import functools
import hashlib
import json
import os
import re
import sys
import threading
import types
import __future__
from dataclasses import dataclass, field, fields, replace, KW_ONLY
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "moa_loop.py")
PRICING = os.path.join(QUARRY, "agent", "usage_pricing.py")

MODULE_TARGETS = {
    "_field", "_slot_labels", "_agent_cache_opts", "_with_cache_disabled",
    "_hash_messages", "_tool_activity_since_last_user", "_placeholder_output",
    "_join_reference_outputs", "_guidance_inputs", "_sum_reference_accounting",
    "_reference_context_length", "_price_reference_response", "_record_late",
    "_settle_interrupted",
}
METHOD_TARGETS = {
    "_fold_pending_accounting", "_fanout_cache_key", "_build_guidance",
    "_run_fanout",
}
WANTED = MODULE_TARGETS | METHOD_TARGETS
# Module siblings seamed in composite flows (claimed ports or other targets).
MODULE_SEAMS = {
    "_slot_label", "_is_failed_reference", "_redact_reference_outputs",
    "_redact_reference_text", "_redact_trace_accounting", "_degraded_notice",
    "_preset_temperature", "_run_references_parallel", "_sum_reference_accounting",
    "_guidance_inputs", "_join_reference_outputs", "_placeholder_output",
    "_hash_messages",
}
# Facade-method seams injected onto self for _run_fanout (excluded from state).
RUN_FANOUT_SELF_SEAMS = {
    "_emit", "_fold_pending_accounting", "_record_late_reference_accounting",
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
    "CancelledError": asyncio.CancelledError, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls in BUILTIN_EXC:
        return BUILTIN_EXC[cls](msg)
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

USAGE_FIELDS = ("input_tokens", "output_tokens", "cache_read_tokens",
                "cache_write_tokens", "reasoning_tokens", "request_count")
ACCT_FIELDS = ("usage", "cost_usd", "cost_status", "cost_source", "messages",
               "output", "model", "provider", "temperature")


def norm_ret(x):
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        if type(x).__name__ == "CanonicalUsage":
            d = {"_type": "CanonicalUsage"}
            for f in USAGE_FIELDS:
                d[f] = norm_ret(getattr(x, f))
            return d
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
    return x


def materialize_usage(j):
    if isinstance(j, dict) and j.get("_type") == "CanonicalUsage":
        return CanonicalUsage(**{f: j.get(f, 0) for f in USAGE_FIELDS})
    return j


def materialize_acct(j):
    if isinstance(j, dict) and j.get("_type") == "_RefAccounting":
        return _RefAccounting(
            usage=materialize_usage(j.get("usage")),
            cost_usd=j.get("cost_usd"), cost_status=j.get("cost_status"),
            cost_source=j.get("cost_source"), messages=j.get("messages"),
            output=j.get("output"), model=j.get("model"),
            provider=j.get("provider"), temperature=j.get("temperature"))
    return j


def materialize_output(j):
    if isinstance(j, (list, tuple)) and len(j) == 3:
        return (j[0], j[1], materialize_acct(j[2]))
    return j


def materialize_outputs(j):
    if isinstance(j, list):
        return [materialize_output(o) for o in j]
    return j


def materialize_attr(j):
    if j is None:
        return None
    if isinstance(j, dict):
        return types.SimpleNamespace(**j)
    return j


# ── per-seam argument projection (identical field sets to the C args) ──

ARGMAP = {
    "_slot_label": lambda a, k: {"slot": a[0]},
    "_is_failed_reference": lambda a, k: {"text": a[0]},
    "_redact_reference_outputs": lambda a, k: {"reference_outputs": a[0]},
    "_redact_reference_text": lambda a, k: {"text": a[0]},
    "_redact_trace_accounting": lambda a, k: {"acct": a[0]},
    "_degraded_notice": lambda a, k: {"failed_labels": a[0], "policy": a[1]},
    "_preset_temperature": lambda a, k: {"preset": a[0], "key": a[1]},
    "_run_references_parallel": lambda a, k: {
        "reference_models": a[0], "ref_messages": a[1],
        "temperature": k.get("temperature"),
        "reference_timeout": k.get("reference_timeout"),
        "agent": k.get("agent"),
        "progress_callback": "<callable>", "late_accounting_sink": "<callable>"},
    "_sum_reference_accounting": lambda a, k: {"outputs": a[0]},
    "_fold_pending_accounting": lambda a, k: {"usage": a[0], "cost": a[1]},
    "_guidance_inputs": lambda a, k: {
        "reference_outputs": a[0], "privacy_full": a[1], "policy": a[2]},
    "_join_reference_outputs": lambda a, k: {
        "outputs": a[0], "degraded": a[1] if len(a) > 1 else k.get("degraded", "")},
    "_placeholder_output": lambda a, k: {"slot": a[0], "note": a[1]},
    "_hash_messages": lambda a, k: {"msgs": a[0]},
    "_emit": lambda a, k: dict({"event": a[0]}, **k),
    "slot_metrics": lambda a, k: {"acct": a[0], "label": a[1], "output": k.get("output")},
    "get_model_context_length": lambda a, k: {
        "model": k.get("model"), "base_url": k.get("base_url"),
        "api_key": k.get("api_key"), "provider": k.get("provider")},
    "normalize_usage": lambda a, k: {
        "raw_usage": a[0], "provider": k.get("provider"), "api_mode": k.get("api_mode")},
    "estimate_usage_cost": lambda a, k: {
        "model": a[0], "usage": a[1], "provider": k.get("provider"),
        "base_url": k.get("base_url"), "api_key": k.get("api_key")},
    "future.cancel": lambda a, k: {"idx": a[0]},
    "future.done": lambda a, k: {"idx": a[0]},
    "future.result": lambda a, k: {"idx": a[0]},
    "future.add_done_callback": lambda a, k: {"idx": a[0], "label": k.get("label")},
    "sink": lambda a, k: {"label": a[0], "acct": a[1]},
}


def materialize_result(name, r):
    if name == "_sum_reference_accounting":
        return (materialize_usage(r[0]), r[1])
    if name == "_placeholder_output":
        return materialize_output(r)
    if name == "_guidance_inputs":
        return (materialize_outputs(r[0]), r[1], r[2])
    if name == "_redact_reference_outputs":
        return materialize_outputs(r)
    if name == "normalize_usage":
        return materialize_usage(r)
    if name == "future.result":
        return materialize_output(r)
    if name == "estimate_usage_cost":
        if r is None:
            return None
        return types.SimpleNamespace(amount_usd=r.get("amount_usd"),
                                     status=r.get("status"), source=r.get("source"))
    return r


def stub(name):
    def f(*a, **k):
        args = {kk: norm_ret(vv) for kk, vv in ARGMAP[name](a, k).items()
                if vv is not None}
        r = script_next(name)
        rec({"ev": name, "args": args, "ret": r})
        if name == "_run_references_parallel":
            cb = k.get("progress_callback")
            for p in (r or {}).get("progress", []):
                if cb is not None:
                    try:
                        cb(p.get("refs_done"), p.get("refs_total"), p.get("label"))
                    except Exception:
                        pass   # live collect() suppresses progress failures
            return materialize_outputs((r or {}).get("outputs", []))
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return materialize_result(name, r)
    return f


# ── extraction ──

def compile_node(node, path):
    return compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                   flags=__future__.annotations.compiler_flag)


def extract():
    global CanonicalUsage, _RefAccounting
    modname = "moa_loop_flow_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": modname,
        "contextlib": contextlib, "functools": functools, "hashlib": hashlib,
        "json": json, "re": re, "threading": threading,
        "dataclass": dataclass, "field": field, "fields": fields,
        "replace": replace, "KW_ONLY": KW_ONLY,
        "Any": Any, "Optional": Optional,
        "logging": types.SimpleNamespace(getLogger=lambda name: SilentLogger()),
        "logger": SilentLogger(),
    })

    # Real CanonicalUsage + _PER_RESPONSE_FIELDS from usage_pricing.py.
    ptree = ast.parse(open(PRICING, encoding="utf-8").read())
    pns = {"dataclass": dataclass, "fields": fields, "Any": Any, "Optional": Optional}
    for node in ptree.body:
        if isinstance(node, ast.ClassDef) and node.name == "CanonicalUsage":
            exec(compile_node(node, PRICING), pns)
        elif isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_PER_RESPONSE_FIELDS"
                for t in node.targets):
            exec(compile_node(node, PRICING), pns)
    CanonicalUsage = pns["CanonicalUsage"]
    ns["CanonicalUsage"] = CanonicalUsage

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
        if isinstance(node, ast.ClassDef) and node.name == "_RefAccounting":
            exec(compile_node(node, SRC), ns)
            _RefAccounting = ns["_RefAccounting"]
        elif isinstance(node, ast.FunctionDef) and node.name in MODULE_TARGETS:
            exec(compile_node(node, SRC), ns)
            real[node.name] = ns[node.name]
        elif isinstance(node, ast.ClassDef) and node.name == "MoAChatCompletions":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in METHOD_TARGETS:
                    sub = copy.copy(sub)
                    sub.decorator_list = []
                    exec(compile_node(sub, SRC), ns)
                    real[sub.name] = ns[sub.name]
    missing = WANTED - set(real)
    if missing:
        raise RuntimeError("extraction missed: %s" % sorted(missing))
    for const in ("_ADVISORY_INSTRUCTION", "_STALE_GUIDANCE_NOTE",
                  "_INTERRUPTED_REFERENCE_NOTE"):
        if const not in ns:
            raise RuntimeError("constant not seeded: %s" % const)
    return ns, real


def modstub(fullname, **attrs):
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
    for kk, vv in attrs.items():
        setattr(sys.modules[fullname], kk, vv)


def install_module_stubs():
    modstub("agent.moa_trace", slot_metrics=stub("slot_metrics"))
    modstub("agent.model_metadata",
            get_model_context_length=stub("get_model_context_length"))
    modstub("agent.usage_pricing",
            normalize_usage=stub("normalize_usage"),
            estimate_usage_cost=stub("estimate_usage_cost"),
            CanonicalUsage=CanonicalUsage)


# ── Future double (cancel / done / result / add_done_callback seams) ──

class FutureDouble:
    def __init__(self, idx):
        self.idx = idx

    def cancel(self):
        return stub("future.cancel")(self.idx)

    def done(self):
        return stub("future.done")(self.idx)

    def result(self):
        return stub("future.result")(self.idx)

    def add_done_callback(self, cb):
        label = cb.args[1] if isinstance(cb, functools.partial) and len(cb.args) > 1 else None
        return stub("future.add_done_callback")(self.idx, label=label)


# ── per-case setup ──

def setup_case(ns, real, inp, target):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    # Module siblings: the target runs REAL (if module-level), others seamed.
    for name in MODULE_SEAMS:
        ns[name] = real[name] if name == target else stub(name)
    for name in MODULE_TARGETS:
        if name not in MODULE_SEAMS:
            ns[name] = real[name] if name == target else stub(name)


def self_state(inp):
    return dict(inp.get("state") or {})


def flow_state(slf, injected):
    return {k: norm_ret(v) for k, v in vars(slf).items() if k not in injected}


def drive(ns, real, func, inp):
    """Returns (ret, state_dict)."""
    st = inp.get("state") or {}

    if func == "_field":
        obj = inp.get("obj")
        if isinstance(obj, dict) and "__attr__" in obj:
            obj = types.SimpleNamespace(**obj["__attr__"])
        return ns[func](obj, inp.get("name", "")), {}

    if func == "_slot_labels":
        return ns[func](inp.get("slots") or []), {}

    if func == "_agent_cache_opts":
        return ns[func](materialize_attr(inp.get("agent"))), {}

    if func == "_with_cache_disabled":
        return ns[func](inp.get("runtime") or {}, inp.get("cache_disabled")), {}

    if func == "_hash_messages":
        return ns[func](inp.get("msgs") or []), {}

    if func == "_tool_activity_since_last_user":
        return ns[func](inp.get("messages") or []), {}

    if func == "_placeholder_output":
        return ns[func](inp.get("slot") or {}, inp.get("note")), {}

    if func == "_join_reference_outputs":
        return ns[func](materialize_outputs(inp.get("outputs") or []),
                        inp.get("degraded", "")), {}

    if func == "_guidance_inputs":
        return ns[func](materialize_outputs(inp.get("reference_outputs") or []),
                        inp.get("privacy_full", False), inp.get("policy", "")), {}

    if func == "_sum_reference_accounting":
        return ns[func](materialize_outputs(inp.get("outputs") or [])), {}

    if func == "_reference_context_length":
        cj = st.get("ctx_cache")
        cache = None
        if isinstance(cj, dict):
            cache = {tuple(k.split("\t", 1)): v for k, v in cj.items()}
        r = ns[func](inp.get("slot") or {}, inp.get("runtime") or {}, cache)
        out = dict(st)
        if cache is not None:
            out["ctx_cache"] = {"%s\t%s" % k: norm_ret(v) for k, v in cache.items()}
        else:
            out["ctx_cache"] = None
        return r, out

    if func == "_price_reference_response":
        response = types.SimpleNamespace(usage=inp.get("response_usage"))
        return ns[func](response, inp.get("slot") or {}, inp.get("runtime") or {}), {}

    if func == "_record_late":
        fut = FutureDouble(inp.get("future_idx", 0))
        return ns[func](stub("sink"), inp.get("label", ""), fut), {}

    if func == "_settle_interrupted":
        futures = {}
        for fs in inp.get("futures") or []:
            futures[FutureDouble(fs.get("idx", 0))] = fs.get("idx", 0)
        results = [materialize_output(r) if r is not None else None
                   for r in (st.get("results") or [])]
        sink = (lambda *a, **k: None) if inp.get("sink_present") else None
        ns[func](futures, results, inp.get("reference_models") or [], sink)
        out = dict(st)
        out["results"] = norm_ret(results)
        return None, out

    if func == "_fold_pending_accounting":
        slf = types.SimpleNamespace(**self_state(inp))
        if isinstance(slf._pending_reference_usage, dict):
            slf._pending_reference_usage = materialize_usage(slf._pending_reference_usage)
        slf._accounting_lock = NoopLock()
        m = types.MethodType(real[func], slf)
        m(materialize_usage(inp.get("usage")), inp.get("cost"))
        return None, flow_state(slf, {"_accounting_lock"})

    if func == "_fanout_cache_key":
        slf = types.SimpleNamespace(**self_state(inp))
        m = types.MethodType(real[func], slf)
        r = m(inp.get("preset") or {}, inp.get("ref_messages") or [],
              inp.get("reference_models") or [])
        return r, flow_state(slf, set())

    if func == "_build_guidance":
        slf = types.SimpleNamespace(**self_state(inp))
        m = types.MethodType(real[func], slf)
        r = m(materialize_outputs(inp.get("reference_outputs") or []),
              inp.get("aggregator") or {}, inp.get("degraded_reference_policy", "loud"),
              stale=inp.get("stale", False))
        return r, flow_state(slf, set())

    if func == "_run_fanout":
        slf = types.SimpleNamespace(**self_state(inp))
        slf._emit = stub("_emit")
        slf._fold_pending_accounting = stub("_fold_pending_accounting")
        slf._record_late_reference_accounting = lambda *a, **k: None
        m = types.MethodType(real[func], slf)
        r = m(inp.get("preset") or {}, inp.get("ref_messages") or [],
              inp.get("reference_models") or [], inp.get("aggregator") or {},
              inp.get("aggregator_temperature"), inp.get("cache_key"))
        return r, flow_state(slf, RUN_FANOUT_SELF_SEAMS)

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
            want = {"ret": norm_ret(ret), "raise": None, "state": norm_ret(state)}
        except BaseException as e:   # CancelledError / SystemExit included
            want = {"ret": None,
                    "raise": {"__raise__": type(e).__name__, "msg": str(e)},
                    "state": norm_ret(inp.get("state") or {})}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE moa_loop_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())