#!/usr/bin/env python3
"""Oracle for src/agent/tool_dispatch_flow2.c — drives the LIVE unclaimed
remainder of the delegate_task tool-dispatch surface under IDENTICAL scripted
universal seams. The task label "agent/tool_dispatch.py" does not exist in the
quarry; the dispatch core tools/delegate_tool_dispatch.py is fully claimed by
tool_dispatch_flow. This oracle takes the REMAINING unclaimed collaborator
functions across their defining modules (28 funcs):

  tools/delegate_tool_child_run.py (12), tools/delegate_tool_progress.py (6),
  tools/delegate_tool_prefix_gate.py (6 incl. PrefixCacheGate.for_children/run),
  tools/delegate_tool_config.py (1), tools/delegate_tool_auto_resume.py (1),
  gateway/session_finite.py (1), tools/process_registry_notifications.py (1).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted; claimed
siblings (_clean_error_text, _emit_parent_console, request_hard_interrupt) and
foreign leaves (service_tier_word, failure_cause_gloss, _load_config) are
recording seams fed the SAME scripts the C harness feeds; ported siblings
compose for REAL.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every except Exception guard exactly as live (real builtin
classes). Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed.

Value model: attribute doubles {"__attr__": ..., "__id__": ...} <->
SimpleNamespace (interned by __id__ so `is` agrees; state echo RE-WRAPS,
injected seam methods excluded); the parent's _active_children is a
{"__children__": [...]} marker <-> a ChildrenDouble list (append/remove are
deterministic modeled seams); the gate's leader_done is a GateEvent (is_set/set
silent state flags, wait rides the event.wait seam); a foreign {"__event__": id}
marker's is_set rides the event.is_set seam; callables project to
{"__callable__": seam}; integral floats normalize to ints on both sides.

Domain (documented for the generator): ASCII fixture text; the live em-dash
(U+2014) literal rides _process_accounting_lines output as UTF-8;
_format_duration / _get_prefix_stagger_seconds avoid integral floats in
str()-computed positions; float() coercion covers numbers/bools/ASCII numeric
strings/raising forms (no exotic underscore/inf/nan strings); accounting /
activity carriers are lists-or-null / dict-or-null.

Summary: 'ORACLE tool_dispatch_flow2: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import sys
import types
import __future__
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable, Dict, List, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "tools" / "delegate_tool_child_run.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

ANN_FLAG = __future__.annotations.compiler_flag

# drive keys == live names (gate methods carry the PrefixCacheGate. prefix)
FUNCS = [
    "_num", "_str_or_none", "_tier_in_effect", "_route_report", "_fabricated_entry",
    "_append_missed_steer", "_close_child", "_with_children_lock", "_attach_child",
    "_detach_child", "_signal_child_stop", "_child_activity_fingerprint",
    "_quiet", "_safe_progress", "describe_subagent_failure", "_format_duration",
    "_print_completion_line", "_short",
    "_route_key", "_caches_prompts", "_first_response_started", "_stop_requested",
    "PrefixCacheGate.for_children", "PrefixCacheGate.run",
    "_get_prefix_stagger_seconds", "waiting_on_resume", "finite_turn_required",
    "_process_accounting_lines",
]

CURRENT = {"ctx": None, "rec": None, "ids": {}}
NS = {}
PrefixCacheGate = None  # bound to the extracted class in load_ns


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__td2_seam__ = seam
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {"TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
               "AttributeError": AttributeError, "KeyError": KeyError, "Exception": Exception,
               "TimeoutError": TimeoutError, "OSError": OSError, "LookupError": LookupError,
               "IndexError": IndexError, "StopIteration": StopIteration,
               "InterruptedError": InterruptedError, "NotImplementedError": NotImplementedError,
               "ModuleNotFoundError": ModuleNotFoundError, "ImportError": ImportError,
               "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
               "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
               "ConnectionError": ConnectionError, "UnicodeError": UnicodeError,
               "AssertionError": AssertionError, "NameError": NameError,
               "RecursionError": RecursionError, "MemoryError": MemoryError,
               "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
               "BufferError": BufferError, "EOFError": EOFError, "ReferenceError": ReferenceError,
               "SystemError": SystemError, "SyntaxError": SyntaxError, "IndentationError": IndentationError,
               "TabError": TabError, "StopAsyncIteration": StopAsyncIteration,
               "UnboundLocalError": UnboundLocalError, "BlockingIOError": BlockingIOError,
               "ChildProcessError": ChildProcessError, "BrokenPipeError": BrokenPipeError,
               "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
               "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
               "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
               "ProcessLookupError": ProcessLookupError, "UnicodeDecodeError": UnicodeDecodeError,
               "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
               "Warning": Warning, "DeprecationWarning": DeprecationWarning,
               "PendingDeprecationWarning": PendingDeprecationWarning, "RuntimeWarning": RuntimeWarning,
               "SyntaxWarning": SyntaxWarning, "UserWarning": UserWarning, "FutureWarning": FutureWarning,
               "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
               "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning}
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


def norm_value(v):
    if PrefixCacheGate is not None and isinstance(v, PrefixCacheGate):
        return project_gate(v)
    if isinstance(v, ChildrenDouble):
        return [norm_value(x) for x in v.items]
    if isinstance(v, GateEvent):
        return bool(v._flag)
    if isinstance(v, SilentLock):
        return {"__lock__": True}
    if isinstance(v, ForeignEvent):
        return {"__event__": v.eid}
    if callable(v):
        return {"__callable__": getattr(v, "__td2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__td2_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__td2") and k not in inj}
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
    if isinstance(v, ChildrenDouble):
        return [norm_state(x) for x in v.items]
    if isinstance(v, GateEvent):
        return bool(v._flag)
    if isinstance(v, SilentLock):
        return {"__lock__": True}
    if isinstance(v, ForeignEvent):
        return {"__event__": v.eid}
    if callable(v):
        return {"__callable__": getattr(v, "__td2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__td2_injected__", set())
        inner = {k: norm_state(x) for k, x in vars(v).items()
                 if not k.startswith("__td2") and k not in inj}
        out = {"__attr__": inner}
        idv = getattr(v, "__td2_id__", None)
        if idv is not None:
            out["__id__"] = idv
        cls = getattr(v, "__td2_class__", None)
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


def double_proj(v):
    if isinstance(v, SimpleNamespace):
        return norm_state(v)
    return norm_value(v)


def project_gate(g):
    return {"leader": norm_state(g.leader), "max_wait_s": norm_value(g.max_wait_s),
            "leader_route": [norm_value(x) for x in g._leader_route],
            "leader_done": bool(g._leader_done.is_set())}


def proj(name, a, k):
    if name.startswith("logger."):
        d = {"msg": a[0], "args": [str(x) if isinstance(x, BaseException) else x for x in a[1:]]}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "service_tier_word":
        return {"tier": a[0]}
    if name == "failure_cause_gloss":
        return {"failure_reason": a[0], "subject": k.get("subject"), "possessive": k.get("possessive")}
    if name == "clean_error_text":
        return {"error": a[0], "max_chars": a[1] if len(a) > 1 else k.get("max_chars")}
    if name == "emit_parent_console":
        return {"parent_agent": double_proj(a[0]), "line": a[1]}
    if name == "request_hard_interrupt":
        return {"child": double_proj(a[0]), "reason": list(a[1:]), "tool_reason": k.get("tool_reason")}
    if name == "child.close":
        return {}
    if name == "child.interrupt":
        return {"message": a[0]}
    if name == "child.get_activity_summary":
        return {}
    if name == "active_children_op":
        return {"op": k.get("op"), "child": double_proj(k.get("child"))}
    if name == "event.is_set":
        return {"id": k.get("id")}
    if name == "event.wait":
        return {"timeout": a[0] if a else k.get("timeout")}
    if name == "time.monotonic":
        return {}
    if name == "quiet.body":
        return {}
    if name == "progress.cb":
        return {"event_type": a[0], "args": list(a[1:]), "kwargs": dict(k)}
    if name == "spinner.print_above":
        return {"line": a[0]}
    if name == "gate.run_fn":
        return {}
    if name == "load_config":
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
        call.__td2_seam__ = name
        return call


def _inject(obj, **kw):
    for kk, vv in kw.items():
        setattr(obj, kk, vv)
    inj = getattr(obj, "__td2_injected__", None)
    if inj is None:
        inj = set()
        obj.__td2_injected__ = inj
    inj.update(kw.keys())


class ChildrenDouble:
    """The parent's _active_children list: append/remove are deterministic
    modeled seams (mirror the C dispatcher's active_children_op)."""
    def __init__(self, items):
        self.items = list(items)

    def append(self, child):
        ctx = CURRENT["ctx"]
        args = norm_args(proj("active_children_op", (), {"op": "append", "child": child}))
        self.items.append(child)
        ctx.trace.append({"ev": "active_children_op", "args": args, "ret": None})

    def remove(self, child):
        ctx = CURRENT["ctx"]
        args = norm_args(proj("active_children_op", (), {"op": "remove", "child": child}))
        if child in self.items:
            self.items.remove(child)
            ctx.trace.append({"ev": "active_children_op", "args": args, "ret": None})
        else:
            spec = {"__raise__": "ValueError", "message": "list.remove(x): x not in list"}
            ctx.trace.append({"ev": "active_children_op", "args": args, "ret": spec})
            raise ValueError("list.remove(x): x not in list")


class GateEvent:
    """The gate's leader_done Event: is_set/set are silent state flags; wait
    rides the event.wait seam (return ignored by _wait)."""
    def __init__(self):
        self._flag = False

    def is_set(self):
        return self._flag

    def set(self):
        self._flag = True

    def wait(self, timeout=None):
        return CURRENT["rec"]("event.wait")(timeout)


class ForeignEvent:
    """A foreign {"__event__": id} marker: is_set rides the event.is_set seam."""
    def __init__(self, eid):
        self.eid = eid

    def is_set(self):
        return CURRENT["rec"]("event.is_set")(id=self.eid)


class SilentLock:
    """A {"__lock__": true} marker: the parent's _active_children_lock CM is
    silent in the single-threaded harness (no seam; C models it as a no-op)."""
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        return CURRENT["rec"]("logger." + level)(msg, *args, **kw)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)

    def error(self, msg, *args, **kw):
        return self._log("error", msg, args, kw)


