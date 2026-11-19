#!/usr/bin/env python3
"""Oracle for src/agent/tool_dispatch_flow.c — drives the LIVE
tools/delegate_tool_dispatch.py FLOW defs (21 UNCLAIMED functions: the _Batch
owner_kwargs / run_child methods, the batch announce / origin capture, the
child-done reporter / finished-child recorder, the resume probe / unstarted
release / gated run, the parallel children runner, the execute-and-aggregate
join, the sync fallback note, the async wake-sid / session-key resolvers, the
progress token, the dispatched payload / unit partition / unit dispatch /
parent-cancellation restore, the background dispatcher and the detached-only
refusal) under IDENTICAL scripted universal seams.

The task label "agent/tool_dispatch.py" does not exist in the quarry; the live
tool-dispatch surface is tools/delegate_tool_dispatch.py (the real-gap-list
"18 gaps" module). _run_batch is CLAIMED (src/tools/clarify_tool.c PoP) and is
NOT ported; every other top-level def plus both _Batch methods are ported.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted (the _Batch
dataclass WITH its @dataclass decorator); claimed sibling _run_batch and all
foreign I/O are recording seams fed the SAME scripts the C harness feeds;
ported siblings compose for REAL.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) propagates past
every except Exception guard exactly as live (real builtin classes). Per-case
BaseException catch -> __raise__ spec; scripted raises round-trip BARE via
identity match against Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (state
echo RE-WRAPS, injected seam methods excluded); _Batch is a REAL dataclass
(norm -> plain field dict); futures are per-item scripted doubles (done /
result / cancelled scripts live on the submit-result spec, keyed by id);
callables project to {"__callable__": true}; integral floats normalize to ints
on both sides. ASCII fixture text; the live emoji literals (U+1F500 U+26A0
U+2713 U+2717 U+00B7 U+2014) ride seam args as UTF-8.

Domain (documented for the generator):
  - json.dumps returns (_run_sync_with_note / _dispatch_background) carry NO
    integral floats (libjson has no int/float identity); total_duration is
    scripted non-integral. Entry duration_seconds is int / non-integral float.
  - round(x, 2) fixtures avoid exact half-way decimals.
  - _run_children_parallel pending sets iterate in ascending future-id order
    (FutureDouble.__hash__ = id suffix; ids "fN", N = child position, <8), so
    the C port models the initial pending as children order; cf_wait returns
    scripted ordered lists.
  - entries are plain dicts; task_index / goal / model keys present where the
    live code subscripts them (KeyError cases script the absence deliberately).

Summary: 'ORACLE tool_dispatch_flow: N cases, M mismatches'.
"""
import ast
import copy
import dataclasses
import json
import math
import os
import sys
import types
import __future__
from contextlib import contextmanager
from dataclasses import dataclass, field, replace
from functools import partial
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
if not (QUARRY / "tools" / "delegate_tool_dispatch.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "tools" / "delegate_tool_dispatch.py")

ANN_FLAG = __future__.annotations.compiler_flag

# drive keys == live names (methods carry the _Batch. prefix)
FUNCS = [
    "_Batch.owner_kwargs", "_Batch.run_child", "_announce_batch", "_capture_origin",
    "_report_child_done", "_record_finished_child", "_waiting_on_resume",
    "_release_unstarted_child", "_run_gated", "_run_children_parallel",
    "_execute_and_aggregate", "_run_sync_with_note", "_resolve_async_wake_sid",
    "_resolve_async_session_key", "_batch_progress_token", "_dispatched_payload",
    "_units_of", "_dispatch_unit", "_restore_parent_cancellation",
    "_dispatch_background", "_refuse_detached_only",
]
# the 19 module-level FunctionDefs extracted for real
PY_FUNCS = [f for f in FUNCS if not f.startswith("_Batch.")]

CURRENT = {"ctx": None, "rec": None, "ids": {}, "futures": {}}
NS = {}


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__seam_name__ = seam
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
               "JSONDecodeError": json.JSONDecodeError,
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
    if kind == "JSONDecodeError":
        return cls(msg, "", 0)
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        return type(kind, (ValueError,), {})(msg)
    return cls(msg)


_MISSING = object()


def _unwrap_script(val):
    """__script__ wrapper -> ret; __raise__ -> Raise; else the value."""
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


def _dc_to_dict(v):
    return {f.name: getattr(v, f.name) for f in dataclasses.fields(v)}


def norm_value(v):
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {k: norm_value(x) for k, x in _dc_to_dict(v).items()}
    if callable(v):
        return {"__callable__": getattr(v, "__td_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__td_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__td") and k not in inj}
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
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {k: norm_state(x) for k, x in _dc_to_dict(v).items()}
    if callable(v):
        return {"__callable__": getattr(v, "__td_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__td_injected__", set())
        inner = {k: norm_state(x) for k, x in vars(v).items()
                 if not k.startswith("__td") and k not in inj}
        cls = getattr(v, "__td_class__", None)
        out = {"__attr__": inner}
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


def triple_proj(children):
    return [[i, norm_value(t), double_proj(c)] for (i, t, c) in children]


def _future_ids(fs):
    ids = [getattr(f, "id", None) for f in fs]
    if isinstance(fs, (set, frozenset)):
        ids = sorted(ids, key=lambda x: (x is None, x))
    return ids


def proj(name, a, k):
    if name.startswith("logger."):
        d = {"msg": a[0], "args": list(a[1:])}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "format_batch_tag":
        return {"deleg_id": a[0], "parent_agent": double_proj(a[1]) if len(a) > 1 else None}
    if name == "print_completion_line":
        d = {"parent_agent": double_proj(a[0]), "spinner": double_proj(a[1]), "line": a[2]}
        if "console_line" in k:
            d["console_line"] = k["console_line"]
        return d
    if name == "describe_subagent_failure":
        return {"failure_reason": a[0], "error": a[1], "max_chars": k.get("max_chars")}
    if name == "get_session_env":
        d = {"key": a[0]}
        if len(a) > 1:
            d["default"] = a[1]
        return d
    if name == "capture_gateway_steer_authority":
        return {"ui_session_id": a[0]}
    if name == "record_unit_child":
        return {"unit_id": a[0], "entry": a[1]}
    if name == "waiting_on_resume":
        return {"entry": a[0]}
    if name == "release_unstarted":
        return {"child": double_proj(a[0])}
    if name in ("detach_child", "attach_child"):
        return {"parent_agent": double_proj(a[0]), "child": double_proj(a[1])}
    if name in ("close_child", "signal_child_stop"):
        return {"child": double_proj(a[0]), "message": a[1]}
    if name == "fabricated_entry":
        return {"idx": a[0], "status": a[1], "error": a[2], "child": double_proj(a[3])}
    if name == "gate.run":
        return {"child": double_proj(a[0]), "fn": norm_value(a[1]), "parent": double_proj(k.get("parent"))}
    if name == "executor.new":
        return {"max_workers": k.get("max_workers")}
    if name == "executor.submit":
        return {"ctx_run": norm_value(a[0]), "fn": norm_value(a[1]), "i": a[2],
                "task": norm_value(a[3]), "child": double_proj(a[4])}
    if name == "executor.shutdown":
        return {"wait": k.get("wait"), "cancel_futures": k.get("cancel_futures")}
    if name == "gate.for_children":
        return {"children": triple_proj(a[0]), "stagger": a[1]}
    if name == "cf_wait":
        return {"pending": _future_ids(a[0]), "timeout": k.get("timeout"),
                "return_when": k.get("return_when")}
    if name in ("future.done", "future.result", "future.cancelled"):
        return {"id": k.get("id")}
    if name == "push_task_failure_notice":
        return {"unit_id": a[0], "payload": a[1], "n_tasks": k.get("n_tasks")}
    if name == "run_single_child":
        d = dict(k)
        for kk in ("child", "parent_agent", "owner_transport", "owner_session_record"):
            if kk in d:
                d[kk] = double_proj(d[kk])
        return d
    if name == "finalize_child_results":
        return {"results": a[0], "task_list": a[1], "children": triple_proj(a[2]),
                "parent_agent": double_proj(a[3])}
    if name == "writer.finalize":
        return {"index": k.get("index"), "entry": a[0]}
    if name == "update_manifest_statuses":
        return {"deleg_id": a[0], "results": a[1], "home": k.get("home")}
    if name == "finish_delegation_unit":
        return {"task_list": a[0], "results": a[1], "background": k.get("background")}
    if name == "process_accounting_lines":
        return {"entry": a[0]}
    if name == "dispatch_async":
        d = dict(k)
        for kk in ("runner", "interrupt_fn", "progress_fn"):
            if kk in d:
                d[kk] = norm_value(d[kk])
        return d
    if name == "get_current_session_key":
        return {"default": k.get("default")}
    if name == "child.get_activity_summary":
        return {"index": k.get("index")}
    if name == "spinner.update_text":
        return {"text": a[0]}
    if name == "tool_error":
        return {"message": a[0]}
    if name in ("current_origin_session_id", "session_history_delivery_supported",
                "get_prefix_stagger_seconds", "get_independent_completions",
                "get_max_async_children", "new_delegation_id", "finite_turn_required",
                "async_delivery_supported", "time.monotonic"):
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
        call.__td_seam__ = name
        return call


def _inject(obj, **kw):
    for kk, vv in kw.items():
        setattr(obj, kk, vv)
    inj = getattr(obj, "__td_injected__", None)
    if inj is None:
        inj = set()
        obj.__td_injected__ = inj
    inj.update(kw.keys())


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            inner = j["__attr__"] or {}
            cls = j.get("__class__")
            kids = {kk: materialize_double(vv) for kk, vv in inner.items()}
            obj = SimpleNamespace(**kids)
            obj.__td_class__ = cls
            obj.__td_injected__ = set()
            return obj
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        if "__ref__" in j:
            return CURRENT["ids"][j["__ref__"]]
        return {kk: materialize_double(vv) for kk, vv in j.items()}
    if isinstance(j, list):
        return [materialize_double(vv) for vv in j]
    return j


class FutureDouble:
    """Per-item scripted future: done/result/cancelled scripts live on the
    submit-result spec (single value reused, bare array = per-call queue)."""
    def __init__(self, spec):
        self.spec = spec if isinstance(spec, dict) else {}
        self.id = self.spec.get("id")
        self.cursors = {}

    def _item(self, fieldname, seam):
        ctx = CURRENT["ctx"]
        args = norm_args(proj(seam, (), {"id": self.id}))
        entry = self.spec.get(fieldname, _MISSING)
        if entry is _MISSING:
            spec = {"__raise__": "RuntimeError", "message": "no script for " + seam}
            r = Raise(spec, build_exc(spec))
            ctx.script.handed.append(r)
            ctx.trace.append({"ev": seam, "args": args, "ret": spec})
            raise r.exc
        if isinstance(entry, list):
            idx = self.cursors.get(fieldname, 0)
            if idx >= len(entry):
                spec = {"__raise__": "RuntimeError", "message": "no script for " + seam}
                r = Raise(spec, build_exc(spec))
                ctx.script.handed.append(r)
                ctx.trace.append({"ev": seam, "args": args, "ret": spec})
                raise r.exc
            self.cursors[fieldname] = idx + 1
            val = entry[idx]
        else:
            val = entry
        raw = _unwrap_script(val)
        if isinstance(raw, Raise):
            ctx.script.handed.append(raw)
            ctx.trace.append({"ev": seam, "args": args, "ret": raw.spec})
            raise raw.exc
        ctx.trace.append({"ev": seam, "args": args, "ret": norm_value(raw)})
        return raw

    def done(self):
        return self._item("done", "future.done")

    def result(self):
        return self._item("result", "future.result")

    def cancelled(self):
        return self._item("cancelled", "future.cancelled")

    # Deterministic set iteration: the live `pending = set(futures)` iterates
    # in hash order; stable small-int hashes (id "fN" -> N) make CPython's
    # open-addressing table iterate in ascending id order, which the C port
    # models as children order. Domain: ids are "fN", N = child position, <8.
    def __hash__(self):
        try:
            return int(str(self.id)[1:])
        except (ValueError, TypeError):
            return id(self)

    def __eq__(self, other):
        return isinstance(other, FutureDouble) and self.id == other.id


class ExecutorDouble:
    pass


class GateDouble(SimpleNamespace):
    pass


class PrefixCacheGateDouble:
    for_children = staticmethod(_stub_fn("gate.for_children"))


def _lookup_future(fid):
    fut = CURRENT["futures"].get(fid)
    if fut is None:
        raise RuntimeError("unknown future id %s" % (fid,))
    return fut


def materialize_result(name, raw):
    if name == "executor.new":
        ex = ExecutorDouble()
        _inject(ex, submit=CURRENT["rec"]("executor.submit"),
                shutdown=CURRENT["rec"]("executor.shutdown"))
        return ex
    if name == "gate.for_children":
        if raw is None:
            return None
        gate = GateDouble()
        _inject(gate, run=CURRENT["rec"]("gate.run"))
        return gate
    if name == "executor.submit":
        fut = FutureDouble(raw)
        CURRENT["futures"][fut.id] = fut
        return fut
    if name == "cf_wait":
        done_ids, pending_ids = raw
        return ([_lookup_future(i) for i in done_ids],
                [_lookup_future(i) for i in pending_ids])
    if isinstance(raw, dict) and ("__attr__" in raw or "__callable__" in raw):
        return materialize_double(raw)
    return raw


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


def make_quiet():
    """Mirror of the live _quiet contextmanager (delegate_tool_progress): any
    Exception is swallowed; a non-None log_message is logged at debug with the
    exception filling a trailing unsatisfied %s. The logger.debug call rides
    the universal seam on BOTH sides (C models the same swallow + log)."""
    @contextmanager
    def _quiet(log_message, *log_args, exc_info=False):
        try:
            yield
        except Exception as exc:
            if log_message is None:
                return
            if log_message.count("%") > len(log_args):
                log_args = log_args + (exc,)
            CURRENT["rec"]("logger.debug")(log_message, *log_args, exc_info=exc_info)
    return _quiet


# ---- live namespace seeding ----
def _exec_seg(src, node, ns, path):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _exec_class_seg(src, node, ns, path):
    # ast.get_source_segment EXCLUDES decorators; rebuild from the first one.
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno if node.decorator_list else node.lineno) - 1
    seg = "\n".join(lines[start:node.end_lineno])
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


CONST_ASSIGNS = ["_SYNC_FALLBACK_NOTES", "_BACKGROUND_NOTES", "SUBAGENT_FAILURE_STATUSES"]


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


def load_ns():
    global NS
    mod = types.ModuleType("td_live")
    sys.modules["td_live"] = mod
    ns = mod.__dict__
    NS = ns
    import contextvars
    from concurrent.futures import FIRST_COMPLETED
    ns.update({
        "__name__": "td_live", "json": json, "contextvars": contextvars,
        "dataclass": dataclass, "field": field, "replace": replace, "partial": partial,
        "Any": Any, "Optional": Optional, "Callable": Callable, "Dict": dict, "List": list,
        "FIRST_COMPLETED": FIRST_COMPLETED,
        "_cf_wait": _stub_fn("cf_wait"),
        # byte-pinned from tools.delegate_tool_progress (module-top import)
        "SUBAGENT_FAILURE_STATUSES": frozenset({"failed", "error", "timeout"}),
        "logger": LoggerDouble(), "time": TimeDouble(),
        "_quiet": make_quiet(),
        # module-top foreign seams
        "begin_delegation_run": _stub_fn("begin_delegation_run"),
        "finish_delegation_unit": _stub_fn("finish_delegation_unit"),
        "_new_delegation_id": _stub_fn("new_delegation_id"),
        "record_unit_child": _stub_fn("record_unit_child"),
        "_attach_child": _stub_fn("attach_child"),
        "_close_child": _stub_fn("close_child"),
        "_detach_child": _stub_fn("detach_child"),
        "_fabricated_entry": _stub_fn("fabricated_entry"),
        "_signal_child_stop": _stub_fn("signal_child_stop"),
        "_get_max_async_children": _stub_fn("get_max_async_children"),
        "_print_completion_line": _stub_fn("print_completion_line"),
        "describe_subagent_failure": _stub_fn("describe_subagent_failure"),
        "format_batch_tag": _stub_fn("format_batch_tag"),
        "_capture_gateway_steer_authority": _stub_fn("capture_gateway_steer_authority"),
        "_finalize_child_results": _stub_fn("finalize_child_results"),
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    for n in tree.body:
        if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id in CONST_ASSIGNS for t in n.targets):
            _exec_seg(src, n, ns, SRC)
        if isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) and n.target.id in CONST_ASSIGNS:
            _exec_seg(src, n, ns, SRC)
    # the _Batch dataclass (WITH decorator) and the 19 ported defs
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == "_Batch":
            _exec_class_seg(src, n, ns, SRC)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in PY_FUNCS:
            _exec_seg(src, n, ns, SRC)
    # in-function import stubs (recorders resolve CURRENT["rec"] at call time)
    _install_stub("tools.delegate_tool", {
        "_run_single_child": _stub_fn("run_single_child"),
        "session_joins_delegation": _stub_fn("session_joins_delegation")})
    _install_stub("tools.async_delegation", {
        "_current_origin_session_id": _stub_fn("current_origin_session_id"),
        "push_task_failure_notice": _stub_fn("push_task_failure_notice"),
        "dispatch_async_delegation_batch": _stub_fn("dispatch_async"),
        "_new_delegation_id": _stub_fn("new_delegation_id"),
        "record_unit_child": _stub_fn("record_unit_child")})
    _install_stub("gateway.session_context", {
        "get_session_env": _stub_fn("get_session_env"),
        "session_history_delivery_supported": _stub_fn("session_history_delivery_supported"),
        "async_delivery_supported": _stub_fn("async_delivery_supported")})
    _install_stub("gateway.session_finite", {"finite_turn_required": _stub_fn("finite_turn_required")})
    _install_stub("tools.delegate_tool_auto_resume", {"waiting_on_resume": _stub_fn("waiting_on_resume")})
    _install_stub("tools.delegate_tool_resumable", {"release_unstarted": _stub_fn("release_unstarted")})
    _install_stub("tools.daemon_pool", {"DaemonThreadPoolExecutor": _stub_fn("executor.new")})
    _install_stub("tools.delegate_tool_config", {
        "_get_prefix_stagger_seconds": _stub_fn("get_prefix_stagger_seconds"),
        "_get_independent_completions": _stub_fn("get_independent_completions"),
        "_get_max_async_children": _stub_fn("get_max_async_children")})
    _install_stub("tools.delegate_tool_prefix_gate", {"PrefixCacheGate": PrefixCacheGateDouble})
    _install_stub("tools.delegation_live_log", {"update_manifest_statuses": _stub_fn("update_manifest_statuses")})
    _install_stub("tools.process_registry_notifications", {"_process_accounting_lines": _stub_fn("process_accounting_lines")})
    _install_stub("tools.approval_context", {"get_current_session_key": _stub_fn("get_current_session_key")})
    _install_stub("tools.registry", {"tool_error": _stub_fn("tool_error")})
    return ns


def _state(inp, key):
    st = inp.get("state") or {}
    return copy.deepcopy(st.get(key))


def _mat_state(inp, key):
    return materialize_double(_state(inp, key))


def _make_batch(j):
    mat = materialize_double(copy.deepcopy(j))
    return NS["_Batch"](**mat)


def _win(**kw):
    CURRENT["stwin"] = kw
    return kw


def _bind_spinner(spinner):
    if isinstance(spinner, SimpleNamespace):
        _inject(spinner, update_text=CURRENT["rec"]("spinner.update_text"))


def _bind_writers(writers):
    if isinstance(writers, list):
        for i, w in enumerate(writers):
            if isinstance(w, SimpleNamespace):
                _inject(w, finalize=(lambda idx: lambda entry: CURRENT["rec"]("writer.finalize")(entry, index=idx))(i))


def _bind_progress_children(children):
    if isinstance(children, list):
        for i, c in enumerate(children):
            if isinstance(c, SimpleNamespace):
                _inject(c, get_activity_summary=(lambda idx: lambda: CURRENT["rec"]("child.get_activity_summary")(index=idx))(i))


def drive(func, inp):
    CURRENT["ids"] = {}
    CURRENT["futures"] = {}
    CURRENT["stwin"] = {}
    if func == "_Batch.owner_kwargs":
        batch = _make_batch(_state(inp, "batch"))
        _win(batch=batch)
        ret = batch.owner_kwargs()
        return ret, CURRENT["stwin"]
    if func == "_Batch.run_child":
        batch = _make_batch(_state(inp, "batch"))
        child = materialize_double(copy.deepcopy(inp.get("child")))
        _win(batch=batch, child=child)
        ret = batch.run_child(inp.get("i", 0), inp.get("task") or {}, child)
        return ret, CURRENT["stwin"]
    if func == "_announce_batch":
        agent = _mat_state(inp, "parent_agent")
        _win(parent_agent=agent)
        ret = NS[func](agent, inp.get("n_tasks", 0), inp.get("live_deleg_id"))
        return ret, CURRENT["stwin"]
    if func == "_capture_origin":
        _win()
        ret = NS[func]()
        return ret, CURRENT["stwin"]
    if func == "_report_child_done":
        agent = _mat_state(inp, "parent_agent")
        spinner = materialize_double(copy.deepcopy(inp.get("spinner")))
        _bind_spinner(spinner)
        _win(parent_agent=agent, spinner=spinner)
        ret = NS[func](agent, spinner, copy.deepcopy(inp.get("entry") or {}),
                       inp.get("tag", ""), inp.get("task_labels") or [],
                       inp.get("n_tasks", 0), inp.get("remaining", 0))
        return ret, CURRENT["stwin"]
    if func == "_record_finished_child":
        batch = _make_batch(_state(inp, "batch"))
        _win(batch=batch)
        ret = NS[func](batch, copy.deepcopy(inp.get("entry")), bool(inp.get("honor_parent_interrupt")))
        return ret, CURRENT["stwin"]
    if func == "_waiting_on_resume":
        _win()
        ret = NS[func](copy.deepcopy(inp.get("entry") or {}))
        return ret, CURRENT["stwin"]
    if func == "_release_unstarted_child":
        batch = _make_batch(_state(inp, "batch"))
        child = materialize_double(copy.deepcopy(inp.get("child")))
        _win(batch=batch, child=child)
        ret = NS[func](batch, child)
        return ret, CURRENT["stwin"]
    if func == "_run_gated":
        batch = _make_batch(_state(inp, "batch"))
        gate = materialize_double(copy.deepcopy(inp.get("gate")))
        if isinstance(gate, SimpleNamespace):
            _inject(gate, run=CURRENT["rec"]("gate.run"))
        child = materialize_double(copy.deepcopy(inp.get("child")))
        _win(batch=batch, gate=gate, child=child)
        ret = NS[func](batch, gate, bool(inp.get("honor_parent_interrupt")),
                       inp.get("i", 0), inp.get("task") or {}, child)
        return ret, CURRENT["stwin"]
    if func == "_run_children_parallel":
        batch = _make_batch(_state(inp, "batch"))
        results = _state(inp, "results")
        if results is None:
            results = []
        _win(batch=batch, results=results)
        ret = NS[func](batch, results, honor_parent_interrupt=bool(inp.get("honor_parent_interrupt")))
        return ret, CURRENT["stwin"]
    if func == "_execute_and_aggregate":
        batch = _make_batch(_state(inp, "batch"))
        _bind_writers(batch.live_writers)
        _win(batch=batch)
        ret = NS[func](batch, honor_parent_interrupt=bool(inp.get("honor_parent_interrupt", True)))
        return ret, CURRENT["stwin"]
    if func == "_run_sync_with_note":
        batch = _make_batch(_state(inp, "batch"))
        _bind_writers(batch.live_writers)
        _win(batch=batch)
        ret = NS[func](batch, inp.get("reason", "no_async"))
        return ret, CURRENT["stwin"]
    if func == "_resolve_async_wake_sid":
        _win()
        ret = NS[func](inp.get("origin_wake_sid", ""),
                       origin_session_history_delivery=bool(inp.get("origin_session_history_delivery")),
                       oneshot_session=bool(inp.get("oneshot_session")))
        return ret, CURRENT["stwin"]
    if func == "_resolve_async_session_key":
        agent = _mat_state(inp, "parent_agent")
        _win(parent_agent=agent)
        ret = NS[func](agent, inp.get("origin_ui_session_id", ""))
        return ret, CURRENT["stwin"]
    if func == "_batch_progress_token":
        children = materialize_double(copy.deepcopy(inp.get("child_agents") or []))
        _bind_progress_children(children)
        _win(child_agents=children)
        ret = NS[func](children)
        return ret, CURRENT["stwin"]
    if func == "_dispatched_payload":
        batch = _make_batch(_state(inp, "batch"))
        units = [(_make_batch(u), uid) for (u, uid) in (inp.get("dispatched") or [])]
        _win(batch=batch)
        ret = NS[func](batch, units)
        return ret, CURRENT["stwin"]
    if func == "_units_of":
        batch = _make_batch(_state(inp, "batch"))
        _win(batch=batch)
        ret = NS[func](batch)
        return ret, CURRENT["stwin"]
    if func == "_dispatch_unit":
        unit = _make_batch(_state(inp, "unit"))
        _win(unit=unit)
        ret = NS[func](unit, inp.get("unit_id"), inp.get("slot_key"),
                       copy.deepcopy(inp.get("routing") or {}))
        return ret, CURRENT["stwin"]
    if func == "_restore_parent_cancellation":
        unit = _make_batch(_state(inp, "unit"))
        _win(unit=unit)
        ret = NS[func](unit)
        return ret, CURRENT["stwin"]
    if func == "_dispatch_background":
        batch = _make_batch(_state(inp, "batch"))
        _bind_writers(batch.live_writers)
        _win(batch=batch)
        ret = NS[func](batch)
        return ret, CURRENT["stwin"]
    if func == "_refuse_detached_only":
        batch = _make_batch(_state(inp, "batch"))
        _win(batch=batch)
        ret = NS[func](batch, inp.get("why", ""))
        return ret, CURRENT["stwin"]
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
    print("ORACLE tool_dispatch_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())