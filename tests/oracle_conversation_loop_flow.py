#!/usr/bin/env python3
"""Oracle for src/agent/conversation_loop_flow.c — drives the LIVE
agent/conversation_loop.py FLOW defs (24 UNCLAIMED functions: the continuation
overlap KMP leaf, the partial-turn result builder, the display-metadata
force-redaction + failed-turn metadata + durable failed-turn closer, the MoA
guidance peel / inline-turn decode, the credential-refresh profile pick, the
guidance printer, the context-engine hook override probe, the run-budget wrap-up
injection, the Ollama context-limit error, the system-prompt persist, the
pinned-tools restore, the Bot Chat tools refresh / prompt-stale probe, the
usage-window refusal, the fallback-restart arm, the preflight-timeout result, and
the turn-loop phase machinery _run_phase / _run_api_retry_loop /
_finalize_from_state / _turn_without_loop / _codex_app_server_turn) under
IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted; claimed
siblings (_sync_failover_system_message) and all foreign I/O are recording seams
fed the SAME scripts the C harness feeds; ported siblings compose for REAL.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) propagates past
every except Exception guard exactly as live (real builtin classes). Per-case
BaseException catch -> __raise__ spec; scripted raises round-trip BARE via
identity match against Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (state echo
RE-WRAPS); class-tagged doubles {"__class__": "_LoopState"} give type() identity;
callable doubles {"__callable__": "<seam>"}; identity refs {"__ref__": "<id>"};
phase verdicts are real dataclasses (fields() order == script key order); integral
floats normalize to ints on both sides. ASCII fixture text.

Summary: 'ORACLE conversation_loop_flow: N cases, M mismatches'.
"""
import ast
import copy
import dataclasses
import json
import math
import os
import re as _re
import sys
import types
import inspect
import __future__
from dataclasses import dataclass, field, fields, make_dataclass
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
    "_continuation_overlap_length", "_partial_turn_result", "_redact_display_metadata",
    "_peel_moa_guidance", "_credential_refresh_profile", "_decode_inline_moa_turn",
    "_print_guidance", "_engine_overrides_hook", "_maybe_inject_run_budget_wrapup",
    "_ollama_context_limit_error", "_persist_system_prompt", "_restore_pinned_tools",
    "_refresh_bot_chat_tools", "_bot_chat_prompt_stale", "_usage_window_refusal",
    "_arm_fallback_restart", "_preflight_timeout_result", "_run_phase",
    "_run_api_retry_loop", "_finalize_from_state", "_turn_without_loop",
    "_codex_app_server_turn", "_failed_turn_display_metadata", "_close_durable_failed_turn",
]

# bare-global module-top imports / claimed siblings that are seamed
SEAM_GLOBALS = {
    "_sync_failover_system_message": "sync_failover_system_message",
    "finalize_turn": "finalize_turn",
    "failed_turn_notice": "failed_turn_notice",
}
PHASE_GLOBALS = ["nous_rate_limit_guard", "build_api_request", "perform_api_call",
                 "check_api_response", "handle_api_interrupt", "handle_api_error"]
PHASE_SEAMS = set(PHASE_GLOBALS) | {"phase_fn"}

CURRENT = {"ctx": None, "rec": None, "ids": {}}
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
        # real classes need multi-arg ctors; model as a ValueError-family subclass
        return type(kind, (ValueError,), {})(msg)
    if kind == "SystemExit":
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


def _dc_to_dict(v):
    return {f.name: getattr(v, f.name) for f in fields(v)}


def norm_value(v):
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return {k: norm_value(x) for k, x in _dc_to_dict(v).items()}
    if callable(v):
        return {"__callable__": getattr(v, "__cl_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__cl_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__cl") and k not in inj}
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
        return {"__callable__": getattr(v, "__cl_seam__", True)}
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


def proj_arg_value(v):
    """Projection for a phase kwarg / agent value: exception -> str, double ->
    __attr__ wrapper, else norm_value."""
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, SimpleNamespace):
        return norm_state(v)
    return norm_value(v)


def double_proj(v):
    if isinstance(v, SimpleNamespace):
        return norm_state(v)
    return norm_value(v)

