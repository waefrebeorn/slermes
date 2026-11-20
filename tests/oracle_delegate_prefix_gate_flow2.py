#!/usr/bin/env python3
"""Oracle for src/tools/delegate_prefix_gate_flow2.c — drives the LIVE
tools/delegate_tool_prefix_gate.py gate surface under IDENTICAL scripted
universal seams.

All 9 defs of the module are covered: _route_key, _caches_prompts,
_first_response_started, _stop_requested, PrefixCacheGate.__init__,
.for_children, .follows, .run, ._wait. tool_dispatch_flow2 holds PoP claims for
6 of these (it models them as value calls inside the dispatch surface); the
3 unclaimed ones are __init__ / follows / _wait. This dedicated oracle checks
the whole gate lifecycle as one standalone differential unit.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec) and the state window. Live defs are AST-extracted from the
quarry; time.monotonic, event.wait and the child's run() callable are recording
seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past any except site exactly as live (real builtin classes); the
BaseException catch maps a scripted raise back to its BARE spec by identity.

Value model: attribute doubles {"__attr__":...,"__id__":...} <-> SimpleNamespace
(interned by __id__ so `is` agrees with the C __id__ model); a gate is
{"leader","max_wait_s","leader_route","leader_done"}; leader_done is a
GateEvent (is_set/set are silent state flags, wait rides the event.wait seam);
a callable projects to {"__callable__":seam}; integral floats normalize to ints.

Summary: 'ORACLE delegate_prefix_gate_flow2: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import sys
import types
import textwrap
import __future__
from types import SimpleNamespace
from typing import Any, Callable, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

from pathlib import Path
_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "tools" / "delegate_tool_prefix_gate.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "_route_key", "_caches_prompts", "_first_response_started", "_stop_requested",
    "PrefixCacheGate.__init__", "PrefixCacheGate.for_children",
    "PrefixCacheGate.follows", "PrefixCacheGate.run", "PrefixCacheGate._wait",
]

CURRENT = {"ctx": None, "rec": None, "ids": {}, "stwin": {}}
NS = {}
PrefixCacheGate = None


def build_exc(spec):
    """Materialize a REAL exception instance from a raise spec. Full builtin
    hierarchy (py3.14): the BaseException trio + CancelledError stay outside
    Exception; every other builtin maps to its real class."""
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {
        "BaseException": BaseException, "Exception": Exception,
        "ArithmeticError": ArithmeticError, "AssertionError": AssertionError,
        "AttributeError": AttributeError, "BufferError": BufferError,
        "EOFError": EOFError, "FloatingPointError": FloatingPointError,
        "GeneratorExit": GeneratorExit, "ImportError": ImportError,
        "IndexError": IndexError, "KeyError": KeyError, "KeyboardInterrupt": KeyboardInterrupt,
        "LookupError": LookupError, "MemoryError": MemoryError,
        "ModuleNotFoundError": ModuleNotFoundError, "NameError": NameError,
        "NotImplementedError": NotImplementedError, "OSError": OSError,
        "OverflowError": OverflowError, "RecursionError": RecursionError,
        "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
        "StopAsyncIteration": StopAsyncIteration, "StopIteration": StopIteration,
        "SyntaxError": SyntaxError, "SystemError": SystemError, "SystemExit": SystemExit,
        "TypeError": TypeError, "UnboundLocalError": UnboundLocalError,
        "ValueError": ValueError, "ZeroDivisionError": ZeroDivisionError,
        "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
        "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
        "ConnectionAbortedError": ConnectionAbortedError,
        "ConnectionRefusedError": ConnectionRefusedError,
        "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
        "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
        "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
        "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
        "TimeoutError": TimeoutError, "EnvironmentError": EnvironmentError,
        "IOError": IOError, "WindowsError": OSError,
        "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
        "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
        "DeprecationWarning": DeprecationWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
        "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
        "UserWarning": UserWarning, "FutureWarning": FutureWarning,
        "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
        "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
        "Warning": Warning, "IndentationError": IndentationError, "TabError": TabError,
    }
    if kind in base:
        cls = base[kind]
    elif kind in builtin:
        cls = builtin[kind]
    elif kind == "CancelledError":
        import asyncio
        cls = asyncio.CancelledError
    else:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


_MISSING = object()


def _unwrap_script(val):
    if isinstance(val, dict) and "__raise__" in val:
        return Raise(val, build_exc(val))
    if isinstance(val, dict) and val.get("__script__") is True:
        return val.get("ret")
    return val


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        r = _unwrap_script(val)
        if isinstance(r, Raise):
            self.handed.append(r)
        return r

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r


# ---- doubles + normalization ----
class GateEvent:
    """The gate's leader_done Event: is_set/set silent state flags; wait
    rides the event.wait seam (its return is ignored by _wait)."""
    def __init__(self):
        self._flag = False

    def is_set(self):
        return self._flag

    def set(self):
        self._flag = True

    def wait(self, timeout=None):
        return CURRENT["rec"]("event.wait")(timeout)


class TimeDouble:
    def monotonic(self):
        return CURRENT["rec"]("time.monotonic")()


class ThreadingDouble:
    Event = GateEvent


def project_gate(g):
    return {"leader": norm_state(g.leader), "max_wait_s": norm_value(g.max_wait_s),
            "leader_route": [norm_value(x) for x in g._leader_route],
            "leader_done": bool(g._leader_done.is_set())}


def norm_value(v):
    if PrefixCacheGate is not None and isinstance(v, PrefixCacheGate):
        return project_gate(v)
    if isinstance(v, GateEvent):
        return bool(v._flag)
    if callable(v):
        return {"__callable__": getattr(v, "__dpg2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__dpg2_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__dpg2") and k not in inj}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    return v


def norm_state(v):
    if PrefixCacheGate is not None and isinstance(v, PrefixCacheGate):
        return project_gate(v)
    if isinstance(v, GateEvent):
        return bool(v._flag)
    if callable(v):
        return {"__callable__": getattr(v, "__dpg2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__dpg2_injected__", set())
        inner = {k: norm_state(x) for k, x in vars(v).items()
                 if not k.startswith("__dpg2") and k not in inj}
        out = {"__attr__": inner}
        idv = getattr(v, "__dpg2_id__", None)
        if idv is not None:
            out["__id__"] = idv
        cls = getattr(v, "__dpg2_class__", None)
        if cls:
            out["__class__"] = cls
        return out
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_state(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_state(x) for x in v]
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    return v


norm_ret = norm_state


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj(name, a, k):
    if name == "event.wait":
        return {"timeout": a[0] if a else k.get("timeout")}
    if name == "time.monotonic":
        return {}
    if name == "gate.run_fn":
        return {}
    return {**{("arg%d" % i): v for i, v in enumerate(a)}, **k}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            return materialize_result(name, raw)
        call.__dpg2_seam__ = name
        return call


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            idv = j.get("__id__")
            if idv is not None and idv in CURRENT["ids"]:
                return CURRENT["ids"][idv]
            inner = j["__attr__"] or {}
            kids = {kk: materialize_double(vv) for kk, vv in inner.items()}
            obj = SimpleNamespace(**kids)
            obj.__dpg2_id__ = idv
            obj.__dpg2_class__ = j.get("__class__")
            obj.__dpg2_injected__ = set()
            if idv is not None:
                CURRENT["ids"][idv] = obj
            return obj
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        return {kk: materialize_double(vv) for kk, vv in j.items()}
    if isinstance(j, list):
        return [materialize_double(vv) for vv in j]
    return j


def materialize_result(name, raw):
    if isinstance(raw, dict) and ("__attr__" in raw or "__callable__" in raw):
        return materialize_double(raw)
    return raw


# ---- live namespace seeding (AST-extract the one defining module) ----
def _exec_seg(src, node, ns, path):
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _exec_class_seg(src, node, ns, path):
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno if node.decorator_list else node.lineno) - 1
    seg = "\n".join(lines[start:node.end_lineno])
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def load_ns():
    global NS, PrefixCacheGate
    mod = types.ModuleType("dpg2_live")
    sys.modules["dpg2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "dpg2_live", "json": json, "os": os,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "threading": ThreadingDouble(), "time": TimeDouble(),
    })
    path = str(QUARRY / "tools" / "delegate_tool_prefix_gate.py")
    src = Path(path).read_text(encoding="utf-8")
    tree = ast.parse(src)
    assigns = {"_FIRST_CHUNK_ACTIVITY", "_POLL_S"}
    for n in tree.body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            tgts = n.targets if isinstance(n, ast.Assign) else [n.target]
            if any(isinstance(t, ast.Name) and t.id in assigns for t in tgts):
                _exec_seg(src, n, ns, path)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "_route_key", "_caches_prompts", "_first_response_started", "_stop_requested"):
            _exec_seg(src, n, ns, path)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == "PrefixCacheGate":
            _exec_class_seg(src, n, ns, path)
    PrefixCacheGate = ns["PrefixCacheGate"]
    return ns


def _state(inp, key):
    return copy.deepcopy((inp.get("state") or {}).get(key))


def _win(**kw):
    CURRENT["stwin"] = kw
    return kw


def _gate_from_dict(gd):
    gd = gd or {}
    leader = materialize_double(copy.deepcopy(gd.get("leader")))
    g = PrefixCacheGate(leader, gd.get("max_wait_s", 0))
    g._leader_route = tuple(gd.get("leader_route") or [])
    g._leader_done._flag = bool(gd.get("leader_done"))
    return g


def _mat_triples(children):
    out = []
    for tr in (children or []):
        i, t, c = tr
        out.append([i, copy.deepcopy(t), materialize_double(copy.deepcopy(c))])
    return out


def drive(func, inp):
    CURRENT["ids"] = {}
    CURRENT["stwin"] = {}
    g = inp.get
    if func in ("_route_key", "_caches_prompts", "_first_response_started", "_stop_requested"):
        child = materialize_double(_state(inp, "child"))
        _win(child=child)
        return NS[func](child), CURRENT["stwin"]
    if func == "PrefixCacheGate.__init__":
        leader = materialize_double(copy.deepcopy(g("leader")))
        gate = PrefixCacheGate(leader, g("max_wait_s", 0))
        _win(gate=gate)
        return None, CURRENT["stwin"]
    if func == "PrefixCacheGate.for_children":
        children = _mat_triples(g("children"))
        ret = PrefixCacheGate.for_children(children, g("max_wait_s", 0))
        _win(gate=ret) if ret is not None else _win()
        return ret, CURRENT["stwin"]
    if func == "PrefixCacheGate.follows":
        gate = _gate_from_dict(_state(inp, "gate"))
        child = materialize_double(copy.deepcopy(g("child")))
        _win(gate=gate)
        return gate.follows(child), CURRENT["stwin"]
    if func == "PrefixCacheGate.run":
        gate = _gate_from_dict(_state(inp, "gate"))
        child = materialize_double(copy.deepcopy(g("child")))
        parent = materialize_double(copy.deepcopy(g("parent")))
        run = materialize_double(copy.deepcopy(g("run")))
        _win(gate=gate)
        return gate.run(child, run, parent=parent), CURRENT["stwin"]
    if func == "PrefixCacheGate._wait":
        gate = _gate_from_dict(_state(inp, "gate"))
        child = materialize_double(copy.deepcopy(g("child")))
        parent = materialize_double(copy.deepcopy(g("parent")))
        _win(gate=gate)
        return gate._wait(child, parent), CURRENT["stwin"]
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    CURRENT["rec"] = ctx.recorder
    CURRENT["stwin"] = {}
    try:
        ret, stwin = drive(func, inp)
        return {"ret": norm_ret(ret), "trace": ctx.trace, "st": norm_state(stwin)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": norm_state(CURRENT["stwin"])}


def main():
    try:
        load_ns()
    except BaseException as e:
        print("PYERR load_ns: %s: %s" % (type(e).__name__, e))
        return 1
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = norm_value({"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")})
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE delegate_prefix_gate_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
