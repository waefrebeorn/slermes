#!/usr/bin/env python3
"""Oracle for the gateway/session_context.py FLOW remainder port
(src/gateway/session_context_flow2.c).

Drives the LIVE gateway/session_context.py unclaimed leaves with SCRIPTED
seams: the in-function agent.runtime_cwd import + getattr call, the turn's
running_follow_up context manager (enter/body/exit) and the scoped-CM body
are recording stubs fed the same scripts the C harness feeds its universal
seam. Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"}
in "trace") plus the final return ("ret") and the ContextVar state echo
("state").

The live defs are AST-extracted from gateway/session_context.py (module
constants seeded twice, Assign AND AnnAssign, plain-Name or tuple-of-Names
targets only — the _SESSION_VARS tuple-unpack needs the latter; real
contextvars.ContextVar / contextlib.contextmanager so the @contextmanager
defs become genuine CM factories). The exec namespace is a registered
sys.modules module. The heavy quarry import chain is never imported; the
in-function `from agent import runtime_cwd` resolves against a stub package.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE session_context_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- _UNSET projects to {"__unset__": true}; a missing state key reads as _UNSET
  (turn_event defaults to None). Only fixture-present state keys are echoed.
- Event/turn/source dicts materialize to SimpleNamespace recursively (getattr
  reads); denorm back for trace args / state echo.
- A dict running_follow_up result is a CM carrier (enter/exit fire seams);
  any other result passes through to the live `with` protocol (py3.14
  TypeError). An __exit__ raise replaces the pending body raise; a truthy
  result suppresses it; a failed cm expression / __enter__ skips body+exit;
  the finally token reset always runs.
- Scripted seams: plain value reused, bare array per-call queue (no repeat),
  {"__script__": true, "ret"/"raise"}; a bare uniform {"__raise__","message"}
  dict also raises. Missing/exhausted script => fail-closed RuntimeError;
  absent io => "no seam channel installed for <name>".
- None-valued arg keys dropped on both sides. ASCII corpus; str() domains
  str/int/bool/null; metadata dict/null; raise kinds plain-message classes
  (KeyError excluded). The FULL builtin exception hierarchy decides the
  `except Exception` swallow in _runtime_cwd (BaseException line passes).
"""
import ast
import asyncio
import contextlib
import json
import os
import re
import sys
import textwrap
import types
from contextvars import ContextVar
from pathlib import Path
from typing import Any, Iterator
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "gateway", "session_context.py")
_FLAGS = _fut.annotations.compiler_flag