def proj(name, a, k):
    if name.startswith("logger."):
        d = {"msg": a[0], "args": list(a[1:])}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name in PHASE_SEAMS:
        d = {"agent": proj_arg_value(a[0]) if a else None}
        for kk, vv in k.items():
            d[kk] = proj_arg_value(vv)
        return d
    if name == "finalize_turn":
        d = {"agent": proj_arg_value(a[0]) if a else None}
        for kk, vv in k.items():
            d[kk] = proj_arg_value(vv)
        return d
    if name == "redact_sensitive_text":
        return {"arg0": a[0], "force": k.get("force")}
    if name == "peel_reference_guidance":
        return {"messages": a[0], "guidance": a[1]}
    if name == "route_profiles":
        return {"hook": a[0], "provider": a[1], "base_url": a[2]}
    if name == "decode_moa_turn":
        return {"arg0": a[0]}
    if name == "vprint":
        return {"arg0": a[0], "force": k.get("force"), "diagnostic": k.get("diagnostic")}
    if name in ("time", "wall_time", "active_policy", "load_disabled_toolsets", "limit_hit_payload"):
        return {}
    if name == "agent_interface":
        return {"agent": double_proj(a[0])}
    if name == "update_system_prompt":
        return {"session_id": a[0], "prompt": a[1]}
    if name in ("persist_agent_tool_names", "agent_tool_names", "agent_home",
                "current_model_is_top_tier", "note_turn_persisted"):
        return {"agent": double_proj(a[0])}
    if name == "restore_agent_tool_prefix":
        return {"agent": double_proj(a[0]), "pin": a[1]}
    if name == "refresh_agent_mcp_tools":
        d = {"agent": double_proj(a[0]), "enabled_override": k.get("enabled_override"),
             "quiet_mode": k.get("quiet_mode"), "content_aware": k.get("content_aware")}
        if "disabled_override" in k:
            d["disabled_override"] = k.get("disabled_override")
        return d
    if name == "load_enabled_toolsets":
        return {"platform": a[0]}
    if name in ("stored_prompt_capability_stale", "stored_bot_chat_prompt_needs_upgrade"):
        return {"stored_prompt": a[0], "home": a[1]}
    if name in ("sessions_get_title", "db_get_session_title", "latest_conversation_role"):
        return {"session_id": a[0]}
    if name == "window_hard_stop":
        return {"state": double_proj(a[0]), "model_is_free": k.get("model_is_free"),
                "model_is_top_tier": k.get("model_is_top_tier")}
    if name == "window_stop_resets_at_ms":
        return {"state": double_proj(a[0]), "model_is_top_tier": k.get("model_is_top_tier")}
    if name == "estimate_messages_tokens_rough":
        return {"arg0": a[0]}
    if name == "limit_hit":
        return {"agent": double_proj(a[0]), "message": k.get("message"),
                "needed_tokens": k.get("needed_tokens")}
    if name == "sync_failover_system_message":
        return {"agent": double_proj(a[0]), "api_messages": a[1], "active_system_prompt": a[2]}
    if name == "run_codex_app_server_turn":
        return {"user_message": k.get("user_message"),
                "original_user_message": k.get("original_user_message"),
                "messages": k.get("messages"), "effective_task_id": k.get("effective_task_id"),
                "should_review_memory": k.get("should_review_memory"), "turn_id": k.get("turn_id")}
    if name == "activate_codex_app_server_fallback":
        return {"agent": double_proj(a[0]), "codex_result": a[1]}
    if name == "build_error_surface_from_result":
        return {"result": a[0], "provider": k.get("provider"), "model": k.get("model")}
    if name == "failed_turn_notice":
        return {"turn_messages": a[0]}
    if name == "append_message":
        return {"message": double_proj(a[1])}
    if name == "flush_messages_to_session_db":
        return {"arg0": a[0]}
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


class _LoopState(SimpleNamespace):
    pass


class LimitHitDouble(SimpleNamespace):
    pass


def make_verdict(d):
    flds = [(kk, Any, field(default=None)) for kk in d.keys()]
    Cls = make_dataclass("Verdict", flds)
    vals = {kk: materialize_double(vv) for kk, vv in d.items()}
    return Cls(**vals)