class TimeDouble:
    def monotonic(self):
        return CURRENT["rec"]("time.monotonic")()


class ThreadingDouble:
    Event = GateEvent


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            idv = j.get("__id__")
            if idv is not None and idv in CURRENT["ids"]:
                return CURRENT["ids"][idv]
            inner = j["__attr__"] or {}
            kids = {kk: materialize_double(vv) for kk, vv in inner.items()}
            obj = SimpleNamespace(**kids)
            obj.__td2_id__ = idv
            obj.__td2_class__ = j.get("__class__")
            obj.__td2_injected__ = set()
            if idv is not None:
                CURRENT["ids"][idv] = obj
            return obj
        if "__children__" in j:
            return ChildrenDouble([materialize_double(c) for c in (j["__children__"] or [])])
        if "__event__" in j:
            return ForeignEvent(j["__event__"])
        if "__lock__" in j:
            return SilentLock()
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        return {kk: materialize_double(vv) for kk, vv in j.items()}
    if isinstance(j, list):
        return [materialize_double(vv) for vv in j]
    return j


def materialize_result(name, raw):
    if isinstance(raw, dict) and ("__attr__" in raw or "__callable__" in raw or "__children__" in raw):
        return materialize_double(raw)
    return raw
# ---- live namespace seeding (AST-extract from the 7 defining modules) ----
def _exec_seg(src, node, ns, path):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _exec_class_seg(src, node, ns, path):
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno if node.decorator_list else node.lineno) - 1
    seg = "\n".join(lines[start:node.end_lineno])
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _install_stub(modname, attrs):
    parts = modname.split(".")
    for i in range(1, len(parts) + 1):
        name = ".".join(parts[:i])
        if name not in sys.modules or not isinstance(sys.modules[name], types.ModuleType):
            m = types.ModuleType(name)
            m.__path__ = []
            sys.modules[name] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], sys.modules[name])
    mod = sys.modules[modname]
    for kk, vv in attrs.items():
        setattr(mod, kk, vv)
    return mod


