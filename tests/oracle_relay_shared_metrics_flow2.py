#!/usr/bin/env python3
"""Oracle for src/cli/relay_shared_metrics_flow2.c — drives the LIVE
hermes_cli/observability/relay_shared_metrics.py UNCLAIMED-remainder FLOW + leaf
defs (29 functions) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted (module
functions + _Runtime methods, decorators stripped); the _DISPATCHED_SOURCES
constant is seeded; claimed siblings / foreign I/O (relay_runtime, contract,
the Relay host/subscriber/thread/clock, logger, enabled/_get_runtime, the
_RUNTIME registry gates, atexit) are recording seams fed the SAME scripts the C
harness feeds. Ported siblings stay REAL so composition is exercised
(_resolved_send_config -> _raw_config, _emit_rows -> _guarded,
_release -> _join_send_thread, the wrappers -> _process_marks_admitted, and the
_text leaf everywhere).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. A scripted BaseException (SystemExit / KeyboardInterrupt /
GeneratorExit / CancelledError) propagates past every `except Exception` guard
exactly as live. Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed. The state window is
projected from carriers materialized BEFORE the call, so a raise leaves the
mutation history at that point (matching the C in-place st).

Value model: attribute doubles (session/task/turn/active_turn/runtime) are
SimpleNamespace / RuntimeDouble projected back to their raw dict shape;
identity (`is`, id()-dedup) is modelled by a "ref" marker (equal ref == same
object); callable doubles {"__callable__": "<seam>"} fire that seam with
{args, kwargs}; tuple/set -> array (sets sorted); integral floats -> ints.

Domain (ASCII scope, documented in the port header): str()/strip()/lower()
operands are ASCII; _text/_sole/_forget operands are scalars or ref-marked
doubles; object seam results are null or a NON-EMPTY record; attribute doubles
carry the fields the live body reads. Lazy imports always resolve to stub
modules (no import-failure fixtures); a scripted seam raise covers real-exc.

Summary: 'ORACLE relay_shared_metrics_flow2: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import contextlib
import copy
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "observability" / "relay_shared_metrics.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "observability" / "relay_shared_metrics.py")
NAME = "relay_shared_metrics_flow2"

PORTED_MODULE = [
    "_text", "_elapsed_ms", "_scope_handle", "_sole", "_forget",
    "_task_parent_handle", "_with_launch_entrypoint", "_process_marks_admitted",
    "_raw_config", "_resolved_send_config", "_close_child_session",
    "_run_task_hook", "record_process_mark", "record_process_marks_saved",
    "close_session_run", "rotate_segment", "shutdown_runtimes",
]
PORTED_METHODS = [
    "_cold_key", "_guarded", "_with_scope_stack", "_emit_rows", "_any_session",
    "_task_pair", "_abort_session", "start_user_turn", "_deregister",
    "_join_send_thread", "_release", "_lineage_spent",
]
PORTED = PORTED_MODULE + PORTED_METHODS
CONSTANTS = ["_DISPATCHED_SOURCES"]

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
METHOD_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in PORTED_MODULE:
        DEF_CODE[_node.name] = _compile_node(_node)
    elif isinstance(_node, ast.ClassDef) and _node.name == "_Runtime":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name in PORTED_METHODS:
                _s = copy.copy(_sub)
                _s.decorator_list = []
                METHOD_CODE[_sub.name] = _compile_node(_s)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name) and _tg.id in CONSTANTS:
            CONST_CODE[_tg.id] = _compile_node(_node)
_missing = (set(PORTED_MODULE) - set(DEF_CODE)) | (set(PORTED_METHODS) - set(METHOD_CODE)) \
    | (set(CONSTANTS) - set(CONST_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


class _RuntimeBase:
    pass


class RuntimeDouble(_RuntimeBase):
    """isinstance(x, _Runtime) is True; arbitrary methods project to rt.<name>."""
    def __init__(self, d):
        self.__dict__["__runtime__"] = True
        self.__dict__["id"] = d.get("id", "")
        self.__dict__["_safe"] = _make_safe_recorder(d.get("id", ""))

    def __getattr__(self, name):
        return _make_callable_recorder("rt." + name)


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, (RuntimeDouble, SimpleNamespace)):
        return {k: norm_value(x) for k, x in vars(v).items()
                if not callable(x) and not hasattr(x, "__enter__")}
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm_value(x) for x in v), key=lambda s: str(s))
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj_callable(v):
    nm = getattr(v, "__seam_name__", None)
    if nm is not None:
        return {"__callable__": nm}
    return norm_value(v)


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("message", marker.get("msg", ""))
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {"TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
               "AttributeError": AttributeError, "KeyError": KeyError, "Exception": Exception,
               "TimeoutError": TimeoutError, "OSError": OSError, "LookupError": LookupError,
               "InterruptedError": InterruptedError, "FileNotFoundError": FileNotFoundError,
               "IndexError": IndexError, "StopIteration": StopIteration}
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
    return cls(msg)


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_MISSING = object()
CURRENT = {"ctx": None}


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


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
        return self._unwrap(val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, make_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, make_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def proj(name, a, k):
    if name in ("monotonic_ns", "is_single_query_session", "get_conversation_context",
                "relay_runtime.current_turn", "read_raw_config_readonly",
                "relay.get_scope_stack", "thread.is_alive", "enabled",
                "relay_runtime.current_profile_key"):
        return {}
    if name == "relay_runtime.active_turn":
        return {"session_id": a[0] if a else None}
    if name == "contract.execution_surface":
        return {"event": norm_value(a[0])}
    if name == "get_session_env":
        return {"name": a[0], "default": a[1] if len(a) > 1 else None}
    if name == "resolve_send_config":
        return {"config": norm_value(a[0])}
    if name in ("logger.warning", "logger.debug"):
        d = {"msg": a[0], "args": list(a[1:])}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "rt._task_session":
        d = {"event": norm_value(a[0])}
        d.update({kk: norm_value(vv) for kk, vv in k.items()})
        return d
    if name in ("rt._session", "rt.start_task", "rt.close_session"):
        return {"event": norm_value(a[0])}
    if name == "rt._finish_task":
        return {"session": norm_value(a[0]), "task_id": a[1], "event": norm_value(a[2])}
    if name == "rt._end_pending_model_calls":
        return {"session": norm_value(a[0]), "event": norm_value(a[1])}
    if name == "host.release_managed_execution":
        return {"name": a[0]}
    if name == "thread.join":
        return {"timeout": a[0] if a else k.get("timeout")}
    if name == "atexit.unregister":
        return {"callback": proj_callable(a[0])}
    if name == "_get_runtime":
        return {"retry_failed": k.get("retry_failed", False)}
    if name in ("cb", "rt.record_process_mark", "rt._mark"):
        return {"args": norm_value(list(a)), "kwargs": norm_value(k)}
    return {**{("arg%d" % i): norm_value(v) for i, v in enumerate(a)},
            **{kk: norm_value(vv) for kk, vv in k.items()}}


def record(name, argsdict):
    ctx = CURRENT["ctx"]
    args = norm_args(argsdict)
    raw = ctx.script.next(name)
    if isinstance(raw, Raise):
        ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
        raise raw.exc
    ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
    return materialize_result(name, raw)


def _make_callable_recorder(name):
    def f(*a, **k):
        return record(name, {"args": list(a), "kwargs": k})
    f.__seam_name__ = name
    return f


def _make_safe_recorder(rid):
    def f(callback, *a, **k):
        return record("rt._safe", {"callback": proj_callable(callback),
                                   "args": list(a), "kwargs": k, "runtime_id": rid})
    return f


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        return record(self.name, proj(self.name, a, k))


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, a, k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, a, k)


# ── attribute-double materializers (preserve raw dict shape on projection) ──
def mk_active_turn(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    if isinstance(d.get("lease"), dict):
        ns.lease = SimpleNamespace(**d["lease"])
    return ns


def mk_turn(d):
    return SimpleNamespace(**d) if isinstance(d, dict) else d


def mk_task(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    if isinstance(d.get("cost"), dict):
        ns.cost = SimpleNamespace(**d["cost"])
    return ns


def mk_session(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.lock = contextlib.nullcontext()
    if isinstance(d.get("relay_session"), dict):
        ns.relay_session = SimpleNamespace(**d["relay_session"])
    return ns


def mk_runtime(d):
    return RuntimeDouble(d) if isinstance(d, dict) else d


ATTR_SEAMS = {
    "relay_runtime.active_turn": mk_active_turn,
    "relay_runtime.current_turn": mk_turn,
    "rt.start_task": mk_task,
    "rt._task_session": mk_session,
    "rt._session": mk_session,
    "_get_runtime": mk_runtime,
}


def materialize_result(name, raw):
    if raw is None:
        return None
    if isinstance(raw, dict) and "__callable__" in raw:
        return _make_callable_recorder(raw["__callable__"])
    if name in ATTR_SEAMS and isinstance(raw, dict):
        return ATTR_SEAMS[name](raw)
    return raw


def materialize_double(j):
    if isinstance(j, dict) and "__callable__" in j:
        return _make_callable_recorder(j["__callable__"])
    return j


# ── live namespace ──────────────────────────────────────────────────────────
_MOD = types.ModuleType("rsm2_live")
sys.modules["rsm2_live"] = _MOD
NS = _MOD.__dict__
NS.update({
    "__name__": "rsm2_live", "__builtins__": builtins,
    "contextlib": contextlib, "Any": Any, "Callable": Callable,
    "logger": LoggerDouble(),
    "_Runtime": _RuntimeBase,
    "_RUNTIME_LOCK": contextlib.nullcontext(),
    "monotonic_ns": SeamFn("monotonic_ns"),
    "get_conversation_context": SeamFn("get_conversation_context"),
    "enabled": SeamFn("enabled"),
    "_get_runtime": SeamFn("_get_runtime"),
    "relay_runtime": SimpleNamespace(
        active_turn=SeamFn("relay_runtime.active_turn"),
        current_turn=SeamFn("relay_runtime.current_turn"),
        current_profile_key=SeamFn("relay_runtime.current_profile_key"),
    ),
    "contract": SimpleNamespace(execution_surface=SeamFn("contract.execution_surface")),
    "atexit": SimpleNamespace(unregister=SeamFn("atexit.unregister")),
})
for _nm in CONSTANTS:
    exec(CONST_CODE[_nm], NS)
for _nm in PORTED_MODULE:
    exec(DEF_CODE[_nm], NS)
for _nm in PORTED_METHODS:
    exec(METHOD_CODE[_nm], NS)
NS["logger"] = LoggerDouble()


def _install_import_stubs():
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    hc = mod("hermes_cli"); hc.__path__ = []
    cfg = mod("hermes_cli.config", read_raw_config_readonly=SeamFn("read_raw_config_readonly"))
    hc.config = cfg
    obs = mod("hermes_cli.observability"); obs.__path__ = []
    hc.observability = obs
    sc = mod("hermes_cli.observability.shared_metrics_send_config",
             resolve_send_config=SeamFn("resolve_send_config"))
    obs.shared_metrics_send_config = sc
    ag = mod("agent"); ag.__path__ = []
    of = mod("agent.oneshot_footprint", is_single_query_session=SeamFn("is_single_query_session"))
    ag.oneshot_footprint = of
    gw = mod("gateway"); gw.__path__ = []
    sctx = mod("gateway.session_context", get_session_env=SeamFn("get_session_env"))
    gw.session_context = sctx


_install_import_stubs()


# ── per-case context ────────────────────────────────────────────────────────
class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))


def _registry(inp):
    reg = {}
    for k, v in (inp.get("state", {}).get("_RUNTIMES") or {}).items():
        if isinstance(v, dict) and v.get("__runtime__"):
            reg[k] = RuntimeDouble(v)
        else:
            reg[k] = object()   # _RUNTIME_FAILED marker: isinstance False
    return reg


def _self_thread(st):
    tj = st.get("_send_thread")
    if not isinstance(tj, dict):
        return None
    return SimpleNamespace(is_alive=SeamFn("thread.is_alive"), join=SeamFn("thread.join"))


def drive(func, inp, holder):
    st_in = inp.get("state") or {}
    holder["proj"] = lambda: norm_value(st_in)
    ev = inp.get("event") or {}

    if func == "_text":
        return NS["_text"](inp.get("event") or {}, inp.get("key", ""))
    if func == "_elapsed_ms":
        return NS["_elapsed_ms"](inp.get("started_ns", 0))
    if func == "_scope_handle":
        sess = mk_session(inp.get("session")) if inp.get("session") is not None else None
        task = mk_task(inp.get("task")) if inp.get("task") is not None else None
        return NS["_scope_handle"](sess, task)
    if func == "_sole":
        items, seen = [], {}
        for it in (inp.get("items") or []):
            r = it.get("ref") if isinstance(it, dict) else None
            if r is not None and r in seen:
                items.append(seen[r])
            else:
                o = mk_session(it) if isinstance(it, dict) else it
                items.append(o)
                if r is not None:
                    seen[r] = o
        return NS["_sole"](items)
    if func == "_forget":
        owner = mk_session(inp.get("owner")) if isinstance(inp.get("owner"), dict) else inp.get("owner")
        oref = getattr(owner, "ref", None)
        index = {}
        for k, v in (st_in.get("index") or {}).items():
            if isinstance(v, dict) and oref is not None and v.get("ref") == oref:
                index[k] = owner
            else:
                index[k] = mk_session(v) if isinstance(v, dict) else v
        holder["proj"] = lambda: {"index": norm_value(index)}
        NS["_forget"](index, inp.get("key", ""), owner)
        return None
    if func == "_task_parent_handle":
        sess = mk_session(inp.get("session")) if inp.get("session") is not None else None
        return NS["_task_parent_handle"](sess, inp.get("task_id", ""))
    if func == "_with_launch_entrypoint":
        return NS["_with_launch_entrypoint"](inp.get("event") or {})
    if func == "_cold_key":
        return NS["_cold_key"](inp.get("session_id", ""))
    if func == "_process_marks_admitted":
        return NS["_process_marks_admitted"]()
    if func == "_raw_config":
        return NS["_raw_config"]()
    if func == "_resolved_send_config":
        return NS["_resolved_send_config"]()
    if func == "_guarded":
        cb = materialize_double(inp.get("callback"))
        return NS["_guarded"](inp.get("message", ""), cb,
                              *(inp.get("args") or []), **(inp.get("kwargs") or {}))
    if func == "_with_scope_stack":
        slf = SimpleNamespace(relay=SimpleNamespace(get_scope_stack=SeamFn("relay.get_scope_stack")))
        cb = materialize_double(inp.get("callback"))
        return NS["_with_scope_stack"](slf, cb, *(inp.get("args") or []), **(inp.get("kwargs") or {}))
    if func == "_emit_rows":
        slf = SimpleNamespace(_guarded=NS["_guarded"],
                              record_process_mark=_make_callable_recorder("rt.record_process_mark"),
                              _mark=_make_callable_recorder("rt._mark"))
        sess = mk_session(inp.get("session")) if inp.get("session") is not None else None
        rows = [tuple(r) for r in (inp.get("rows") or [])]
        NS["_emit_rows"](slf, sess, rows)
        return None
    if func == "_any_session":
        slf = SimpleNamespace(_task_session=SeamFn("rt._task_session"), _session=SeamFn("rt._session"))
        return NS["_any_session"](slf, ev)
    if func == "_task_pair":
        slf = SimpleNamespace(_task_session=SeamFn("rt._task_session"), start_task=SeamFn("rt.start_task"))
        r = NS["_task_pair"](slf, ev, start=inp.get("start", False), **(inp.get("lookup") or {}))
        return r
    if func == "_abort_session":
        session = mk_session(copy.deepcopy(st_in.get("session") or {}))
        holder["proj"] = lambda: {"session": norm_value(session)}
        slf = SimpleNamespace(_finish_task=SeamFn("rt._finish_task"),
                              _end_pending_model_calls=SeamFn("rt._end_pending_model_calls"))
        return NS["_abort_session"](slf, session, inp.get("base_event") or {})
    if func == "start_user_turn":
        task_box = {}

        def start_task_rec(event):
            t = record("rt.start_task", proj("rt.start_task", (event,), {}))
            task_box["t"] = t
            return t

        slf = SimpleNamespace(start_task=start_task_rec)
        NS["start_user_turn"](slf, ev)
        if task_box.get("t") is not None:
            holder["proj"] = lambda: {"task": norm_value(task_box["t"])}
        return None
    if func == "_deregister":
        slf = SimpleNamespace(**copy.deepcopy(st_in))
        slf._safe = _make_safe_recorder(st_in.get("id", ""))
        slf.relay = SimpleNamespace(subscribers=SimpleNamespace(
            deregister=_make_callable_recorder("relay.subscribers.deregister")))
        slf.host = SimpleNamespace(release_managed_execution=SeamFn("host.release_managed_execution"))
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_deregister"](slf)
        return None
    if func == "_join_send_thread":
        slf = SimpleNamespace(_send_lock=contextlib.nullcontext(), _send_thread=_self_thread(st_in))
        NS["_join_send_thread"](slf, timeout=inp.get("timeout", 2.0))
        return None
    if func == "_release":
        slf = SimpleNamespace(_send_lock=contextlib.nullcontext(), _send_thread=_self_thread(st_in),
                              shutdown=_make_callable_recorder("rt.shutdown"))
        slf._join_send_thread = types.MethodType(NS["_join_send_thread"], slf)
        NS["_release"](slf)
        return None
    if func == "_lineage_spent":
        lineages = {k: (SimpleNamespace(**v) if isinstance(v, dict) else v)
                    for k, v in (st_in.get("_lineages") or {}).items()}
        slf = SimpleNamespace(_lineage_lock=contextlib.nullcontext(),
                              _lineages=lineages,
                              _lineage_of=copy.deepcopy(st_in.get("_lineage_of") or {}))
        return NS["_lineage_spent"](slf, inp.get("session_id", ""))
    if func == "_close_child_session":
        rt = SimpleNamespace(close_session=SeamFn("rt.close_session"))
        NS["_close_child_session"](rt, inp.get("kwargs") or {})
        return None
    if func == "_run_task_hook":
        return NS["_run_task_hook"](inp.get("method", ""), retry_failed=inp.get("retry_failed", False),
                                    **(inp.get("event") or {}))
    if func == "record_process_mark":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["record_process_mark"](inp.get("mark"), inp.get("data") or {})
    if func == "record_process_marks_saved":
        NS["_RUNTIMES"] = _registry(inp)
        marks = [tuple(m) for m in (inp.get("marks") or [])]
        return NS["record_process_marks_saved"](marks)
    if func == "close_session_run":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["close_session_run"](inp.get("session_id", ""))
    if func == "rotate_segment":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["rotate_segment"](inp.get("old_session_id", ""), inp.get("new_session_id", ""))
    if func == "shutdown_runtimes":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["shutdown_runtimes"]()
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    holder = {}
    try:
        ret = drive(func, inp, holder)
        st = holder["proj"]() if "proj" in holder else norm_value(inp.get("state") or {})
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": st}
    except BaseException as e:  # noqa: BLE001 - fail-closed per case
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        st = holder["proj"]() if "proj" in holder else {}
        return {"ret": spec, "trace": ctx.trace, "st": st}


def main():
    n = mism = 0
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
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:700]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:700]))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


# ── self-test: drive every fixture, flag suspicious raises / drive failures ──
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    bad = 0
    names = set(PORTED)
    funcs_seen = set()
    for i, rec in enumerate(fixtures):
        func = rec.get("func")
        funcs_seen.add(func)
        if func not in names:
            bad += 1
            print(f"SELFTEST case {i}: func {func} not in PORTED")
            continue
        try:
            out = run_case(func, rec.get("in") or {})
            spec = out["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("message", "")
                if any(("'" + n + "'") in msg for n in names) or "SimpleNamespace" in msg:
                    bad += 1
                    print(f"SELFTEST case {i} {func}: suspicious AttributeError {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {func}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    missing = names - funcs_seen
    if missing:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing)}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "relay_shared_metrics_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())