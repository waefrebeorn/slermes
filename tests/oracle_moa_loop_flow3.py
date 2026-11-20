#!/usr/bin/env python3
"""Oracle for src/agent/moa_loop_flow3.c — drives the LIVE agent/moa_loop.py
REMAINING-unclaimed lifecycle FLOW defs under IDENTICAL scripted seams.

The 15 targets (10 MoAChatCompletions methods + 5 MoAClient methods, all
claimed only by printf-stub-era fakes — see the port header's claim landscape)
are AST-extracted in source order; module Assign/AnnAssign constants are
seeded (plain-Name targets only), the real _RefAccounting dataclass and the
real CanonicalUsage dataclass (agent/usage_pricing.py) are extracted, and
every node is compiled with the PEP 563 future flag so annotations stay
strings. The exec namespace is a registered module (py3.14 dataclasses needs
sys.modules[__name__]).

All claimed siblings (flow1/flow2 ports + stub-claimed leaves) and every
foreign-module import are replaced by recording stubs consuming the SAME "io"
scripts the C harness feeds its universal seam: absent/null -> None on every
call; {"seq": [...]} -> per-call sequence (None when exhausted); any other
value -> reused. Each call records {ev, args, ret} (ret = RAW scripted value,
deep-copied at call time) in call order. Ported functions compose REAL
(internal composition: prepare->create->_call_prepared_aggregator,
MoAClient.*->MoAChatCompletions.*, MoAClient.__init__->MoAChatCompletions
.__init__ via a factory double). Seamed SELF methods bind under their live
attribute names with their REAL signatures (CPython raises the py3.14
call-binding TypeErrors at the live call site).

Value model: attribute doubles {"__attr__": ...} materialize to
SimpleNamespace; callable doubles {"__call__": name} materialize to tagged
recorder functions; _RefAccounting instances normalize to tagged dicts;
CanonicalUsage INPUT tags {"_type": "CanonicalUsage", ...} materialize to real
instances (ALWAYS truthy) and normalize via asdict to plain field dicts;
sets normalize to SORTED arrays; tuples to arrays; the one-chunk stream
iterator projects to {"__iter__": [chunk]}; the merge_same_role_messages
identity ret {"__echo__": "messages"} returns the live input object itself.
Scripted raises are REAL exception instances (builtins by name, synthetic
Exception subclasses otherwise); a scripted exception propagating out is
reported with its spec VERBATIM (identity match), an unscripted live raise
with {type, str(e)}. BaseException-only kinds (SystemExit / KeyboardInterrupt
/ GeneratorExit / CancelledError) pass through every `except Exception` /
suppress(Exception) site exactly as live.

State windows: MoAChatCompletions.* carry the facade attribute window (the
self SimpleNamespace double; the unobservable _accounting_lock and the
bound-method keys are stripped from the echo); MoAClient.* carry
{"chat": {"completions": <facade window>}}. Both __init__ funcs construct a
FRESH object (in.state ignored). Partial mutations survive a raise (the
drive's finally re-echoes the live vars).

Input: stdin JSON lines from t_port_moa_loop_flow3:
    {"func": ..., "in": {...}, "out": {"ret","raise","trace","state"}}
Summary: ORACLE moa_loop_flow3: N cases, M mismatches.
Fail-closed: exit 1 on any mismatch, on a setup PYERR, when zero cases ran,
or when a target func never appeared in the case stream.
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
from dataclasses import dataclass, field, replace, KW_ONLY, fields
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "moa_loop.py")
USAGE_SRC = os.path.join(QUARRY, "agent", "usage_pricing.py")

MCC_PORTED = [
    "__init__", "consume_reference_usage", "last_reference_metrics",
    "_record_late_reference_accounting", "consume_and_save_trace", "_emit",
    "prepare", "rebase_prepared_request", "create", "_call_prepared_aggregator",
]
CLIENT_PORTED = [
    "__init__", "consume_reference_usage", "last_aggregator_slot",
    "consume_and_save_trace", "last_reference_metrics",
]
MCC_SEAMED_METHODS = [
    "_fold_pending_accounting", "_fanout_cache_key", "_run_fanout",
    "_build_guidance", "_plan_aggregator_cache",
]
FACADE_BINDS = set(MCC_PORTED) | set(MCC_SEAMED_METHODS)
WANTED = ({"MoAChatCompletions." + n for n in MCC_PORTED}
          | {"MoAClient." + n for n in CLIENT_PORTED})
# Claimed module siblings seamed wherever a ported flow reaches them.
MODULE_SEAMS = [
    "_sum_reference_accounting", "_resolve_preset_cached", "_moa_privacy_mode",
    "_reference_messages", "_preset_temperature", "_attach_reference_guidance",
    "_redact_trace_messages", "_slot_label", "_tool_activity_since_last_user",
    "_slot_runtime", "_merge_slot_extra_body", "_aggregator_reasoning_config",
    "_extract_text", "_completed_response_as_stream_chunk",
    "call_llm", "destination_key", "merge_same_role_messages",
    "is_role_alternation_rejection",
]
# Foreign in-function imports -> sys.modules recording stubs.
IMPORT_SEAMS = {
    "agent.moa_trace": {"save_moa_turn"},
    "agent.prompt_cache_clock": {"attempt_facts"},
}

CUR = {"in": {}, "trace": [], "counts": {}, "scripted": [], "state_out": None}
REAL = {}
NS = None
CanonicalUsage = None
_RefAccounting = None

CU_FIELDS = ("input_tokens", "output_tokens", "cache_read_tokens",
             "cache_write_tokens", "reasoning_tokens", "request_count",
             "raw_usage", "cache_write_by_ttl")
ACCT_FIELDS = ("usage", "cost_usd", "cost_status", "cost_source", "messages",
               "output", "model", "provider", "temperature")


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

def norm_ret(x):
    if isinstance(x, types.SimpleNamespace):
        return {"__attr__": {k: norm_ret(v) for k, v in vars(x).items()}}
    if callable(x) and hasattr(x, "_ml3_seam"):
        return {"__call__": x._ml3_seam}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        if type(x).__name__ == "_RefAccounting":
            d = {"_type": "_RefAccounting"}
            for f in ACCT_FIELDS:
                d[f] = norm_ret(getattr(x, f))
            return d
        return norm_ret(dataclasses.asdict(x))
    if isinstance(x, (set, frozenset)):
        return sorted((norm_ret(v) for v in x))
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


def proj_ret(ret):
    """The one-chunk stream iterator projects to {"__iter__": [items]}."""
    if (hasattr(ret, "__next__") and hasattr(ret, "__iter__")
            and not isinstance(ret, (list, tuple, dict, str, bytes,
                                     set, frozenset))):
        return {"__iter__": [norm_ret(x) for x in ret]}
    return norm_ret(ret)


def recorder(name):
    def f(*a, **k):
        return stub(name)(*a, **k)
    f._ml3_seam = name
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


def mat_cu(v):
    if isinstance(v, dict) and v.get("_type") == "CanonicalUsage":
        return CanonicalUsage(**{k: v[k] for k in CU_FIELDS if k in v})
    return v


def mat_tuple_tree(v):
    if isinstance(v, list):
        return tuple(mat_tuple_tree(x) for x in v)
    return v


# ── per-seam argument projection (identical field sets to the C args) ──

ARGMAP = {
    "_sum_reference_accounting": lambda a, k: {"outputs": a[0]},
    "_fold_pending_accounting": lambda a, k: {"usage": a[0], "cost": a[1]},
    "_resolve_preset_cached": lambda a, k: {"preset_name": a[0]},
    "_moa_privacy_mode": lambda a, k: {"moa_raw": a[0]},
    "_reference_messages": lambda a, k: {"messages": a[0]},
    "_preset_temperature": lambda a, k: {"preset": a[0], "key": a[1]},
    "_attach_reference_guidance": lambda a, k: {
        "agg_messages": a[0], "guidance": a[1]},
    "_redact_trace_messages": lambda a, k: {"messages": a[0]},
    "_slot_label": lambda a, k: {"slot": a[0]},
    "_tool_activity_since_last_user": lambda a, k: {"messages": a[0]},
    "_slot_runtime": lambda a, k: {"slot": a[0]},
    "_merge_slot_extra_body": lambda a, k: {
        "slot_extra_body": a[0], "caller_extra_body": a[1]},
    "_aggregator_reasoning_config": lambda a, k: {"aggregator": a[0]},
    "_extract_text": lambda a, k: {"response": a[0]},
    "_completed_response_as_stream_chunk": lambda a, k: {"response": a[0]},
    "call_llm": lambda a, k: dict(k),
    "destination_key": lambda a, k: {"runtime": a[0]},
    "merge_same_role_messages": lambda a, k: {"messages": a[0]},
    "is_role_alternation_rejection": lambda a, k: {
        "exc": a[0], "runtime": a[1]},
    "attempt_facts": lambda a, k: {"payload": a[0]},
    "save_moa_turn": lambda a, k: dict(k),
    "_fanout_cache_key": lambda a, k: {
        "preset": a[0], "ref_messages": a[1], "reference_models": a[2]},
    "_run_fanout": lambda a, k: {
        "preset": a[0], "ref_messages": a[1], "reference_models": a[2],
        "aggregator": a[3], "aggregator_temperature": a[4], "cache_key": a[5]},
    "_build_guidance": lambda a, k: {
        "reference_outputs": a[0], "aggregator": a[1],
        "degraded_reference_policy": a[2], "stale": k.get("stale")},
    "_plan_aggregator_cache": lambda a, k: {
        "agg_messages": a[0], "tools": a[1], "guidance": a[2],
        "agg_runtime": a[3]},
    "reference_callback": lambda a, k: {"event": a[0], "kwargs": k},
}


def materialize_result(name, r, raw_args):
    if name == "call_llm":
        return mat_value(r)   # agg_response may be an __attr__ double
    if name == "destination_key":
        return tuple(r) if isinstance(r, list) else r
    if name == "_fanout_cache_key":
        return mat_tuple_tree(r)
    if name == "merge_same_role_messages":
        if isinstance(r, dict) and r.get("__echo__") == "messages":
            return raw_args.get("messages")   # identity: the input object
        return r
    return r


def stub(name):
    def f(*a, **k):
        raw = ARGMAP[name](a, k)
        args = {kk: norm_ret(vv) for kk, vv in raw.items() if vv is not None}
        r = script_next(name)
        rec({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(r)})
        if isinstance(r, dict) and "__raise__" in r:
            e = make_exc(r)
            CUR["scripted"].append((e, r))
            raise e
        return materialize_result(name, r, raw)
    return f


# ── extraction ──

def compile_node(node, path):
    return compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                   flags=__future__.annotations.compiler_flag)


def strip_decorators(node):
    n = copy.copy(node)
    n.decorator_list = []
    return n


METHOD_STUB_SRC = '''
def _fold_pending_accounting(self, usage, cost):
    return stub("_fold_pending_accounting")(usage, cost)
def _fanout_cache_key(self, preset, ref_messages, reference_models):
    return stub("_fanout_cache_key")(preset, ref_messages, reference_models)
def _run_fanout(self, preset, ref_messages, reference_models, aggregator, aggregator_temperature, cache_key):
    return stub("_run_fanout")(preset, ref_messages, reference_models, aggregator, aggregator_temperature, cache_key)
def _build_guidance(self, reference_outputs, aggregator, degraded_reference_policy, stale=False):
    return stub("_build_guidance")(reference_outputs, aggregator, degraded_reference_policy, stale=stale)
def _plan_aggregator_cache(self, agg_messages, tools, guidance, agg_runtime):
    return stub("_plan_aggregator_cache")(agg_messages, tools, guidance, agg_runtime)
'''


def extract_canonical_usage():
    global CanonicalUsage
    modname = "moa_loop_flow3_cu_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns2 = mod.__dict__
    ns2.update({"__name__": modname, "dataclass": dataclass,
                "field": field, "fields": fields,
                "Optional": Optional, "Any": Any})
    tree = ast.parse(open(USAGE_SRC, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "CanonicalUsage":
            exec(compile_node(node, USAGE_SRC), ns2)
            CanonicalUsage = ns2["CanonicalUsage"]
            return
    raise RuntimeError("CanonicalUsage class not found")


def extract():
    global _RefAccounting, NS
    extract_canonical_usage()
    modname = "moa_loop_flow3_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": modname,
        "contextlib": contextlib, "functools": functools, "json": json,
        "re": re, "threading": threading,
        "dataclass": dataclass, "field": field, "replace": replace,
        "KW_ONLY": KW_ONLY, "Any": Any,
        "SimpleNamespace": types.SimpleNamespace,
        "logging": types.SimpleNamespace(getLogger=lambda name: SilentLogger()),
        "logger": SilentLogger(),
        "CanonicalUsage": CanonicalUsage,
        "stub": stub,
        "flatten_message_text": lambda *a, **k: None,
        "DATA_USE": None,
        "get_transport": lambda *a, **k: None,
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

    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_RefAccounting":
            exec(compile_node(node, SRC), ns)
            _RefAccounting = ns["_RefAccounting"]
        elif isinstance(node, ast.ClassDef) and node.name == "MoAChatCompletions":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in MCC_PORTED:
                    exec(compile_node(strip_decorators(sub), SRC), ns)
                    REAL["MoAChatCompletions." + sub.name] = ns[sub.name]
        elif isinstance(node, ast.ClassDef) and node.name == "MoAClient":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in CLIENT_PORTED:
                    exec(compile_node(strip_decorators(sub), SRC), ns)
                    REAL["MoAClient." + sub.name] = ns[sub.name]

    missing = WANTED - set(REAL)
    if missing:
        raise RuntimeError("extraction missed: %s" % sorted(missing))
    if _RefAccounting is None:
        raise RuntimeError("_RefAccounting not extracted")

    # Seamed SELF methods: real signatures, recorder bodies.
    exec(compile(METHOD_STUB_SRC, "<ml3 method stubs>", "exec"), ns)

    # MoAChatCompletions factory double for MoAClient.__init__'s construction
    # call (the live class body is heavy; only __init__ is ported).
    def _moa_cc_factory(preset_name, reference_callback=None, agent=None):
        slf = types.SimpleNamespace()
        REAL["MoAChatCompletions.__init__"](
            slf, preset_name, reference_callback=reference_callback, agent=agent)
        bind_facade(slf)
        return slf
    ns["MoAChatCompletions"] = _moa_cc_factory

    for const in ("_RELAY_EVENTS",):
        if const not in ns:
            raise RuntimeError("constant not seeded: %s" % const)
    return ns


def bind_facade(slf):
    slf._accounting_lock = NoopLock()
    for name in MCC_PORTED:
        setattr(slf, name, types.MethodType(REAL["MoAChatCompletions." + name], slf))
    for name in MCC_SEAMED_METHODS:
        setattr(slf, name, types.MethodType(NS[name], slf))


def build_facade(j):
    """Materialize a facade attribute window into a live self double."""
    if j is None:
        return None
    if not isinstance(j, dict):
        return mat_value(j)
    slf = types.SimpleNamespace()
    for k, v in j.items():
        if k == "_accounting_lock" or k in FACADE_BINDS:
            continue
        if k == "_pending_reference_usage":
            setattr(slf, k, mat_cu(v))
        elif k == "_ref_cache_key":
            setattr(slf, k, mat_tuple_tree(v))
        elif k == "_ref_cache_outputs":
            if isinstance(v, list):
                rows = []
                for row in v:
                    if isinstance(row, list):
                        rows.append(tuple(materialize_acct(x) for x in row))
                    else:
                        rows.append(row)
                setattr(slf, k, rows)
            else:
                setattr(slf, k, v)
        elif k == "_merge_same_role_destinations":
            if isinstance(v, list):
                setattr(slf, k, set(tuple(x) if isinstance(x, list) else x
                                    for x in v))
            else:
                setattr(slf, k, v)
        else:
            setattr(slf, k, mat_value(v))
    bind_facade(slf)
    return slf


def facade_echo(slf):
    return {k: norm_ret(v) for k, v in vars(slf).items()
            if k not in FACADE_BINDS and k != "_accounting_lock"}


def build_client(st):
    cl = types.SimpleNamespace()
    if "chat" in st:
        cj = st["chat"]
        if isinstance(cj, dict):
            chat = types.SimpleNamespace()
            for k, v in cj.items():
                if k == "completions":
                    chat.completions = build_facade(v)
                else:
                    setattr(chat, k, mat_value(v))
            cl.chat = chat
        else:
            cl.chat = mat_value(cj)
    return cl


def client_echo(cl):
    out = {}
    for k, v in vars(cl).items():
        if (k == "chat" and hasattr(v, "__dict__")
                and not isinstance(v, (str, int, float, bool, list, dict,
                                       tuple, types.SimpleNamespace))):
            chat_out = {}
            for ck, cv in vars(v).items():
                if ck == "completions" and isinstance(cv, types.SimpleNamespace):
                    chat_out[ck] = facade_echo(cv)
                else:
                    chat_out[ck] = norm_ret(cv)
            out[k] = chat_out
        elif k == "chat" and isinstance(v, types.SimpleNamespace):
            chat_out = {}
            for ck, cv in vars(v).items():
                if ck == "completions" and isinstance(cv, types.SimpleNamespace):
                    chat_out[ck] = facade_echo(cv)
                else:
                    chat_out[ck] = norm_ret(cv)
            out[k] = chat_out
        else:
            out[k] = norm_ret(v)
    return out


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


# ── per-case setup ──

def setup_case(inp):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    CUR["scripted"] = []
    CUR["state_out"] = None
    for name in MODULE_SEAMS:
        NS[name] = stub(name)
    NS["logger"] = SilentLogger()


def drive(func, inp):
    """Returns (ret, state_dict); CUR["state_out"] holds partial state on raise."""
    st = inp.get("state") or {}

    if func == "MoAChatCompletions.__init__":
        slf = types.SimpleNamespace()
        bind_facade(slf)
        CUR["state_out"] = {}
        try:
            REAL[func](slf, inp.get("preset_name"),
                       reference_callback=mat_value(inp.get("reference_callback")),
                       agent=mat_value(inp.get("agent")))
        finally:
            CUR["state_out"] = facade_echo(slf)
        return None, CUR["state_out"]

    if func.startswith("MoAChatCompletions."):
        name = func.split(".", 1)[1]
        slf = build_facade(st)
        m = types.MethodType(REAL[func], slf)
        try:
            if name == "consume_reference_usage":
                ret = m()
            elif name == "last_reference_metrics":
                ret = m()
            elif name == "_record_late_reference_accounting":
                ret = m(inp.get("label", ""),
                        materialize_acct(inp.get("accounting")))
            elif name == "consume_and_save_trace":
                ret = m(inp.get("session_id"),
                        aggregator_output_fallback=inp.get("aggregator_output_fallback"))
            elif name == "_emit":
                ret = m(inp.get("event", ""), **(inp.get("kwargs") or {}))
            elif name == "prepare":
                ret = m(inp.get("messages"))
            elif name == "rebase_prepared_request":
                ret = m(inp.get("prepared"), inp.get("messages"))
            elif name == "create":
                ret = m(**(inp.get("api_kwargs") or {}))
            elif name == "_call_prepared_aggregator":
                ret = m(inp.get("prepared"), inp.get("api_kwargs") or {})
            else:
                raise ValueError("unknown MCC func: %s" % name)
        finally:
            CUR["state_out"] = facade_echo(slf)
        return ret, CUR["state_out"]

    if func == "MoAClient.__init__":
        cl = types.SimpleNamespace()
        CUR["state_out"] = {}
        try:
            REAL[func](cl, inp.get("preset_name"),
                       reference_callback=mat_value(inp.get("reference_callback")),
                       agent=mat_value(inp.get("agent")))
        finally:
            CUR["state_out"] = client_echo(cl)
        return None, CUR["state_out"]

    if func.startswith("MoAClient."):
        name = func.split(".", 1)[1]
        cl = build_client(st)
        try:
            if name == "last_aggregator_slot":
                ret = REAL[func](cl)   # property (decorator stripped)
            elif name == "consume_and_save_trace":
                m = types.MethodType(REAL[func], cl)
                ret = m(inp.get("session_id"),
                        aggregator_output_fallback=inp.get("aggregator_output_fallback"))
            else:
                m = types.MethodType(REAL[func], cl)
                ret = m()
        finally:
            CUR["state_out"] = client_echo(cl)
        return ret, CUR["state_out"]

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
    ns = extract()
    install_module_stubs()
    cases = 0
    mism = 0
    seen = set()
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
        seen.add(func)
        setup_case(inp)
        try:
            ret, state = drive(func, inp)
            want = {"ret": proj_ret(ret), "raise": None,
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
    print("ORACLE moa_loop_flow3: %d cases, %d mismatches" % (cases, mism))
    never = WANTED - seen
    if never:
        print("SETUP PYERR: targets never exercised: %s" % sorted(never))
        return 1
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