def make_phase(seam, params, fname):
    """Build a real function whose inspect.signature matches params (minus agent)
    and whose body records the phase seam."""
    plist = list(params)
    sig = "agent" + ("".join(", " + p for p in plist))
    callargs = "".join(", %s=%s" % (p, p) for p in plist)
    body = "    return __rec__(agent%s)\n" % callargs
    src = "def _phase_fn(%s):\n%s" % (sig, body)
    g = {"__rec__": CURRENT["rec"](seam)}
    exec(compile(src, "<phase>", "exec"), g)
    fn = g["_phase_fn"]
    fn.__name__ = fname
    fn.__cl_seam__ = seam
    return fn


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            inner = j["__attr__"] or {}
            cls = j.get("__class__")
            kids = {kk: materialize_double(vv) for kk, vv in inner.items()}
            if cls == "_LoopState":
                obj = _LoopState(**kids)
            else:
                obj = SimpleNamespace(**kids)
            obj.__cl_raw__ = j
            obj.__cl_class__ = cls
            obj.__cl_injected__ = set()
            return obj
        cb = j.get("__callable__")
        if isinstance(cb, str):
            fn = CURRENT["rec"](cb)
            if "__func__" in j:
                fn.__func__ = materialize_double(j["__func__"])
            return fn
        if "__ref__" in j:
            return CURRENT["ids"][j["__ref__"]]
        return {kk: materialize_double(vv) for kk, vv in j.items()}
    if isinstance(j, list):
        return [materialize_double(vv) for vv in j]
    return j


def _inject(obj, **kw):
    for kk, vv in kw.items():
        setattr(obj, kk, vv)
    inj = getattr(obj, "__cl_injected__", None)
    if inj is None:
        inj = set()
        obj.__cl_injected__ = inj
    inj.update(kw.keys())


def materialize_result(name, raw):
    if name in PHASE_SEAMS and isinstance(raw, dict):
        return make_verdict(raw)
    if name == "limit_hit" and isinstance(raw, dict):
        hit = LimitHitDouble(**{kk: materialize_double(vv) for kk, vv in raw.items()})
        _inject(hit, to_payload=CURRENT["rec"]("limit_hit_payload"))
        return hit
    if name == "agent_interface" and isinstance(raw, dict) and "__attr__" in raw:
        store = materialize_double(raw)
        sess = getattr(store, "sessions", None)
        if isinstance(sess, SimpleNamespace):
            _inject(sess, update_system_prompt=CURRENT["rec"]("update_system_prompt"),
                    get_title=CURRENT["rec"]("sessions_get_title"),
                    latest_conversation_role=CURRENT["rec"]("latest_conversation_role"))
        msgs = getattr(store, "messages", None)
        if isinstance(msgs, SimpleNamespace):
            _inject(msgs, latest_conversation_role=CURRENT["rec"]("latest_conversation_role"))
        return store
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


def make_append_message():
    """append_message(messages, message): record the seam, then stamp timestamp
    (wall_time seam) and append — the live side effect, mirrored by the C port."""
    def append_message(messages, message, *, timestamp=None):
        ctx = CURRENT["ctx"]
        args = norm_args(proj("append_message", (messages, message), {}))
        raw = ctx.script.next("append_message")
        if isinstance(raw, Raise):
            ctx.trace.append({"ev": "append_message", "args": args, "ret": raw.spec})
            raise raw.exc
        ctx.trace.append({"ev": "append_message", "args": args, "ret": norm_value(raw)})
        if message.get("timestamp") is None:
            message["timestamp"] = CURRENT["rec"]("wall_time")() if timestamp is None else timestamp
        messages.append(message)
        return message
    return append_message

# ---- live namespace seeding ----
def _exec_seg(src, node, ns, path):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


CONST_ASSIGNS = ["RUN_BUDGET_WRAPUP_NOTICE", "_FAILED_TURN_ERROR_MAX_CHARS",
                 "_LATCHED_VERDICT_FIELDS", "_MIN_CONTINUATION_OVERLAP",
                 "_PHASE_PARAMS", "_CTX_FIELDS"]


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


class _CEDouble:
    pass