# (module path, function names, class names, assign names)
EXTRACT = [
    ("tools/delegate_tool_child_run.py",
     ["_num", "_str_or_none", "_tier_in_effect", "_route_report", "_fabricated_entry",
      "_append_missed_steer", "_close_child", "_with_children_lock", "_attach_child",
      "_detach_child", "_signal_child_stop", "_child_activity_fingerprint"], [], []),
    ("tools/delegate_tool_progress.py",
     ["_quiet", "_safe_progress", "describe_subagent_failure", "_format_duration",
      "_print_completion_line", "_short"], [], []),
    ("tools/delegate_tool_prefix_gate.py",
     ["_route_key", "_caches_prompts", "_first_response_started", "_stop_requested"],
     ["PrefixCacheGate"], ["_FIRST_CHUNK_ACTIVITY", "_POLL_S"]),
    ("tools/delegate_tool_config.py",
     ["_knob", "_get_prefix_stagger_seconds"], [], []),
    ("tools/delegate_tool_auto_resume.py", ["waiting_on_resume"], [], []),
    ("gateway/session_finite.py", ["finite_turn_required"], [], ["_finite_turn"]),
    ("tools/process_registry_notifications.py", ["_process_accounting_lines"], [], []),
]


def load_ns():
    global NS, PrefixCacheGate
    mod = types.ModuleType("td2_live")
    sys.modules["td2_live"] = mod
    ns = mod.__dict__
    NS = ns
    import contextvars
    import threading as _real_threading
    ns.update({
        "__name__": "td2_live", "json": json, "os": os, "contextvars": contextvars,
        "ContextVar": contextvars.ContextVar,
        "Any": Any, "Optional": Optional, "Callable": Callable, "Dict": dict, "List": list,
        "threading": ThreadingDouble(), "time": TimeDouble(), "logger": LoggerDouble(),
        # claimed siblings + foreign leaves -> recording seams
        "request_hard_interrupt": _stub_fn("request_hard_interrupt"),
        "_clean_error_text": _stub_fn("clean_error_text"),
        "_emit_parent_console": _stub_fn("emit_parent_console"),
        "_load_config": _stub_fn("load_config"),
        "service_tier_word": _stub_fn("service_tier_word"),
        "failure_cause_gloss": _stub_fn("failure_cause_gloss"),
    })
    for rel, fns, classes, assigns in EXTRACT:
        path = str(QUARRY / rel)
        src = Path(path).read_text(encoding="utf-8")
        tree = ast.parse(src)
        for n in tree.body:
            if isinstance(n, (ast.Assign, ast.AnnAssign)):
                tgts = n.targets if isinstance(n, ast.Assign) else [n.target]
                if any(isinstance(t, ast.Name) and t.id in assigns for t in tgts):
                    _exec_seg(src, n, ns, path)
        for n in tree.body:
            if isinstance(n, ast.FunctionDef) and n.name in fns:
                _exec_seg(src, n, ns, path)
        for n in tree.body:
            if isinstance(n, ast.ClassDef) and n.name in classes:
                _exec_class_seg(src, n, ns, path)
    # _quiet is a @contextmanager (decorator stripped by segment extraction)
    ns["_quiet"] = contextmanager(ns["_quiet"])
    PrefixCacheGate = ns["PrefixCacheGate"]
    # in-function import stubs
    _install_stub("agent.fast_mode", {"service_tier_word": _stub_fn("service_tier_word")})
    _install_stub("agent.turn_failure_copy", {"failure_cause_gloss": _stub_fn("failure_cause_gloss")})
    return ns


