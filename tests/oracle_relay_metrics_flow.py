#!/usr/bin/env python3
"""Oracle for src/cli/relay_metrics_flow.c — drives the LIVE
hermes_cli/observability/relay_shared_metrics.py UNCLAIMED third-wave FLOW +
leaf defs (25 functions) under IDENTICAL scripted universal seams. (The task
brief named "relay_metrics.py"; no such quarry file exists — the relay-metrics
module IS relay_shared_metrics.py, and every PoP in the port names it.)

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted (module
functions + _Runtime methods, decorators stripped; _owner_scope re-wrapped in
contextlib.contextmanager); _SNAPSHOT_RECHECK_NS and the contract mark
constants / WASTE_REASONS are seeded from the quarry ASTs. Claimed siblings /
foreign I/O (relay_runtime, the Relay host/subscriber/store/thread/clock,
logger, enabled/_get_runtime/_raw_config/_resolved_send_config/_scope_handle/
_absorb_peak, the _RUNTIMES registry gates, hermes_constants home overrides,
pathlib exists, the lazy-import constructors) are recording seams fed the SAME
scripts the C harness feeds. Ported siblings stay REAL so composition is
exercised (_emit_install_snapshot_if_due -> _mark -> _run_scoped;
_run_flush_worker -> _flush_and_export -> _owner_scope; _close_and_export ->
_flush_and_export; _close_unseen_segment -> _close_segment + _emit_summary_rows;
_flush_lineage -> _emit_summary_rows; record_switch_after -> _with_route_run;
_reconcile_send_consent_once -> _reconcile_store_consent).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. A scripted BaseException (SystemExit / KeyboardInterrupt /
GeneratorExit / CancelledError) propagates past every `except Exception` guard
exactly as live. Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed. The state window is
projected from carriers materialized BEFORE the call, so a raise leaves the
mutation history at that point (matching the C in-place st).

Value model: attribute doubles (session/task/lineage/thread/store/resolved/
efficiency/run/peak/tally/tools) are SimpleNamespace projected back to their raw
dict shape (callables / context managers excluded); sets -> sorted arrays;
tuple -> array; integral floats -> ints; callable doubles {"__callable__":
"<seam>"} fire that seam with {args, kwargs}; CM doubles {"__cm__": "<name>"}
fire <name>.__enter__ / <name>.__exit__ ({exc: kind-or-null}), a truthy exit
result suppresses a body raise and an exit raise overrides it. The
still_consented closure of _run_send_pass projects to {"__callable__":
"still_consented"} (the scripted sender double never invokes it).

Domain (ASCII scope, documented in the port header): object seam results are
null or a record; attribute doubles carry the fields the live body reads;
state models per func: _emit_install_snapshot_if_due {_snapshot_checked_ns},
_schedule_flush_and_export / _run_flush_worker {_flush_pending,_flush_thread,
host?}, _close_and_export {_active,_sessions,_lineage_of,_registered,id,host},
_share_spent {session,_lineages,_lineage_of}, _close_segment /
_close_unseen_segment / _flush_lineage / record_switch_after / record_friction
{_lineages,_lineage_of}, _send_exported_packages {_send_thread,subscriber},
_run_send_pass {subscriber}, _reconcile_send_consent_once
{_consent_reconcile_done}, the registry wrappers {_RUNTIMES}. Home paths are
normalized POSIX strings (no "."/".."/"//" components); run.take results are
lists or null; tools.rows results are [mark, data] pair lists; CM factory
results are {"__cm__": name} markers or null. EXCLUDED: integral-FLOAT literals
in str() positions; non-marker CM factory dicts; discard_turns non-list results.

Summary: 'ORACLE relay_metrics_flow: N cases, M mismatches'.
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
from pathlib import Path, PurePosixPath
from types import SimpleNamespace
from typing import Any, Callable, Iterator

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
# The runner hands the oracle a scrubbed environment; derive home from __file__.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "observability" / "relay_shared_metrics.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "observability" / "relay_shared_metrics.py")
CONTRACT_SRC = str(QUARRY / "hermes_cli" / "observability" / "shared_metrics_contract.py")
NAME = "relay_metrics_flow"

PORTED_MODULE = [
    "_reconcile_store_consent", "_reconcile_send_consent_once",
    "record_session_friction", "record_session_tools",
    "record_known_cache_break", "record_model_switch_after",
]
PORTED_METHODS = [
    "_run_scoped", "_mark", "_emit_install_snapshot_if_due", "_owner_scope",
    "_flush_and_export", "_run_flush_worker", "_schedule_flush_and_export",
    "_close_and_export", "_share_spent", "_with_route_run", "_close_segment",
    "_emit_summary_rows", "_close_unseen_segment", "_flush_lineage",
    "_send_exported_packages", "_run_send_pass", "record_auxiliary_tokens",
    "record_switch_after", "record_friction",
]
PORTED = PORTED_MODULE + PORTED_METHODS
CONSTANTS = ["_SNAPSHOT_RECHECK_NS"]
CONTRACT_CONSTANTS = [
    "INSTALL_SNAPSHOT_MARK", "SESSION_MARK", "CONTEXT_PEAK_MARK",
    "MODEL_TOKENS_MARK", "MODEL_SWITCH_AFTER_MARK", "MODEL_FRICTION_MARK",
    "WASTE_REASONS",
]

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

_CTREE = ast.parse(Path(CONTRACT_SRC).read_text(encoding="utf-8"))
CCONST_CODE = {}
for _node in _CTREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name) and _tg.id in CONTRACT_CONSTANTS:
            CCONST_CODE[_tg.id] = compile(ast.Module(body=[_node], type_ignores=[]),
                                          CONTRACT_SRC, "exec", flags=_FLAGS, dont_inherit=True)
_cmissing = set(CONTRACT_CONSTANTS) - set(CCONST_CODE)
if _cmissing:
    print(f"ORACLE {NAME}: FATAL missing contract constants: {sorted(_cmissing)}", file=sys.stderr)
    sys.exit(1)
CNS = {"__builtins__": builtins}
for _nm in CONTRACT_CONSTANTS:
    exec(CCONST_CODE[_nm], CNS)

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
    if callable(v) and getattr(v, "__name__", "") == "still_consented":
        return {"__callable__": "still_consented"}
    if callable(v) and getattr(v, "__name__", "") in PORTED:
        # a ported sibling passed as a callback VALUE (e.g. _reconcile_store_consent
        # into the seamed _guarded): the C projects the live name.
        return {"__callable__": v.__name__}
    return norm_value(v)


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("message", marker.get("msg", ""))
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = None
    if cls is None:
        if kind == "CancelledError":
            import asyncio
            cls = asyncio.CancelledError
        else:
            cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    # special constructors: the scripted kind must raise the REAL class; the
    # spec round-trips via Script.handed identity, so str(e) need not equal msg.
    if kind == "UnicodeDecodeError":
        return cls("utf-8", b"x", 0, 1, msg or "boom")
    if kind == "UnicodeEncodeError":
        return cls("utf-8", "x", 0, 1, msg or "boom")
    if kind == "UnicodeTranslateError":
        return cls("x", 0, 1, msg or "boom")
    if kind in ("ExceptionGroup", "BaseExceptionGroup"):
        # BaseExceptionGroup with Exception-only contents auto-collapses to
        # ExceptionGroup live; keep the nominal kind via a base-only inner.
        inner = KeyboardInterrupt("inner") if kind == "BaseExceptionGroup" else RuntimeError("inner")
        return cls(msg or "boom", [inner])
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
    if name in ("monotonic_ns", "store.install_snapshot_due", "_raw_config",
                "rt._event_metadata", "rt._export", "relay.subscribers.flush",
                "thread.start", "thread.is_alive", "rt._deregister", "rt._release",
                "run.take", "peak.context_peak_fields", "tally.fields", "tools.rows",
                "eff.ToolUsage", "eff.SessionEfficiency", "_resolved_send_config",
                "store._connection", "get_hermes_home", "SharedMetricsStore",
                "enabled", "relay_runtime.relay_instrumentation_enabled",
                "relay_runtime.current_profile_key"):
        return {}
    if name.endswith(".__enter__"):
        return {}
    if name.endswith(".__exit__"):
        return {"exc": k.get("exc")}
    if name == "collect_install_snapshot":
        return {"config": norm_value(a[0])}
    if name == "_scope_handle":
        return {"session": norm_value(a[0]), "task": norm_value(a[1]) if len(a) > 1 else None}
    if name in ("rt._run_in_task", "host.run_in_session"):
        first = "task" if name == "rt._run_in_task" else "relay_session"
        return {first: norm_value(a[0]), "callback": proj_callable(a[1]),
                "args": norm_value(list(a[2:])), "kwargs": norm_value(k)}
    if name == "set_hermes_home_override":
        return {"profile_key": a[0] if a else None}
    if name == "reset_hermes_home_override":
        return {"token": a[0] if a else None}
    if name in ("logger.warning", "logger.debug"):
        d = {"msg": a[0], "args": list(a[1:])}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "spawn_context_thread":
        return {"callback": proj_callable(a[0]) if a else None,
                "name": k.get("name"), "args": norm_value(k.get("args"))}
    if name == "rt._lineage_spent":
        return {"session_id": a[0]}
    if name == "_absorb_peak":
        return {"into": norm_value(a[0]), "segment": norm_value(a[1])}
    if name == "tally.absorb":
        return {"tally": norm_value(a[0])}
    if name == "tools.absorb":
        return {"tools": norm_value(a[0])}
    if name == "rt._emit_rows":
        return {"session": norm_value(a[0]), "rows": norm_value(a[1])}
    if name == "efficiency.discard_turns":
        return {"signal": a[0], "turns": a[1], "route": norm_value(a[2])}
    if name == "model.friction_fields":
        return {"signal": a[0], "route": norm_value(a[1])}
    if name == "rt.record_process_mark":
        return {"mark": a[0], "data": norm_value(a[1])}
    if name == "engagement.switch_after_fields":
        return {"args": norm_value(list(a))}
    if name == "rt._session":
        return {"event": norm_value(a[0])}
    if name == "rt._guarded":
        return {"message": a[0], "callback": proj_callable(a[1]),
                "args": norm_value(list(a[2:])), "kwargs": norm_value(k)}
    if name == "rt._with_scope_stack":
        return {"callback": proj_callable(a[0]), "args": norm_value(list(a[1:])),
                "kwargs": norm_value(k)}
    if name == "fields.model_token_fields":
        # usage is POSITIONAL live: model_token_fields(event.get("usage"), ...)
        return {"usage": a[0] if a else None, "call_role": k.get("call_role"),
                "aux_task": k.get("aux_task"), "model": k.get("model"),
                "provider": k.get("provider")}
    if name == "write_txn":
        return {"connection": a[0]}
    if name == "reconcile_send_consent":
        return {"connection": a[0], "send_enabled": a[1]}
    if name == "SharedMetricsSender":
        return {"store": norm_value(a[0]), "endpoint": a[1],
                "consent_check": proj_callable(k.get("consent_check"))}
    if name == "_get_runtime":
        return {"retry_failed": k.get("retry_failed", False)}
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


class CMDouble:
    """{"__cm__": name} marker: enter/exit seams; truthy exit suppresses."""
    def __init__(self, name):
        self.name = name

    def __enter__(self):
        return record(self.name + ".__enter__", {})

    def __exit__(self, et, ev, tb):
        return record(self.name + ".__exit__", {"exc": et.__name__ if et is not None else None})


# ── attribute-double materializers (preserve raw dict shape on projection) ──
def mk_run(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.take = SeamFn("run.take")
    return ns


def mk_peak(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.context_peak_fields = SeamFn("peak.context_peak_fields")
    return ns


def mk_tally(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.fields = SeamFn("tally.fields")
    ns.absorb = SeamFn("tally.absorb")
    return ns


def mk_tools(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.rows = SeamFn("tools.rows")
    ns.absorb = SeamFn("tools.absorb")
    return ns


def mk_efficiency(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.discard_turns = SeamFn("efficiency.discard_turns")
    return ns


def mk_session(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.lock = contextlib.nullcontext()
    if isinstance(d.get("relay_session"), dict):
        ns.relay_session = SimpleNamespace(**d["relay_session"])
    if isinstance(d.get("efficiency"), dict):
        ns.efficiency = mk_efficiency(d["efficiency"])
    if isinstance(d.get("model_state"), dict):
        ns.model_state = SimpleNamespace(**d["model_state"])
    if isinstance(d.get("route_run"), dict):
        ns.route_run = mk_run(d["route_run"])
    return ns


def mk_task(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    if isinstance(d.get("cost"), dict):
        ns.cost = SimpleNamespace(**d["cost"])
    return ns


def mk_lineage(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace()
    ns.open = set(d.get("open") or [])
    ns.peak = mk_peak(d.get("peak"))
    ns.tally = mk_tally(d.get("tally"))
    ns.tools = mk_tools(d.get("tools"))
    ns.run = mk_run(d.get("run"))
    ns.spent = d.get("spent")
    ns.connectors = d.get("connectors")
    return ns


def mk_thread(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.is_alive = SeamFn("thread.is_alive")
    ns.start = SeamFn("thread.start")
    ns.join = SeamFn("thread.join")
    return ns


def mk_store(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns._connection = SeamFn("store._connection")
    return ns


def mk_resolved(d):
    return SimpleNamespace(**d) if isinstance(d, dict) else d


def mk_sender(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.send_pending = _make_callable_recorder("sender.send_pending")
    return ns


def mk_runtime(d):
    return RuntimeDouble(d) if isinstance(d, dict) else d


ATTR_SEAMS = {
    "rt._session": mk_session,
    "_get_runtime": mk_runtime,
    "spawn_context_thread": mk_thread,
    "SharedMetricsStore": mk_store,
    "SharedMetricsSender": mk_sender,
    "_resolved_send_config": mk_resolved,
    "eff.SessionEfficiency": mk_efficiency,
    "eff.ToolUsage": mk_tools,
}


def materialize_result(name, raw):
    if raw is None:
        return None
    if isinstance(raw, dict) and "__callable__" in raw:
        return _make_callable_recorder(raw["__callable__"])
    if isinstance(raw, dict) and "__cm__" in raw:
        return CMDouble(raw["__cm__"])
    if name == "get_hermes_home":
        return PurePosixPath(raw) if isinstance(raw, str) else raw
    if name in ATTR_SEAMS and isinstance(raw, dict):
        return ATTR_SEAMS[name](raw)
    return raw


def materialize_double(j):
    if isinstance(j, dict) and "__callable__" in j:
        return _make_callable_recorder(j["__callable__"])
    return j


# ── live namespace ──────────────────────────────────────────────────────────
_MOD = types.ModuleType("rm_live")
sys.modules["rm_live"] = _MOD
NS = _MOD.__dict__
NS.update({
    "__name__": "rm_live", "__builtins__": builtins,
    # `from .shared_metrics_snapshot import ...` in _emit_install_snapshot_if_due
    # resolves relative to the quarry package (stubbed in sys.modules).
    "__package__": "hermes_cli.observability",
    "contextlib": contextlib, "Any": Any, "Callable": Callable, "Iterator": Iterator,
    "logger": LoggerDouble(),
    "_Runtime": _RuntimeBase,
    "_RUNTIME_LOCK": contextlib.nullcontext(),
    "_RUNTIMES": {},
    "_consent_reconcile_done": False,
    "monotonic_ns": SeamFn("monotonic_ns"),
    "set_hermes_home_override": SeamFn("set_hermes_home_override"),
    "reset_hermes_home_override": SeamFn("reset_hermes_home_override"),
    "enabled": SeamFn("enabled"),
    "_get_runtime": SeamFn("_get_runtime"),
    "_raw_config": SeamFn("_raw_config"),
    "_resolved_send_config": SeamFn("_resolved_send_config"),
    "_scope_handle": SeamFn("_scope_handle"),
    "_absorb_peak": SeamFn("_absorb_peak"),
    "relay_runtime": SimpleNamespace(
        relay_instrumentation_enabled=SeamFn("relay_runtime.relay_instrumentation_enabled"),
        current_profile_key=SeamFn("relay_runtime.current_profile_key"),
    ),
    "contract": SimpleNamespace(**{nm: CNS[nm] for nm in CONTRACT_CONSTANTS}),
    "eff": SimpleNamespace(SessionEfficiency=SeamFn("eff.SessionEfficiency"),
                           ToolUsage=SeamFn("eff.ToolUsage")),
    "engagement_": SimpleNamespace(switch_after_fields=SeamFn("engagement.switch_after_fields")),
    "model_": SimpleNamespace(friction_fields=SeamFn("model.friction_fields")),
    "fields_": SimpleNamespace(model_token_fields=SeamFn("fields.model_token_fields")),
})
for _nm in CONSTANTS:
    exec(CONST_CODE[_nm], NS)
for _nm in PORTED_MODULE:
    exec(DEF_CODE[_nm], NS)
for _nm in PORTED_METHODS:
    exec(METHOD_CODE[_nm], NS)
NS["logger"] = LoggerDouble()
# @contextlib.contextmanager is dropped by segment extraction — re-apply it.
NS["_owner_scope"] = contextlib.contextmanager(NS["_owner_scope"])


def _install_import_stubs():
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    hc = mod("hermes_cli"); hc.__path__ = []
    obs = mod("hermes_cli.observability"); obs.__path__ = []
    hc.observability = obs
    snap = mod("hermes_cli.observability.shared_metrics_snapshot",
               collect_install_snapshot=SeamFn("collect_install_snapshot"))
    obs.shared_metrics_snapshot = snap
    sender = mod("hermes_cli.observability.shared_metrics_sender",
                 reconcile_send_consent=SeamFn("reconcile_send_consent"),
                 SharedMetricsSender=SeamFn("SharedMetricsSender"))
    obs.shared_metrics_sender = sender
    sm = mod("hermes_cli.observability.shared_metrics",
             SharedMetricsStore=SeamFn("SharedMetricsStore"))
    obs.shared_metrics = sm
    mod("hermes_cli.sqlite_util", write_txn=SeamFn("write_txn"))
    mod("hermes_constants", get_hermes_home=SeamFn("get_hermes_home"))
    ag = mod("agent"); ag.__path__ = []
    mp = mod("agent.memory_provider", spawn_context_thread=SeamFn("spawn_context_thread"))
    ag.memory_provider = mp


_install_import_stubs()
# Live Path.exists() hits the real disk unless patched onto the class.
PurePosixPath.exists = lambda self: record("path.exists", {"path": self.as_posix()})


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


def _lineage_state(st_in):
    return {k: mk_lineage(v) for k, v in (st_in.get("_lineages") or {}).items()}


def _slf_base(st_in):
    slf = SimpleNamespace(**copy.deepcopy(st_in))
    if isinstance(st_in.get("host"), dict):
        slf.host = SimpleNamespace(**st_in["host"])
    return slf


def _relay_scope(slf):
    slf.relay = SimpleNamespace(scope=SimpleNamespace(
        event=_make_callable_recorder("relay.scope.event")))


def _flush_slf(slf, st_in):
    """Inject the seams the composed _flush_and_export / _owner_scope read."""
    slf.relay = SimpleNamespace(subscribers=SimpleNamespace(
        flush=SeamFn("relay.subscribers.flush")))
    if not isinstance(getattr(slf, "host", None), SimpleNamespace):
        slf.host = SimpleNamespace(**(st_in.get("host") or {}))
    slf._export = SeamFn("rt._export")
    slf._owner_scope = types.MethodType(NS["_owner_scope"], slf)


def drive(func, inp, holder):
    st_in = inp.get("state") or {}
    holder["proj"] = lambda: norm_value(st_in)

    if func == "_run_scoped":
        slf = SimpleNamespace(_run_in_task=SeamFn("rt._run_in_task"),
                              host=SimpleNamespace(run_in_session=SeamFn("host.run_in_session")))
        sess = mk_session(inp.get("session")) if inp.get("session") is not None else None
        task = mk_task(inp.get("task")) if inp.get("task") is not None else None
        cb = materialize_double(inp.get("callback"))
        return NS["_run_scoped"](slf, sess, task, cb,
                                 *(inp.get("args") or []), **(inp.get("kwargs") or {}))
    if func == "_mark":
        slf = SimpleNamespace()
        slf._run_scoped = types.MethodType(NS["_run_scoped"], slf)
        slf._event_metadata = SeamFn("rt._event_metadata")
        slf._run_in_task = SeamFn("rt._run_in_task")
        slf.host = SimpleNamespace(run_in_session=SeamFn("host.run_in_session"))
        _relay_scope(slf)
        sess = mk_session(inp.get("session")) if inp.get("session") is not None else None
        task = mk_task(inp.get("task")) if inp.get("task") is not None else None
        NS["_mark"](slf, sess, task, inp.get("name", ""), inp.get("data"))
        return None
    if func == "_emit_install_snapshot_if_due":
        slf = SimpleNamespace(_snapshot_checked_ns=st_in.get("_snapshot_checked_ns"),
                              subscriber=SimpleNamespace(store=SimpleNamespace(
                                  install_snapshot_due=SeamFn("store.install_snapshot_due"))))
        slf._mark = types.MethodType(NS["_mark"], slf)
        slf._run_scoped = types.MethodType(NS["_run_scoped"], slf)
        slf._event_metadata = SeamFn("rt._event_metadata")
        slf._run_in_task = SeamFn("rt._run_in_task")
        slf.host = SimpleNamespace(run_in_session=SeamFn("host.run_in_session"))
        _relay_scope(slf)
        sess = mk_session(inp.get("session")) if inp.get("session") is not None else None
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_emit_install_snapshot_if_due"](slf, sess)
        return None
    if func == "_owner_scope":
        slf = SimpleNamespace(host=SimpleNamespace(**(st_in.get("host") or {})))
        body = materialize_double(inp.get("body"))
        cm = NS["_owner_scope"](slf)
        with cm:
            if body is not None:
                body()
        return None
    if func == "_flush_and_export":
        slf = SimpleNamespace()
        _flush_slf(slf, st_in)
        NS["_flush_and_export"](slf, inp.get("failure_message", ""))
        return None
    if func == "_run_flush_worker":
        slf = _slf_base(st_in)
        slf._flush_lock = contextlib.nullcontext()
        slf._flush_and_export = types.MethodType(NS["_flush_and_export"], slf)
        _flush_slf(slf, st_in)
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_run_flush_worker"](slf, inp.get("failure_message", ""))
        return None
    if func == "_schedule_flush_and_export":
        slf = _slf_base(st_in)
        slf._flush_lock = contextlib.nullcontext()
        slf._run_flush_worker = _make_callable_recorder("rt._run_flush_worker")
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_schedule_flush_and_export"](slf, inp.get("failure_message", ""))
        return None
    if func == "_close_and_export":
        slf = _slf_base(st_in)
        slf._sessions_lock = contextlib.nullcontext()
        slf._lineage_lock = contextlib.nullcontext()
        slf._safe = _make_safe_recorder(st_in.get("id", ""))
        slf.close_session = _make_callable_recorder("rt.close_session")
        slf._close_unseen_segment = _make_callable_recorder("rt._close_unseen_segment")
        slf._flush_and_export = types.MethodType(NS["_flush_and_export"], slf)
        slf._deregister = SeamFn("rt._deregister")
        slf._release = SeamFn("rt._release")
        _flush_slf(slf, st_in)
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_close_and_export"](slf)
        return None
    if func == "_share_spent":
        session = mk_session(copy.deepcopy(st_in.get("session") or {}))
        slf = _slf_base(st_in)
        slf._lineage_spent = SeamFn("rt._lineage_spent")
        slf._lineage_lock = contextlib.nullcontext()
        slf._lineages = _lineage_state(st_in)
        slf._lineage_of = copy.deepcopy(st_in.get("_lineage_of") or {})
        holder["proj"] = lambda: {k: (norm_value(session) if k == "session"
                                      else norm_value(getattr(slf, k))) for k in st_in}
        NS["_share_spent"](slf, session)
        return None
    if func == "_with_route_run":
        slf = SimpleNamespace(_lineage_lock=contextlib.nullcontext(),
                              _lineages=_lineage_state(st_in),
                              _lineage_of=copy.deepcopy(st_in.get("_lineage_of") or {}))
        own = mk_run(inp.get("own"))
        update = materialize_double(inp.get("update"))
        return NS["_with_route_run"](slf, inp.get("session_id", ""), own, update)
    if func == "_close_segment":
        slf = SimpleNamespace(_lineage_lock=contextlib.nullcontext(),
                              _lineages=_lineage_state(st_in),
                              _lineage_of=copy.deepcopy(st_in.get("_lineage_of") or {}))
        state = mk_peak(inp.get("segment_state"))
        tally = mk_tally(inp.get("tally"))
        tools = mk_tools(inp.get("tools"))
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        return NS["_close_segment"](slf, inp.get("session_id", ""), state, tally, tools)
    if func == "_emit_summary_rows":
        slf = SimpleNamespace(_emit_rows=SeamFn("rt._emit_rows"))
        session = mk_session(inp.get("session"))
        peak = mk_peak(inp.get("peak"))
        tally = mk_tally(inp.get("tally"))
        tools = mk_tools(inp.get("tools"))
        extra = [tuple(e) for e in (inp.get("extra") or [])]
        NS["_emit_summary_rows"](slf, session, peak, tally, tools, *extra)
        return None
    if func == "_close_unseen_segment":
        slf = SimpleNamespace(_lineage_lock=contextlib.nullcontext(),
                              _lineages=_lineage_state(st_in),
                              _lineage_of=copy.deepcopy(st_in.get("_lineage_of") or {}),
                              _emit_rows=SeamFn("rt._emit_rows"))
        slf._close_segment = types.MethodType(NS["_close_segment"], slf)
        slf._emit_summary_rows = types.MethodType(NS["_emit_summary_rows"], slf)
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_close_unseen_segment"](slf, inp.get("session_id", ""))
        return None
    if func == "_flush_lineage":
        slf = SimpleNamespace(_lineage_lock=contextlib.nullcontext(),
                              _lineages=_lineage_state(st_in),
                              _lineage_of=copy.deepcopy(st_in.get("_lineage_of") or {}),
                              _emit_rows=SeamFn("rt._emit_rows"))
        slf._emit_summary_rows = types.MethodType(NS["_emit_summary_rows"], slf)
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_flush_lineage"](slf, inp.get("key", ""))
        return None
    if func == "_send_exported_packages":
        slf = _slf_base(st_in)
        slf._send_lock = contextlib.nullcontext()
        slf._guarded = SeamFn("rt._guarded")
        slf._run_send_pass = _make_callable_recorder("rt._run_send_pass")
        if isinstance(st_in.get("subscriber"), dict):
            sub = dict(st_in["subscriber"])
            sub["store"] = mk_store(sub.get("store"))
            slf.subscriber = SimpleNamespace(**sub)
        if isinstance(st_in.get("_send_thread"), dict):
            slf._send_thread = mk_thread(st_in["_send_thread"])
        holder["proj"] = lambda: {k: norm_value(getattr(slf, k)) for k in st_in}
        NS["_send_exported_packages"](slf)
        return None
    if func == "_run_send_pass":
        store = (st_in.get("subscriber") or {}).get("store")
        slf = SimpleNamespace(subscriber=SimpleNamespace(store=mk_store(store)),
                              _guarded=SeamFn("rt._guarded"))
        NS["_run_send_pass"](slf, inp.get("endpoint", ""))
        return None
    if func == "record_auxiliary_tokens":
        slf = SimpleNamespace(_with_scope_stack=SeamFn("rt._with_scope_stack"),
                              _event_metadata=SeamFn("rt._event_metadata"))
        _relay_scope(slf)
        NS["record_auxiliary_tokens"](slf, inp.get("event") or {})
        return None
    if func == "record_switch_after":
        slf = SimpleNamespace(_session=SeamFn("rt._session"),
                              record_process_mark=SeamFn("rt.record_process_mark"),
                              _lineage_lock=contextlib.nullcontext(),
                              _lineages=_lineage_state(st_in),
                              _lineage_of=copy.deepcopy(st_in.get("_lineage_of") or {}))
        slf._with_route_run = types.MethodType(NS["_with_route_run"], slf)
        NS["record_switch_after"](slf, inp.get("session_id", ""))
        return None
    if func == "record_friction":
        slf = SimpleNamespace(_session=SeamFn("rt._session"),
                              _lineage_spent=SeamFn("rt._lineage_spent"),
                              record_process_mark=SeamFn("rt.record_process_mark"),
                              _emit_rows=SeamFn("rt._emit_rows"),
                              _lineage_lock=contextlib.nullcontext(),
                              _lineages=_lineage_state(st_in),
                              _lineage_of=copy.deepcopy(st_in.get("_lineage_of") or {}))
        NS["record_friction"](slf, inp.get("signal", ""), inp.get("session_id", ""),
                              inp.get("fallback_route") or {}, turns=inp.get("turns", 1))
        return None
    if func == "_reconcile_store_consent":
        store = mk_store(inp.get("store"))
        NS["_reconcile_store_consent"](store, inp.get("send_enabled", False))
        return None
    if func == "_reconcile_send_consent_once":
        NS["_consent_reconcile_done"] = bool(st_in.get("_consent_reconcile_done", False))
        holder["proj"] = lambda: {k: norm_value(NS[k]) for k in st_in if k in NS}
        NS["_reconcile_send_consent_once"]()
        return None
    if func == "record_session_friction":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["record_session_friction"](inp.get("signal", ""), inp.get("session_id", ""),
                                             inp.get("fallback_route") or {}, turns=inp.get("turns", 1))
    if func == "record_session_tools":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["record_session_tools"](inp.get("session_id", ""), inp.get("agent"),
                                          inp.get("tools_for_api") or [])
    if func == "record_known_cache_break":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["record_known_cache_break"](inp.get("cause", ""), inp.get("route") or {},
                                              inp.get("session_id", ""))
    if func == "record_model_switch_after":
        NS["_RUNTIMES"] = _registry(inp)
        return NS["record_model_switch_after"](inp.get("session_id", ""))
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
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    names = set(PORTED)
    funcs_seen = set()
    failclosed = {}
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
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {func}: NameError {spec.get('message')}")
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("message", "")
                if any(("'" + n + "'") in msg for n in names):
                    bad += 1
                    print(f"SELFTEST case {i} {func}: suspicious AttributeError {msg}")
            if isinstance(spec, dict) and spec.get("__raise__") == "RuntimeError":
                msg = spec.get("message", "")
                if msg.startswith("no script for "):
                    failclosed.setdefault(msg[len("no script for "):], 0)
                    failclosed[msg[len("no script for "):]] += 1
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {func}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    missing = names - funcs_seen
    if missing:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing)}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    print("SELFTEST fail-closed seams: %s" % json.dumps(failclosed, sort_keys=True))
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "relay_metrics_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())