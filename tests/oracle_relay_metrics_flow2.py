#!/usr/bin/env python3
"""Oracle for src/cli/relay_metrics_flow2.c — drives the LIVE
hermes_cli/observability/relay_shared_metrics.py UNCLAIMED fourth-wave FLOW
defs (10 functions) under IDENTICAL scripted universal seams. (The task brief
named "relay_metrics.py"; no such quarry file exists — the relay-metrics module
IS relay_shared_metrics.py, and every PoP in the port names it.)

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted (_Runtime
methods, decorators stripped; the still_consented closure lifted out of
_run_send_pass with `endpoint` seeded per case; the _PeakLineage dataclass exec'd
with dataclass/field + the model_/eff/engagement_ constructor seams seeded).
_MAX_LINEAGES and the contract mark constants are seeded from the quarry ASTs.
Claimed siblings / foreign I/O (the connectors/engagement/model/efficiency
helpers, relay host/subscriber/clock, uuid, _resolved_send_config, and every
claimed _Runtime sibling) are recording seams fed the SAME scripts the C harness
feeds. None of the ten ported bodies carries a try/except guard, so a scripted
BaseException passes straight through to the case envelope exactly as live.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed. The state window is
projected from carriers materialized BEFORE the call, so a raise leaves the
mutation history at that point (matching the C in-place st).

Value model: attribute doubles (session/task/model_call/lineage/tools/efficiency/
cost/model_state/resolved/subscriber) are SimpleNamespace projected back to their
raw dict shape (callables / context managers excluded); sets -> sorted arrays;
tuple -> array; integral floats -> ints; a tuple-KEYED dict (session.tool_calls)
projects to an insertion-ordered [[key, value], ...] pairs array. callable doubles
{"__callable__": "<seam>"} fire that seam with {args, kwargs}; the lambda
`run: run.observe(...)` passed to the seamed _with_route_run projects to
{"__callable__": "run.observe"} (its body is out of seam scope). A rt._session
seam result may be {"__session_ref__": "<state key>"} resolving to the state
carrier double (so mutations echo), a plain record (fresh double), or null.

Domain (ASCII scope, documented in the port header): object seam results are null
or a record; attribute doubles carry the fields the live body reads; fields/event
are dicts; tools_for_api is a list or null; eff.tool_snapshot results are lists;
rt._close_segment results are 3-arrays; marks are [mark, data] pair lists;
uuid.uuid4 results are null or {"hex": ...}; _event_metadata results are dicts;
id() results are scalars. EXCLUDED: integral-FLOAT literals in str() positions;
non-list tool_snapshot / non-3-array _close_segment results (stub-qualname binding
TypeErrors are oracle-injection-dependent); non-ASCII identity elements.

Summary: 'ORACLE relay_metrics_flow2: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import contextlib
import copy
import dataclasses
import json
import os
import sys
import types
from pathlib import Path
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
NAME = "relay_metrics_flow2"

PORTED_METHODS = [
    "_settle_connector_batch", "_observe_model_turn", "_observe_turn_call",
    "record_known_cache_break", "record_session_tools", "rotate_segment",
    "_emit_session_summary", "record_process_mark", "record_process_marks_saved",
]
PORTED_CLOSURE = "still_consented"
PORTED = PORTED_METHODS + [PORTED_CLOSURE]
CONSTANTS = ["_MAX_LINEAGES"]
CONTRACT_CONSTANTS = ["ENGAGEMENT_TURN_MARK", "MODEL_FRICTION_MARK", "COMMIT_TICKET_KEY"]

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


METHOD_CODE = {}
CONST_CODE = {}
LINEAGE_CODE = None
CLOSURE_CODE = None
for _node in _TREE.body:
    if isinstance(_node, ast.ClassDef) and _node.name == "_Runtime":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name in PORTED_METHODS:
                _s = copy.copy(_sub)
                _s.decorator_list = []
                METHOD_CODE[_sub.name] = _compile_node(_s)
            if isinstance(_sub, ast.FunctionDef) and _sub.name == "_run_send_pass":
                for _inner in _sub.body:
                    if isinstance(_inner, ast.FunctionDef) and _inner.name == PORTED_CLOSURE:
                        CLOSURE_CODE = _compile_node(_inner)
    elif isinstance(_node, ast.ClassDef) and _node.name == "_PeakLineage":
        LINEAGE_CODE = _compile_node(_node)   # keep @dataclass decorator
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name) and _tg.id in CONSTANTS:
            CONST_CODE[_tg.id] = _compile_node(_node)
_missing = (set(PORTED_METHODS) - set(METHOD_CODE)) | (set(CONSTANTS) - set(CONST_CODE))
if CLOSURE_CODE is None:
    _missing.add(PORTED_CLOSURE)
if LINEAGE_CODE is None:
    _missing.add("_PeakLineage")
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


class ToolCallsDict(dict):
    """session.tool_calls: a tuple-keyed dict that ALWAYS projects to a pairs
    array (even when empty -> []), matching the C insertion-ordered pairs model."""


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, ToolCallsDict):
        return [[list(k), norm_value(x)] for k, x in v.items()]
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()
                if not callable(x) and not hasattr(x, "__enter__")}
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {k: norm_value(x) for k, x in vars(v).items()
                if not callable(x) and not hasattr(x, "__enter__")}
    if isinstance(v, dict):
        if any(isinstance(k, tuple) for k in v):
            # tuple-keyed dict (session.tool_calls) -> insertion-ordered pairs array
            return [[list(k), norm_value(x)] for k, x in v.items()]
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
    if callable(v) and getattr(v, "__name__", "") == "<lambda>":
        # the only lambda passed to a seamed collaborator here is run.observe
        return {"__callable__": "run.observe"}
    if callable(v) and getattr(v, "__name__", "") == "still_consented":
        return {"__callable__": "still_consented"}
    if callable(v) and getattr(v, "__name__", "") in PORTED:
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
    if kind == "UnicodeDecodeError":
        return cls("utf-8", b"x", 0, 1, msg or "boom")
    if kind == "UnicodeEncodeError":
        return cls("utf-8", "x", 0, 1, msg or "boom")
    if kind == "UnicodeTranslateError":
        return cls("x", 0, 1, msg or "boom")
    if kind in ("ExceptionGroup", "BaseExceptionGroup"):
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
    if name in ("monotonic_ns", "rt._event_metadata", "relay.subscribers.flush",
                "model.ModelSessionState", "eff.ToolUsage", "engagement.RouteRun",
                "uuid.uuid4", "_resolved_send_config"):
        return {}
    if name == "connectors.is_connector_batch":
        return {"event": norm_value(a[0])}
    if name == "connectors.is_batch_entry":
        return {"identity": norm_value(a[0]), "wrapper": norm_value(a[1])}
    if name == "connectors.batch_entry_ran":
        return {"identities": norm_value(a[0]), "wrapper": norm_value(a[1])}
    if name == "rt._finish_tool_call":
        return {"task": norm_value(a[0]), "tool_call": norm_value(a[1]), "event": norm_value(a[2])}
    if name == "model_state.observe_turn":
        return {"outcome": a[0], "route": norm_value(a[1]), "last_turn_ns": a[2]}
    if name == "rt._with_route_run":
        return {"session_id": a[0], "own": norm_value(a[1]), "update": proj_callable(a[2])}
    if name == "engagement.engaged_turn":
        return {"start_fields": norm_value(a[0]), "user_turn": a[1]}
    if name == "rt._guarded":
        return {"message": a[0], "callback": proj_callable(a[1]),
                "args": norm_value(list(a[2:])), "kwargs": norm_value(k)}
    if name == "model.attended":
        return {"start_fields": norm_value(a[0])}
    if name == "model.friction_fields":
        return {"signal": a[0], "route": norm_value(a[1])}
    if name == "efficiency.finish_turn":
        return {"cost": norm_value(a[0]), "fields": norm_value(a[1]),
                "route": norm_value(k.get("route")), "model_calls": k.get("model_calls"),
                "tool_calls": k.get("tool_calls")}
    if name == "rt._emit_rows":
        return {"session": norm_value(a[0]), "rows": norm_value(a[1])}
    if name == "cost.add_call":
        return {"route": norm_value(a[0]), "usage": norm_value(a[1])}
    if name == "rt._cold_key":
        return {"session_id": a[0]}
    if name == "rt._consume_cold":
        return {"key": a[0]}
    if name == "efficiency.observe_cache":
        return {"route": norm_value(a[0]), "usage": norm_value(a[1]),
                "started_ns": a[2], "now_ns": a[3], "expected": k.get("expected")}
    if name == "eff.cache_break_row":
        return {"cause": a[0], "route": norm_value(a[1])}
    if name == "rt._announce_cold":
        return {"key": a[0]}
    if name == "rt._session":
        return {"event": norm_value(a[0])}
    if name == "id":
        return {"value": norm_value(a[0])}
    if name == "eff.tool_snapshot":
        return {"agent": norm_value(a[0]), "tools_for_api": norm_value(a[1])}
    if name == "efficiency.tools.observe":
        return {"args": norm_value(list(a))}
    if name == "model.model_route":
        return {"provider": norm_value(a[0]), "model": norm_value(a[1])}
    if name == "rt._share_spent":
        return {"session": norm_value(a[0])}
    if name == "rt.close_session":
        return {"event": norm_value(a[0])}
    if name == "rt._flush_lineage":
        return {"key": a[0]}
    if name == "engagement.SessionTally":
        if not a:
            return {}
        return {"start_fields": norm_value(a[0]), "turns": a[1], "failed_turns": a[2],
                "last_outcome": a[3], "first_turn_ns": a[4], "last_turn_ns": a[5],
                "api_calls": a[6], "tool_call_total": a[7], "replies": a[8]}
    if name == "rt._close_segment":
        return {"session_id": a[0], "segment_state": norm_value(a[1]),
                "tally": norm_value(a[2]), "tools": norm_value(a[3])}
    if name == "model_state.quick_abandon_route":
        return {"now_ns": a[0]}
    if name == "rt._emit_summary_rows":
        return {"session": norm_value(a[0]), "peak": norm_value(a[1]),
                "tally": norm_value(a[2]), "tools": norm_value(a[3]),
                "extra": norm_value(list(a[4:]))}
    if name == "rt._with_scope_stack":
        return {"callback": proj_callable(a[0]), "args": norm_value(list(a[1:])),
                "kwargs": norm_value(k)}
    if name == "subscriber.take_saved":
        return {"ticket": a[0]}
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
    ns.observe = SeamFn("efficiency.tools.observe")
    return ns


def mk_cost(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.add_call = SeamFn("cost.add_call")
    return ns


def mk_efficiency(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.finish_turn = SeamFn("efficiency.finish_turn")
    ns.observe_cache = SeamFn("efficiency.observe_cache")
    if isinstance(d.get("tools"), dict):
        ns.tools = mk_tools(d["tools"])
    if isinstance(d.get("tools_key"), list):
        ns.tools_key = tuple(d["tools_key"])
    return ns


def mk_model_state(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.observe_turn = SeamFn("model_state.observe_turn")
    ns.quick_abandon_route = SeamFn("model_state.quick_abandon_route")
    return ns


def mk_model_call(d):
    return SimpleNamespace(**d) if isinstance(d, dict) else d


def mk_session(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.lock = contextlib.nullcontext()
    if isinstance(d.get("efficiency"), dict):
        ns.efficiency = mk_efficiency(d["efficiency"])
    if isinstance(d.get("model_state"), dict):
        ns.model_state = mk_model_state(d["model_state"])
    if isinstance(d.get("route_run"), dict):
        ns.route_run = mk_run(d["route_run"])
    if isinstance(d.get("tool_calls"), list):
        ns.tool_calls = ToolCallsDict((tuple(k), v) for k, v in d["tool_calls"])
    return ns


def mk_task(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    if isinstance(d.get("cost"), dict):
        ns.cost = mk_cost(d["cost"])
    for key in ("completed_tool_call_ids", "tool_call_ids"):
        if isinstance(d.get(key), list):
            setattr(ns, key, set(tuple(x) for x in d[key]))
    for key in ("model_call_ids", "turn_ids"):
        if isinstance(d.get(key), list):
            setattr(ns, key, set(d[key]))
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


def mk_subscriber(d):
    if not isinstance(d, dict):
        return d
    ns = SimpleNamespace(**d)
    ns.take_saved = SeamFn("subscriber.take_saved")
    return ns


def mk_resolved(d):
    return SimpleNamespace(**d) if isinstance(d, dict) else d


def mk_uuid(d):
    return SimpleNamespace(**d) if isinstance(d, dict) else d


ATTR_SEAMS = {
    "rt._session": mk_session,
    "eff.ToolUsage": mk_tools,
    "engagement.SessionTally": mk_tally,
    "model.ModelSessionState": mk_model_state,
    "eff.SessionEfficiency": mk_efficiency,
    "_resolved_send_config": mk_resolved,
    "uuid.uuid4": mk_uuid,
}


def materialize_result(name, raw):
    if raw is None:
        return None
    if isinstance(raw, dict) and "__session_ref__" in raw:
        return CURRENT["ctx"].carriers.get(raw["__session_ref__"])
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
_MOD = types.ModuleType("rm2_live")
sys.modules["rm2_live"] = _MOD
NS = _MOD.__dict__
NS.update({
    "__name__": "rm2_live", "__builtins__": builtins,
    "__package__": "hermes_cli.observability",
    "contextlib": contextlib, "Any": Any, "Callable": Callable, "Iterator": Iterator,
    "dataclass": dataclasses.dataclass, "field": dataclasses.field,
    "logger": LoggerDouble(),
    "monotonic_ns": SeamFn("monotonic_ns"),
    "_resolved_send_config": SeamFn("_resolved_send_config"),
    # record_session_tools keys the toolset snapshot by id(); seam it so the
    # scripted id queue (not real memory addresses) drives the comparison.
    "id": SeamFn("id"),
    "uuid": SimpleNamespace(uuid4=SeamFn("uuid.uuid4")),
    "contract": SimpleNamespace(**{nm: CNS[nm] for nm in CONTRACT_CONSTANTS}),
    "connectors_": SimpleNamespace(
        is_connector_batch=SeamFn("connectors.is_connector_batch"),
        is_batch_entry=SeamFn("connectors.is_batch_entry"),
        batch_entry_ran=SeamFn("connectors.batch_entry_ran"),
        ConnectorSessionState=object),
    "eff": SimpleNamespace(cache_break_row=SeamFn("eff.cache_break_row"),
                           tool_snapshot=SeamFn("eff.tool_snapshot"),
                           ToolUsage=SeamFn("eff.ToolUsage"),
                           SessionEfficiency=SeamFn("eff.SessionEfficiency")),
    "engagement_": SimpleNamespace(engaged_turn=SeamFn("engagement.engaged_turn"),
                                   SessionTally=SeamFn("engagement.SessionTally"),
                                   RouteRun=SeamFn("engagement.RouteRun")),
    "model_": SimpleNamespace(attended=SeamFn("model.attended"),
                              friction_fields=SeamFn("model.friction_fields"),
                              model_route=SeamFn("model.model_route"),
                              ModelSessionState=SeamFn("model.ModelSessionState")),
})
for _nm in CONSTANTS:
    exec(CONST_CODE[_nm], NS)
# _PeakLineage dataclass: default_factory references the constructor seams above.
exec(LINEAGE_CODE, NS)
for _nm in PORTED_METHODS:
    exec(METHOD_CODE[_nm], NS)
exec(CLOSURE_CODE, NS)
NS["logger"] = LoggerDouble()


# ── per-case context ────────────────────────────────────────────────────────
class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))
        self.carriers = {}


def _materialize_state(st_in):
    carriers = {}
    for k, v in st_in.items():
        if k in ("session", "old", "new"):
            carriers[k] = mk_session(copy.deepcopy(v))
        elif k == "task":
            carriers[k] = mk_task(copy.deepcopy(v))
        elif k == "model_call":
            carriers[k] = mk_model_call(copy.deepcopy(v))
        elif k == "subscriber":
            carriers[k] = mk_subscriber(copy.deepcopy(v))
        elif k == "_lineages":
            carriers[k] = {kk: mk_lineage(vv) for kk, vv in (v or {}).items()}
        else:
            carriers[k] = copy.deepcopy(v)
    return carriers


def _relay_scope(slf):
    slf.relay = SimpleNamespace(
        scope=SimpleNamespace(event=_make_callable_recorder("relay.scope.event")),
        subscribers=SimpleNamespace(flush=SeamFn("relay.subscribers.flush")))


def drive(func, inp, holder):
    st_in = inp.get("state") or {}
    ctx = CURRENT["ctx"]
    carriers = _materialize_state(st_in)
    ctx.carriers = carriers
    holder["proj"] = lambda: {k: norm_value(carriers[k]) for k in st_in}
    slf = SimpleNamespace(**carriers)
    slf._lineage_lock = contextlib.nullcontext()
    slf._sessions_lock = contextlib.nullcontext()

    if func == "_settle_connector_batch":
        slf._finish_tool_call = SeamFn("rt._finish_tool_call")
        wrapper = tuple(inp.get("wrapper") or [])
        return NS["_settle_connector_batch"](slf, carriers.get("session"),
                                             carriers.get("task"), wrapper, inp.get("event") or {})
    if func == "_observe_model_turn":
        slf._with_route_run = SeamFn("rt._with_route_run")
        slf._guarded = SeamFn("rt._guarded")
        slf._mark = _make_callable_recorder("rt._mark")
        slf._emit_rows = SeamFn("rt._emit_rows")
        NS["_observe_model_turn"](slf, carriers.get("session"), carriers.get("task"),
                                  inp.get("fields") or {})
        return None
    if func == "_observe_turn_call":
        slf._consume_cold = SeamFn("rt._consume_cold")
        slf._cold_key = SeamFn("rt._cold_key")
        slf._emit_rows = SeamFn("rt._emit_rows")
        NS["_observe_turn_call"](slf, carriers.get("session"), carriers.get("task"),
                                 carriers.get("model_call"), inp.get("event") or {})
        return None
    if func == "record_known_cache_break":
        slf._announce_cold = SeamFn("rt._announce_cold")
        slf._cold_key = SeamFn("rt._cold_key")
        slf._emit_rows = SeamFn("rt._emit_rows")
        NS["record_known_cache_break"](slf, inp.get("cause", ""), inp.get("route") or {},
                                       inp.get("session_id", ""))
        return None
    if func == "record_session_tools":
        slf._session = SeamFn("rt._session")
        slf._announce_cold = SeamFn("rt._announce_cold")
        slf._cold_key = SeamFn("rt._cold_key")
        slf._emit_rows = SeamFn("rt._emit_rows")
        agent = inp.get("agent")
        agent = SimpleNamespace(**agent) if isinstance(agent, dict) else agent
        NS["record_session_tools"](slf, inp.get("session_id", ""), agent,
                                   inp.get("tools_for_api"))
        return None
    if func == "rotate_segment":
        slf._session = SeamFn("rt._session")
        slf._share_spent = SeamFn("rt._share_spent")
        slf.close_session = SeamFn("rt.close_session")
        slf._flush_lineage = SeamFn("rt._flush_lineage")
        NS["rotate_segment"](slf, inp.get("old_id", ""), inp.get("new_id", ""))
        return None
    if func == "_emit_session_summary":
        slf._close_segment = SeamFn("rt._close_segment")
        slf._emit_summary_rows = SeamFn("rt._emit_summary_rows")
        NS["_emit_session_summary"](slf, carriers.get("session"))
        return None
    if func == "record_process_mark":
        slf._with_scope_stack = SeamFn("rt._with_scope_stack")
        slf._event_metadata = SeamFn("rt._event_metadata")
        _relay_scope(slf)
        NS["record_process_mark"](slf, inp.get("mark", ""), inp.get("data"))
        return None
    if func == "record_process_marks_saved":
        slf._with_scope_stack = SeamFn("rt._with_scope_stack")
        slf._event_metadata = SeamFn("rt._event_metadata")
        _relay_scope(slf)
        marks = [tuple(m) for m in (inp.get("marks") or [])] if inp.get("marks") is not None else None
        return NS["record_process_marks_saved"](slf, marks)
    if func == "still_consented":
        NS["endpoint"] = inp.get("endpoint", "")
        return NS["still_consented"]()
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
        fx = _HERE.parent / "fixtures" / "relay_metrics_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())