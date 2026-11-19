#!/usr/bin/env python3
"""Oracle for src/gateway/platforms/platform_registry_flow.c -- drives the LIVE
gateway/platform_registry.py FLOW defs (22 UNCLAIMED functions: the plugin-scope
inference leaves, core_ships_platform, registry construction, current_scope_key,
the scope-map / registration-state / prune accessors, the activation-env
record/read, the snapshot/CAS-restore pair, the pending-name walk, the
configured/loaded entry materializers, forget_failed_loads, _load_pending,
required_env_names, the off-loop deps CM and ensure_deps) under IDENTICAL
scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted (module
functions, class methods dedented + bound via MethodType, staticmethods plain,
the @contextmanager CM re-wrapped). Claimed siblings (get, _resolve_all) and all
foreign I/O (hermes_home_key, sys._getframe, tools.registry.plugin_scope_for_
callable, gateway.config.Platform bundled scan, logger, entry callables) are
recording seams fed the SAME scripts the C harness feeds; ported siblings compose
for REAL.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every except Exception guard exactly as live; the
(ImportError, AttributeError) catch swallows only that family. Per-case
BaseException catch -> __raise__ spec; scripted raises round-trip BARE via
identity match against Script.handed.

Value model: registry state travels flat; load-key tuples encode as compact JSON
[scope,name]; entry records echo the fixture-declared fields (incl __id__);
loader markers {"__loader__": id}; identity refs {"__ref__": id} interned per
case; callable markers {"__callable__": seam}; integral floats normalize to ints.
Set-valued state is sorted on both sides; the configured flows' trailing wanted
events (set iteration live) are sorted by name on both sides. ASCII fixture text.

Summary: 'ORACLE platform_registry_flow: N cases, M mismatches'.
"""
import ast
import builtins
import contextlib
import contextvars
import copy
import json
import math
import os
import sys
import threading
import types
import __future__
from dataclasses import dataclass, field, fields as dc_fields
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Awaitable, Callable, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "gateway" / "platform_registry.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "gateway" / "platform_registry.py")

ANN_FLAG = __future__.annotations.compiler_flag

MOD_FUNCS = ["_plugin_scope_from_callable", "_caller_plugin_scope", "core_ships_platform"]
METHOD_FUNCS = [
    "__init__", "current_scope_key", "_scope_maps", "_registration_state",
    "_prune_scope", "note_activation_env", "activation_env",
    "snapshot_registration", "restore_registration", "_pending_names",
    "pending_names", "configured_entries", "configured_load", "loaded_entries",
    "forget_failed_loads", "_load_pending", "required_env_names",
    "deps_resolved_off_loop", "ensure_deps",
]
FUNCS = MOD_FUNCS + METHOD_FUNCS
# instance methods bound via MethodType (current_scope_key / deps_resolved_off_loop
# are staticmethods -> plain).
INSTANCE_METHODS = [m for m in METHOD_FUNCS
                    if m not in ("current_scope_key", "deps_resolved_off_loop")]
STATE_KEYS = [
    "_entries", "_scoped_entries", "_deferred", "_scoped_deferred", "_inflight",
    "_inflight_loaders", "_cancelled_inflight", "_consumed_loaders",
    "_failed_loads", "_activation_env", "_loading",
]

CURRENT = {"ctx": None, "rec": None, "ids": {}, "stwin": {}}
NS = {}
METHODS = {}
PlatformEntry = None  # bound to the live dataclass by load_ns


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__seam_name__ = seam
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


