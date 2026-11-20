#!/usr/bin/env python3
"""Oracle for src/cli/shared_metrics_flow.c — drives the LIVE
hermes_cli/observability/shared_metrics.py UNCLAIMED-remainder FLOW + leaf defs
(21 functions) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted (module
functions + SharedMetricsStore methods, decorators stripped; _write re-wrapped
with contextlib.contextmanager). Claimed siblings (_connection, _utc_now,
_isoformat, _parse_state_timestamp, _validate_counter, _install_id,
_record_counter_in_transaction, _package_metric) and foreign I/O (sqlite
db.execute, write_txn, contract validators, Path methods, time.monotonic,
atexit.register, logger) are recording seams fed the SAME scripts the C harness
feeds. Ported siblings stay REAL so composition is exercised
(record_install_snapshot -> _record_once_per_interval, record_milestone /
record_feature_adoption -> _record_once_ever, recorded_milestones /
recorded_features -> _latched, update_rollup_state -> _write_or_defer ->
_update_rollup_state, the latch cores -> _write).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. A scripted BaseException (SystemExit / KeyboardInterrupt /
GeneratorExit / CancelledError) propagates past every `except Exception` /
`except OSError` guard exactly as live. Per-case BaseException catch ->
__raise__ spec; scripted raises round-trip BARE via identity match against
Script.handed. Exception classes are the REAL builtins / sqlite3 classes (full
hierarchy via getattr(builtins/sqlite3), no hand-rolled parent table).

Value model: datetimes travel as canonical ISO strings (_utc_now /
_parse_state_timestamp materialize to real datetime; norm projects back via
isoformat()). sqlite rows are RowDouble (missing column -> IndexError("No item
with that key")); db.execute script result is a cursor spec {one, all, rowcount}.
The connection handle is the opaque store._connection result. Deferred writes
are callable doubles {"__callable__": seam} or the REAL rollup lambda
{"__fn__": "_update_rollup_state", state_key, update} (projected via closure
inspection). _DEFERRING_STORES is a list of {"id": ..} markers. Integral floats
-> ints; frozenset -> sorted list; ASCII scope; _compact_json domain excludes
floats (documented in the port header).

Summary: 'ORACLE shared_metrics_flow: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import contextlib
import copy
import json
import sqlite3
import sys
import types
from datetime import datetime, timedelta, timezone
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
if not (QUARRY / "hermes_cli" / "observability" / "shared_metrics.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "observability" / "shared_metrics.py")
CONTRACT_SRC = str(QUARRY / "hermes_cli" / "observability" / "shared_metrics_contract.py")
NAME = "shared_metrics_flow"

PORTED_MODULE = ["drain_deferring_stores", "_compact_json", "_row_resource", "_ensure_private"]
PORTED_METHODS = [
    "record_install_snapshot", "install_snapshot_due", "_record_once_per_interval",
    "update_rollup_state", "_update_rollup_state", "recorded_milestones",
    "record_milestone", "recorded_features", "record_feature_adoption",
    "record_counter_once_per_day", "_latched", "_record_once_ever",
    "_write_or_defer", "drain_deferred", "_write", "_mark_packaged", "_package_metrics",
]
PORTED = PORTED_MODULE + PORTED_METHODS
STORE_METHODS = set(PORTED_METHODS) - {"_mark_packaged", "_package_metrics"}
CONSTANTS = [
    "_INSTALL_SNAPSHOT_STATE_KEY", "_MILESTONE_STATE_PREFIX", "_FEATURE_STATE_PREFIX",
    "_DAILY_ONCE_STATE_PREFIX", "_BUSY_TIMEOUT_MS", "_SCHEMA_BUSY_TIMEOUT_MS",
    "_DEFERRED_WRITES_MAX", "_RESOURCE_COLUMNS", "_ACTIVE_INSTALL_INTERVAL",
]
CONTRACT_CONSTANTS = ["INSTALL_SNAPSHOT_METRIC", "MILESTONE_METRIC", "FEATURE_ADOPTION_METRIC"]

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
    elif isinstance(_node, ast.ClassDef) and _node.name == "SharedMetricsStore":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name in PORTED_METHODS:
                _s = copy.copy(_sub)
                _s.decorator_list = []
                METHOD_CODE[_sub.name] = _compile_node(_s)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name) and _tg.id in CONSTANTS:
            CONST_CODE[_tg.id] = _compile_node(_node)

CONTRACT_CODE = {}
_CTREE = ast.parse(Path(CONTRACT_SRC).read_text(encoding="utf-8"))
for _node in _CTREE.body:
    if isinstance(_node, ast.Assign):
        _tg = _node.targets[0]
        if isinstance(_tg, ast.Name) and _tg.id in CONTRACT_CONSTANTS:
            CONTRACT_CODE[_tg.id] = compile(ast.Module(body=[_node], type_ignores=[]),
                                            CONTRACT_SRC, "exec", flags=_FLAGS, dont_inherit=True)

_missing = (set(PORTED_MODULE) - set(DEF_CODE)) | (set(PORTED_METHODS) - set(METHOD_CODE)) \
    | (set(CONSTANTS) - set(CONST_CODE)) | (set(CONTRACT_CONSTANTS) - set(CONTRACT_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


class RowDouble:
    def __init__(self, d):
        self._d = dict(d)

    def __getitem__(self, k):
        if k not in self._d:
            raise IndexError("No item with that key")
        return self._d[k]


class CursorDouble:
    def __init__(self, spec):
        self.spec = spec if isinstance(spec, dict) else {}

    def fetchone(self):
        r = self.spec.get("one")
        return RowDouble(r) if isinstance(r, dict) else r

    def fetchall(self):
        return [RowDouble(r) if isinstance(r, dict) else r for r in self.spec.get("all", [])]

    @property
    def rowcount(self):
        return self.spec.get("rowcount", 0)


class ConnectionDouble:
    def __init__(self, spec):
        self.spec = spec

    def execute(self, sql, params=()):
        return record("db.execute", {"sql": sql, "params": norm_value(params)})


class CMDouble:
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self.value

    def __exit__(self, *a):
        return False


class PathDouble:
    def __init__(self, s):
        self.s = s

    def mkdir(self, **k):
        return record("path.mkdir", {"path": self.s, **k})

    def touch(self, **k):
        return record("path.touch", {"path": self.s, **k})

    def chmod(self, mode):
        return record("path.chmod", {"path": self.s, "mode": mode})


class StoreDouble:
    pass


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, datetime):
        return v.isoformat()
    if isinstance(v, RowDouble):
        return {k: norm_value(x) for k, x in v._d.items()}
    if isinstance(v, ConnectionDouble):
        return norm_value(v.spec)
    if isinstance(v, CursorDouble):
        return norm_value(v.spec)
    if isinstance(v, PathDouble):
        return v.s
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items() if not callable(x)}
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
    if isinstance(v, types.FunctionType) and v.__closure__:
        fv = dict(zip(v.__code__.co_freevars, (c.cell_contents for c in v.__closure__)))
        if "state_key" in fv and "update" in fv:
            return {"__fn__": "_update_rollup_state", "state_key": fv["state_key"],
                    "update": proj_callable(fv["update"])}
    return norm_value(v)


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("message", marker.get("msg", ""))
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    if kind in base:
        cls = base[kind]
    elif kind == "CancelledError":
        import asyncio
        cls = asyncio.CancelledError
    elif hasattr(sqlite3, kind) and isinstance(getattr(sqlite3, kind), type) \
            and issubclass(getattr(sqlite3, kind), BaseException):
        cls = getattr(sqlite3, kind)
    elif hasattr(builtins, kind) and isinstance(getattr(builtins, kind), type) \
            and issubclass(getattr(builtins, kind), BaseException):
        cls = getattr(builtins, kind)
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
    if name in ("time.monotonic", "_utc_now"):
        return {}
    if name == "_isoformat":
        return {"value": norm_value(a[0])}
    if name == "store._parse_state_timestamp":
        return {"value": norm_value(a[0])}
    if name == "store._validate_counter":
        return {"metric_name": norm_value(a[0]), "dimensions": norm_value(a[1]),
                "resource": norm_value(a[2])}
    if name == "store._install_id":
        return {"connection": norm_value(a[0])}
    if name == "store._record_counter_in_transaction":
        d = {"connection": norm_value(a[0]), "metric_name": norm_value(a[1]),
             "dimensions": norm_value(a[2]), "resource": norm_value(a[3]),
             "period_start": norm_value(a[4])}
        if len(a) > 5:
            d["amount"] = norm_value(a[5])
        return d
    if name == "store._connection":
        return {"busy_timeout_ms": k.get("busy_timeout_ms", 250)}
    if name == "write_txn":
        return {"connection": norm_value(a[0])}
    if name == "contract.counter_dimensions_are_valid":
        return {"metric_name": norm_value(a[0]), "dimensions": norm_value(a[1])}
    if name == "contract.client_resource_is_valid":
        return {"resource": norm_value(a[0])}
    if name == "store._package_metric":
        return {"row": norm_value(a[0])}
    if name == "atexit.register":
        return {"callback": proj_callable(a[0])}
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

    def info(self, msg, *a, **k):
        return self._log("info", msg, a, k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, a, k)


def materialize_result(name, raw):
    # context-manager seams ALWAYS yield a CM double, even for a null script
    # (live _connection / write_txn are contextmanagers, never None).
    if name == "store._connection":
        return CMDouble(ConnectionDouble(copy.deepcopy(raw)))
    if name == "write_txn":
        return CMDouble(copy.deepcopy(raw))
    if raw is None:
        return None
    if isinstance(raw, dict) and "__callable__" in raw:
        return _make_callable_recorder(raw["__callable__"])
    if name == "db.execute":
        return CursorDouble(copy.deepcopy(raw))
    if name in ("_utc_now", "store._parse_state_timestamp"):
        if isinstance(raw, str):
            return datetime.fromisoformat(raw)
        return raw
    if isinstance(raw, (dict, list)):
        return copy.deepcopy(raw)
    return raw


def materialize_double(j):
    if isinstance(j, dict) and "__callable__" in j:
        return _make_callable_recorder(j["__callable__"])
    return j


def materialize_deferred(x, slf):
    if isinstance(x, dict) and "__callable__" in x:
        return _make_callable_recorder(x["__callable__"])
    if isinstance(x, dict) and x.get("__fn__") == "_update_rollup_state":
        upd = materialize_double(x.get("update"))
        sk = x.get("state_key")
        return lambda conn: NS["_update_rollup_state"](slf, conn, sk, upd)
    return x


# ── live namespace ──────────────────────────────────────────────────────────
_MOD = types.ModuleType("shm_live")
sys.modules["shm_live"] = _MOD
NS = _MOD.__dict__
NS.update({
    "__name__": "shm_live", "__builtins__": builtins,
    "json": json, "datetime": datetime, "timedelta": timedelta,
    "timezone": timezone, "UTC": timezone.utc, "sqlite3": sqlite3,
    "contextlib": contextlib, "Any": Any, "Callable": Callable,
    "logger": LoggerDouble(),
    "time": SimpleNamespace(monotonic=SeamFn("time.monotonic")),
    "atexit": SimpleNamespace(register=SeamFn("atexit.register")),
    "write_txn": SeamFn("write_txn"),
    "counter_dimensions_are_valid": SeamFn("contract.counter_dimensions_are_valid"),
    "client_resource_is_valid": SeamFn("contract.client_resource_is_valid"),
    "_utc_now": SeamFn("_utc_now"),
    "_isoformat": SeamFn("_isoformat"),
    "_DEFERRING_STORES_LOCK": contextlib.nullcontext(),
    "_DEFERRING_STORES": [],
})
for _nm in CONSTANTS:
    exec(CONST_CODE[_nm], NS)
for _nm in CONTRACT_CONSTANTS:
    exec(CONTRACT_CODE[_nm], NS)
for _nm in PORTED_MODULE:
    exec(DEF_CODE[_nm], NS)
for _nm in PORTED_METHODS:
    exec(METHOD_CODE[_nm], NS)
# _write was extracted with @contextmanager stripped; re-wrap it.
NS["_write"] = contextlib.contextmanager(NS["_write"])
# project the bound drain_deferred passed to atexit.register as a callable double.
NS["drain_deferred"].__seam_name__ = "store.drain_deferred"
NS["logger"] = LoggerDouble()


# ── per-case carriers ───────────────────────────────────────────────────────
class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))


def make_deferring_store(sid):
    def drain(*, busy_timeout_ms):
        return record("store.drain_deferred", {"store_id": sid, "busy_timeout_ms": busy_timeout_ms})
    return SimpleNamespace(id=sid, drain_deferred=drain)


def install_deferring_stores(st_in):
    stores = []
    for s in st_in.get("_DEFERRING_STORES", []):
        sid = s.get("id", "") if isinstance(s, dict) else ""
        stores.append(make_deferring_store(sid))
    NS["_DEFERRING_STORES"] = stores


def make_store(st_in):
    slf = StoreDouble()
    base = copy.deepcopy(st_in)
    base.pop("_DEFERRING_STORES", None)
    slf.__dict__.update(base)
    slf.__dict__.setdefault("_deferred", [])
    slf.__dict__.setdefault("_drain_at_exit", False)
    slf._deferred = [materialize_deferred(x, slf) for x in slf._deferred]
    slf._deferred_lock = contextlib.nullcontext()
    # claimed sibling seams
    slf._validate_counter = SeamFn("store._validate_counter")
    slf._record_counter_in_transaction = SeamFn("store._record_counter_in_transaction")
    slf._install_id = SeamFn("store._install_id")
    slf._parse_state_timestamp = SeamFn("store._parse_state_timestamp")
    slf._connection = SeamFn("store._connection")
    # ported methods bound REAL
    for nm in PORTED_METHODS:
        setattr(slf, nm, types.MethodType(NS[nm], slf))
    return slf


def proj_state(st_in, slf):
    out = {}
    for k in st_in:
        if k == "_deferred":
            out[k] = [proj_callable(x) for x in (slf._deferred if slf else [])]
        elif k == "_drain_at_exit":
            out[k] = getattr(slf, "_drain_at_exit", False) if slf else False
        elif k == "_DEFERRING_STORES":
            out[k] = [{"id": getattr(s, "id", "")} for s in NS["_DEFERRING_STORES"]]
        elif k == "id":
            out[k] = getattr(slf, "id", "") if slf else norm_value(st_in[k])
        else:
            out[k] = norm_value(st_in[k])
    return out


def drive(func, inp, holder):
    st_in = inp.get("state") or {}
    holder["proj"] = lambda: proj_state(st_in, None)
    install_deferring_stores(st_in)
    slf = make_store(st_in) if func in STORE_METHODS else None
    holder["proj"] = lambda: proj_state(st_in, slf)

    if func == "_compact_json":
        return NS["_compact_json"](inp.get("value"))
    if func == "_row_resource":
        r = inp.get("row")
        row = RowDouble(r) if isinstance(r, dict) else r
        return NS["_row_resource"](row)
    if func == "_ensure_private":
        NS["_ensure_private"](PathDouble(inp.get("path", "")), inp.get("mode", 0))
        return None
    if func == "drain_deferring_stores":
        NS["drain_deferring_stores"](inp.get("deadline", 0))
        return None
    if func == "_write":
        with slf._write(busy_timeout_ms=inp.get("busy_timeout_ms", 250)):
            pass
        return None
    if func == "install_snapshot_due":
        return slf.install_snapshot_due()
    if func == "_record_once_per_interval":
        return slf._record_once_per_interval(inp.get("state_key", ""), inp.get("metric_name", ""),
                                             inp.get("dimensions"), inp.get("resource"))
    if func == "record_install_snapshot":
        return slf.record_install_snapshot(inp.get("dimensions"), inp.get("resource"))
    if func == "_record_once_ever":
        return slf._record_once_ever(inp.get("state_key", ""), inp.get("metric_name", ""),
                                     inp.get("dimensions"), inp.get("resource"))
    if func == "record_milestone":
        return slf.record_milestone(inp.get("milestone", ""), inp.get("install_age_bucket", ""),
                                    inp.get("resource"))
    if func == "record_feature_adoption":
        return slf.record_feature_adoption(inp.get("feature", ""), inp.get("days_bucket", ""),
                                           inp.get("resource"))
    if func == "_latched":
        return slf._latched(inp.get("prefix", ""))
    if func == "recorded_milestones":
        return slf.recorded_milestones()
    if func == "recorded_features":
        return slf.recorded_features()
    if func == "record_counter_once_per_day":
        return slf.record_counter_once_per_day(inp.get("metric_name", ""), inp.get("dimensions"),
                                               inp.get("resource"))
    if func == "_update_rollup_state":
        conn = ConnectionDouble(inp.get("connection"))
        upd = materialize_double(inp.get("update"))
        slf._update_rollup_state(conn, inp.get("state_key", ""), upd)
        return None
    if func == "_write_or_defer":
        slf._write_or_defer(materialize_double(inp.get("write")))
        return None
    if func == "update_rollup_state":
        slf.update_rollup_state(inp.get("state_key", ""), materialize_double(inp.get("update")))
        return None
    if func == "drain_deferred":
        slf.drain_deferred(busy_timeout_ms=inp.get("busy_timeout_ms", 5000))
        return None
    if func == "_mark_packaged":
        conn = ConnectionDouble(inp.get("connection"))
        rows = [RowDouble(r) if isinstance(r, dict) else r for r in (inp.get("rows") or [])]
        rv = tuple(inp.get("resource_values") or [])
        NS["_mark_packaged"](conn, rows, inp.get("period_value", ""), rv)
        return None
    if func == "_package_metrics":
        cls = SimpleNamespace(_package_metric=SeamFn("store._package_metric"))
        rows = [RowDouble(r) if isinstance(r, dict) else r for r in (inp.get("rows") or [])]
        return NS["_package_metrics"](cls, rows)
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    inp = copy.deepcopy(inp)
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
                if any((("'" + n + "'") in msg) for n in names) or "StoreDouble" in msg:
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
        fx = _HERE.parent / "fixtures" / "shared_metrics_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())