def load_ns():
    global NS
    mod = types.ModuleType("cl_live")
    sys.modules["cl_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cl_live", "json": json, "re": _re, "math": math,
        "inspect": inspect, "fields": fields, "dataclass": dataclass, "field": field,
        "Any": Any, "Optional": Optional, "Callable": Callable, "Dict": dict, "List": list,
        "logger": LoggerDouble(), "time": TimeDouble(),
        # byte-pinned imported constants
        "MINIMUM_CONTEXT_LENGTH": 64000,
        "PRE_LLM_BLOCK_EXIT_REASON": "blocked_by_plugin_pre_llm_call",
        "OPEN_TURN_TAIL_ROLES": frozenset({"user", "tool"}),
        "FAILED_TURN_DISPLAY_KIND": "failed_turn",
        "_LoopState": _LoopState,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    # module constants (Assign / AnnAssign with plain-Name targets)
    for n in tree.body:
        if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id in CONST_ASSIGNS for t in n.targets):
            _exec_seg(src, n, ns, SRC)
        if isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name) and n.target.id in CONST_ASSIGNS:
            _exec_seg(src, n, ns, SRC)
    # the 24 ported defs (source order)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in FUNCS:
            _exec_seg(src, n, ns, SRC)
    # bare-global seams
    for live, seam in SEAM_GLOBALS.items():
        ns[live] = _stub_fn(seam)
    for nm in PHASE_GLOBALS:
        ns[nm] = _stub_fn(nm)
    ns["append_message"] = make_append_message()
    # in-function import stubs (recorders resolve CURRENT["rec"] at call time)
    _install_stub("agent.provider_route_hooks", {"route_profiles": _stub_fn("route_profiles")})
    _install_stub("hermes_cli.moa_config", {"decode_moa_turn": _stub_fn("decode_moa_turn")})
    _install_stub("agent.moa_loop", {"peel_reference_guidance": _stub_fn("peel_reference_guidance")})
    _install_stub("agent.context_engine", {"ContextEngine": _CEDouble})
    _install_stub("agent.context_compressor", {"_DB_PERSISTED_MARKER": "_db_persisted"})
    _install_stub("agent.system_prompt", {"_agent_home": _stub_fn("agent_home")})
    _install_stub("tools.bot_mode_probe", {
        "BOT_CHAT_TITLE": "Bot Chat",
        "stored_bot_chat_prompt_needs_upgrade": _stub_fn("stored_bot_chat_prompt_needs_upgrade"),
        "stored_prompt_capability_stale": _stub_fn("stored_prompt_capability_stale")})
    _install_stub("agent.agent_init_session", {"agent_interface": _stub_fn("agent_interface")})
    _install_stub("tools.mcp_tool_agent", {
        "agent_tool_names": _stub_fn("agent_tool_names"),
        "persist_agent_tool_names": _stub_fn("persist_agent_tool_names"),
        "restore_agent_tool_prefix": _stub_fn("restore_agent_tool_prefix"),
        "refresh_agent_mcp_tools": _stub_fn("refresh_agent_mcp_tools")})
    _install_stub("gateway.session_policy", {"active_policy": _stub_fn("active_policy")})
    _install_stub("tui_gateway.server", {
        "_load_disabled_toolsets": _stub_fn("load_disabled_toolsets"),
        "_load_enabled_toolsets": _stub_fn("load_enabled_toolsets")})
    _install_stub("agent.credits_tracker", {
        "_window_hard_stop": _stub_fn("window_hard_stop"),
        "window_stop_resets_at_ms": _stub_fn("window_stop_resets_at_ms")})
    _install_stub("agent.plan_limits", {
        "current_model_is_top_tier": _stub_fn("current_model_is_top_tier"),
        "limit_hit": _stub_fn("limit_hit")})
    _install_stub("agent.model_metadata", {"estimate_messages_tokens_rough": _stub_fn("estimate_messages_tokens_rough")})
    _install_stub("agent.error_surface", {"build_error_surface_from_result": _stub_fn("build_error_surface_from_result")})
    _install_stub("agent.redact", {"redact_sensitive_text": _stub_fn("redact_sensitive_text")})
    _install_stub("agent.agent_runtime_helpers", {"note_turn_persisted": _stub_fn("note_turn_persisted")})
    _install_stub("agent.turn_recovery", {"activate_codex_app_server_fallback": _stub_fn("activate_codex_app_server_fallback")})
    return ns


