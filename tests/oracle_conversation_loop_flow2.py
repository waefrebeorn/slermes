#!/usr/bin/env python3
"""Oracle for src/agent/conversation_loop_flow2.c — drives the LIVE
agent/conversation_loop.py UNCLAIMED-remainder FLOW defs (the review-fork
input-budget gate _review_input_budget_exhausted, the completed-compaction rearm
probe _should_rearm_compression_budget, the billing terminal prefix
_billing_terminal_label, the structured billing terminal result
_billing_failure_result, the per-turn loop-state dataclass _LoopState, and the
161-line outer turn driver _run_conversation_turn) under IDENTICAL scripted
universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted; claimed
siblings (_decode_inline_moa_turn, _preflight_timeout_result, _turn_without_loop,
_run_phase, _run_api_retry_loop, _finalize_from_state, _billing_or_entitlement_message,
_billing_block_dict) and all foreign I/O are recording seams fed the SAME scripts
the C harness feeds; ported siblings compose for REAL (_billing_terminal_label
inside _billing_failure_result, _LoopState inside _run_conversation_turn). The
seamed _run_phase is modeled by its contract: the scripted verdict is copied back
onto the loop-state double (skip action/result; handle_api_error latch sets True).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) propagates past
every except Exception guard exactly as live (real builtin classes). Per-case
BaseException catch -> __raise__ spec; scripted raises round-trip BARE via
identity match against Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (state echo
RE-WRAPS); the _LoopState dataclass projects as a PLAIN field dict (fields()
order); callable doubles {"__callable__": "<seam>"}; phase verdicts are real
dataclasses (fields() order == script key order); integral floats normalize to
ints on both sides; set() state writes project to sorted arrays. ASCII fixture text.

Summary: 'ORACLE conversation_loop_flow2: N cases, M mismatches'.
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
from dataclasses import dataclass, field, fields, make_dataclass, MISSING
from pathlib import Path
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
if not (QUARRY / "agent" / "conversation_loop.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "agent" / "conversation_loop.py")

ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "_review_input_budget_exhausted", "_should_rearm_compression_budget",
    "_billing_terminal_label", "_billing_failure_result", "_run_conversation_turn",
]

# claimed siblings seamed (live name -> seam channel)
SEAM_GLOBALS = {
    "_decode_inline_moa_turn": "decode_inline_moa_turn",
    "_preflight_timeout_result": "preflight_timeout_result",
    "_turn_without_loop": "turn_without_loop",
    "_run_api_retry_loop": "run_api_retry_loop",
    "_finalize_from_state": "finalize_from_state",
    "_billing_or_entitlement_message": "billing_or_entitlement_message",
    "_billing_block_dict": "billing_block_dict",
    "begin_fast_mode_turn": "begin_fast_mode_turn",
    "held_stream_transforms": "held_stream_transforms",
    "build_turn_context": "build_turn_context",
    "play_prelude": "play_prelude",
    "_restore_or_build_system_prompt": "restore_or_build_system_prompt",
    "_ra": "ra",
    "_install_safe_stdio": "install_safe_stdio",
    "_sanitize_surrogates": "sanitize_surrogates",
    "_summarize_user_message_for_log": "summarize_user_message_for_log",
    "set_session_context": "set_session_context",
    "set_current_write_origin": "set_current_write_origin",
    "TurnRetryState": "turn_retry_state",
}
PHASE_NAMES = ["begin_iteration", "prepare_iteration", "route_change_offload",
               "assemble_api_request", "run_preflight_gate", "announce_api_call",
               "apply_retry_restarts", "normalize_model_response", "run_tool_round",
               "finish_text_response", "handle_outer_loop_error"]

CURRENT = {"ctx": None, "rec": None, "ids": {}, "stwin": {}}
NS = {}


class PreflightCompressionTimedOut(Exception):
    pass


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__seam_name__ = seam
    f.__name__ = seam
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "PreflightCompressionTimedOut":
        return PreflightCompressionTimedOut(msg)
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


def _dc_to_dict(v):
    return {f.name: getattr(v, f.name) for f in fields(v)}


def _set_norm(v, fn):
    items = [fn(x) for x in v]
    return sorted(items, key=lambda x: json.dumps(x, sort_keys=True, ensure_ascii=False, default=str))


def norm_value(v):
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {k: norm_value(x) for k, x in _dc_to_dict(v).items()}
    if callable(v):
        return {"__callable__": getattr(v, "__cl_seam__", getattr(v, "__seam_name__", True))}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__cl_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__cl") and k not in inj}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, (set, frozenset)):
        return _set_norm(v, norm_value)
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
        return {"__callable__": getattr(v, "__cl_seam__", getattr(v, "__seam_name__", True))}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__cl_injected__", set())
        inner = {k: norm_state(x) for k, x in vars(v).items()
                 if not k.startswith("__cl") and k not in inj}
        cls = getattr(v, "__cl_class__", None)
        out = {"__attr__": inner}
        if cls:
            out["__class__"] = cls
        return out
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, (set, frozenset)):
        return _set_norm(v, norm_state)
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
    return norm_state(v)


def proj_fn(v):
    if callable(v):
        return {"__callable__": getattr(v, "__cl_seam__", getattr(v, "__seam_name__", None)),
                "__name__": getattr(v, "__name__", None)}
    return norm_value(v)


def _fnref(name):
    return {"__callable__": name, "__name__": name}


def proj(name, a, k):
    if name.startswith("logger."):
        d = {"msg": a[0], "args": list(a[1:])}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name in ("time", "held_stream_transforms", "turn_retry_state",
                "try_refresh_env_client_credentials"):
        return {}
    if name == "decode_inline_moa_turn":
        return {"user_message": a[0], "persist_user_message": a[1]}
    if name == "begin_fast_mode_turn":
        return {"agent": double_proj(a[0]), "conversation_history": a[1]}
    if name == "build_turn_context":
        return {
            "agent": double_proj(a[0]), "user_message": a[1], "system_message": a[2],
            "conversation_history": a[3], "task_id": a[4], "stream_callback": a[5],
            "persist_user_message": a[6], "persist_user_timestamp": a[7],
            "persist_user_display_kind": k.get("persist_user_display_kind"),
            "persist_user_display_metadata": k.get("persist_user_display_metadata"),
            "persist_user_platform_id": k.get("persist_user_platform_id"),
            "turn_author": k.get("turn_author"),
            "restore_or_build_system_prompt": _fnref("restore_or_build_system_prompt"),
            "install_safe_stdio": _fnref("install_safe_stdio"),
            "sanitize_surrogates": _fnref("sanitize_surrogates"),
            "summarize_user_message_for_log": _fnref("summarize_user_message_for_log"),
            "set_session_context": _fnref("set_session_context"),
            "set_current_write_origin": _fnref("set_current_write_origin"),
            "ra": _fnref("ra"),
            "moa_active": k.get("moa_active"),
            "title_user_message": k.get("title_user_message"),
        }
    if name == "preflight_timeout_result":
        return {"agent": double_proj(a[0]), "exc": str(a[1]), "conversation_history": a[2]}
    if name == "begin_voice_turn_route":
        return {"agent": double_proj(a[0]), "messages": a[1], "active_system_prompt": a[2]}
    if name == "turn_without_loop":
        return {"agent": double_proj(a[0]), "s": double_proj(a[1]), "block_message": a[2]}
    if name == "play_prelude":
        return {"agent": double_proj(a[0]), "s": double_proj(a[1]), "prelude": a[2]}
    if name == "run_phase":
        d = {"fn": proj_fn(a[0]), "agent": double_proj(a[1]), "state": double_proj(a[2])}
        for kk, vv in k.items():
            d[kk] = str(vv) if isinstance(vv, BaseException) else double_proj(vv)
        return d
    if name == "run_api_retry_loop":
        return {"agent": double_proj(a[0]), "s": double_proj(a[1])}
    if name == "finalize_from_state":
        return {"agent": double_proj(a[0]), "s": double_proj(a[1])}
    if name == "billing_or_entitlement_message":
        return dict(k)
    if name == "billing_block_dict":
        return {"arg0": a[0], "arg1": a[1], "arg2": a[2],
                "arg3": a[3] if len(a) > 3 else None, "unverified": k.get("unverified")}
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
        call.__cl_seam__ = name
        return call


def make_verdict(d):
    flds = [(kk, Any, field(default=None)) for kk in d.keys()]
    Cls = make_dataclass("Verdict", flds)
    vals = {kk: materialize_double(vv) for kk, vv in d.items()}
    return Cls(**vals)


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            inner = j["__attr__"] or {}
            cls = j.get("__class__")
            kids = {kk: materialize_double(vv) for kk, vv in inner.items()}
            obj = SimpleNamespace(**kids)
            obj.__cl_raw__ = j
            obj.__cl_class__ = cls
            obj.__cl_injected__ = set()
            return obj
        cb = j.get("__callable__")
        if isinstance(cb, str):
            fn = CURRENT["rec"](cb)
            return fn
        if "__ref__" in j:
            return CURRENT["ids"][j["__ref__"]]
        return {kk: materialize_double(vv) for kk, vv in j.items()}
    if isinstance(j, list):
        return [materialize_double(vv) for vv in j]
    return j


def materialize_result(name, raw):
    if name == "run_phase":
        if isinstance(raw, dict):
            return make_verdict(raw)
        return SimpleNamespace(action=None)
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
    def time(self):
        return CURRENT["rec"]("time")()


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


CONST_ASSIGNS = ["_CTX_FIELDS", "_COMPRESSION_TIMEOUT_FINAL_RESPONSE", "_LATCHED_VERDICT_FIELDS"]


def _exec_seg(src, node, ns, path):
    import textwrap
    text = ast.get_source_segment(src, node)
    if isinstance(node, ast.ClassDef) and node.decorator_list:
        lines = src.splitlines(keepends=True)
        start = node.decorator_list[0].lineno - 1
        text = "".join(lines[start:node.end_lineno])
    exec(compile(textwrap.dedent(text), path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _run_phase_stub(fn, agent, state, **extra):
    ctx = CURRENT["ctx"]
    args = norm_args(proj("run_phase", (fn, agent, state), extra))
    raw = ctx.script.next("run_phase")
    if isinstance(raw, Raise):
        ctx.trace.append({"ev": "run_phase", "args": args, "ret": raw.spec})
        raise raw.exc
    ctx.trace.append({"ev": "run_phase", "args": args, "ret": norm_value(raw)})
    if not isinstance(raw, dict):
        return SimpleNamespace(action=None)
    verdict = make_verdict(raw)
    latched = NS.get("_LATCHED_VERDICT_FIELDS", {}).get(getattr(fn, "__name__", ""), ())
    for f in fields(verdict):
        if f.name in ("action", "result"):
            continue
        value = getattr(verdict, f.name)
        if f.name not in latched:
            setattr(state, f.name, value)
        elif value:
            setattr(state, f.name, True)
    return verdict


def load_ns():
    global NS
    mod = types.ModuleType("cl2_live")
    sys.modules["cl2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cl2_live", "json": json, "math": math,
        "fields": fields, "dataclass": dataclass, "field": field,
        "Any": Any, "Optional": Optional, "Callable": Callable, "Dict": dict, "List": list,
        "logger": LoggerDouble(), "time": TimeDouble(),
        "PRE_LLM_BLOCK_EXIT_REASON": "blocked_by_plugin_pre_llm_call",
        "PreflightCompressionTimedOut": PreflightCompressionTimedOut,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    for n in tree.body:
        if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id in CONST_ASSIGNS for t in n.targets):
            _exec_seg(src, n, ns, SRC)
        if isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) and n.target.id in CONST_ASSIGNS:
            _exec_seg(src, n, ns, SRC)
    # the _LoopState dataclass (with @dataclass decorator) + the 5 ported defs
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == "_LoopState":
            _exec_seg(src, n, ns, SRC)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in FUNCS:
            _exec_seg(src, n, ns, SRC)
    # capture the constructed _LoopState into the state window
    real_init = ns["_LoopState"].__init__

    def reg_init(self, *a, **k):
        real_init(self, *a, **k)
        sw = CURRENT.get("stwin")
        if sw is not None:
            sw["s"] = self
    ns["_LoopState"].__init__ = reg_init
    # claimed siblings + foreign bare-global seams
    for live, seam in SEAM_GLOBALS.items():
        ns[live] = _stub_fn(seam)
    # _run_phase is a claimed sibling modeled by its contract
    ns["_run_phase"] = _run_phase_stub
    # phase markers (passed as fn to _run_phase)
    for nm in PHASE_NAMES:
        f = _stub_fn(nm)
        f.__name__ = nm
        f.__cl_seam__ = nm
        ns[nm] = f
    # agent method seam
    # in-function import stubs
    _install_stub("agent.voice_turn_route", {"begin_voice_turn_route": _stub_fn("begin_voice_turn_route")})
    _install_stub("agent.turn_context", {
        "build_turn_context": _stub_fn("build_turn_context"),
        "PreflightCompressionTimedOut": PreflightCompressionTimedOut,
        "PRE_LLM_BLOCK_EXIT_REASON": "blocked_by_plugin_pre_llm_call"})
    return ns


def _state(inp, key):
    st = inp.get("state") or {}
    return copy.deepcopy(st.get(key))


def _mat_state(inp, key):
    return materialize_double(_state(inp, key))


def _win(**kw):
    CURRENT["stwin"] = kw
    return kw


def drive(func, inp):
    CURRENT["ids"] = {}
    CURRENT["stwin"] = {}
    if func == "_review_input_budget_exhausted":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        ret = NS[func](agent)
        return ret, CURRENT["stwin"]
    if func == "_should_rearm_compression_budget":
        _win()
        ret = NS[func](inp.get("compression_attempts"),
                       completed_compaction_pending=inp.get("completed_compaction_pending"),
                       prompt_tokens=inp.get("prompt_tokens"),
                       threshold_tokens=inp.get("threshold_tokens"))
        return ret, CURRENT["stwin"]
    if func == "_billing_terminal_label":
        _win()
        ret = NS[func](inp.get("summary"), inp.get("unverified"))
        return ret, CURRENT["stwin"]
    if func == "_billing_failure_result":
        classified = materialize_double(copy.deepcopy(inp.get("classified")))
        _win()
        kw = {}
        if "guidance" in inp:
            kw["guidance"] = inp.get("guidance")
        if "usage_window" in inp:
            uw = inp.get("usage_window")
            kw["usage_window"] = tuple(uw) if isinstance(uw, list) else uw
        ret = NS[func](classified=classified, summary=inp.get("summary"),
                       messages=copy.deepcopy(inp.get("messages")), api_call_count=inp.get("api_call_count"),
                       provider=inp.get("provider"), base_url=inp.get("base_url"),
                       model=inp.get("model"), **kw)
        return ret, CURRENT["stwin"]
    if func == "_LoopState":
        ctx = materialize_double(copy.deepcopy(inp.get("ctx")))
        _win()
        ret = NS["_LoopState"](system_message=inp.get("system_message"),
                               moa_config=inp.get("moa_config"),
                               max_compression_attempts=inp.get("max_compression_attempts"),
                               **{f.name: getattr(ctx, f.name.lstrip("_"))
                                  for f in fields(NS["_LoopState"]) if f.name in NS["_CTX_FIELDS"]})
        return ret, CURRENT["stwin"]
    if func == "_run_conversation_turn":
        agent = _mat_state(inp, "agent")
        agent._try_refresh_env_client_credentials = CURRENT["rec"]("try_refresh_env_client_credentials")
        agent.__cl_injected__.add("_try_refresh_env_client_credentials")
        _win(agent=agent)
        ret = NS[func](
            agent, inp.get("user_message"),
            system_message=inp.get("system_message"),
            conversation_history=copy.deepcopy(inp.get("conversation_history")),
            task_id=inp.get("task_id"),
            stream_callback=inp.get("stream_callback"),
            persist_user_message=inp.get("persist_user_message"),
            persist_user_timestamp=inp.get("persist_user_timestamp"),
            persist_user_display_kind=inp.get("persist_user_display_kind"),
            persist_user_display_metadata=copy.deepcopy(inp.get("persist_user_display_metadata")),
            persist_user_platform_id=inp.get("persist_user_platform_id"),
            turn_author=copy.deepcopy(inp.get("turn_author")),
            moa_config=copy.deepcopy(inp.get("moa_config")),
            title_user_message=inp.get("title_user_message"),
            prelude=copy.deepcopy(inp.get("prelude")),
        )
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
    print("ORACLE conversation_loop_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())