def _state(inp, key):
    st = inp.get("state") or {}
    return copy.deepcopy(st.get(key))


def _mat_state(inp, key):
    return materialize_double(_state(inp, key))


def _win(**kw):
    CURRENT["stwin"] = kw
    return kw


def _mat_triples(children):
    out = []
    for tr in (children or []):
        i, t, c = tr
        out.append([i, copy.deepcopy(t), materialize_double(copy.deepcopy(c))])
    return out


def _gate_from_dict(gd):
    leader = materialize_double(copy.deepcopy(gd.get("leader")))
    g = NS["PrefixCacheGate"](leader, gd.get("max_wait_s", 0))
    g._leader_route = tuple(gd.get("leader_route") or [])
    g._leader_done._flag = bool(gd.get("leader_done"))
    return g


def drive(func, inp):
    CURRENT["ids"] = {}
    CURRENT["stwin"] = {}
    g = inp.get
    if func == "_num":
        _win()
        return NS[func](g("value"), g("default", 0)), CURRENT["stwin"]
    if func == "_str_or_none":
        _win()
        return NS[func](g("value")), CURRENT["stwin"]
    if func in ("_tier_in_effect", "_route_report",
                "_child_activity_fingerprint", "_route_key", "_caches_prompts",
                "_first_response_started", "_stop_requested"):
        child = _mat_state(inp, "child")
        if func == "_child_activity_fingerprint" and isinstance(child, SimpleNamespace):
            _inject(child, get_activity_summary=CURRENT["rec"]("child.get_activity_summary"))
        _win(child=child)
        return NS[func](child), CURRENT["stwin"]
    if func == "_close_child":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](child, g("log_message")), CURRENT["stwin"]
    if func == "_fabricated_entry":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](g("idx", 0), g("status", ""), g("error"), child,
                        duration=g("duration", 0)), CURRENT["stwin"]
    if func == "_append_missed_steer":
        entry = _state(inp, "entry")
        if entry is None:
            entry = {}
        _win(entry=entry)
        return NS[func](entry, g("late_steer")), CURRENT["stwin"]
    if func in ("_with_children_lock", "_attach_child", "_detach_child"):
        parent = _mat_state(inp, "parent_agent")
        child = _mat_state(inp, "child")
        _win(parent_agent=parent, child=child)
        if func == "_with_children_lock":
            return NS[func](parent, g("op"), child), CURRENT["stwin"]
        return NS[func](parent, child), CURRENT["stwin"]
    if func == "_signal_child_stop":
        child = _mat_state(inp, "child")
        _win(child=child)
        reasons = g("reasons") or []
        tr = g("tool_reason", "parent delegation ended")
        return NS[func](child, *reasons, tool_reason=tr), CURRENT["stwin"]
    if func == "_quiet":
        _win()
        lm = g("log_message")
        la = g("log_args") or []
        ei = g("exc_info", False)
        with NS["_quiet"](lm, *la, exc_info=ei):
            CURRENT["rec"]("quiet.body")()
        return None, CURRENT["stwin"]
    if func == "_safe_progress":
        _win()
        cb = materialize_double(copy.deepcopy(g("cb")))
        args = g("args") or []
        kwargs = g("kwargs") or {}
        return NS[func](cb, g("event_type"), *args, **kwargs), CURRENT["stwin"]
    if func == "describe_subagent_failure":
        _win()
        return NS[func](g("failure_reason"), g("error"),
                        max_chars=g("max_chars", 200)), CURRENT["stwin"]
    if func == "_format_duration":
        _win()
        return NS[func](g("seconds")), CURRENT["stwin"]
    if func == "_print_completion_line":
        _win()
        parent = materialize_double(copy.deepcopy(g("parent_agent")))
        spinner = materialize_double(copy.deepcopy(g("spinner")))
        return NS[func](parent, spinner, g("line"),
                        console_line=g("console_line")), CURRENT["stwin"]
    if func == "_short":
        _win()
        return NS[func](g("text", ""), g("n", 0)), CURRENT["stwin"]
    if func == "PrefixCacheGate.for_children":
        children = _mat_triples(g("children"))
        ret = NS["PrefixCacheGate"].for_children(children, g("max_wait_s", 0))
        if ret is not None:
            _win(gate=ret)
        else:
            _win()
        return ret, CURRENT["stwin"]
    if func == "PrefixCacheGate.run":
        gd = _state(inp, "gate") or {}
        gate = _gate_from_dict(gd)
        child = materialize_double(copy.deepcopy(g("child")))
        parent = materialize_double(copy.deepcopy(g("parent")))
        run = materialize_double(copy.deepcopy(g("run")))
        _win(gate=gate)
        return gate.run(child, run, parent=parent), CURRENT["stwin"]
    if func == "_get_prefix_stagger_seconds":
        _win()
        return NS[func](), CURRENT["stwin"]
    if func == "waiting_on_resume":
        _win()
        return NS[func](copy.deepcopy(g("entry") or {})), CURRENT["stwin"]
    if func == "finite_turn_required":
        _win()
        NS["_finite_turn"].set(g("finite_turn"))
        return NS[func](), CURRENT["stwin"]
    if func == "_process_accounting_lines":
        _win()
        return NS[func](copy.deepcopy(g("r") or {})), CURRENT["stwin"]
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
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE tool_dispatch_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())