PORTED = ["_runtime_cwd", "scoped_turn_actor", "scoped_session_key",
          "bound_session_key", "source_route_metadata", "_bound_session_value",
          "session_history_delivery_supported"]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(textwrap.dedent(seg(src, node)), path, "exec",
                 flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def _name_target(t):
    # plain Name, or a tuple-unpack of Names (_SESSION_VARS = (a, b, ...) = ...)
    if isinstance(t, ast.Name):
        return True
    if isinstance(t, (ast.Tuple, ast.List)):
        return all(_name_target(e) for e in t.elts)
    return False


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(_name_target(t) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


def extract_ns():
    mod = new_module("scx2_oracle_sess")
    ns = mod.__dict__
    ns.update({"os": os, "contextmanager": contextlib.contextmanager,
               "ContextVar": ContextVar, "Any": Any, "Iterator": Iterator,
               "SimpleNamespace": types.SimpleNamespace})
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, SRC)
    seed_constants(src, ns, SRC)
    required = PORTED + ["_UNSET", "_VAR_MAP", "_SESSION_VARS",
                         "_SESSION_TURN_EVENT", "_SESSION_ASYNC_DELIVERY",
                         "_SESSION_HISTORY_DELIVERY", "_ACTOR_VAR_NAMES"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_context_flow2: missing {missing}")
        sys.exit(2)
    return ns


NS = extract_ns()
UNSET = NS["_UNSET"]
VAR_NAMES = list(NS["_VAR_MAP"])


# ── canonical projection ──
def canon(v):
    def conv(x):
        if x is UNSET:
            return {"__unset__": True}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted((conv(i) for i in x), key=lambda z: json.dumps(z, sort_keys=True))
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def denorm(x):
    if isinstance(x, types.SimpleNamespace):
        return {k: denorm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(i) for i in x]
    return x


def mat(x):
    if isinstance(x, dict):
        return types.SimpleNamespace(**{k: mat(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mat(i) for i in x]
    return x


# ── exception materialization / projection ──
# FULL builtin hierarchy (py3.14); ExceptionGroup walks via Exception.
import builtins as _bi
_BUILTIN_EXC = {}
for _n in dir(_bi):
    _o = getattr(_bi, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        _BUILTIN_EXC[_n] = _o
_BUILTIN_EXC["CancelledError"] = asyncio.CancelledError
# Classes whose __init__/__str__ do not accept/render a lone message: build a
# same-named subclass keeping the REAL lineage, with args-based __str__ and a
# safe construction (object.__new__ is unsafe on py3.14; the *Group __new__
# needs (msg, excs)).
_BYPASS_INIT = {"UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError",
                "ExceptionGroup", "BaseExceptionGroup"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    real = _BUILTIN_EXC.get(kind)
    if real is None:
        return type(str(kind), (Exception,), {})(msg)
    if kind in _BYPASS_INIT:
        cls = type(str(kind), (real,),
                   {"__str__": lambda self: Exception.__str__(self)})
        if kind in ("ExceptionGroup", "BaseExceptionGroup"):
            e = real.__new__(cls, msg, [ValueError(msg)])
        else:
            e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return real(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C scx2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io            # None == no seam channel installed
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in args.items() if v is not None}
        if self.io is None:
            self._record_raise(name, args,
                               RuntimeError(f"no seam channel installed for {name}"))
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args,
                                   RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        if isinstance(ret, dict) and "__raise__" in ret:
            raise make_exc(ret)
        return ret


# ── turn / CM doubles ──
class CMDouble:
    def __init__(self, raw, ctx):
        self.raw = raw
        self.ctx = ctx

    def __enter__(self):
        return self.ctx.call("turn.running_follow_up.__enter__", {"cm": self.raw})

    def __exit__(self, et, ev, tb):
        exc = proj_exc(ev) if ev is not None else None
        r = self.ctx.call("turn.running_follow_up.__exit__",
                          {"cm": self.raw, "exc": exc})
        return bool(r)


def make_turn(t, ctx):
    if isinstance(t, dict):
        ns = types.SimpleNamespace()

        def running_follow_up(event):
            raw = ctx.call("turn.running_follow_up", {"event": denorm(event)})
            if isinstance(raw, dict):
                return CMDouble(raw, ctx)
            return raw

        ns.running_follow_up = running_follow_up
        return ns
    return t


# ── agent.runtime_cwd stub package (in-function import target) ──
_AGENT_STUB = types.ModuleType("agent")
_AGENT_STUB.__path__ = []
sys.modules["agent"] = _AGENT_STUB


def install_runtime_cwd(inp, ctx):
    if inp.get("import_fail"):
        sys.modules["agent.runtime_cwd"] = None
        _AGENT_STUB.runtime_cwd = None
        return
    m = types.ModuleType("agent.runtime_cwd")
    funcs = inp.get("runtime_cwd_attrs")
    if funcs is None:
        funcs = [(inp.get("args") or {}).get("func", "")]
    for nm in funcs:
        def rec(*a, _nm=nm):
            return ctx.call("runtime_cwd." + _nm, {"args": list(a)})
        setattr(m, nm, rec)
    sys.modules["agent.runtime_cwd"] = m
    _AGENT_STUB.runtime_cwd = m


# ── ContextVar state injection / echo (fixture-present keys only) ──
def _is_marker(v):
    return isinstance(v, dict) and v.get("__unset__") is True


def inject_state(state):
    vars_d = state.get("vars") or {}
    for name in VAR_NAMES:
        v = vars_d.get(name, _MISSING)
        NS["_VAR_MAP"][name].set(UNSET if v is _MISSING or _is_marker(v) else v)
    for key, var in (("async_delivery", NS["_SESSION_ASYNC_DELIVERY"]),
                     ("history_delivery", NS["_SESSION_HISTORY_DELIVERY"])):
        v = state.get(key, _MISSING)
        var.set(UNSET if v is _MISSING or _is_marker(v) else v)
    te = state.get("turn_event", _MISSING)
    NS["_SESSION_TURN_EVENT"].set(None if te is _MISSING else mat(te))
    NS["_session_context_engaged"] = state.get("engaged", False)


def _proj_var(v):
    return {"__unset__": True} if v is UNSET else v


def echo_state(state):
    out = {}
    if "vars" in state:
        # Unknown names are stray state keys: C echoes them verbatim and
        # ignores them on reads (_VAR_MAP.get -> None); echo the fixture value.
        out["vars"] = {name: (_proj_var(NS["_VAR_MAP"][name].get())
                              if name in NS["_VAR_MAP"] else val)
                       for name, val in (state["vars"] or {}).items()}
    if "turn_event" in state:
        out["turn_event"] = denorm(NS["_SESSION_TURN_EVENT"].get())
    if "history_delivery" in state:
        out["history_delivery"] = _proj_var(NS["_SESSION_HISTORY_DELIVERY"].get())
    if "async_delivery" in state:
        out["async_delivery"] = _proj_var(NS["_SESSION_ASYNC_DELIVERY"].get())
    if "engaged" in state:
        out["engaged"] = NS["_session_context_engaged"]
    return out


def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    inject_state(state)
    install_runtime_cwd(inp, ctx)
    try:
        if func == "_runtime_cwd":
            ret = NS["_runtime_cwd"](args.get("func", ""), *(args.get("args") or []))
        elif func == "scoped_turn_actor":
            event = mat(args.get("event"))
            turn = make_turn(args.get("turn"), ctx)
            cm = NS["scoped_turn_actor"](event, turn)
            with cm:
                ctx.call("body", {})
            ret = None
        elif func == "scoped_session_key":
            cm = NS["scoped_session_key"](args.get("session_key"))
            with cm:
                ctx.call("body", {})
            ret = None
        elif func == "bound_session_key":
            ret = NS["bound_session_key"]()
        elif func == "source_route_metadata":
            ret = NS["source_route_metadata"](mat(args.get("source")), args.get("metadata"))
        elif func == "_bound_session_value":
            ret = NS["_bound_session_value"](args.get("name", ""))
        elif func == "session_history_delivery_supported":
            ret = NS["session_history_delivery_supported"]()
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo_state(state))}


# ── three-way seam-name cross-check ──
SEAM_NAMES = ["body", "turn.running_follow_up",
              "turn.running_follow_up.__enter__", "turn.running_follow_up.__exit__"]
SEAM_PREFIX = "runtime_cwd."


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_context_flow2.c",
                encoding="utf-8").read()
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if '"runtime_cwd.%s"' not in csrc:
        missing_c.append("runtime_cwd.<func>")
    if missing_c:
        print(f"FATAL oracle_session_context_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'"(turn\.running_follow_up[^"]*)"', osrc))
    used = {u for u in used if not u.startswith("runtime_cwd.")}
    unknown = used - set(SEAM_NAMES)
    if unknown:
        print(f"FATAL oracle_session_context_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_context_flow2.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = {k for k in fix_keys
               if k not in SEAM_NAMES and not k.startswith(SEAM_PREFIX)}
        if bad:
            print(f"FATAL oracle_session_context_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_session_context_flow2: fixture funcs not ported: {sorted(badf)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE session_context_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()