def _state(inp, key):
    st = inp.get("state") or {}
    return copy.deepcopy(st.get(key))


def _mat_state(inp, key):
    return materialize_double(_state(inp, key))


def _win(**kw):
    CURRENT["stwin"] = kw
    return kw


def _bind_db(db):
    if isinstance(db, SimpleNamespace):
        _inject(db, update_system_prompt=CURRENT["rec"]("update_system_prompt"),
                get_session_title=CURRENT["rec"]("db_get_session_title"),
                latest_conversation_role=CURRENT["rec"]("latest_conversation_role"))


def _make_finalize(params):
    plist = list(params)
    sig = "agent, *" + "".join(", " + p for p in plist) if plist else "agent"
    callargs = "".join(", %s=%s" % (p, p) for p in plist)
    body = "    return __rec__(agent%s)\n" % callargs
    src = "def finalize_turn(%s):\n%s" % (sig, body)
    g = {"__rec__": CURRENT["rec"]("finalize_turn")}
    exec(compile(src, "<finalize>", "exec"), g)
    return g["finalize_turn"]

def drive(func, inp):
    CURRENT["ids"] = {}
    CURRENT["stwin"] = {}
    if func == "_continuation_overlap_length":
        _win()
        ret = NS[func](inp.get("previous", ""), inp.get("continuation", ""))
        return ret, CURRENT["stwin"]
    if func == "_partial_turn_result":
        _win()
        ret = NS[func](inp.get("final_response"), inp.get("messages"),
                       inp.get("api_call_count"), **(inp.get("flags") or {}))
        return ret, CURRENT["stwin"]
    if func == "_redact_display_metadata":
        _win()
        ret = NS[func](inp.get("metadata") or {})
        return ret, CURRENT["stwin"]
    if func == "_peel_moa_guidance":
        _win()
        ret = NS[func](inp.get("messages"), inp.get("guidance"))
        return ret, CURRENT["stwin"]
    if func == "_credential_refresh_profile":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        ret = NS[func](agent)
        return ret, CURRENT["stwin"]
    if func == "_decode_inline_moa_turn":
        _win()
        ret = NS[func](inp.get("user_message"), inp.get("persist_user_message"))
        return ret, CURRENT["stwin"]
    if func == "_print_guidance":
        agent = _mat_state(inp, "agent")
        _inject(agent, _vprint=CURRENT["rec"]("vprint"))
        _win(agent=agent)
        ret = NS[func](agent, inp.get("message", ""))
        return ret, CURRENT["stwin"]
    if func == "_engine_overrides_hook":
        ce_marker = lambda *a, **k: None
        own_marker = lambda *a, **k: None
        CURRENT["ids"]["ce_func"] = ce_marker
        CURRENT["ids"]["own_func"] = own_marker
        name = inp.get("name", "")
        if inp.get("import_fails"):
            sys.modules["agent.context_engine"] = None
        else:
            _install_stub("agent.context_engine", {"ContextEngine": _CEDouble})
            if inp.get("ce_has_hook"):
                setattr(_CEDouble, name, ce_marker)
            elif hasattr(_CEDouble, name):
                delattr(_CEDouble, name)
        engine = materialize_double(copy.deepcopy(inp.get("engine")))
        _win()
        ret = NS[func](engine, name)
        return ret, CURRENT["stwin"]
    if func == "_maybe_inject_run_budget_wrapup":
        agent = _mat_state(inp, "agent")
        messages = _state(inp, "messages")
        _win(agent=agent, messages=messages)
        ret = NS[func](agent, messages)
        return ret, CURRENT["stwin"]
    if func == "_ollama_context_limit_error":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        ret = NS[func](agent, inp.get("request_tokens", 0))
        return ret, CURRENT["stwin"]
    if func == "_persist_system_prompt":
        agent = _mat_state(inp, "agent")
        _bind_db(getattr(agent, "_session_db", None))
        _win(agent=agent)
        ret = NS[func](agent, inp.get("failure_message", ""),
                       persist_tools=bool(inp.get("persist_tools")))
        return ret, CURRENT["stwin"]
    if func == "_restore_pinned_tools":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        ret = NS[func](agent, inp.get("session_row"))
        return ret, CURRENT["stwin"]
    if func == "_refresh_bot_chat_tools":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        ret = NS[func](agent)
        return ret, CURRENT["stwin"]
    if func == "_bot_chat_prompt_stale":
        agent = _mat_state(inp, "agent")
        _bind_db(getattr(agent, "_session_db", None))
        _win(agent=agent)
        ret = NS[func](agent, inp.get("stored_prompt"))
        return ret, CURRENT["stwin"]
    if func == "_usage_window_refusal":
        agent = _mat_state(inp, "agent")
        classified = materialize_double(copy.deepcopy(inp.get("classified")))
        _win(agent=agent)
        ret = NS[func](agent, classified, inp.get("api_messages"))
        return ret, CURRENT["stwin"]
    if func == "_arm_fallback_restart":
        agent = _mat_state(inp, "agent")
        retry = _mat_state(inp, "retry")
        _win(agent=agent, retry=retry)
        ret = NS[func](agent, inp.get("api_messages"), inp.get("active_system_prompt"), retry)
        return ret, CURRENT["stwin"]
    if func == "_preflight_timeout_result":
        agent = _mat_state(inp, "agent")
        exc = build_exc(inp.get("exc") or {"__raise__": "RuntimeError", "message": ""})
        _win(agent=agent)
        ret = NS[func](agent, exc, inp.get("conversation_history"))
        return ret, CURRENT["stwin"]
    if func == "_run_phase":
        fnd = inp.get("fn") or {}
        fn = make_phase(fnd.get("__callable__", "phase_fn"), fnd.get("__params__") or [],
                        fnd.get("__name__", "phase_fn"))
        agent = materialize_double(copy.deepcopy(inp.get("agent")))
        state = _mat_state(inp, "state")
        extra = {}
        for kk, vv in (inp.get("extra") or {}).items():
            extra[kk] = build_exc(vv) if isinstance(vv, dict) and "__raise__" in vv else materialize_double(vv)
        _win(state=state)
        ret = NS[func](fn, agent, state, **extra)
        return ret, CURRENT["stwin"]
    if func == "_run_api_retry_loop":
        agent = _mat_state(inp, "agent")
        s = _mat_state(inp, "s")
        for nm, fnd in (inp.get("phases") or {}).items():
            NS[nm] = make_phase(fnd.get("__callable__", nm), fnd.get("__params__") or [], nm)
        _win(agent=agent, s=s)
        ret = NS[func](agent, s)
        return ret, CURRENT["stwin"]
    if func == "_finalize_from_state":
        agent = _mat_state(inp, "agent")
        s = _mat_state(inp, "s")
        NS["finalize_turn"] = _make_finalize(inp.get("finalize_params") or [])
        _win(agent=agent, s=s)
        ret = NS[func](agent, s)
        return ret, CURRENT["stwin"]
    if func == "_turn_without_loop":
        agent = _mat_state(inp, "agent")
        s = _mat_state(inp, "s")
        _inject(agent, _run_codex_app_server_turn=CURRENT["rec"]("run_codex_app_server_turn"))
        NS["finalize_turn"] = _make_finalize(inp.get("finalize_params") or [])
        _win(agent=agent, s=s)
        ret = NS[func](agent, s, inp.get("block_message", ""))
        return ret, CURRENT["stwin"]
    if func == "_codex_app_server_turn":
        agent = _mat_state(inp, "agent")
        s = _mat_state(inp, "s")
        _inject(agent, _run_codex_app_server_turn=CURRENT["rec"]("run_codex_app_server_turn"))
        _win(agent=agent, s=s)
        ret = NS[func](agent, s)
        return ret, CURRENT["stwin"]
    if func == "_failed_turn_display_metadata":
        agent = _mat_state(inp, "agent")
        _win(agent=agent)
        ret = NS[func](agent, inp.get("result") or {})
        return ret, CURRENT["stwin"]
    if func == "_close_durable_failed_turn":
        agent = _mat_state(inp, "agent")
        result = _state(inp, "result")
        _bind_db(getattr(agent, "_session_db", None))
        _inject(agent, _flush_messages_to_session_db=CURRENT["rec"]("flush_messages_to_session_db"))
        _win(agent=agent, result=result)
        ret = NS[func](agent, result)
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
    print("ORACLE conversation_loop_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
