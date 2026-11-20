#!/usr/bin/env python3
"""Oracle for the gateway/session_finite.py FLOW remainder port
(src/gateway/session_finite_flow2.c).

Drives the LIVE gateway/session_finite.py UNCLAIMED defs (6 — the module's full
unclaimed ceiling; finite_turn_required is claimed by tool_dispatch_flow2 and is
NOT re-ported) under IDENTICAL scripted universal seams. The foreign collaborator
leaves (policy_for_source, execute_admission, and the four turn-scope CM
factories) are recording stubs fed the same scripts the C harness feeds its
universal seam; ported siblings compose for REAL (_may_run_unattended inside
admit_finite, finite_turn_scope inside execute_finite_admission). Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus the
final return ("ret") and the ContextVar state echo ("state").

The live defs are AST-extracted from gateway/session_finite.py (module constants
seeded — the two ContextVars and FINITE_CLARIFY_NOTICE; real contextvars.ContextVar
/ contextlib.contextmanager so finite_turn_scope is a genuine CM factory). The
real AuthorityRefusal (PersistenceError, ValueError) is AST-extracted from
persistence/core/errors.py (_one_line + the two classes). The exec namespace is a
registered sys.modules module; the heavy quarry import chain is never imported —
the in-function `from gateway.* import ...` resolves against stub packages
(halted via sys.modules[full]=None AND parent.attr=None for the import_fail gate).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). NONE of the six defs has an `except` clause,
so every raise — including the BaseException line — propagates unchanged; scripted
raises materialize to the REAL builtin classes (full hierarchy via a builtins
scan) plus the real AuthorityRefusal. Summary:
'ORACLE session_finite_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- ContextVar state: finite_turn (default None) / unattended_turn (default False);
  only fixture-present keys echo; set+reset restores presence (final == initial).
- connection/ref/source/actor/entry/policy dicts materialize to SimpleNamespace
  with their domain keys injected (absent == None); C reads absent->null, so a
  non-object carrier raises the deterministic '<type>' AttributeError.
- A dict CM-factory result is a CM carrier (enter/exit fire seams); any other
  result passes through to the live `with` protocol (py3.14 TypeError). An
  __exit__ raise replaces the pending body raise; a truthy result suppresses it
  (outer CMs then see exc=None); a failed cm expression / __enter__ skips that
  CM's body+exit; the finally token reset always runs. `with A, B, ...` is nested:
  enter in order, exit in REVERSE, each CM's args evaluated just before its
  factory call (a KeyError there exits the already-entered CMs).
- Scripted seams: plain value reused, bare array per-call queue (no repeat),
  {"__script__": true, "ret"/"raise"}; a bare uniform {"__raise__","message"}
  dict also raises. Missing/exhausted script => fail-closed RuntimeError; absent
  io => "no seam channel installed for <name>".
- None-valued TOP-LEVEL arg keys dropped on both sides; nested nulls kept.
- ASCII corpus; params dict-or-null; submitter null/list/str/dict (iterable
  splat); user_id/subject str-or-null; session_id str/int/bool/null (KeyError
  str()==repr(key)); row dict-or-null; payload dict/null/str/int/bool/list (float
  excluded). Integral floats normalize to ints on both sides. Scripted raise
  kinds are plain-message classes (KeyError excluded from scripted raises).
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
import __future__ as _fut
from contextvars import ContextVar
from pathlib import Path
from types import SimpleNamespace

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "gateway" / "session_finite.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

SRC = str(QUARRY / "gateway" / "session_finite.py")
ERR_SRC = str(QUARRY / "persistence" / "core" / "errors.py")
_FLAGS = _fut.annotations.compiler_flag

PORTED = ["finite_clarify_reply", "unattended_turn", "finite_turn_scope",
          "admit_finite", "_may_run_unattended", "execute_finite_admission"]


# ── AST extraction ──
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
    mod = new_module("sf2_oracle_sess")
    ns = mod.__dict__
    ns.update({"__name__": "sf2_oracle_sess", "contextmanager": contextlib.contextmanager,
               "ContextVar": ContextVar, "json": json, "os": os})
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in PORTED:
            exec_node(src, node, ns, SRC)
    seed_constants(src, ns, SRC)
    # real AuthorityRefusal lineage from persistence/core/errors.py
    esrc = open(ERR_SRC, encoding="utf-8").read()
    for node in ast.parse(esrc).body:
        if isinstance(node, ast.FunctionDef) and node.name == "_one_line":
            exec_node(esrc, node, ns, ERR_SRC)
        if isinstance(node, ast.ClassDef) and node.name in ("PersistenceError", "AuthorityRefusal"):
            exec_node(esrc, node, ns, ERR_SRC)
    required = PORTED + ["_finite_turn", "_unattended_turn", "FINITE_CLARIFY_NOTICE",
                         "AuthorityRefusal"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_session_finite_flow2: missing {missing}")
        sys.exit(2)
    return ns


NS = extract_ns()


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def denorm(x):
    if isinstance(x, SimpleNamespace):
        return {k: denorm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(i) for i in x]
    return x


# ── attribute-double materializers (domain keys injected; absent == None) ──
def mat_source(s):
    if isinstance(s, dict):
        return SimpleNamespace(user_id=s.get("user_id"))
    return s


def mat_entry(e):
    if isinstance(e, dict):
        return SimpleNamespace(source=mat_source(e.get("source")))
    return e


def mat_actor(a):
    if isinstance(a, dict):
        return SimpleNamespace(subject=a.get("subject"))
    return a


def mat_authority(au):
    if isinstance(au, dict):
        sess = au.get("sessions")
        sess_d = {k: mat_entry(v) for k, v in sess.items()} if isinstance(sess, dict) else sess
        return SimpleNamespace(sessions=sess_d, runner=au.get("runner"))
    return au


def mat_conn(c):
    if isinstance(c, dict):
        return SimpleNamespace(native_owner=c.get("native_owner"),
                               authority=mat_authority(c.get("authority")),
                               actor=mat_actor(c.get("actor")))
    return c


def mat_ref(r):
    if isinstance(r, dict):
        return SimpleNamespace(session_id=r.get("session_id"))
    return r


# ── exception materialization / projection ──
import builtins as _bi
_BUILTIN_EXC = {}
for _n in dir(_bi):
    _o = getattr(_bi, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        _BUILTIN_EXC[_n] = _o
_BUILTIN_EXC["CancelledError"] = asyncio.CancelledError
_BYPASS_INIT = {"UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError",
                "ExceptionGroup", "BaseExceptionGroup"}


def make_exc(spec):
    kind = spec.get("type") or spec.get("__raise__") or "RuntimeError"
    msg = spec.get("message", "")
    if kind == "AuthorityRefusal":
        return NS["AuthorityRefusal"](msg)
    real = _BUILTIN_EXC.get(kind)
    if real is None:
        return type(str(kind), (Exception,), {})(msg)
    if kind in _BYPASS_INIT:
        cls = type(str(kind), (real,), {"__str__": lambda self: Exception.__str__(self)})
        if kind in ("ExceptionGroup", "BaseExceptionGroup"):
            e = real.__new__(cls, msg, [ValueError(msg)])
        else:
            e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return real(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C sf2_call exactly) ──
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
            self._record_raise(name, args, RuntimeError(f"no seam channel installed for {name}"))
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        used = entry
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
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


# ── foreign CM double ──
class CMDouble:
    def __init__(self, seam, raw, ctx):
        self.seam = seam
        self.raw = raw
        self.ctx = ctx

    def __enter__(self):
        return self.ctx.call(self.seam + ".__enter__", {"cm": self.raw})

    def __exit__(self, et, ev, tb):
        exc = proj_exc(ev) if ev is not None else None
        r = self.ctx.call(self.seam + ".__exit__", {"cm": self.raw, "exc": exc})
        return bool(r)


# ── in-function import stubs (gateway.* submodules) ──
def install_imports(inp, ctx):
    halted = set(inp.get("import_fail") or [])
    gw = types.ModuleType("gateway")
    gw.__path__ = []
    sys.modules["gateway"] = gw

    def sub(full, attrs):
        short = full.split(".")[-1]
        if full in halted:
            sys.modules[full] = None
            setattr(gw, short, None)
            return
        m = types.ModuleType(full)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[full] = m
        setattr(gw, short, m)

    def cm_factory(seam, argnames):
        def f(*a):
            args = {argnames[i]: a[i] for i in range(len(a))}
            raw = ctx.call(seam, args)
            if isinstance(raw, dict):
                return CMDouble(seam, raw, ctx)
            return raw
        return f

    async def exec_admission(authority, ref, row):
        return ctx.call("execute_admission", {"authority": authority, "ref": ref, "row": row})

    def policy_for_source(runner, source):
        raw = ctx.call("policy_for_source", {"runner": denorm(runner), "source": denorm(source)})
        if isinstance(raw, dict):
            return SimpleNamespace(platform=raw.get("platform"))
        return raw

    sub("gateway.session_policy", {"policy_for_source": policy_for_source})
    sub("gateway.session_ingress", {"execute_admission": exec_admission})
    sub("gateway.session_display", {
        "display_turn_scope": cm_factory("display_turn_scope", ["admission_id", "display"]),
        "submission_turn_scope": cm_factory("submission_turn_scope", ["admission_id", "request_id"])})
    sub("gateway.session_surface", {"surface_turn_scope": cm_factory("surface_turn_scope", ["surface"])})
    sub("gateway.session_cli_turn", {"cli_turn_scope": cm_factory("cli_turn_scope", ["admission_id", "cli_turn"])})


# ── ContextVar state injection / echo (fixture-present keys only) ──
def inject_state(state):
    ft = state.get("finite_turn", _MISSING)
    NS["_finite_turn"].set(None if ft is _MISSING else ft)
    ut = state.get("unattended_turn", _MISSING)
    NS["_unattended_turn"].set(False if ut is _MISSING else ut)


def echo_state(state):
    out = {}
    if "finite_turn" in state:
        out["finite_turn"] = NS["_finite_turn"].get()
    if "unattended_turn" in state:
        out["unattended_turn"] = NS["_unattended_turn"].get()
    return out


def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    inject_state(state)
    install_imports(inp, ctx)
    try:
        if func == "finite_clarify_reply":
            ret = NS["finite_clarify_reply"]()
        elif func == "unattended_turn":
            ret = NS["unattended_turn"]()
        elif func == "finite_turn_scope":
            finite = args.get("finite")
            unattended = args.get("unattended", False)
            with NS["finite_turn_scope"](finite, unattended):
                ctx.call("body", {})
            ret = None
        elif func == "admit_finite":
            params = args.get("params")
            submitter = args.get("submitter")
            if isinstance(submitter, list) and len(submitter) == 2:
                submitter = [mat_conn(submitter[0]), mat_ref(submitter[1])]
            ret = NS["admit_finite"](params, submitter=submitter)
        elif func == "_may_run_unattended":
            conn = mat_conn(args.get("connection"))
            ref = mat_ref(args.get("ref"))
            ret = NS["_may_run_unattended"](conn, ref)
        elif func == "execute_finite_admission":
            ret = asyncio.run(NS["execute_finite_admission"](
                args.get("authority"), args.get("ref"), args.get("row")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo_state(state))}


# ── three-way seam-name cross-check ──
SEAM_NAMES = ["body", "policy_for_source", "surface_turn_scope", "display_turn_scope",
              "submission_turn_scope", "cli_turn_scope", "execute_admission"]
CM_FACTORIES = ["surface_turn_scope", "display_turn_scope", "submission_turn_scope", "cli_turn_scope"]
REGISTRY = set(SEAM_NAMES) | {f + ".__enter__" for f in CM_FACTORIES} | {f + ".__exit__" for f in CM_FACTORIES}


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_finite_flow2.c", encoding="utf-8").read()
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if '"%s.__enter__"' not in csrc:
        missing_c.append("<factory>.__enter__")
    if '"%s.__exit__"' not in csrc:
        missing_c.append("<factory>.__exit__")
    if missing_c:
        print(f"FATAL oracle_session_finite_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_finite_flow2.jsonl"
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
        bad = fix_keys - REGISTRY
        if bad:
            print(f"FATAL oracle_session_finite_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_session_finite_flow2: fixture funcs not ported: {sorted(badf)}")
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
    print(f"ORACLE session_finite_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()
