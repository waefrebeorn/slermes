#!/usr/bin/env python3
"""Oracle for src/agent/conversation_compression_flow2.c — drives the LIVE
agent/conversation_compression.py second-wave FLOW defs (20 UNCLAIMED functions:
the commit-fence deadline arming/extension, the activity-heartbeat suppression /
touch, the compaction-lifecycle terminal edge, the durable-lease refresher /
holder release / nested-finally release, the attempt telemetry-hold CM and
compressor restore, the context-engine session-db binding / close, the
aux-threshold lowering notice, the custom-provider config read, and five
run_compress_context_with_progress_timeout / summary-dispatch closures) under
IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted (module
functions, class methods dedented + bound via MethodType, and read-only closures
exec'd with their free variables seeded as globals; the one nonlocal closure
_stop_heartbeat is driven through a maker wrapper). Claimed siblings
(_swallow, _emit_compaction_done, _emit_feasibility_notice,
_release_compression_lock, _restore_compressor_attempt_state,
clear_cancelled_lock_release, finish_lock_setup, the fence is_cancelled /
deadline_exceeded properties) and all foreign I/O are recording seams fed the
SAME scripts the C harness feeds; ported siblings compose for REAL.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) propagates past
every except Exception guard exactly as live. Per-case BaseException catch ->
__raise__ spec; scripted raises round-trip BARE via identity match against
Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (state echo
RE-WRAPS); class-tagged doubles {"__class__": "Fence"|"ContextCompressor"|
"Persistence"} <-> SimpleNamespace subclasses (Fence carries the seamed
is_cancelled / deadline_exceeded properties; CC/Persistence give type()/isinstance
identity); callable doubles {"__callable__": "<seam>"}; identity refs
{"__ref__": "<id>"}; integral floats normalize to ints on both sides. ASCII
fixture text; POSIX paths (PurePosixPath).

Summary: 'ORACLE conversation_compression_flow2: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import re as _re
import sys
import types
import contextlib
import concurrent.futures
import __future__
from enum import Enum
from pathlib import Path, PurePosixPath
from types import SimpleNamespace
from typing import Any, Callable, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "conversation_compression.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "agent" / "conversation_compression.py")
ACT_SRC = str(QUARRY / "agent" / "session_activity.py")

ANN_FLAG = __future__.annotations.compiler_flag

# module-level functions
MOD_FUNCS = [
    "context_engine_session_db", "close_context_engine_session_db",
    "_lower_threshold_to_aux_context", "_configured_custom_providers",
    "_hold_attempt_telemetry",
]
# class methods (extracted dedented, bound per case)
METHOD_FUNCS = [
    "set_precommit_deadline_monotonic", "extend_deadline",
    "_should_suppress", "_touch", "complete", "status_emitted",
    "start_refresher", "release_holder_only", "release", "restore_compressor",
]
# read-only closures (free vars seeded as globals per case)
CLOSURE_FUNCS = [
    "_fence_gated_worker", "_is_unchanged_snapshot",
    "_compression_cancel_requested", "_resolve_fallback_prompt",
]
FUNCS = MOD_FUNCS + METHOD_FUNCS + CLOSURE_FUNCS + ["_stop_heartbeat"]

SEAM_GLOBALS = [
    "normalize_activity_provenance", "_emit_compaction_done",
    "_emit_feasibility_notice", "_release_compression_lock",
    "_restore_compressor_attempt_state",
]

CURRENT = {"ctx": None, "rec": None, "ids": {}}
NS = {}
METHODS = {}


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


def norm_value(v):
    if isinstance(v, PurePosixPath):
        return str(v)
    if callable(v):
        return {"__callable__": getattr(v, "__cp2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__cp2_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__cp2") and k not in inj}
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
    """State echo: RE-WRAP attribute doubles into {"__attr__": ...} form so the
    C state window (which keeps the wrapper) matches. Injected seam scaffolding
    is excluded."""
    if isinstance(v, PurePosixPath):
        return str(v)
    if callable(v):
        return {"__callable__": getattr(v, "__cp2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__cp2_injected__", set())
        inner = {k: norm_state(x) for k, x in vars(v).items()
                 if not k.startswith("__cp2") and k not in inj}
        cls = getattr(v, "__cp2_class__", None)
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
    if isinstance(v, SimpleNamespace) and hasattr(v, "__cp2_raw__"):
        return norm_state(v)
    return norm_value(v)


def cb_proj(v):
    return {"__callable__": True} if callable(v) else norm_value(v)


def prov_value(v):
    if isinstance(v, Enum):
        return v.value
    return v


def proj(name, a, k):
    if name in ("lock_acquire",):
        return {"blocking": k.get("blocking")}
    if name in ("lock_release", "monotonic", "fence_is_cancelled", "fence_deadline_exceeded",
                "refresher_start", "refresher_stop", "finish_lock_setup", "event_is_set",
                "session_auxiliary_get", "load_config_readonly", "system_prompt_fallback",
                "plugin_db_close"):
        return {}
    if name == "swallow":
        return {"message": a[0] if a else k.get("message"), "exc_info": k.get("exc_info")}
    if name.startswith("logger."):
        d = {"msg": a[0], "args": list(a[1:])}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "normalize_activity_provenance":
        return {"provenance": a[0]}
    if name == "touch_activity":
        return {"desc": a[0], "provenance": prov_value(k.get("provenance")),
                "force_persist": k.get("force_persist")}
    if name == "_emit_compaction_done":
        return {"agent": double_proj(a[0])}
    if name == "refresher_ctor":
        return {"db": double_proj(a[0]), "sid": a[1], "holder": a[2], "ttl": a[3],
                "refresh_interval": a[4], "store": double_proj(k.get("store"))}
    if name == "_release_compression_lock":
        return {"lease": double_proj(a[0])}
    if name == "clear_cancelled_lock_release":
        return {"release": cb_proj(a[0])}
    if name == "_restore_compressor_attempt_state":
        return {"compressor": double_proj(a[0]), "snapshot": norm_value(a[1]),
                "durable_cooldown_authoritative": k.get("durable_cooldown_authoritative"),
                "durable_cooldown_state": norm_value(k.get("durable_cooldown_state")),
                "attempt_generation": k.get("attempt_generation")}
    if name == "engine_session_db":
        return {"compressor": double_proj(a[0]), "path": a[1], "read_only": k.get("read_only")}
    if name == "effective_threshold_percent":
        return {"arg0": a[0], "arg1": a[1]}
    if name == "compute_threshold_tokens":
        return {"arg0": a[0], "arg1": a[1], "arg2": a[2]}
    if name == "_emit_feasibility_notice":
        return {"agent": double_proj(a[0]), "msg": a[1]}
    if name == "get_compatible_custom_providers":
        return {"arg0": norm_value(a[0])}
    if name == "worker":
        return {"worker_fence": double_proj(a[0])}
    if name == "heartbeat_stop":
        return {"desc": a[0]}
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
        call.__cp2_seam__ = name
        return call


class FenceDouble(SimpleNamespace):
    @property
    def is_cancelled(self):
        return CURRENT["rec"]("fence_is_cancelled")()

    @property
    def deadline_exceeded(self):
        return CURRENT["rec"]("fence_deadline_exceeded")()


class ContextCompressorDouble(SimpleNamespace):
    pass


class PersistenceDouble(SimpleNamespace):
    pass


def materialize_double(j):
    if isinstance(j, dict):
        if "__path__" in j:
            return PurePosixPath(j["__path__"])
        if "__attr__" in j:
            inner = j["__attr__"] or {}
            cls = j.get("__class__")
            kids = {k: materialize_double(v) for k, v in inner.items()}
            if cls == "Fence":
                obj = FenceDouble(**kids)
            elif cls == "ContextCompressor":
                obj = ContextCompressorDouble(**kids)
            elif cls == "Persistence":
                obj = PersistenceDouble(**kids)
            else:
                obj = SimpleNamespace(**kids)
            obj.__cp2_raw__ = j
            obj.__cp2_class__ = cls
            obj.__cp2_injected__ = set()
            return obj
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        if "__ref__" in j:
            return CURRENT["ids"][j["__ref__"]]
        return {k: materialize_double(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize_double(v) for v in j]
    return j


def _inject(obj, **kw):
    """Attach observation scaffolding (seam handles / silent locks) that live
    reads but which is NOT modeled state; excluded from the state echo."""
    for k, v in kw.items():
        setattr(obj, k, v)
    inj = getattr(obj, "__cp2_injected__", None)
    if inj is None:
        inj = set()
        obj.__cp2_injected__ = inj
    inj.update(kw.keys())


def materialize_result(name, raw):
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


class TimeDouble:
    def monotonic(self):
        return CURRENT["rec"]("monotonic")()


class SwallowCM:
    """_swallow is a claimed sibling CM: the factory call is a seam; the
    except-Exception -> logger.debug + suppress behaviour is transcribed."""
    def __init__(self, a, k):
        ctx = CURRENT["ctx"]
        self.message = a[0] if a else k.get("message", "")
        self.exc_info = bool(k.get("exc_info", False))
        args = norm_args(proj("swallow", a, k))
        raw = ctx.script.next("swallow")
        if isinstance(raw, Raise):
            ctx.trace.append({"ev": "swallow", "args": args, "ret": raw.spec})
            raise raw.exc
        ctx.trace.append({"ev": "swallow", "args": args, "ret": norm_value(raw)})
        self.marker = raw

    def __enter__(self):
        return self.marker

    def __exit__(self, et, ev, tb):
        if et is not None and issubclass(et, Exception):
            if self.exc_info:
                CURRENT["rec"]("logger.debug")(self.message, exc_info=True)
            else:
                CURRENT["rec"]("logger.debug")(self.message, ev)
            return True
        return False


def _swallow_factory(*a, **k):
    return SwallowCM(a, k)


# ---- live namespace seeding ----
def _exec_seg(src, node, ns, path):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _find_def(tree, name):
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            return n
    return None


def load_ns():
    global NS
    mod = types.ModuleType("cp2_live")
    sys.modules["cp2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cp2_live", "json": json, "re": _re, "math": math,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "Dict": dict, "List": list, "Tuple": tuple,
        "contextlib": contextlib, "concurrent": sys.modules.get("concurrent") or __import__("concurrent"),
        "copy": copy, "Path": Path, "Enum": Enum,
        "logger": LoggerDouble(), "time": TimeDouble(),
        "_swallow": _swallow_factory,
        "_TERMINAL_COMPRESSION_PROVENANCES": frozenset(
            {"agent.compression_timeout", "agent.compression_cooldown"}),
        "_ATTEMPT_TELEMETRY_FIELDS": (
            "_last_compression_telemetry", "_active_compression_telemetry",
            "_compression_telemetry_seed"),
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    # ActivityProvenance enum from session_activity (str Enum, no heavy deps)
    asrc = Path(ACT_SRC).read_text(encoding="utf-8")
    atree = ast.parse(asrc)
    for n in atree.body:
        if isinstance(n, ast.ClassDef) and n.name == "ActivityProvenance":
            _exec_seg(asrc, n, ns, ACT_SRC)
    # module functions (source order)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in MOD_FUNCS:
            _exec_seg(src, n, ns, SRC)
    # class methods (dedent strips class indent); store unbound
    for nm in METHOD_FUNCS:
        node = _find_def(tree, nm)
        if node is None:
            raise RuntimeError("missing method def: " + nm)
        _exec_seg(src, node, ns, SRC)
        METHODS[nm] = ns[nm]
    # read-only closures: exec at module level (free vars resolve as globals)
    for nm in CLOSURE_FUNCS:
        node = _find_def(tree, nm)
        if node is None:
            raise RuntimeError("missing closure def: " + nm)
        _exec_seg(src, node, ns, SRC)
    # nonlocal closure _stop_heartbeat: wrap in a maker that binds the free var
    node = _find_def(tree, "_stop_heartbeat")
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    wrapper = ("def __mk_stop_heartbeat__(_activity_heartbeat):\n"
               + textwrap.indent(seg, "    ")
               + "\n    return _stop_heartbeat, lambda: _activity_heartbeat\n")
    exec(compile(wrapper, SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
    # module-global seams
    for nm in SEAM_GLOBALS:
        ns[nm] = _stub_fn(nm)
    # _CompressionLockLeaseRefresher ctor -> refresher_ctor seam (returns the
    # scripted candidate double whose .start() is the refresher_start seam).
    ns["_CompressionLockLeaseRefresher"] = _stub_fn("refresher_ctor")
    # _hold_attempt_telemetry's @contextlib.contextmanager decorator is dropped
    # by AST segment extraction (decorators precede node.lineno); re-apply it.
    ns["_hold_attempt_telemetry"] = contextlib.contextmanager(ns["_hold_attempt_telemetry"])
    # class-identity stubs (type()/isinstance) + seamed staticmethods
    ns["ContextCompressor"] = ContextCompressorDouble
    ns["Persistence"] = PersistenceDouble
    ContextCompressorDouble._compute_threshold_tokens = staticmethod(_stub_fn("compute_threshold_tokens"))
    ContextCompressorDouble._effective_threshold_percent = staticmethod(_stub_fn("effective_threshold_percent"))
    # in-function import stubs (overwrite parent packages so the real quarry
    # submodule cannot leak past the stub).
    agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    cc = types.ModuleType("agent.context_compressor")
    cc.ContextCompressor = ContextCompressorDouble
    sys.modules["agent.context_compressor"] = cc
    agent_pkg.context_compressor = cc
    pa = types.ModuleType("persistence_assembly")
    pa.Persistence = PersistenceDouble
    sys.modules["persistence_assembly"] = pa
    hs = types.ModuleType("hermes_state")
    hs._engine_session_db = _stub_fn("engine_session_db")
    sys.modules["hermes_state"] = hs
    atc = types.ModuleType("agent.auxiliary_task_config")
    atc._SESSION_AUXILIARY = SimpleNamespace(get=_stub_fn("session_auxiliary_get"))
    sys.modules["agent.auxiliary_task_config"] = atc
    agent_pkg.auxiliary_task_config = atc
    hc = types.ModuleType("hermes_cli"); hc.__path__ = []
    sys.modules["hermes_cli"] = hc
    hcc = types.ModuleType("hermes_cli.config")
    hcc.get_compatible_custom_providers = _stub_fn("get_compatible_custom_providers")
    hcc.load_config_readonly = _stub_fn("load_config_readonly")
    sys.modules["hermes_cli.config"] = hcc
    hc.config = hcc
    return ns


def _state(inp, key):
    st = inp.get("state") or {}
    return copy.deepcopy(st.get(key))


def _mat_state(inp, key):
    return materialize_double(_state(inp, key))


def _bind(obj, nm):
    setattr(obj, nm, types.MethodType(METHODS[nm], obj))
    inj = getattr(obj, "__cp2_injected__", None)
    if inj is None:
        inj = set(); obj.__cp2_injected__ = inj
    inj.add(nm)


def _win(**kw):
    """Register the live state carriers so run_case can echo the mutation
    history even when the live call raises (the C harness emits the mutated
    state window on a raise too)."""
    CURRENT["stwin"] = kw
    return kw


def drive(func, inp):
    """Return (ret, stwin) where stwin maps state keys to LIVE doubles (mutated
    in place). Raises on live exception (caught by run_case, which still echoes
    CURRENT['stwin'])."""
    CURRENT["ids"] = {}
    CURRENT["stwin"] = {}
    if func == "set_precommit_deadline_monotonic":
        fence = _mat_state(inp, "fence")
        _win(fence=fence)
        METHODS[func](fence, inp.get("deadline"))
        return None, CURRENT["stwin"]
    if func == "extend_deadline":
        fence = _mat_state(inp, "fence")
        _inject(fence, _lock=SimpleNamespace(acquire=CURRENT["rec"]("lock_acquire"),
                                             release=CURRENT["rec"]("lock_release")))
        _win(fence=fence)
        ret = METHODS[func](fence, inp.get("seconds"))
        return ret, CURRENT["stwin"]
    if func == "_should_suppress":
        hb = _mat_state(inp, "hb")
        _win(hb=hb)
        ret = METHODS[func](hb)
        return ret, CURRENT["stwin"]
    if func == "_touch":
        hb = _mat_state(inp, "hb")
        _bind(hb, "_should_suppress")
        _win(hb=hb)
        METHODS[func](hb, inp.get("desc", ""),
                      allow_terminal_overwrite=bool(inp.get("allow_terminal_overwrite")),
                      force_persist=bool(inp.get("force_persist")))
        return None, CURRENT["stwin"]
    if func == "complete":
        lc = _mat_state(inp, "lifecycle")
        _win(lifecycle=lc)
        METHODS[func](lc, force_terminal=bool(inp.get("force_terminal")))
        return None, CURRENT["stwin"]
    if func == "status_emitted":
        lease = _mat_state(inp, "lease")
        _win(lease=lease)
        ret = METHODS[func](lease)
        return ret, CURRENT["stwin"]
    if func == "start_refresher":
        lease = _mat_state(inp, "lease")
        _inject(lease, _release_guard=contextlib.nullcontext())
        _win(lease=lease)
        METHODS[func](lease)
        return None, CURRENT["stwin"]
    if func == "release_holder_only":
        lease = _mat_state(inp, "lease")
        _inject(lease, _release_guard=contextlib.nullcontext())
        _win(lease=lease)
        METHODS[func](lease)
        return None, CURRENT["stwin"]
    if func == "release":
        lease = _mat_state(inp, "lease")
        _inject(lease, _release_guard=contextlib.nullcontext())
        _bind(lease, "release_holder_only")
        # finish_lock_setup is a CLAIMED sibling -> seam method on the lease
        _inject(lease, finish_lock_setup=CURRENT["rec"]("finish_lock_setup"))
        lc = lease._lifecycle
        _bind(lc, "complete")
        fence = getattr(lease, "_commit_fence", None)
        if fence is not None:
            _inject(fence, clear_cancelled_lock_release=CURRENT["rec"]("clear_cancelled_lock_release"))
        _win(lease=lease)
        METHODS[func](lease)
        return None, CURRENT["stwin"]
    if func == "restore_compressor":
        attempt = _mat_state(inp, "attempt")
        compressor = materialize_double(copy.deepcopy(inp.get("compressor")))
        _win(attempt=attempt)
        METHODS[func](attempt, compressor)
        return None, CURRENT["stwin"]
    if func == "context_engine_session_db":
        agent = _mat_state(inp, "agent")
        session_db = materialize_double(copy.deepcopy(inp.get("session_db")))
        _win(agent=agent)
        ret = NS[func](agent, session_db)
        return ret, CURRENT["stwin"]
    if func == "close_context_engine_session_db":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        NS[func](agent)
        return None, CURRENT["stwin"]
    if func == "_lower_threshold_to_aux_context":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        NS[func](agent, aux_model=inp.get("aux_model", ""), aux_context=inp.get("aux_context"),
                 aux_provider=inp.get("aux_provider", ""), aux_base_url=inp.get("aux_base_url", ""))
        return None, CURRENT["stwin"]
    if func == "_configured_custom_providers":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        ret = NS[func](agent)
        return ret, CURRENT["stwin"]
    if func == "_hold_attempt_telemetry":
        compressor = _mat_state(inp, "compressor")
        _win(compressor=compressor)
        reset = inp.get("reset") or {}
        with NS[func](compressor):
            for k, v in reset.items():
                setattr(compressor, k, materialize_double(v))
        return None, CURRENT["stwin"]
    if func == "_fence_gated_worker":
        messages = _state(inp, "messages")
        NS["messages"] = messages
        NS["worker"] = materialize_double(copy.deepcopy(inp.get("worker")))
        fence = materialize_double(copy.deepcopy(inp.get("worker_fence")))
        ret = NS[func](fence)
        return ret, _win()
    if func == "_is_unchanged_snapshot":
        messages = _state(inp, "messages")
        mid = inp.get("messages_id", "m1")
        CURRENT["ids"][mid] = messages
        NS["messages"] = messages
        result = materialize_double(copy.deepcopy(inp.get("result")))
        ret = NS[func](result)
        return ret, _win()
    if func == "_compression_cancel_requested":
        NS["hard_cancel_event"] = materialize_double(copy.deepcopy(inp.get("hard_cancel_event")))
        NS["commit_fence"] = materialize_double(copy.deepcopy(inp.get("commit_fence")))
        ret = NS[func]()
        return ret, _win()
    if func == "_resolve_fallback_prompt":
        NS["system_prompt_fallback"] = materialize_double(copy.deepcopy(inp.get("system_prompt_fallback")))
        ret = NS[func]()
        return ret, _win()
    if func == "_stop_heartbeat":
        hb = materialize_double(copy.deepcopy((inp.get("state") or {}).get("heartbeat")))
        fn, getter = NS["__mk_stop_heartbeat__"](hb)
        _win(heartbeat=hb)
        fn(inp.get("desc", ""))
        CURRENT["stwin"]["heartbeat"] = getter()
        return None, CURRENT["stwin"]
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
    print("ORACLE conversation_compression_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())