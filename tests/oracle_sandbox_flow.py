#!/usr/bin/env python3
"""Oracle for the sandbox-scope FLOW surface (src/tools/sandbox_flow.c).

Drives the LIVE tools/environments/sandbox_scope.py defs (the quarry's tools/
sandbox module) with SCRIPTED seams: targets are AST-extracted (no heavy
import), module constants/classes are exec'd, and every external dependency
(current_scope, bind_state, provider.create_environment, inner.<op>, pipe.<op>,
the wrapped fn) is replaced by a recording stub returning the same scripted
results the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered {ev,args,ret} trace) plus the return value, propagated exception and
(for env_setattr) the mutated state.

Fail-closed: a MISSING script key or an EMPTY array script yields a
{"__raise__":"RuntimeError","msg":"no script for <name>"} spec that is RECORDED
as the event ret and then raised (mirrors the C dispatcher exactly). Scripted
raises use REAL exception classes (extracted module classes, then builtins —
the BaseException trio + StopIteration are the real builtins, never lookalikes).

Input: stdin JSON lines from t_port_sandbox_flow:
    {"func": ..., "in": {...}, "out": {"ret","exc","trace"[,"state"]}}
Summary: ORACLE sandbox_flow: N cases, M mismatches. Exit 1 on any mismatch,
and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
import __future__
import functools
import inspect
import posixpath
from dataclasses import dataclass, field
from typing import Any, Callable, IO, Optional
from urllib.parse import quote

for _stream in (sys.stdout, sys.stdin):
    try:
        if hasattr(_stream, "reconfigure"):
            _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = r"C:\Users\eman5\hermes-agent"
ANN_FLAG = __future__.annotations.compiler_flag
PROVIDER_MODULE = "sandbox_provider"   # pinned stub __module__ (collision msg)

CUR = {"func": None, "in": {}, "trace": [], "counts": {}, "created_repr": "",
       "scripted_excs": []}
NS = {}

FUNC_TARGETS = [
    "hosted_scope", "sandbox_identity", "current_sandbox_key", "sandbox_visible",
    "sandbox_lexical_path", "producer_fields", "event_visible", "require_identity",
    "admit_env", "create_hosted_environment", "accepts_scope",
    "require_hosted_interface", "_scoped_call",
]
CLASS_TARGETS = [
    "CrossAgentHandleRefused", "HostedSandboxRequired", "SandboxIdentity",
    "ProcessRef", "ProcessStream", "ScopedEnvironment", "ScopedPipe",
]
CONST_TARGETS = ["REQUIRED_OPERATIONS", "OPTIONAL_OPERATIONS", "_LIFECYCLE"]


def rec(ev):
    CUR["trace"].append(ev)


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def mk_exc(kind, msg):
    cls = NS.get(kind)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


def consume(name):
    """Script for a seam: single value reused, non-empty array consumed per
    call (last repeats). MISSING key / EMPTY array -> fail-closed raise spec."""
    io = CUR["in"].get("io") or {}
    if name not in io:
        return {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
    script = io[name]
    if isinstance(script, list):
        if not script:
            return {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        return script[min(i, len(script) - 1)]
    return script


def seam(name, args):
    sc = consume(name)
    rec({"ev": name, "args": args, "ret": sc})
    if is_raise(sc):
        e = mk_exc(sc["__raise__"], sc.get("msg", ""))
        CUR["scripted_excs"].append((e, sc))
        raise e
    return sc


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def proj_arg(v):
    """Trace args carry JSON projections: a scope double -> {tenant,agent}."""
    if isinstance(v, types.SimpleNamespace) and hasattr(v, "tenant") and hasattr(v, "agent"):
        return {"tenant": v.tenant, "agent": v.agent}
    return v


# ── AST extraction ──
def build_ns():
    mod = types.ModuleType("sandbox_scope_extracted")
    sys.modules["sandbox_scope_extracted"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "sandbox_scope_extracted", "functools": functools,
               "inspect": inspect, "os": os, "posixpath": posixpath,
               "dataclass": dataclass, "field": field, "quote": quote,
               "Any": Any, "Callable": Callable, "IO": IO, "Optional": Optional,
               "Scope": object})
    p = os.path.join(QUARRY, "tools", "environments", "sandbox_scope.py")
    src = open(p, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tg = []
            if isinstance(node, ast.Assign):
                tg = [t.id for t in node.targets if isinstance(t, ast.Name)]
            elif node.value and isinstance(node.target, ast.Name):
                tg = [node.target.id]
            if tg:
                exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
    for nm in FUNC_TARGETS + CLASS_TARGETS + CONST_TARGETS:
        if nm not in ns:
            raise RuntimeError("extraction missing %s" % nm)
    ns["current_scope"] = _current_scope_rec
    fake_module("hermes_scope", Scope=ns["Scope"], current_scope=_current_scope_rec)
    fake_module("agent.runtime_scope", bind_state=_bind_state_rec)
    NS.update(ns)
    return ns


def fake_module(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts) + 1):
        pn = ".".join(parts[:i])
        m = types.ModuleType(pn)
        m.__path__ = []
        sys.modules[pn] = m
    m = sys.modules[name]
    for k, v in attrs.items():
        setattr(m, k, v)
    if len(parts) > 1:
        setattr(sys.modules[".".join(parts[:-1])], parts[-1], m)
    return m


# ── external seam recorders ──
def _current_scope_rec():
    sc = seam("current_scope", {})
    if sc is None:
        return None
    return scope_double(sc)


class _CM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def _bind_state_rec(owner):
    seam("bind_state", {"owner": proj_scope(owner)})
    return _CM()


def _fn_rec(*a, **k):
    return seam("fn", {"args": list(a), "kwargs": dict(k)})


# ── projections ──
def proj_scope(s):
    if s is None:
        return None
    return {"tenant": s.tenant, "agent": s.agent}


def proj_identity(i):
    if i is None:
        return None
    return {"tenant": i.tenant, "agent": i.agent}


def proj_env(e, inner_repr):
    inner = {}
    for k, v in e._inner.__dict__.items():
        if hasattr(v, "__sb_desc__"):
            inner[k] = v.__sb_desc__
        else:
            inner[k] = {"callable": False, "value": v}
    sbx = e.sandbox
    own = e.owner
    return {"__scoped_env__": True, "inner": inner, "inner_repr": inner_repr,
            "sandbox": proj_identity(sbx) if isinstance(sbx, NS["SandboxIdentity"]) else sbx,
            "owner": proj_scope(own) if own is not None else None}


def proj_stream(s):
    idp = proj_identity(s.stdin._identity)
    return {"ref": {"sandbox": s.ref.sandbox, "name": s.ref.name},
            "stdin": {"__scoped_pipe__": proj_identity(s.stdin._identity)},
            "stdout": {"__scoped_pipe__": proj_identity(s.stdout._identity)},
            "poll": {"__scoped_call__": "poll", "identity": idp}}


def proj_pipe(d):
    out = {"methods": list(d.__sb_methods__)}
    if hasattr(d, "closed"):
        out["closed"] = d.closed
    return out


# ── materializers ──
def scope_double(rec):
    if not isinstance(rec, dict):
        return rec
    return types.SimpleNamespace(is_local=(rec.get("tenant") == "local"), **rec)


def identity_inst(rec):
    if isinstance(rec, dict):
        return NS["SandboxIdentity"](rec["tenant"], rec["agent"])
    return rec


def ref_mat(rec):
    if isinstance(rec, dict) and rec.get("__process_ref__"):
        return NS["ProcessRef"](rec["sandbox"], rec["name"])
    return rec


class _FS:
    def __init__(self, v):
        self.v = v

    def __fspath__(self):
        return self.v


def path_mat(rec):
    if isinstance(rec, dict) and "__fspath__" in rec:
        return _FS(rec["__fspath__"])
    return rec


def build_sig(params):
    po = [p["name"] for p in params if p["kind"] == "pos_only"]
    pk = [p["name"] for p in params if p["kind"] == "pos_or_kw"]
    vp = [p["name"] for p in params if p["kind"] == "var_pos"]
    ko = [p["name"] for p in params if p["kind"] == "kw_only"]
    vk = [p["name"] for p in params if p["kind"] == "var_kw"]
    parts = list(po)
    if po:
        parts.append("/")
    parts += pk
    fwd_a = list(po + pk)
    fwd_k = []
    if vp:
        parts.append("*" + vp[0])
        fwd_a.append("*" + vp[0])
    elif ko:
        parts.append("*")
    parts += ko
    for k in ko:
        fwd_k.append("'%s': %s" % (k, k))
    if vk:
        parts.append("**" + vk[0])
        fwd_k.append("**" + vk[0])
    return ", ".join(parts), "[" + ", ".join(fwd_a) + "]", "{" + ", ".join(fwd_k) + "}"


def synth_method(name, desc):
    params = desc.get("params") or []
    sig, fa, fk = build_sig(params)
    evname = "inner." + name

    def __rec__(*a, **k):
        return seam(evname, {"args": [proj_arg(x) for x in a],
                             "kwargs": {kk: proj_arg(vv) for kk, vv in k.items()}})

    g = {"__rec__": __rec__}
    exec("def m(%s):\n    return __rec__(*(%s), **(%s))\n" % (sig, fa, fk), g)
    f = g["m"]
    f.__name__ = name
    f.__sb_desc__ = desc
    return f


def _proc_stub(op):
    if op == "start_process":
        def start_process(name, argv, *, child_env=None, scope=None):
            sc = seam("inner.start_process",
                      drop_none({"name": name, "argv": argv, "child_env": child_env,
                                 "scope": proj_scope(scope)}))
            return stream_double(sc) if isinstance(sc, dict) else sc
        return start_process
    if op == "attach_process":
        def attach_process(name, *, scope=None):
            sc = seam("inner.attach_process", {"name": name, "scope": proj_scope(scope)})
            return stream_double(sc) if isinstance(sc, dict) else sc

        return attach_process

    def stop_process(name, *, scope=None):
        return seam("inner.stop_process", {"name": name, "scope": proj_scope(scope)})
    return stop_process


def _pipe_stub(op):
    if op in ("read", "read1", "readline"):
        def f(size=-1):
            return seam("pipe." + op, {"size": size})
    elif op == "readlines":
        def f(hint=-1):
            return seam("pipe.readlines", {"hint": hint})
    elif op == "readinto":
        def f(buffer):
            return seam("pipe.readinto", {"buffer": buffer})
    elif op == "write":
        def f(data):
            return seam("pipe.write", {"data": data})
    elif op == "writelines":
        def f(lines):
            return seam("pipe.writelines", {"lines": lines})
    else:
        def f():
            return seam("pipe." + op, {})
    f.__name__ = op
    return f


def pipe_double(d):
    if not isinstance(d, dict):
        return d
    methods = list(d.get("methods") or [])
    attrs = {op: _pipe_stub(op) for op in methods}
    if "closed" in d:
        attrs["closed"] = d["closed"]
    pd = types.SimpleNamespace(**attrs)
    pd.__sb_methods__ = methods
    return pd


def stream_double(d):
    if not isinstance(d, dict):
        return d
    attrs = {}
    if "stdin" in d:
        attrs["stdin"] = pipe_double(d["stdin"])
    if "stdout" in d:
        attrs["stdout"] = pipe_double(d["stdout"])
    if "poll" in d:
        attrs["poll"] = _fn_rec
    return types.SimpleNamespace(**attrs)


def inner_double(map_, mode, inner_repr=""):
    if not isinstance(map_, dict):
        return map_
    if mode in ("repr", "create"):
        class RD(types.SimpleNamespace):
            def __repr__(self):
                return inner_repr
        base = RD
    else:
        base = types.SimpleNamespace
    attrs = {}
    for name, desc in map_.items():
        if isinstance(desc, dict) and desc.get("callable"):
            if mode == "call" and name in ("start_process", "attach_process", "stop_process"):
                stub = _proc_stub(name)
                stub.__sb_desc__ = desc   # echo projects the original descriptor
                attrs[name] = stub
            else:
                attrs[name] = synth_method(name, desc)
        elif isinstance(desc, dict) and "value" in desc:
            attrs[name] = desc["value"]
        else:
            attrs[name] = desc
    return base(**attrs)


def provider_double(rec):
    if not isinstance(rec, dict):
        return rec

    def create_environment(**kw):
        args = {"scope": proj_scope(kw.get("scope")), "sandbox": proj_identity(kw.get("sandbox"))}
        for k, v in kw.items():
            if k not in ("scope", "sandbox"):
                args[k] = v
        sc = seam("provider.create_environment", args)
        if sc is None:
            return None
        CUR["created_repr"] = sc.get("inner_repr", "") if isinstance(sc, dict) else ""
        inner = sc.get("inner") if isinstance(sc, dict) else None
        return inner_double(inner or {}, "create", CUR["created_repr"])

    create_environment.__module__ = PROVIDER_MODULE
    create_environment.__qualname__ = "create_environment"
    attrs = {"create_environment": create_environment}
    if "serves_hosted_scopes" in rec:
        attrs["serves_hosted_scopes"] = rec["serves_hosted_scopes"]
    return types.SimpleNamespace(**attrs)


def method_double(rec, name="m"):
    if isinstance(rec, dict):
        if rec.get("signature_error"):
            return object()
        return synth_method(name, rec)
    return rec


def env_inst(rec, mode="call"):
    cls = NS["ScopedEnvironment"]
    e = cls.__new__(cls)
    inner = inner_double(rec.get("inner") or {}, mode, rec.get("inner_repr", ""))
    object.__setattr__(e, "_inner", inner)
    sbx = rec.get("sandbox")
    object.__setattr__(e, "sandbox", identity_inst(sbx))
    own = rec.get("owner")
    object.__setattr__(e, "owner", scope_double(own) if isinstance(own, dict) else own)
    return e


def pipe_inst(rec):
    return NS["ScopedPipe"](pipe_double(rec.get("pipe") or {}), identity_inst(rec.get("identity")))


def env_ret(e, inner_repr):
    if isinstance(e, NS["ScopedEnvironment"]):
        return proj_env(e, inner_repr)
    return e


def stream_ret(s):
    if isinstance(s, NS["ProcessStream"]):
        return proj_stream(s)
    return s


def wrapper_ret(value, name, identity):
    if callable(value):
        kind = "lifecycle" if name in NS["_LIFECYCLE"] else "guarded"
        return {"__wrapped__": kind, "name": name, "identity": proj_identity(identity)}
    return value


# ── input normalization: integral floats read as ints (libjson parity) ──
def norm_num(x):
    if isinstance(x, float) and x == int(x):
        return int(x)
    if isinstance(x, dict):
        return {k: norm_num(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_num(v) for v in x]
    return x


# ── drive the LIVE target for one case ──
def drive(func, inp):
    g = inp.get
    if func == "hosted_scope":
        return {"ret": proj_scope(NS["hosted_scope"]())}
    if func == "sandbox_identity":
        return {"ret": proj_identity(NS["sandbox_identity"](scope_double(g("scope"))))}
    if func == "identity_key":
        return {"ret": NS["SandboxIdentity"](g("tenant", ""), g("agent", "")).key}
    if func == "current_sandbox_key":
        return {"ret": NS["current_sandbox_key"]()}
    if func == "sandbox_visible":
        return {"ret": NS["sandbox_visible"](g("sandbox_key"))}
    if func == "sandbox_lexical_path":
        return {"ret": NS["sandbox_lexical_path"](path_mat(g("path")))}
    if func == "producer_fields":
        return {"ret": NS["producer_fields"](g("sandbox_key"))}
    if func == "event_visible":
        return {"ret": NS["event_visible"](g("event"))}
    if func == "require_identity":
        return {"ret": proj_scope(NS["require_identity"](identity_inst(g("identity")), g("what", "")))}
    if func == "admit_env":
        env = g("env")
        mat = env_inst(env) if isinstance(env, dict) and env.get("__scoped_env__") else env
        return {"ret": env_ret(NS["admit_env"](mat), (env or {}).get("inner_repr", "") if isinstance(env, dict) else "")}
    if func == "create_hosted_environment":
        CUR["created_repr"] = ""
        e = NS["create_hosted_environment"](g("env_type", ""), provider_double(g("provider")),
                                            **(g("kwargs") or {}))
        return {"ret": proj_env(e, CUR["created_repr"])}
    if func == "accepts_scope":
        return {"ret": NS["accepts_scope"](method_double(g("method")))}
    if func == "require_hosted_interface":
        inner = g("inner")
        mat = inner_double(inner, "check") if isinstance(inner, dict) else inner
        return {"ret": NS["require_hosted_interface"](mat, g("env_type", ""))}
    if func == "scoped_call":
        w = NS["_scoped_call"](_fn_rec, identity_inst(g("identity")), g("what", ""))
        return {"ret": w(*(g("args") or []), **(g("kwargs") or {}))}
    if func == "env_init":
        e = NS["ScopedEnvironment"](inner_double(g("inner") or {}, "call", g("inner_repr", "")),
                                    identity_inst(g("identity")), owner=scope_double(g("owner")))
        return {"ret": proj_env(e, g("inner_repr", ""))}
    if func == "env_getattr":
        e = env_inst(g("env") or {})
        return {"ret": wrapper_ret(getattr(e, g("name", "")), g("name", ""), e.sandbox)}
    if func == "env_setattr":
        e = env_inst(g("env") or {})
        setattr(e, g("name", ""), g("value"))
        return {"ret": None, "state": proj_env(e, (g("env") or {}).get("inner_repr", ""))}
    if func == "env_guarded":
        e = env_inst(g("env") or {})
        w = e._guarded(g("name", ""), method_double(g("method"), g("name", "")))
        return {"ret": w(*(g("args") or []), **(g("kwargs") or {}))}
    if func == "env_lifecycle":
        e = env_inst(g("env") or {})
        w = e._lifecycle(g("name", ""), method_double(g("method"), g("name", "")))
        return {"ret": w(*(g("args") or []), **(g("kwargs") or {}))}
    if func == "env_start_process":
        e = env_inst(g("env") or {})
        return {"ret": stream_ret(e.start_process(g("name", ""), g("argv") or [], child_env=g("child_env")))}
    if func == "env_attach_process":
        e = env_inst(g("env") or {})
        return {"ret": stream_ret(e.attach_process(ref_mat(g("ref"))))}
    if func == "env_stop_process":
        e = env_inst(g("env") or {})
        return {"ret": e.stop_process(ref_mat(g("ref")))}
    if func == "env_guard_stream":
        e = env_inst(g("env") or {})
        return {"ret": stream_ret(e._guard_stream(stream_double(g("stream")), g("name")))}
    if func == "env_own_name":
        e = env_inst(g("env") or {})
        return {"ret": e._own_name(ref_mat(g("ref")), g("what", ""))}
    if func == "env_repr":
        e = env_inst(g("env") or {}, mode="repr")
        return {"ret": repr(e)}
    if func == "pipe_op":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": proj_pipe(p._op(g("what", "")))}
    if func == "pipe_read":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.read(g("size", -1))}
    if func == "pipe_read1":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.read1(g("size", -1))}
    if func == "pipe_readline":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.readline(g("size", -1))}
    if func == "pipe_readlines":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.readlines(g("hint", -1))}
    if func == "pipe_readinto":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.readinto(g("buffer"))}
    if func == "pipe_write":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.write(g("data"))}
    if func == "pipe_writelines":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.writelines(g("lines"))}
    if func == "pipe_flush":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.flush()}
    if func == "pipe_close":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.close()}
    if func == "pipe_closed":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.closed}
    if func == "pipe_readable":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.readable()}
    if func == "pipe_writable":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.writable()}
    if func == "pipe_setattr":
        p = pipe_inst(g("pipe_state") or {})
        setattr(p, g("name", ""), g("value"))
        return {"ret": None}
    if func == "pipe_iter":
        p = pipe_inst(g("pipe_state") or {})
        it = iter(p)
        return {"ret": {"__scoped_pipe__": proj_identity(it._identity)}}
    if func == "pipe_next":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": next(p)}
    if func == "pipe_enter":
        p = pipe_inst(g("pipe_state") or {})
        en = p.__enter__()
        return {"ret": {"__scoped_pipe__": proj_identity(en._identity)}}
    if func == "pipe_exit":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": p.__exit__(None, None, None)}
    if func == "pipe_repr":
        p = pipe_inst(g("pipe_state") or {})
        return {"ret": repr(p)}
    raise ValueError("unknown func: %s" % func)


def run_case(func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    CUR["created_repr"] = ""
    CUR["scripted_excs"] = []
    try:
        out = drive(func, inp)
        out["exc"] = None
    except BaseException as e:
        spec = None
        for ex, sc in CUR["scripted_excs"]:
            if e is ex:
                spec = sc   # scripted raise round-trips VERBATIM (KeyError repr etc.)
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "msg": str(e)}
        out = {"ret": None, "exc": spec}
        if func == "env_setattr":
            out["state"] = None
    out["trace"] = CUR["trace"]
    return out


# ── comparison ──
def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def compare(want, got):
    if canon(want.get("trace", [])) != canon(got.get("trace", [])):
        return False
    for k in set(want) | set(got):
        if k == "trace":
            continue
        if canon(want.get(k)) != canon(got.get(k)):
            return False
    return True


def main():
    build_ns()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record_in = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record_in.get("func")
        inp = norm_num(record_in.get("in") or {})
        got = record_in.get("out") or {}
        cases += 1
        try:
            want = run_case(func, inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if not compare(want, got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE sandbox_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