_BUILTIN_EXC = {
    "TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
    "AttributeError": AttributeError, "KeyError": KeyError, "Exception": Exception,
    "TimeoutError": TimeoutError, "OSError": OSError, "LookupError": LookupError,
    "IndexError": IndexError, "StopIteration": StopIteration, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "NameError": NameError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "PythonFinalizationError": getattr(builtins, "PythonFinalizationError", RuntimeError),
    "AssertionError": AssertionError, "ArithmeticError": ArithmeticError,
    "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
    "FloatingPointError": FloatingPointError, "BufferError": BufferError,
    "EOFError": EOFError, "MemoryError": MemoryError, "ReferenceError": ReferenceError,
    "StopAsyncIteration": StopAsyncIteration, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError, "SystemError": SystemError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "UnboundLocalError": UnboundLocalError, "Warning": Warning, "UserWarning": UserWarning,
    "DeprecationWarning": DeprecationWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
    "SyntaxWarning": SyntaxWarning, "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "EncodingWarning": EncodingWarning, "ResourceWarning": ResourceWarning,
    "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
}
_BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
             "GeneratorExit": GeneratorExit}


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind in _BASE_EXC:
        cls = _BASE_EXC[kind]
    elif kind == "CancelledError":
        import asyncio
        cls = asyncio.CancelledError
    elif kind in _BUILTIN_EXC:
        cls = _BUILTIN_EXC[kind]
    else:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    return cls(msg)


