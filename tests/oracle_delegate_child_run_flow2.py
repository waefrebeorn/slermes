#!/usr/bin/env python3
"""Oracle for src/tools/delegate_child_run_flow2.c — drives the LIVE unclaimed
remainder of tools/delegate_tool_child_run.py under IDENTICAL scripted universal
seams. tool_dispatch_flow2.c claimed 12 defs and delegate_tool.c claimed
_dump_subagent_timeout_diagnostic; this oracle takes the 17 REMAINING unclaimed
top-level functions:

  _reserve_child_transcript, _format_thread_stack, _diag_section, _diag_sizes,
  _budget_warning_text, _warn_child_budget, _child_last_event_age,
  _register_child, _create_isolated_worktree, _lease_child_credential,
  _merge_late_steer, _validate_child_output_schema, _build_tool_trace,
  _build_result_entry, _is_image_url, _build_child_goal_message, _checkpointing

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted; claimed
siblings (_str_or_none, _num, _route_report value seams; _quiet CM factory) and
foreign leaves are recording seams fed the SAME scripts the C harness feeds;
ported siblings compose for REAL (_diag_section in _diag_sizes,
_budget_warning_text in _warn_child_budget, _is_image_url in
_build_child_goal_message, _build_tool_trace in _build_result_entry).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every except Exception guard exactly as live (real builtin
classes). Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed.

Value model: attribute doubles {"__attr__": ..., "__id__": ..., "__class__": tag}
<-> SimpleNamespace (interned by __id__; __class__ "RuntimeSessionStore" is a
SimpleNamespace subclass so isinstance agrees); callables project to
{"__callable__": seam} (a marker may carry "_flush"); _SchemaOutcome projects to
{schema, valid, errors, retries}; the _checkpointing closure projects to
{"__callable__": "checkpointing.cb", "_flush"?}; integral floats normalize to
ints on both sides.

Domain (documented for the generator): ASCII fixture text; live em-dash rides
output as UTF-8; _diag_sizes tools_schema is plain JSON (no floats / no
non-serializable objects); _register_child child carries scalar attrs;
_delegate_depth / current_turn_user_idx avoid integral floats in isinstance-int
positions; int()/float() string coercion is ASCII-ws-padded signed digits;
_stale_stop_seconds is int/float.

Summary: 'ORACLE delegate_child_run_flow2: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import sys
import types
import __future__
from dataclasses import dataclass, field
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

FUNCS = [
    "_reserve_child_transcript", "_format_thread_stack", "_diag_section", "_diag_sizes",
    "_budget_warning_text", "_warn_child_budget", "_child_last_event_age",
    "_register_child", "_create_isolated_worktree", "_lease_child_credential",
    "_merge_late_steer", "_validate_child_output_schema", "_build_tool_trace",
    "_build_result_entry", "_is_image_url", "_build_child_goal_message", "_checkpointing",
]

CURRENT = {"ctx": None, "rec": None, "ids": {}}
NS = {}


class RuntimeSessionStore(SimpleNamespace):
    """isinstance target for _reserve_child_transcript (a SimpleNamespace subclass
    so the double machinery / norm_state treat it as an attribute double)."""
    pass


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__dcr2_seam__ = seam
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


def _is_schema_outcome(v):
    so = NS.get("_SchemaOutcome")
    return so is not None and isinstance(v, so)


def _proj_schema_outcome(v, norm):
    return {"schema": norm(v.schema), "valid": norm(v.valid),
            "errors": [norm(x) for x in v.errors], "retries": norm(v.retries)}


def _proj_cb(v, norm):
    out = {"__callable__": "checkpointing.cb"}
    fl = getattr(v, "__dcr2_flush__", None)
    if fl is not None:
        out["_flush"] = norm(fl)
    return out


def norm_value(v):
    if _is_schema_outcome(v):
        return _proj_schema_outcome(v, norm_value)
    if callable(v):
        if getattr(v, "__dcr2_seam__", None) == "checkpointing.cb":
            return _proj_cb(v, norm_value)
        return {"__callable__": getattr(v, "__dcr2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__dcr2_injected__", set())
        return {k: norm_value(x) for k, x in vars(v).items()
                if not k.startswith("__dcr2") and k not in inj}
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
    if _is_schema_outcome(v):
        return _proj_schema_outcome(v, norm_state)
    if callable(v):
        if getattr(v, "__dcr2_seam__", None) == "checkpointing.cb":
            return _proj_cb(v, norm_state)
        return {"__callable__": getattr(v, "__dcr2_seam__", True)}
    if isinstance(v, SimpleNamespace):
        inj = getattr(v, "__dcr2_injected__", set())
        inner = {k: norm_state(x) for k, x in vars(v).items()
                 if not k.startswith("__dcr2") and k not in inj}
        out = {"__attr__": inner}
        idv = getattr(v, "__dcr2_id__", None)
        if idv is not None:
            out["__id__"] = idv
        cls = getattr(v, "__dcr2_class__", None)
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


def proj(name, a, k):
    if name.startswith("logger."):
        d = {"msg": a[0], "args": [str(x) if isinstance(x, BaseException) else x for x in a[1:]]}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "quiet":
        d = {}
        if a and a[0] is not None:
            d["log_message"] = a[0]
        if len(a) > 1:
            d["log_args"] = list(a[1:])
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "_str_or_none":
        return {"value": a[0]}
    if name == "_num":
        return {"value": a[0]}
    if name == "_route_report":
        return {"child": double_proj(a[0])}
    if name == "child.close":
        return {}
    if name == "child.get_activity_summary":
        return {}
    if name == "child.steer":
        return {"text": a[0]}
    if name == "child._swap_credential":
        return {"entry": double_proj(a[0])}
    if name == "child.run_conversation":
        return dict(k)
    if name == "child.persistence.sessions.create":
        return {"new_session": a[0]}
    if name == "NewSession":
        return dict(k)
    if name == "traceback.format_stack":
        return {"frame": a[0]}
    if name == "diag.produce":
        return {}
    if name == "time.time":
        return {}
    if name == "get_session_env":
        return {"key": a[0], "default": a[1]}
    if name == "_capture_gateway_steer_authority":
        return {"owner_session_id": a[0]}
    if name == "_register_subagent":
        return {"entry": a[0]}
    if name == "_get_worktree_isolation":
        return {}
    if name == "local_backend_active":
        return {}
    if name == "get_session_cwd":
        return {"task_id": a[0]}
    if name == "_resolve_workspace_hint":
        return {"parent_agent": double_proj(a[0])}
    if name == "create_subagent_worktree":
        return {"cwd": a[0], "subagent_id": k.get("subagent_id")}
    if name == "child_profile_scope":
        return {"child": double_proj(a[0])}
    if name == "delegated_child_context":
        return {"session_id": a[0]}
    if name == "pool.acquire_lease":
        return {"cred_id": a[0]} if a else {}
    if name == "pool.entries":
        return {}
    if name == "pool.release_lease":
        return {"cred_id": a[0]}
    if name == "credential_pool_entry_serves_endpoint":
        return {"entry": double_proj(a[0]), "base_url": a[1]}
    if name == "_close_subagent_steering":
        return {"subagent_id": a[0], "child": double_proj(a[1])}
    if name == "validate_output":
        return {"text": a[0], "schema": a[1]}
    if name == "build_retry_message":
        return {"errors": a[0]}
    if name == "append_output_contract":
        return {"message": a[0], "schema": a[1]}
    if name == "flatten_message_text":
        return {"content": a[0]}
    if name == "_summarize_tool_arguments":
        return {"arguments": a[0]}
    if name == "_stringify_tool_content":
        return {"content": a[0]}
    if name == "_looks_like_error_output":
        return {"content": a[0]}
    if name == "os.path.isfile":
        return {"path": a[0]}
    if name == "load_config_readonly":
        return {}
    if name == "decide_image_input_mode":
        return {"provider": a[0], "model": a[1], "cfg": a[2], "requested_provider": k.get("requested_provider")}
    if name == "build_native_content_parts":
        return {"goal": a[0], "paths": a[1], "urls": a[2]}
    if name == "inner.cb":
        return {"event_type": a[0], "args": list(a[1:]), "kwargs": dict(k)}
    if name == "checkpoint.cb":
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
        call.__dcr2_seam__ = name
        return call


def _inject(obj, **kw):
    for kk, vv in kw.items():
        setattr(obj, kk, vv)
    inj = getattr(obj, "__dcr2_injected__", None)
    if inj is None:
        inj = set()
        obj.__dcr2_injected__ = inj
    inj.update(kw.keys())


class QuietCM:
    """_quiet is a claimed sibling CM: the factory call is the "quiet" seam; the
    except-Exception -> logger.debug + suppress exit behaviour is transcribed."""
    def __init__(self, a, k):
        ctx = CURRENT["ctx"]
        self.message = a[0] if a else k.get("log_message")
        self.log_args = a[1:]
        self.exc_info = bool(k.get("exc_info", False))
        args = norm_args(proj("quiet", a, k))
        raw = ctx.script.next("quiet")
        if isinstance(raw, Raise):
            ctx.trace.append({"ev": "quiet", "args": args, "ret": raw.spec})
            raise raw.exc
        ctx.trace.append({"ev": "quiet", "args": args, "ret": norm_value(raw)})
        self.marker = raw

    def __enter__(self):
        return self.marker

    def __exit__(self, et, ev, tb):
        if et is not None and issubclass(et, Exception):
            if self.message is None:
                return True
            la = self.log_args
            if self.message.count("%") > len(la):
                la = la + (ev,)
            CURRENT["rec"]("logger.debug")(self.message, *la, exc_info=self.exc_info)
            return True
        return False


def _quiet_factory(*a, **k):
    return QuietCM(a, k)


class ScopeCM:
    """child_profile_scope / delegated_child_context: silent enter/exit CMs whose
    factory call rides a seam."""
    def __init__(self, name, a, k):
        ctx = CURRENT["ctx"]
        args = norm_args(proj(name, a, k))
        raw = ctx.script.next(name)
        if isinstance(raw, Raise):
            ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
            raise raw.exc
        ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})

    def __enter__(self):
        return None

    def __exit__(self, *a):
        return False


def _scope_factory(name):
    def f(*a, **k):
        return ScopeCM(name, a, k)
    return f


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
        return CURRENT["rec"]("time.time")()

    def monotonic(self):
        return CURRENT["rec"]("time.monotonic")()


class PathDouble:
    def isfile(self, p):
        return CURRENT["rec"]("os.path.isfile")(p)


class OsDouble:
    path = PathDouble()


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            idv = j.get("__id__")
            if idv is not None and idv in CURRENT["ids"]:
                return CURRENT["ids"][idv]
            inner = j["__attr__"] or {}
            kids = {kk: materialize_double(vv) for kk, vv in inner.items()}
            cls = j.get("__class__")
            if cls == "RuntimeSessionStore":
                obj = RuntimeSessionStore(**kids)
            else:
                obj = SimpleNamespace(**kids)
            obj.__dcr2_id__ = idv
            obj.__dcr2_class__ = cls
            obj.__dcr2_injected__ = set()
            if idv is not None:
                CURRENT["ids"][idv] = obj
            return obj
        cb = j.get("__callable__")
        if isinstance(cb, str):
            fn = CURRENT["rec"](cb)
            if "_flush" in j:
                fn._flush = materialize_double(j["_flush"])
            return fn
        return {kk: materialize_double(vv) for kk, vv in j.items()}
    if isinstance(j, list):
        return [materialize_double(vv) for vv in j]
    return j


def materialize_result(name, raw):
    # Materialize ANY result (incl. lists carrying attribute doubles, e.g.
    # pool.entries() / build_native_content_parts) so composed readers see
    # SimpleNamespace doubles, not raw dicts.
    return materialize_double(raw)


# ---- live namespace seeding (AST-extract from the defining module) ----
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
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], m)
    mod = sys.modules[modname]
    for kk, vv in attrs.items():
        setattr(mod, kk, vv)
    return mod


EXTRACT = [
    ("tools/delegate_tool_child_run.py", FUNCS, ["_SchemaOutcome"], []),
]


def load_ns():
    global NS
    mod = types.ModuleType("dcr2_live")
    sys.modules["dcr2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "dcr2_live", "json": json, "os": OsDouble(),
        "Any": Any, "Optional": Optional, "Callable": Callable, "Dict": dict, "List": list,
        "dataclass": dataclass, "field": field,
        "time": TimeDouble(), "logger": LoggerDouble(),
        # claimed siblings -> recording seams
        "_str_or_none": _stub_fn("_str_or_none"),
        "_num": _stub_fn("_num"),
        "_route_report": _stub_fn("_route_report"),
        "_quiet": _quiet_factory,
        # foreign module-level leaves -> recording seams
        "_get_worktree_isolation": _stub_fn("_get_worktree_isolation"),
        "_resolve_workspace_hint": _stub_fn("_resolve_workspace_hint"),
        "_capture_gateway_steer_authority": _stub_fn("_capture_gateway_steer_authority"),
        "_register_subagent": _stub_fn("_register_subagent"),
        "_close_subagent_steering": _stub_fn("_close_subagent_steering"),
        # delegate_tool_results leaves (module-level imports in the quarry file)
        "_summarize_tool_arguments": _stub_fn("_summarize_tool_arguments"),
        "_stringify_tool_content": _stub_fn("_stringify_tool_content"),
        "_looks_like_error_output": _stub_fn("_looks_like_error_output"),
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
    # in-function import stubs
    _install_stub("agent.runtime_session_store", {"RuntimeSessionStore": RuntimeSessionStore})
    _install_stub("persistence.conversation.sessions", {"NewSession": _stub_fn("NewSession")})
    _install_stub("traceback", {"format_stack": _stub_fn("traceback.format_stack")})
    _install_stub("gateway.session_context", {"get_session_env": _stub_fn("get_session_env")})
    _install_stub("tools.subagent_worktree", {
        "local_backend_active": _stub_fn("local_backend_active"),
        "create_subagent_worktree": _stub_fn("create_subagent_worktree"),
    })
    _install_stub("tools.terminal_tool", {"get_session_cwd": _stub_fn("get_session_cwd")})
    _install_stub("agent.credential_pool", {
        "credential_pool_entry_serves_endpoint": _stub_fn("credential_pool_entry_serves_endpoint"),
    })
    _install_stub("tools.delegate_tool_routes", {"child_profile_scope": _scope_factory("child_profile_scope")})
    _install_stub("tools.delegation_output_schema", {
        "append_output_contract": _stub_fn("append_output_contract"),
        "build_retry_message": _stub_fn("build_retry_message"),
        "validate_output": _stub_fn("validate_output"),
    })
    _install_stub("agent.delegation_context", {"delegated_child_context": _scope_factory("delegated_child_context")})
    _install_stub("session_rules.turns", {"flatten_message_text": _stub_fn("flatten_message_text")})
    _install_stub("agent.image_routing", {
        "build_native_content_parts": _stub_fn("build_native_content_parts"),
        "decide_image_input_mode": _stub_fn("decide_image_input_mode"),
    })
    _install_stub("hermes_cli.config", {"load_config_readonly": _stub_fn("load_config_readonly")})
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
    g = inp.get
    if func == "_reserve_child_transcript":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](child), CURRENT["stwin"]
    if func == "_format_thread_stack":
        _win()
        return NS[func](g("frame"), g("indent", "")), CURRENT["stwin"]
    if func == "_diag_section":
        _win()
        produce = materialize_double(copy.deepcopy(g("produce")))
        return NS[func](g("label", ""), produce), CURRENT["stwin"]
    if func == "_diag_sizes":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](child), CURRENT["stwin"]
    if func == "_budget_warning_text":
        _win()
        return NS[func](g("idle_seconds"), g("child_timeout")), CURRENT["stwin"]
    if func == "_warn_child_budget":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](child, g("idle_seconds"), g("child_timeout")), CURRENT["stwin"]
    if func == "_child_last_event_age":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](child), CURRENT["stwin"]
    if func == "_register_child":
        child = _mat_state(inp, "child")
        parent = _mat_state(inp, "parent_agent")
        _win(child=child, parent_agent=parent)
        return NS[func](child, parent, g("goal", ""),
                        owner_session_id=g("owner_session_id"),
                        owner_transport=g("owner_transport"),
                        owner_session_record=g("owner_session_record")), CURRENT["stwin"]
    if func == "_create_isolated_worktree":
        parent = _mat_state(inp, "parent_agent")
        _win(parent_agent=parent)
        return NS[func](parent, g("parent_task_id"), g("subagent_id")), CURRENT["stwin"]
    if func == "_lease_child_credential":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](child), CURRENT["stwin"]
    if func == "_merge_late_steer":
        result = _state(inp, "result")
        if result is None:
            result = {}
        child = _mat_state(inp, "child")
        _win(result=result, child=child)
        return NS[func](result, g("subagent_id"), child), CURRENT["stwin"]
    if func == "_validate_child_output_schema":
        child = _mat_state(inp, "child")
        result = _state(inp, "result")
        if result is None:
            result = {}
        relay = materialize_double(copy.deepcopy(g("relay_child_text")))
        _win(child=child, result=result)
        return NS[func](child, result, g("task_index", 0), g("child_task_id", ""), relay), CURRENT["stwin"]
    if func == "_build_tool_trace":
        _win()
        return NS[func](g("messages")), CURRENT["stwin"]
    if func == "_build_result_entry":
        child = _mat_state(inp, "child")
        result = _state(inp, "result")
        if result is None:
            result = {}
        sd = g("schema") or {}
        schema = NS["_SchemaOutcome"](schema=sd.get("schema"), valid=sd.get("valid"),
                                      errors=sd.get("errors") or [], retries=sd.get("retries", 0))
        _win(child=child, result=result)
        return NS[func](child, result, g("task_index", 0), g("duration", 0), schema), CURRENT["stwin"]
    if func == "_is_image_url":
        _win()
        return NS[func](g("ref")), CURRENT["stwin"]
    if func == "_build_child_goal_message":
        child = _mat_state(inp, "child")
        _win(child=child)
        return NS[func](g("goal", ""), g("images") or [], child), CURRENT["stwin"]
    if func == "_checkpointing":
        _win()
        inner = materialize_double(copy.deepcopy(g("inner")))
        checkpoint = materialize_double(copy.deepcopy(g("checkpoint")))
        cb = NS[func](inner, checkpoint)
        cb.__dcr2_seam__ = "checkpointing.cb"
        if hasattr(cb, "_flush"):
            cb.__dcr2_flush__ = cb._flush
        for ev in (g("events") or []):
            cb(ev.get("event_type"), *(ev.get("args") or []), **(ev.get("kwargs") or {}))
        return cb, CURRENT["stwin"]
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
    print("ORACLE delegate_child_run_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())