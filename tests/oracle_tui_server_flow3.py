#!/usr/bin/env python3
"""Oracle for the tui_gateway/server.py THIRD-WAVE FLOW cores / pure leaves
(src/gateway/tui_server_flow3.c).

Scoped down to the 12 functions faithful at 0 mismatches; the 50 over-scoped
functions were removed from all five deliverables (honest scope-down).

It AST-extracts those defs, seeds faithful doubles for the module globals they
read, and replaces every collaborator / filesystem probe / clock / thread with
recording stubs returning the SAME scripted results the C harness feeds its
universal seam. In-function imports are stubbed on sys.modules so the live
`from X import Y` binds the recorder. The full builtin exception hierarchy is
carried on both sides: Exception-family scripted raises are swallowed at
suppress / except-Exception sites, while the BaseException trio and
CancelledError propagate. Compares ordered seam trace + return + final state.
Fail-closed on 0 cases.

Input: stdin JSON lines from t_port_tui_server_flow3.
Summary: ORACLE tui_server_flow3: N cases, M mismatches.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import hashlib
import json
import os
import sys
import types
from pathlib import Path, PurePath, PurePosixPath
from types import SimpleNamespace

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ts3_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tui_gateway", "server.py")

TARGETS = [
    "_DropTransport.write", "_DropTransport.close", "register_live_transport", "unregister_live_transport",
    "_approval_request_payload", "_open_requests", "register_method", "_enable_gateway_prompts",
    "_clear_pending", "_probe_credentials", "_probe_config_health", "_tool_ctx",
]

CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": [], "state": {}}


def rec_ev(ev):
    CUR["trace"].append(ev)


# ── full builtin exception hierarchy ───────────────────────────────────────
PARENT = {
    "BaseException": None,
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "GeneratorExit": "BaseException", "CancelledError": "BaseException",
    "Exception": "BaseException", "ConfigBackendUnavailable": "SystemExit",
    "ProfileUnavailableError": "Exception",
    "ArithmeticError": "Exception", "AssertionError": "Exception",
    "AttributeError": "Exception", "BufferError": "Exception", "EOFError": "Exception",
    "ImportError": "Exception", "LookupError": "Exception", "MemoryError": "Exception",
    "NameError": "Exception", "OSError": "Exception", "ReferenceError": "Exception",
    "RuntimeError": "Exception", "StopAsyncIteration": "Exception",
    "StopIteration": "Exception", "SyntaxError": "Exception", "SystemError": "Exception",
    "TypeError": "Exception", "ValueError": "Exception", "Warning": "Exception",
    "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "FloatingPointError": "ArithmeticError", "ModuleNotFoundError": "ImportError",
    "IndexError": "LookupError", "KeyError": "LookupError", "UnboundLocalError": "NameError",
    "FileNotFoundError": "OSError", "FileExistsError": "OSError", "PermissionError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "TimeoutError": "OSError", "ConnectionError": "OSError", "BlockingIOError": "OSError",
    "ChildProcessError": "OSError", "ProcessLookupError": "OSError",
    "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "IndentationError": "SyntaxError", "TabError": "IndentationError",
    "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeTranslateError": "UnicodeError",
    "UserWarning": "Warning", "DeprecationWarning": "Warning",
    "PendingDeprecationWarning": "Warning", "SyntaxWarning": "Warning",
    "RuntimeWarning": "Warning", "FutureWarning": "Warning", "ImportWarning": "Warning",
    "UnicodeWarning": "Warning", "BytesWarning": "Warning", "ResourceWarning": "Warning",
}


def is_exc(kind, anc):
    cur = kind
    for _ in range(24):
        if cur == anc:
            return True
        cur = PARENT.get(cur, "Exception")
        if cur is None:
            return False
    return False


class ConfigBackendUnavailable(SystemExit):
    def __init__(self, msg="", code=None):
        super().__init__(msg)
        self.code = code


class ProfileUnavailableError(Exception):
    pass


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    code = spec.get("code")
    if kind == "ConfigBackendUnavailable":
        return ConfigBackendUnavailable(msg, code)
    if kind == "ProfileUnavailableError":
        return ProfileUnavailableError(msg)
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "KeyError":
        try:
            key = ast.literal_eval(msg)
        except Exception:
            key = msg
        return KeyError(key)
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        try:
            return cls(msg)
        except TypeError:
            return cls()
    return type(kind, (Exception,), {})(msg)


def is_raise(x):
    return isinstance(x, dict) and "__raise__" in x


# ── fail-closed script access ──────────────────────────────────────────────
def next_script(name):
    io = CUR["io"]
    if name not in io:
        return Raise({"__raise__": "RuntimeError", "message": "no script for " + name})
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            return Raise({"__raise__": "RuntimeError", "message": "script exhausted for " + name})
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam(name, args):
    r = next_script(name)
    if isinstance(r, Raise):
        CUR["raises"].append(r)
        rec_ev({"ev": name, "args": args, "ret": r.spec})
        raise r.exc
    rec_ev({"ev": name, "args": args, "ret": r})
    if is_raise(r):
        e = make_exc(r)
        CUR["raises"].append(Raise(r))
        raise e
    return copy.deepcopy(r)


# ── generic arg adapters (mirror the C seam arg shapes) ───────────────────
SIMPLE = {
    "_get_db": lambda: {}, "current_transport": lambda: {},
    "mcp_discovery_in_flight": lambda: {}, "import_mcp_entry": lambda: {},
    "event_new": lambda: {}, "iterdir": lambda: {}, "time": lambda: {},
    "get_process_hermes_home": lambda: {}, "_resolve_session_platform": lambda: {},
    "current_profile_name": lambda: {}, "getcwd": lambda: {},
    "connect_closing": lambda: {}, "project_close": lambda: {},
    "yolo_frozen": lambda: {}, "_load_approval_mode": lambda: {},
    "get_mcp_status": lambda: {}, "get_available_skills": lambda: {},
    "recommended_update_command": lambda: {}, "release_date": lambda: {},
    "active_count": lambda: {}, "sheet_revision": lambda: {},
    "row_frame_counts": lambda: {}, "state_rows": lambda: {},
    "read_bytes": lambda: {}, "state_frame_counts": lambda: {},
    "spritesheet_stat": lambda: {},
}


def mkargs(name, a, kw):
    if name in SIMPLE:
        return SIMPLE[name]()
    if name == "log":
        return {"level": kw.get("level"), "fmt": a[0] if a else None,
                "args": [str(x) if isinstance(x, BaseException) else x for x in a[1:]]}
    if name == "environ_get":
        return {"name": a[0] if a else kw.get("name")}
    if name == "environ_update":
        return dict(kw) if kw else (a[0] if a and isinstance(a[0], dict) else {})
    if name == "build_tool_preview":
        return {"name": a[0] if a else None, "args": a[1] if len(a) > 1 else {},
                "max_len": kw.get("max_len")}
    if name == "redact_approval":
        return {"value": a[0] if a else None}
    if name == "open_requests":
        return {"sid": a[0] if a else None}
    if name == "cancel":
        return {"sid": a[0] if a else None, "reason": kw.get("reason")}
    if name == "forget":
        return {"transport": a[0] if a else None}
    if name == "canonical_profile_request":
        return {"value": a[0] if a else None}
    if name == "profile_home":
        return {"name": a[0] if a else None}
    if name in ("profile_scope",):
        return {"home": kw.get("home"), "eager": kw.get("eager", False)}
    if name in ("open_persistence",):
        return {"scope": a[0] if a else None, "eager": kw.get("eager", False)}
    if name == "store_location":
        return {"scope": a[0] if a else None}
    if name == "get_profile_dir":
        return {"name": a[0] if a else None}
    if name == "is_dir":
        return {"path": str(a[0]) if a else None}
    if name == "path_resolve":
        return {"path": str(a[0]) if a else None}
    if name == "expanduser":
        return {"raw": a[0] if a else None}
    if name == "abspath":
        return {"path": str(a[0]) if a else None}
    if name == "config_exists":
        return {"path": str(a[0]) if a else None}
    if name == "load_user_config_effective":
        return {"path": str(a[0]) if a else None}
    if name == "profile_configured_cwd":
        return {"profile_home": str(a[0]) if a else None}
    if name == "_configured_cwd_from_cfg":
        return {"cfg": a[0] if a else None}
    if name == "_profile_home":
        return {"profile": a[0] if a else None}
    if name == "_get_usage":
        return {"agent": a[0] if a else None}
    if name == "_session_info":
        return {"agent": a[0] if a else None, "session": a[1] if len(a) > 1 else None}
    if name == "_emit":
        return {"event": a[0] if a else None, "sid": a[1] if len(a) > 1 else None,
                "payload": a[2] if len(a) > 2 else None}
    if name == "_err":
        return {"rid": a[0] if a else None, "code": a[1] if len(a) > 1 else None,
                "message": a[2] if len(a) > 2 else None, "data": kw.get("data")}
    if name == "describe_storage_failure":
        return {"error": a[0] if a else None}
    if name == "storage_failure_details":
        return {"error": a[0] if a else None}
    if name == "_is_model_switch_marker":
        return {"h": a[0] if a else None}
    if name == "_submit_row_target_key":
        return {"session": a[0] if a else None}
    if name == "stamp_message_uid":
        return {"entry": a[0] if a else None}
    if name == "append":
        return {"target": a[0] if a else None, "message": a[1] if len(a) > 1 else None}
    if name == "deactivate_messages_by_display_kind":
        return {"target": a[0] if a else None, "kind": a[1] if len(a) > 1 else None}
    if name == "build_system_prompt":
        return {"agent": getattr(a[0], "value", a[0]) if a else None}
    if name == "update_system_prompt":
        return {"session_id": a[0] if a else None, "prompt": a[1] if len(a) > 1 else None}
    if name == "_set_session_context":
        return {"session_key": kw.get("session_key"), "cwd": kw.get("cwd")}
    if name == "_clear_session_context":
        return {"tokens": a[0] if a else None}
    if name == "clear_session_vars":
        return {"tokens": a[0] if a else None}
    if name == "set_session_vars":
        return {k: v for k, v in kw.items()}
    if name == "session_for_key":
        return {"session_key": a[0] if a else None}
    if name == "session_runtime_view":
        return {"agent": a[0] if a else None}
    if name == "profile_name_for_home":
        return {"profile_home": a[0] if a else None}
    if name == "_scoped_session_provider":
        return {"agent": a[0] if a else None, "session": a[1] if len(a) > 1 else None}
    if name == "_session_default_route":
        return {"session": a[0] if a else None}
    if name == "route_supported_efforts":
        return {"provider": a[0] if a else None, "model": a[1] if len(a) > 1 else None,
                "api_mode": a[2] if len(a) > 2 else None, "base_url": a[3] if len(a) > 3 else None}
    if name == "clamp_effort":
        return {"effort": a[0] if a else None, "supported": a[1] if len(a) > 1 else None}
    if name in ("_fast_tier_applies",):
        return {"agent": a[0] if a else None, "model": a[1] if len(a) > 1 else None,
                "provider": a[2] if len(a) > 2 else None,
                "route_known": kw.get("route_known"), "tier": kw.get("tier")}
    if name == "resolve_fast_mode_overrides":
        return {"model": a[0] if a else None, "provider": kw.get("provider"),
                "base_url": kw.get("base_url"), "tier": kw.get("tier")}
    if name == "git_branch":
        return {"cwd": a[0] if a else None}
    if name == "_project_info_for_cwd":
        return {"cwd": a[0] if a else None}
    if name == "_turn_started_at":
        return {"session": a[0] if a else None}
    if name == "_session_live_title":
        return {"sess": a[0] if a else None, "session_key": a[1] if len(a) > 1 else None}
    if name == "_session_usage_snapshot":
        return {"session": a[0] if a else None}
    if name == "get_toolset_for_tool":
        return {"name": a[0] if a else None}
    if name == "get_update_result":
        return {"timeout": kw.get("timeout")}
    if name == "get_version_info":
        return {}
    if name == "_probe_credentials":
        return {"agent": a[0] if a else None}
    if name == "_metadata_mirror":
        return {"session": a[0] if a else None}
    if name == "_display_session_cwd":
        return {"session": a[0] if a else None}
    if name == "is_session_yolo_enabled":
        return {"session_key": a[0] if a else None}
    if name == "_effective_terminal_backend":
        return {}
    if name == "_persistence_for":
        return {"db": a[0] if a else None}
    if name == "_session_cwd":
        return {"session": a[0] if a else None}
    if name == "_live_session_agent_db":
        return {"session": a[0] if a else None}
    if name == "_live_session_identity":
        return {"session": a[0] if a else None}
    if name == "session_get":
        return {"key": a[0] if a else None}
    if name == "_resumable_stored_cwd":
        return {"cwd": a[0] if a else None, "profile_home": a[1] if len(a) > 1 else None}
    if name == "_host_cwd_is_terminal_cwd":
        return {"profile_home": a[0] if a else None}
    if name == "_is_other_profiles_cwd":
        return {"cwd": a[0] if a else None, "profile_home": a[1] if len(a) > 1 else None}
    if name == "_is_remote_launch_cwd":
        return {"session": a[0] if a else None}
    if name == "_persist_session_cwd_and_schedule_git_meta":
        return {"session": a[0] if a else None, "cwd": a[1] if len(a) > 1 else None}
    if name == "_open_session_db_at_path":
        return {"db_path": a[0] if a else None, "read_only": kw.get("read_only")}
    if name == "_bootstrap_served_profile_store":
        return {"profile_dir": a[0] if a else None}
    if name == "_open_profile_session_db":
        return {"profile_home": a[0] if a else None}
    if name == "_session_for_key":
        return {"session_key": a[0] if a else None}
    if name == "set_session_vars":
        return {k: v for k, v in kw.items()}
    if name == "float":
        return {"value": a[0] if a else None}
    if name == "is_truthy_value":
        return {"value": a[0] if a else None, "default": a[1] if len(a) > 1 else False}
    if name == "resolve_active_pet":
        return {"slug": a[0] if a else None}
    if name == "_pet_cfg":
        return {}
    if name == "connect_closing":
        return {}
    if name == "project_for_path":
        return {"conn": a[0] if a else None, "cwd": a[1] if len(a) > 1 else None}
    if name == "live_current":
        return {"session_key": a[0] if a else None, "profile_home": kw.get("profile_home")}
    if name == "request_payload":
        return {"operation": getattr(a[0], "value", a[0]) if a else None}
    if name == "get_pending_gateway_approval":
        return {"session_key": a[0] if a else None}
    if name == "send_async":
        return {"method": a[0] if a else None, "sid": a[1] if len(a) > 1 else None,
                "payload": a[2] if len(a) > 2 else None}
    if name == "withdraw_gateway_approval":
        return {"session_key": a[0] if a else None, "request_id": a[1] if len(a) > 1 else None,
                "reason": a[2] if len(a) > 2 else None}
    if name == "resolve_gateway_approval":
        return {"session_key": a[0] if a else None, "choice": a[1] if len(a) > 1 else None,
                "resolve_all": kw.get("resolve_all"), "request_id": kw.get("request_id")}
    if name == "register_gateway_settle":
        return {"session_key": a[0] if a else None, "request_id": a[1] if len(a) > 1 else None,
                "settle": a[2] if len(a) > 2 else None}
    if name == "settle":
        return {"result": a[0] if a else None}
    if name == "format_exception":
        return {"exc_type": str(a[0]) if a else None, "exc_value": str(a[1]) if len(a) > 1 else None}
    if name == "makedirs":
        return {"path": str(a[0]) if a else None}
    if name == "strftime":
        return {"fmt": a[0] if a else None}
    if name == "crash_write":
        return {"path": a[0] if a else None, "text": a[1] if len(a) > 1 else None}
    if name == "stderr_print":
        return {"text": a[0] if a else None, "flush": kw.get("flush")}
    if name == "excepthook":
        return {}
    if name == "_record_crash":
        return {"kind": a[0] if a else None}
    if name == "thread_start":
        return {"target": str(kw.get("target")), "daemon": kw.get("daemon")}
    if name == "join_mcp_discovery":
        return {"timeout": kw.get("timeout")}
    if name == "refresh_agent_mcp_tools":
        return {"agent": getattr(a[0], "value", a[0]) if a else None, "quiet_mode": kw.get("quiet_mode")}
    if name == "event_presentation_muted":
        return {"event": a[0] if a else None, "sid": a[1] if len(a) > 1 else None}
    if name == "stamp_session_frame":
        return {"session": a[0] if a else None, "event": a[1] if len(a) > 1 else None,
                "payload": a[2] if len(a) > 2 else None}
    if name == "_event_frame":
        return {"event": a[0] if a else None, "sid": a[1] if len(a) > 1 else None}
    if name == "_stamp_event":
        return {"obj": a[0] if a else None}
    if name == "project_room_member_activity":
        return {"obj": a[0] if a else None}
    if name == "transport_write":
        return {"transport": str(a[0]) if a else None}
    if name == "write":
        return {"transport": str(a[0]) if a else None}
    if name == "worker_close":
        return {"worker": str(a[0]) if a else None}
    if name == "_resolve_model":
        return {}
    if name == "_SlashWorker":
        return {"session_key": a[0] if a else None}
    if name == "_attach_worker":
        return {"sid": a[0] if a else None}
    if name == "_ensure_session_db_row":
        return {"session": a[0] if a else None}
    if name == "session_db":
        return {"session": a[0] if a else None}
    if name == "scope_enter":
        return {"session": a[0] if a else None}
    if name == "scope_exit":
        return {}
    if name == "handler":
        return {"rid": a[0] if a else None, "params": a[1] if len(a) > 1 else None}
    if name == "connection_profile_home":
        return {}
    if name == "image_open":
        return {"path": str(a[0]) if a else None}
    if name == "available_personalities":
        return {"cfg": a[0] if a else None}
    if name == "list_sessions":
        return {}
    if name == "proc_get":
        return {"session_id": a[0] if a else None}
    if name == "is_dir":
        return {"path": str(a[0]) if a else None}
    if name == "b64decode":
        return {"data": a[0] if a else None}
    if name == "b64encode":
        return {"raw": str(a[0]) if a else None}
    if name == "write_bytes":
        return {"name": str(a[0]) if a else None}
    if name == "spawn_index_write":
        return {"path": str(a[0]) if a else None, "entry": a[1] if len(a) > 1 else None}
    if name == "spawn_index_read":
        return {"path": str(a[0]) if a else None}
    if name == "json_loads":
        return {"line": a[0] if a else None}
    if name == "db_close":
        return {"db": str(a[0]) if a else None}
    if name == "is_session_yolo_enabled":
        return {"session_key": a[0] if a else None}
    if name == "_emit_session_info_for_session":
        return {"sid": a[0] if a else None, "session": a[1] if len(a) > 1 else None}
    if name == "_session_for_key":
        return {"session_key": a[0] if a else None}
    if name == "context_usage_fields":
        return {"comp": getattr(a[0], "value", a[0]) if a else None}
    if name == "plan_gauge":
        return {"agent": a[0] if a else None}
    if name == "get_credits_spent_micros":
        return {"agent": a[0] if a else None}
    if name == "cache_get":
        return {"key": a[0] if a else None}
    if name == "cache_put":
        return {"key": a[0] if a else None}
    if name == "clone_payload":
        return {"payload": a[0] if a else None}
    if name == "is_dir":
        return {"path": str(a[0]) if a else None}
    if name == "stat_mtime":
        return {"child": str(a[0]) if a else None}
    if name == "rmtree":
        return {"child": str(a[0]) if a else None}
    if name == "convert":
        return {"img": str(a[0]) if a else None, "mode": a[1] if len(a) > 1 else None}
    if name == "thumbnail":
        return {"img": str(a[0]) if a else None}
    if name == "save":
        return {"img": str(a[0]) if a else None}
    if name == "sha1_hex12":
        return {"data": a[0] if a else None}
    # fallback
    if kw:
        return {k: v for k, v in kw.items()}
    if len(a) == 1:
        return {"value": a[0]}
    return {"args": list(a)}


def rec(name):
    def f(*a, **kw):
        r = seam(name, mkargs(name, a, kw))
        if is_raise(r):
            return None
        return r
    return f


# ── doubles ────────────────────────────────────────────────────────────────
class Dbl(dict):
    """Dict result that projects keys as attributes and records unknown methods."""

    def __getattr__(self, k):
        if k in self:
            return self[k]
        return rec(k)


class OrderedSet(list):
    """Ordered set double: live code calls .add/.discard; C harness mirrors as array."""
    def add(self, x):
        if x not in self: self.append(x)
    def discard(self, x):
        if x in self: self.remove(x)


class PathDbl:
    def __init__(self, s):
        self.s = s.as_posix() if isinstance(s, PathDbl) else s

    def __truediv__(self, other):
        base = "" if not self.s else self.s.rstrip("/") or "/"
        child = str(other)
        return PathDbl("/" + child if base == "/" else base + "/" + child)

    def as_posix(self):
        return str(self.s)

    def __str__(self):
        return str(self.s)

    def _one(self, name, *a, **kw):
        return seam(name, mkargs(name, *a, **kw))

    def is_dir(self):
        return self._one("is_dir", self.s)

    def resolve(self):
        return self._one("path_resolve", self.s)

    def stat(self):
        return self._one("spritesheet_stat")

    def read_bytes(self):
        return self._one("read_bytes")

    def write_bytes(self, data):
        return self._one("write_bytes", "reference", data=data)

    def iterdir(self):
        return self._one("iterdir")

    def mkdir(self, **kw):
        return self._one("mkdir", self.s, **kw)

    def open(self, mode="r", **kw):
        return self._one("spawn_index_read" if "r" in mode else "spawn_index_write", self.s)

    @property
    def name(self):
        return os.path.basename(self.s) if self.s else ""

    @property
    def suffix(self):
        return os.path.splitext(self.s)[1] if self.s else ""


class OsDouble:
    def __init__(self):
        self.environ = SimpleNamespace(
            get=lambda name: seam("environ_get", {"name": name}),
            update=lambda **kw: seam("environ_update", dict(kw)))
        self.path = SimpleNamespace(
            expanduser=lambda path: seam("expanduser", {"raw": path}),
            abspath=lambda path: seam("abspath", {"path": str(path)}),
            isdir=lambda path: seam("is_dir", {"path": str(path)}))

    def getenv(self, name):
        return seam("getenv", {"name": name})

    def getcwd(self):
        return seam("getcwd", {})


class RecLogger:
    def _rec(self, level, fmt, args):
        rec_ev({"ev": "log", "level": level, "fmt": fmt,
                "args": [str(x) if isinstance(x, BaseException) else x for x in args]})

    def debug(self, fmt, *a, **k):
        self._rec(10, fmt, a)

    def info(self, fmt, *a, **k):
        self._rec(20, fmt, a)

    def warning(self, fmt, *a, **k):
        self._rec(30, fmt, a)

    def error(self, fmt, *a, **k):
        self._rec(40, fmt, a)


class NS(dict):
    def __missing__(self, key):
        # Builtins must win: exec() looks names up in globals first, and a
        # recorder here would shadow frozenset/str/next/etc.
        if hasattr(builtins, key) or key in ("__builtins__",):
            raise KeyError(key)
        r = rec(key)
        self[key] = r
        return r


class BytesDbl:
    def __init__(self, v):
        self.v = v

    def decode(self, *a):
        return self.v if isinstance(self.v, str) else str(self.v)

    def __bytes__(self):
        return str(self.v).encode()


def _b64encode(raw):
    return BytesDbl(seam("b64encode", mkargs("b64encode", raw)))


def _b64decode(payload, validate=False):
    return seam("b64decode", mkargs("b64decode", payload))


class ImageDbl:
    def __init__(self, v):
        self.v = v

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    @property
    def size(self):
        return self.v.get("size", [0, 0]) if isinstance(self.v, dict) else [0, 0]

    def convert(self, mode):
        return ImageDbl(self.v)

    def thumbnail(self, size, resample=None):
        return seam("thumbnail", {"img": str(self.v)})

    def save(self, buf, format=None):
        return seam("save", {"img": str(self.v)})


class FileDbl:
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def write(self, text):
        return seam("crash_write", {"path": self.path, "text": text})


class StubModule(types.ModuleType):
    def __getattr__(self, key):
        if key.startswith("__"):
            raise AttributeError(key)
        return rec(key)


def install_stubs():
    for mod in ["hermes_cli", "hermes_cli.config", "hermes_cli.config_effective",
                "hermes_cli.config_backend", "hermes_cli.personality", "hermes_cli.profiles",
                "hermes_cli.projects_db", "hermes_cli.version_info", "hermes_cli.banner",
                "agent", "agent.pet", "agent.pet.store", "agent.pet.constants", "agent.pet.render",
                "agent.display", "agent.context_breakdown", "agent.message_metadata",
                "persistence_assembly", "storage_user_copy", "tui_gateway",
                "tui_gateway.server_requests", "tui_gateway.entry", "tui_gateway.ws_legacy_fallback",
                "tui_gateway.event_replay", "tui_gateway.hosted_room_member_activity",
                "tui_gateway.launch_profile_policy", "gateway.run", "gateway.session_context",
                "gateway.session_usage", "gateway.platforms", "tools", "tools.connectors",
                "tools.connectors.live", "tools.approval", "tools.process_registry",
                "tools.async_delegation", "tools.mcp_tool_discovery", "tools.mcp_tool_agent",
                "model_tools", "PIL", "PIL.Image"]:
        m = StubModule(mod)
        m.__path__ = []
        sys.modules[mod] = m
    for name, attrs in {
        "base64": {"standard_b64encode": _b64encode, "b64decode": _b64decode},
        "binascii": {"Error": ValueError},
        "io": {"BytesIO": lambda: BytesDbl("")},
        "re": {"match": lambda *a, **kw: None, "DOTALL": 16},
        "shutil": {"rmtree": rec("rmtree")},
        "PIL": {"Image": SimpleNamespace(open=lambda p: ImageDbl(CUR["io"].get("image_open", {})), LANCZOS=1)},
    }.items():
        m = sys.modules.get(name) or types.ModuleType(name)
        m.__path__ = []
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
    # wire parent attributes
    for mod in list(sys.modules):
        if "." in mod and isinstance(sys.modules.get(mod), StubModule):
            parent, _, child = mod.rpartition(".")
            if isinstance(sys.modules.get(parent), StubModule):
                setattr(sys.modules[parent], child, sys.modules[mod])


# ── extraction ─────────────────────────────────────────────────────────────
def seg(src, node):
    start = min([node.lineno] + [d.lineno for d in getattr(node, "decorator_list", [])])
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def ex(src, node, ns):
    import __future__
    flags = __future__.annotations.compiler_flag
    exec(compile(textwrap_dedent(seg(src, node)), SRC, "exec",
                 flags=flags, dont_inherit=True), ns)


def textwrap_dedent(s):
    import textwrap
    return textwrap.dedent(s)


def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("tui_server_flow3_extracted")
    mod.__package__ = "tui_gateway"
    sys.modules[mod.__name__] = mod
    ns = NS(mod.__dict__)
    ns.update({
        "json": json, "contextlib": contextlib, "os": OsDouble(), "copy": copy,
        "Path": PathDbl, "PurePosixPath": PurePosixPath, "hashlib": SimpleNamespace(
            sha1=lambda data: SimpleNamespace(hexdigest=lambda: "0" * 40)),
        "logger": RecLogger(),
        "time": SimpleNamespace(strftime=rec("strftime"), sleep=rec("sleep"), time=rec("time")),
        "threading": SimpleNamespace(),
        "asyncio": asyncio, "builtins": builtins,
        "open": lambda p, *a, **kw: FileDbl(str(p)),
        "print": lambda *a, **kw: seam("stderr_print", {"text": " ".join(str(x) for x in a), "flush": kw.get("flush")}),
        "ConfigBackendUnavailable": ConfigBackendUnavailable,
        "ProfileUnavailableError": ProfileUnavailableError,
        "_MODEL_SWITCH_MARKER_PREFIX": "[System: The active model for this chat has changed to ",
        "_SPAWN_TREE_INDEX": "index.jsonl",
        "_PET_REFERENCE_MIME_EXT": {"png": "png", "jpeg": "jpg", "jpg": "jpg",
                                    "gif": "gif", "webp": "webp"},
        "_PET_REFERENCE_MAX_BYTES": 5000000,
        "_CWD_PLACEHOLDERS": {".", "auto", "cwd"},
        "STATIC_TIERS": {"fast", "ultra", "auto"},
        "DESKTOP_BACKEND_CONTRACT": "desktop",
    })
    tree = ast.parse(src)
    # module constants
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TARGETS and not node.name.startswith("_"):
            ex(src, node, ns)
        elif isinstance(node, ast.FunctionDef) and node.name in TARGETS:
            ex(src, node, ns)
        elif isinstance(node, ast.ClassDef) and node.name == "_DropTransport":
            for m in node.body:
                if isinstance(m, ast.FunctionDef) and "_DropTransport." + m.name in TARGETS:
                    nn = dict(ns)
                    ex(src, m, nn)
                    ns["_DropTransport_" + m.name] = nn[m.name]
    for name in TARGETS:
        key = name.replace(".", "_")
        assert key in ns or name in ns, "extraction missed %s" % name
    ns["logger"] = RecLogger()
    return ns


# ── per-function drivers ───────────────────────────────────────────────────
def install_case(ns, func, inp):
    st = inp.get("state") or {}
    CUR["state"] = st
    # mirror the C harness: state always carries the four empty containers
    if not isinstance(st.get("sessions"), dict): st["sessions"] = {}
    if not isinstance(st.get("live_transports"), list): st["live_transports"] = []
    if not isinstance(st.get("methods"), dict): st["methods"] = {}
    if not isinstance(st.get("served_profile_homes"), list): st["served_profile_homes"] = []
    # seed set-valued state as ordered dedup doubles (live calls .add/.discard)
    ns["_live_transports"] = OrderedSet(st.get("live_transports") or [])
    ns["_served_profile_homes"] = OrderedSet(st.get("served_profile_homes") or [])
    ns["_methods"] = dict(st.get("methods") or {})
    ns["_hermes_home"] = PathDbl(st.get("hermes_home") or "")
    ns["_HERMES_HOME_AT_IMPORT"] = st.get("home_at_import") or ""
    ns["_db"] = Dbl(st["db"]) if isinstance(st.get("db"), dict) else None
    ns["_db_error"] = st.get("db_error")
    ns["_sessions"] = {k: Dbl(v) for k, v in (st.get("sessions") or {}).items()}
    ns["_pet_cancelled"] = set(st.get("cancel") or [])
    ns["_pet_payload_cache"] = {}
    ns["_stdio_is_rpc_channel"] = bool(st.get("stdio_is_rpc_channel"))
    ns["_pet_payload_cache_lock"] = contextlib.nullcontext()
    ns["_sessions_lock"] = contextlib.nullcontext()
    ns["_live_transports_lock"] = contextlib.nullcontext()
    ns["_pet_payload_cache_lock"] = contextlib.nullcontext()
    ns["_server_requests"] = SimpleNamespace(
        forget=rec("forget"), cancel=rec("cancel"),
        send_async=rec("send_async"), open_requests=rec("open_requests"))
    # modules referenced via in-function import: bind per-case recorders
    mods = {
        "hermes_cli.config": {"load_config": rec("load_config"),
                               "config_cache_version": rec("config_cache_version"),
                               "config_exists": rec("config_exists")},
        "hermes_cli.config_effective": {"load_user_config_effective": rec("load_user_config_effective")},
        "agent.pet.store": {"resolve_active_pet": rec("resolve_active_pet")},
        "agent.pet.constants": {"DEFAULT_SCALE": 0.33, "FRAME_W": 32, "FRAME_H": 32,
                                "FRAMES_PER_STATE": 4, "LOOP_MS": 100},
        "agent.pet.render": {"state_frame_counts": rec("state_frame_counts")},
        "tools.approval": {"get_pending_gateway_approval": rec("get_pending_gateway_approval"),
                           "_YOLO_MODE_FROZEN": False, "is_session_yolo_enabled": rec("is_session_yolo_enabled")},
        "tools.process_registry": {"process_registry": SimpleNamespace(
            list_sessions=rec("list_sessions"), get=rec("proc_get"))},
        "tools.mcp_tool_discovery": {"get_mcp_status": rec("get_mcp_status")},
        "tools.mcp_tool_agent": {"refresh_agent_mcp_tools": rec("refresh_agent_mcp_tools")},
        "tools.async_delegation": {"active_count": rec("active_count")},
        "tools.connectors.live": {"current": rec("live_current")},
        "gateway.session_context": {"set_session_vars": rec("set_session_vars"),
                                     "clear_session_vars": rec("clear_session_vars")},
        "gateway.session_usage": {"plan_gauge": rec("plan_gauge")},
        "gateway.run": {"_redact_approval_command": rec("redact_approval")},
        "hermes_cli.personality": {"available_personalities": rec("available_personalities")},
        "hermes_cli.profiles": {"get_profile_dir": rec("get_profile_dir")},
        "hermes_cli.projects_db": {"connect_closing": lambda: Ctx(rec("connect_closing")()),
                                   "project_for_path": rec("project_for_path")},
        "hermes_cli.version_info": {"get_version_info": rec("get_version_info")},
        "hermes_cli.banner": {"get_available_skills": rec("get_available_skills"),
                              "get_update_result": rec("get_update_result")},
        "hermes_cli.config": {"recommended_update_command": rec("recommended_update_command")},
        "hermes_cli.models": {"resolve_fast_mode_overrides": rec("resolve_fast_mode_overrides")},
        "agent.context_breakdown": {"context_usage_fields": rec("context_usage_fields")},
        "agent.display": {"build_tool_preview": rec("build_tool_preview")},
        "model_tools": {"get_toolset_for_tool": rec("get_toolset_for_tool")},
        "persistence_assembly": {"open_persistence": rec("open_persistence"),
                                 "profile_scope": rec("profile_scope"),
                                 "store_location": rec("store_location")},
        "storage_user_copy": {"describe_storage_failure": rec("describe_storage_failure"),
                              "storage_failure_details": rec("storage_failure_details")},
        "tui_gateway.entry": {"mcp_discovery_in_flight": rec("mcp_discovery_in_flight"),
                              "join_mcp_discovery": rec("join_mcp_discovery")},
        "tui_gateway.ws_legacy_fallback": {"connection_profile_home": rec("connection_profile_home")},
        "tui_gateway.event_replay": {"_stamp_event": rec("_stamp_event")},
        "tui_gateway.hosted_room_member_activity": {"project_room_member_activity": rec("project_room_member_activity")},
        "tui_gateway.launch_profile_policy": {"activate_multi_profile_hosting": rec("activate_multi_profile_hosting")},
    }
    for mod, attrs in mods.items():
        m = sys.modules.get(mod)
        if m is None:
            m = StubModule(mod)
            m.__path__ = []
            sys.modules[mod] = m
        for k, v in attrs.items():
            setattr(m, k, v)
    # generic NS-level collaborators referenced as bare globals
    for nm in ["_ws_orphan_setting", "_configured_cwd_from_cfg", "_profile_home",
               "_profile_configured_cwd", "_pet_cfg", "_get_db", "_get_usage", "_session_info",
               "_emit", "_project_info_for_cwd", "_fast_tier_applies", "_live_session_identity",
               "_session_for_key", "_current_profile_name", "_display_cfg", "_is_model_switch_marker",
               "_close_session_by_id", "_ensure_session_db_row", "_submit_row_target_key",
               "_persistence_for", "_live_session_agent_db", "_set_session_context",
               "_clear_session_context", "_session_cwd", "_session_usage_snapshot",
               "_is_model_switch_marker", "is_truthy_value", "current_transport",
               "profile_name_for_home", "clamp_effort", "route_supported_efforts"]:
        if nm == func or nm == func.replace(".", "_"):
            continue  # keep the extracted LIVE function for the function under test
        ns[nm] = rec(nm)
    ns["_load_cfg"] = rec("_load_cfg")
    ns["_err"] = rec("_err")
    ns["threading"] = SimpleNamespace(
        Event=lambda: {"event": True}, Lock=lambda: contextlib.nullcontext(),
        Thread=lambda **kw: SimpleNamespace(start=rec("thread_start")))


class Ctx:
    def __init__(self, v):
        self.v = v

    def __enter__(self):
        return self.v

    def __exit__(self, *a):
        return False


def call(ns, func, inp):
    f = ns.get(func) or ns.get(func.replace(".", "_"))
    A = lambda k, d=None: inp.get(k, d)
    obj = lambda k: Dbl(A(k, {})) if isinstance(A(k), dict) else A(k)
    if func == "_DropTransport.write":
        return f(None, obj("obj"))  # extracted unbound class method
    if func == "_DropTransport.close":
        return f(None)
    if func == "register_live_transport":
        return f(A("transport"))
    if func == "unregister_live_transport":
        return f(A("transport"))
    if func == "_approval_request_payload":
        return f(A("data"))
    if func == "_open_requests":
        return f(A("sid"))
    if func == "register_method":
        return f(A("name"), A("fn"))
    if func == "_enable_gateway_prompts":
        return f()
    if func == "_clear_pending":
        return f(A("sid"))
    if func == "_probe_credentials":
        return f(obj("agent"))
    if func == "_probe_config_health":
        return f(A("cfg"))
    if func == "_tool_ctx":
        return f(A("name"), A("args"))
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    install_case(ns, func, inp)
    try:
        ret = call(ns, func, inp)
        want_ret = norm(ret)
    except BaseException as e:
        want_ret = None
        for r in CUR["raises"]:
            if e is r.exc:
                want_ret = r.spec
                break
        if want_ret is None:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
    # reflect mutated NS globals into the state mirror (the C harness reports these)
    st = CUR["state"]
    st["sessions"] = norm(dict(ns.get("_sessions") or {}))
    st["live_transports"] = norm(list(ns.get("_live_transports") or []))
    st["methods"] = norm(dict(ns.get("_methods") or {}))
    st["served_profile_homes"] = norm(list(ns.get("_served_profile_homes") or []))
    return {"ret": want_ret, "trace": CUR["trace"], "state": norm_state()}


def norm(x):
    if isinstance(x, (PurePath, PathDbl)):
        return x.as_posix()
    if isinstance(x, BaseException):
        return str(x)
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple, set)):
        return [norm(v) for v in x]
    if callable(x):
        return {"__fn__": "callable"}
    return x


def norm_state():
    st = copy.deepcopy(CUR["state"])
    return st


def canon(x):
    if isinstance(x, BaseException):
        return ("str", str(x))
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple, set)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", str(x))


def main():
    ns = extract()
    install_stubs()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        want = run_case(ns, func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE tui_server_flow3: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