_MISSING = object()


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
        return self._unwrap(name, val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, name, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, build_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def encode_key(t):
    return json.dumps([t[0], t[1]], separators=(",", ":"))


def decode_key(k):
    a = json.loads(k)
    return (a[0], a[1])


def norm_value(v):
    if isinstance(v, PlatformEntry):
        return norm_entry(v)
    if callable(v):
        lid = getattr(v, "__pr_loader__", None)
        if lid is not None:
            return {"__loader__": lid}
        return {"__callable__": getattr(v, "__pr_seam__", True)}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__pr")}
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(norm_value(x) for x in v)
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    return v


def norm_entry(e):
    out = {}
    for k in e.__pr_keys__:
        if k == "__id__":
            out[k] = e.__pr_id__
        else:
            out[k] = norm_value(getattr(e, k))
    return out


def norm_loader(l):
    return {"__loader__": l.__pr_loader__}


def norm_state(v):
    if isinstance(v, SimpleNamespace) and hasattr(v, "__pr_keys__"):
        return reg_echo(v)
    if isinstance(v, dict):
        return {k: norm_state(x) for k, x in v.items()}
    return norm_value(v)


def reg_echo(reg):
    out = {}
    for key in reg.__pr_keys__:
        v = getattr(reg, key)
        if key == "_entries":
            out[key] = {n: norm_entry(e) for n, e in v.items()}
        elif key == "_scoped_entries":
            out[key] = {s: {n: norm_entry(e) for n, e in inner.items()} for s, inner in v.items()}
        elif key == "_deferred":
            out[key] = {n: norm_loader(l) for n, l in v.items()}
        elif key == "_scoped_deferred":
            out[key] = {s: {n: norm_loader(l) for n, l in inner.items()} for s, inner in v.items()}
        elif key == "_inflight":
            out[key] = {encode_key(k): norm_value(val) for k, val in v.items()}
        elif key in ("_inflight_loaders", "_consumed_loaders"):
            out[key] = {encode_key(k): norm_loader(l) for k, l in v.items()}
        elif key in ("_cancelled_inflight", "_failed_loads"):
            out[key] = sorted(encode_key(k) for k in v)
        elif key == "_activation_env":
            out[key] = {n: sorted(fs) for n, fs in v.items()}
        elif key == "_loading":
            out[key] = sorted(encode_key(k) for k in v.get())
        else:
            out[key] = norm_value(v)
    return out


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def cb_proj(v):
    if callable(v):
        return {"__callable__": getattr(v, "__pr_seam__", True)}
    return norm_value(v)


def proj(name, a, k):
    if name in ("hermes_home_key", "platform_scan_bundled"):
        return {}
    if name == "self.get":
        return {"name": a[0]}
    if name == "self._resolve_all":
        return {"wanted": cb_proj(a[0])}
    if name == "sys._getframe":
        return {"depth": a[0]}
    if name == "plugin_scope_for_callable":
        return {"module": getattr(a[0], "__module__", None)}
    if name == "wanted":
        return {"name": a[0]}
    if name.startswith("logger."):
        d = {"msg": a[0],
             "args": [str(x) if isinstance(x, BaseException) else norm_value(x) for x in a[1:]]}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
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
        call.__pr_seam__ = name
        return call


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        return CURRENT["rec"]("logger." + level)(msg, *args, **kw)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def error(self, msg, *args, **kw):
        return self._log("error", msg, args, kw)


class SilentLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class PlatformDouble:
    def __init__(self, spec):
        spec = spec or {}
        self._member_names_ = list(spec.get("member_names", []))
        self._values = dict(spec.get("values", {}))

    def __getitem__(self, member):
        return SimpleNamespace(value=self._values[member])

    def _scan_bundled_plugin_platforms(self):
        raw = CURRENT["rec"]("platform_scan_bundled")()
        return (raw, {})


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            return SimpleNamespace(**{k: materialize_double(v) for k, v in (j["__attr__"] or {}).items()})
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        return {k: materialize_double(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize_double(v) for v in j]
    return j


def mat_callable(v):
    if isinstance(v, dict) and isinstance(v.get("__callable__"), str):
        return CURRENT["rec"](v["__callable__"])
    return copy.deepcopy(v)


def mat_entry(d):
    eid = d["__id__"]
    keys = list(d.keys())
    flds = {k: mat_callable(v) for k, v in d.items() if k != "__id__"}
    e = PlatformEntry(**flds)
    e.__pr_id__ = eid
    e.__pr_keys__ = keys
    CURRENT["ids"][eid] = e
    return e


def mat_loader(m):
    lid = m["__loader__"]

    def loader():
        pass
    loader.__pr_loader__ = lid
    CURRENT["ids"][lid] = loader
    return loader


def materialize_result(name, raw):
    if name == "self.get":
        if isinstance(raw, dict) and "__id__" in raw:
            return mat_entry(raw)
        return raw
    if name == "sys._getframe":
        if isinstance(raw, dict):
            return SimpleNamespace(**raw)
        return raw
    if isinstance(raw, dict) and "__attr__" in raw:
        return materialize_double(raw)
    return raw

# ---- live namespace seeding ----
def _exec_seg(src, node, ns, path):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _exec_class(src, node, ns, path):
    lines = src.splitlines()
    start = (node.decorator_list[0].lineno if node.decorator_list else node.lineno) - 1
    seg = "\n".join(lines[start:node.end_lineno])
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _find_method(tree, cls_name, name):
    for n in ast.walk(tree):
        if isinstance(n, ast.ClassDef) and n.name == cls_name:
            for m in n.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name == name:
                    return m
    return None


def load_ns():
    global NS
    mod = types.ModuleType("pr_live")
    sys.modules["pr_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "pr_live", "json": json, "contextlib": contextlib,
        "contextvars": contextvars, "threading": threading,
        "Any": Any, "Optional": Optional, "Callable": Callable, "Awaitable": Awaitable,
        "dataclass": dataclass, "field": field,
        "logger": LoggerDouble(),
        "sys": SimpleNamespace(_getframe=_stub_fn("sys._getframe")),
        "hermes_home_key": _stub_fn("hermes_home_key"),
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    # module constants needed at runtime (skip the logger Assign -- re-seeded above)
    for n in tree.body:
        if isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) and n.target.id == "_OFF_LOOP_DEPS_RESULT":
            exec(compile(ast.get_source_segment(src, n), SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
        if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id in ("_LoadKey", "_Loader") for t in n.targets):
            exec(compile(ast.get_source_segment(src, n), SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
    # PlatformEntry dataclass (decorators kept)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == "PlatformEntry":
            _exec_class(src, n, ns, SRC)
    globals()["PlatformEntry"] = ns["PlatformEntry"]
    # module functions
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in MOD_FUNCS:
            _exec_seg(src, n, ns, SRC)
    # class methods (dedent strips class indent; decorators dropped)
    for nm in METHOD_FUNCS:
        node = _find_method(tree, "PlatformRegistry", nm)
        if node is None:
            raise RuntimeError("missing method def: " + nm)
        _exec_seg(src, node, ns, SRC)
        METHODS[nm] = ns[nm]
    # deps_resolved_off_loop's @contextlib.contextmanager is dropped by segment
    # extraction; re-apply it so `with` works.
    ns["deps_resolved_off_loop"] = contextlib.contextmanager(ns["deps_resolved_off_loop"])
    METHODS["deps_resolved_off_loop"] = ns["deps_resolved_off_loop"]
    # stub packages for the in-function imports (per-case attributes installed in drive)
    tools_pkg = types.ModuleType("tools"); tools_pkg.__path__ = []
    sys.modules["tools"] = tools_pkg
    gw_pkg = types.ModuleType("gateway"); gw_pkg.__path__ = []
    sys.modules["gateway"] = gw_pkg
    return ns


def install_import_stubs(inp):
    tools_pkg = sys.modules["tools"]
    if inp.get("import_fails"):
        sys.modules["tools.registry"] = None
        tools_pkg.registry = None
    else:
        tr = types.ModuleType("tools.registry")
        if inp.get("attr_missing"):
            tr.registry = SimpleNamespace()
        else:
            tr.registry = SimpleNamespace(plugin_scope_for_callable=_stub_fn("plugin_scope_for_callable"))
        sys.modules["tools.registry"] = tr
        tools_pkg.registry = tr
    gw_pkg = sys.modules["gateway"]
    if inp.get("platform_import_fails"):
        sys.modules["gateway.config"] = None
        gw_pkg.config = None
    else:
        gc = types.ModuleType("gateway.config")
        gc.Platform = PlatformDouble(inp.get("platform"))
        sys.modules["gateway.config"] = gc
        gw_pkg.config = gc


def mat_state(inp):
    state = inp.get("state") or {}
    reg = SimpleNamespace()
    reg.__pr_keys__ = list(state.keys())
    for key, val in state.items():
        if key == "_entries":
            setattr(reg, key, {n: mat_entry(d) for n, d in val.items()})
        elif key == "_scoped_entries":
            setattr(reg, key, {s: {n: mat_entry(d) for n, d in inner.items()} for s, inner in val.items()})
        elif key == "_deferred":
            setattr(reg, key, {n: mat_loader(m) for n, m in val.items()})
        elif key == "_scoped_deferred":
            setattr(reg, key, {s: {n: mat_loader(m) for n, m in inner.items()} for s, inner in val.items()})
        elif key == "_inflight":
            setattr(reg, key, {decode_key(k): v for k, v in val.items()})
        elif key in ("_inflight_loaders", "_consumed_loaders"):
            setattr(reg, key, {decode_key(k): mat_loader(m) for k, m in val.items()})
        elif key in ("_cancelled_inflight", "_failed_loads"):
            setattr(reg, key, set(decode_key(k) for k in val))
        elif key == "_activation_env":
            setattr(reg, key, {n: frozenset(arr) for n, arr in val.items()})
        elif key == "_loading":
            cv = contextvars.ContextVar("pr_loading", default=frozenset())
            cv.set(frozenset(decode_key(k) for k in val))
            setattr(reg, key, cv)
        else:
            setattr(reg, key, copy.deepcopy(val))
    reg._lock = SilentLock()
    for nm in INSTANCE_METHODS:
        setattr(reg, nm, types.MethodType(METHODS[nm], reg))
    reg.current_scope_key = METHODS["current_scope_key"]
    reg.get = CURRENT["rec"]("self.get")
    reg._resolve_all = CURRENT["rec"]("self._resolve_all")
    return reg


def mat_ref(x):
    if x is None:
        return None
    if isinstance(x, dict):
        if "__ref__" in x:
            return CURRENT["ids"][x["__ref__"]]
        if "__id__" in x:
            return mat_entry(x)
        if "__loader__" in x:
            return mat_loader(x)
    return x


def mat_ref_list(arr):
    return [mat_ref(x) for x in (arr or [])]


def mat_wanted(inp):
    w = inp.get("wanted")
    if isinstance(w, dict) and isinstance(w.get("__callable__"), str):
        return CURRENT["rec"](w["__callable__"])
    return None


def drive(func, inp):
    CURRENT["ids"] = {}
    CURRENT["stwin"] = {}
    install_import_stubs(inp)
    if func == "_plugin_scope_from_callable":
        cb = materialize_double(inp.get("callback"))
        return NS[func](cb), {}
    if func == "_caller_plugin_scope":
        return NS[func](), {}
    if func == "core_ships_platform":
        return NS[func](inp.get("name")), {}
    if func == "deps_resolved_off_loop":
        initial = (inp.get("state") or {}).get("off_loop_deps")
        var = NS["_OFF_LOOP_DEPS_RESULT"]
        token = var.set(initial)
        CURRENT["stwin"] = {"off_loop_deps": initial}
        try:
            with NS["deps_resolved_off_loop"](inp.get("deps_ok")):
                inside = var.get()
                br = inp.get("body_raise")
                if isinstance(br, dict) and "__raise__" in br:
                    raise build_exc(br)
            after = var.get()
            return {"inside": inside, "after": after}, CURRENT["stwin"]
        finally:
            var.reset(token)
    if func == "__init__":
        reg = SimpleNamespace()
        METHODS["__init__"](reg)
        reg.__pr_keys__ = list(STATE_KEYS)
        CURRENT["stwin"] = reg
        return None, reg
    reg = mat_state(inp)
    CURRENT["stwin"] = reg
    if func == "current_scope_key":
        return reg.current_scope_key(), reg
    if func == "_scope_maps":
        return reg._scope_maps(inp.get("scope"), create=bool(inp.get("create"))), reg
    if func == "_registration_state":
        return reg._registration_state(inp.get("scope"), inp.get("name"), create=bool(inp.get("create"))), reg
    if func == "_prune_scope":
        reg._prune_scope(inp.get("scope"))
        return None, reg
    if func == "note_activation_env":
        reg.note_activation_env(inp.get("name"), inp.get("env_names"))
        return None, reg
    if func == "activation_env":
        return reg.activation_env(inp.get("name")), reg
    if func == "snapshot_registration":
        return reg.snapshot_registration(inp.get("name"), scope=inp.get("scope")), reg
    if func == "restore_registration":
        current = mat_ref_list(inp.get("current"))
        previous = mat_ref_list(inp.get("previous"))
        return reg.restore_registration(inp.get("name"), current, previous, scope=inp.get("scope")), reg
    if func == "_pending_names":
        return reg._pending_names(inp.get("active_scope")), reg
    if func == "pending_names":
        return reg.pending_names(), reg
    if func == "configured_entries":
        return reg.configured_entries(mat_wanted(inp)), reg
    if func == "configured_load":
        return reg.configured_load(mat_wanted(inp)), reg
    if func == "loaded_entries":
        return reg.loaded_entries(), reg
    if func == "forget_failed_loads":
        reg.forget_failed_loads(inp.get("scope"))
        return None, reg
    if func == "_load_pending":
        return reg._load_pending(inp.get("scope"), inp.get("name")), reg
    if func == "required_env_names":
        return reg.required_env_names(), reg
    if func == "ensure_deps":
        var = NS["_OFF_LOOP_DEPS_RESULT"]
        token = var.set(inp.get("off_loop_deps"))
        try:
            return reg.ensure_deps(inp.get("name")), reg
        finally:
            var.reset(token)
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    CURRENT["rec"] = ctx.recorder
    CURRENT["stwin"] = {}
    try:
        ret, stwin = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": norm_state(stwin)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": norm_state(CURRENT["stwin"])}


def sort_trailing(trace, name):
    if not isinstance(trace, list) or not name:
        return
    n = len(trace)
    start = n
    while start > 0 and trace[start - 1].get("ev") == name:
        start -= 1
    sub = trace[start:]
    if len(sub) > 1:
        sub.sort(key=lambda ev: (ev.get("args") or {}).get("name", ""))
        trace[start:] = sub


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
        if func in ("configured_load", "configured_entries"):
            wseam = (inp.get("wanted") or {}).get("__callable__", "wanted")
            sort_trailing(want["trace"], wseam)
            sort_trailing(got["trace"], wseam)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE platform_registry_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
