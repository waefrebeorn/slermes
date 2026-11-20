#!/usr/bin/env python3
"""Oracle for the gateway/run_notifications.py UNCLAIMED-remainder FLOW cores
wave 3 (src/gateway/run_notifications_flow3.c).

AST-extracts the LIVE GatewayNotificationsMixin methods (+ the nested
_CompletionClaim dataclass and module constants), the real present_notification
from gateway/warning_notifications.py and the real _send_failed/_send_error from
gateway/run_shutdown.py, and replaces every live I/O dependency with recording
stubs driven by the SAME per-name scripts the C harness feeds its universal
seams (in.io). Compares the ordered seam-call trace plus the return value and
the mutated state.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion, empty -> None);
{"__raise__": kind, "msg": text} raises a REAL exception of that kind; a scripted
null -> None; ABSENT key -> FAIL-CLOSED RuntimeError "no script for <name>"
(recorded as ret, then raised). BaseException kinds pass through every
`except Exception`; except LookupError / WakeNotAccepted / AuthorityRefusal match
by the real builtin hierarchy. Ported siblings run inline (real) on both sides;
claimed / foreign siblings are seams.

Input: stdin JSON lines from t_port_run_notifications_flow3.
Summary: ORACLE run_notifications_flow3: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import contextlib
import contextvars
import copy
import dataclasses
import json
import os
import sys
import types
import typing
import __future__
from pathlib import Path, PurePosixPath, PurePath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_notifications.py")
SHUTDOWN_SRC = os.path.join(QUARRY, "gateway", "run_shutdown.py")
WARN_SRC = os.path.join(QUARRY, "gateway", "warning_notifications.py")

CUR = {"func": None, "in": {}, "trace": []}
SEQPOS = {}


def rec(ev):
    CUR["trace"].append(ev)


def deepcopy(x):
    return copy.deepcopy(x)


def script(name, args):
    io = CUR["in"].get("io") or {}
    if name not in io:
        spec = {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
        rec({"ev": name, "args": args, "ret": spec})
        return spec
    sc = io[name]
    val = sc
    if isinstance(sc, dict) and "__seq__" in sc:
        seq = sc["__seq__"]
        idx = SEQPOS.get(name, 0)
        if not seq:
            val = None
        else:
            if idx >= len(seq):
                idx = len(seq) - 1
            val = seq[idx]
            SEQPOS[name] = idx + 1
    rec({"ev": name, "args": deepcopy(args), "ret": deepcopy(val)})
    return val


# ── custom quarry exception classes (identity-matched by the extracted defs'
# in-function imports via sys.modules) ──
class WakeNotAccepted(RuntimeError):
    def __init__(self, message="internal wake not accepted by adapter", *, reason=None):
        super().__init__(message)
        self.reason = reason
        self.permanent = False


class PersistenceError(Exception):
    pass


class AuthorityRefusal(PersistenceError, ValueError):
    def __init__(self, reason, *, field=None, detail=None):
        self.reason = reason
        self.field = field
        self.detail = detail
        super().__init__(reason)


BUILTIN_EXC = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError, "BaseExceptionGroup": BaseExceptionGroup,
    "Exception": Exception, "ExceptionGroup": ExceptionGroup,
    "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
    "KeyError": KeyError, "LookupError": LookupError, "IndexError": IndexError,
    "OSError": OSError, "FileNotFoundError": FileNotFoundError, "RuntimeError": RuntimeError,
    "TimeoutError": TimeoutError, "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "OverflowError": OverflowError, "ArithmeticError": ArithmeticError,
    "ZeroDivisionError": ZeroDivisionError, "FloatingPointError": FloatingPointError,
    "AssertionError": AssertionError, "BufferError": BufferError, "EOFError": EOFError,
    "MemoryError": MemoryError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "ReferenceError": ReferenceError, "StopAsyncIteration": StopAsyncIteration,
    "StopIteration": StopIteration, "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
    "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
    "EncodingWarning": EncodingWarning, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "WakeNotAccepted":
        e = WakeNotAccepted(msg, reason=v.get("reason"))
        e.permanent = bool(v.get("permanent", False))
        return e
    if kind == "AuthorityRefusal":
        return AuthorityRefusal(msg)
    cls = BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    # KeyError scripts carry the RAW key; str(KeyError(key)) == repr(key), which
    # is the projected form the C port emits (rn3_exc_str).
    return cls(msg)


def materialize(v):
    if isinstance(v, dict) and "__raise__" in v:
        raise make_exc(v)
    return v


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class RecLogger:
    def _norm_arg(self, a):
        if isinstance(a, BaseException):
            return {"__raise__": type(a).__name__, "msg": str(a)}
        return a

    def _rec(self, name, level, fmt, a, k):
        args = {"fmt": fmt}
        if level is not None:
            args["level"] = level
        if a:
            args["args"] = [self._norm_arg(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return materialize(script(name, args))

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", None, fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", None, fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", None, fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", None, fmt, a, k)

    def exception(self, fmt, *a, **k):
        return self._rec("logger.error", None, fmt, a, k)

    def log(self, level, fmt, *a, **k):
        return self._rec("logger.log", level, fmt, a, k)


class Platform:
    def __init__(self, value):
        if value not in PLATFORM_VALUES:
            raise ValueError("%r is not a valid Platform" % value)
        self.value = value

    def __hash__(self):
        return hash(self.value)

    def __eq__(self, other):
        return isinstance(other, Platform) and self.value == other.value

    def __repr__(self):
        return "Platform(%r)" % self.value


PLATFORM_VALUES = [
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
    "signal", "mattermost", "matrix", "email", "sms", "dingtalk", "api_server",
    "webhook", "msgraph_webhook", "feishu", "wecom", "wecom_callback", "weixin",
    "bluebubbles", "qqbot", "relay",
]
for _pv in PLATFORM_VALUES:
    setattr(Platform, _pv.upper(), Platform(_pv))


class BasePlatformAdapter:
    pass


class MessageEvent:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class SessionSource:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class MessageType:
    TEXT = "text"


class AttrDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)

    def __setattr__(self, name, val):
        object.__getattribute__(self, "_d")[name] = val


def mat_send_result(v):
    return AttrDouble(v) if isinstance(v, dict) else v


class AdapterDouble:
    """Adapter double: PRESENT hook keys model callable methods; data fields read."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name == "send":
            if "send" not in d:
                raise AttributeError("missing attribute 'send'")

            async def method(*a, **k):
                return mat_send_result(materialize(script("adapter.send", {
                    "chat_id": a[0], "message": a[1], "metadata": k.get("metadata")})))
            return method
        if name == "send_final_ledgered":
            if "send_final_ledgered" not in d:
                raise AttributeError("missing attribute 'send_final_ledgered'")

            async def method(*a, **k):
                return mat("adapter.send_final_ledgered", materialize(script("adapter.send_final_ledgered", {
                    "event": denorm(a[0]), "session_key": a[1], "text": a[2],
                    "metadata": denorm(a[3]), "reply_to": k.get("reply_to")})))
            return method
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class BaseAdapterDouble(BasePlatformAdapter, AdapterDouble):
    pass


def mat_adapter(x):
    if isinstance(x, dict):
        return BaseAdapterDouble(x) if x.get("__base__") else AdapterDouble(x)
    return x


class SessionStoreDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))
        ents = d.get("_entries") or {}
        object.__setattr__(self, "_entries",
                           {k: (AttrDouble(v) if isinstance(v, dict) else v) for k, v in ents.items()})

    def _generate_session_key(self, source):
        return materialize(script("session_store._generate_session_key", {"source": denorm(source)}))

    def lookup_by_session_key(self, key):
        return materialize(script("session_store.lookup_by_session_key", {"session_key": key}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class TransportDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name == "send":
            if "send" not in d:
                raise AttributeError("missing attribute 'send'")

            async def method(*a, **k):
                return mat_send_result(materialize(script("transport.send", {
                    "platform": a[0].value if isinstance(a[0], Platform) else a[0],
                    "chat_id": a[1], "message": a[2], "metadata": k.get("metadata")})))
            return method
        if name == "adapter":
            if "adapter" not in d:
                raise AttributeError("missing attribute 'adapter'")
            return mat_adapter(d["adapter"])
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class LoopDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name == "is_running":
            def method(*a, **k):
                return materialize(script("loop.is_running", {"id": d.get("id")}))
            return method
        if name in d:
            return d[name]
        raise AttributeError("missing attribute '%s'" % name)


class PathDouble:
    def __init__(self, path):
        self.path = path

    def exists(self):
        return materialize(script("path.exists", {"path": self.path}))

    def read_text(self, encoding=None):
        v = materialize(script("path.read_text", {"path": self.path, "encoding": encoding}))
        if isinstance(v, str):
            return v.removeprefix("\ufeff")   # utf-8-sig strips ONE leading BOM
        return v

    def unlink(self, missing_ok=False):
        return materialize(script("path.unlink", {"path": self.path, "missing_ok": missing_ok}))

    def as_posix(self):
        return self.path


class FutureDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def done(self):
        return materialize(script("future.done", {"id": self._d["id"]}))

    def set_result(self, result):
        materialize(script("future.set_result", {"id": self._d["id"], "result": deepcopy(result)}))
        self._d["done"] = True
        self._d["result"] = result


class QueueDouble:
    def __init__(self, qid):
        self.id = qid

    def put(self, evt):
        return materialize(script("queue.put", {"queue": self.id, "evt": deepcopy(evt)}))


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class AsyncScopeDouble:
    def __init__(self, marker):
        self.marker = marker

    async def __aenter__(self):
        materialize(script("scope_enter", {"scope": deepcopy(self.marker)}))
        return self

    async def __aexit__(self, *exc):
        materialize(script("scope_exit", {"scope": deepcopy(self.marker)}))
        return False


def mat_scope(v):
    if isinstance(v, dict):
        if v.get("scope") == "nullcontext":
            return contextlib.nullcontext()
        return AsyncScopeDouble(v)
    return v


def mat_pcfg(pc):
    if not isinstance(pc, dict):
        return pc
    d = dict(pc)
    if isinstance(d.get("home_channel"), dict):
        d["home_channel"] = AttrDouble(d["home_channel"])
    return AttrDouble(d)


def mat_profile_cfg(pcfg):
    if not isinstance(pcfg, dict):
        return pcfg
    d = dict(pcfg)
    if isinstance(d.get("platforms"), dict):
        d["platforms"] = {Platform(pv): mat_pcfg(pc) for pv, pc in d["platforms"].items()}
    return AttrDouble(d)


def sorted_targets(targets):
    """set of tuples -> sorted list of lists (compact-JSON key, both sides)."""
    return sorted([list(t) for t in targets], key=lambda t: json.dumps(t, separators=(",", ":")))


def mat(name, v):
    if v is None:
        return None
    if name in ("self._build_process_event_source",):
        return types.SimpleNamespace(**v) if isinstance(v, dict) else v
    if name == "self._served_home_channel_configs":
        if isinstance(v, list):
            return [[prof, Platform(pv), mat_pcfg(cfg)] for prof, pv, cfg in v]
        return v
    if name == "self._send_home_channel_startup_notifications":
        return {tuple(t) for t in v} if isinstance(v, list) else v
    if name == "self._home_channel_transports":
        if isinstance(v, list):
            return [[Platform(pv), pcfg,
                     (AttrDouble(h) if isinstance(h, dict) else h),
                     (TransportDouble(t) if isinstance(t, dict) else t)]
                    for pv, pcfg, h, t in v]
        return v
    if name == "self._completion_delivery_identity":
        return tuple(v) if isinstance(v, list) else v
    if name == "self._preflight_completion_delivery":
        return types.SimpleNamespace(**v) if isinstance(v, dict) else v
    if name == "self._adapter_for_source":
        return mat_adapter(v)
    if name == "session_store.lookup_by_session_key":
        return AttrDouble(v) if isinstance(v, dict) else v
    if name == "_served_notice_target_key":
        return tuple(v) if isinstance(v, list) else v
    if name == "describe_storage_failure":
        return types.SimpleNamespace(**v) if isinstance(v, dict) else v
    if name == "get_default_hermes_root":
        return Path(v) if isinstance(v, str) else v
    if name == "restore_native":
        if isinstance(v, dict) and isinstance(v.get("source"), dict):
            return types.SimpleNamespace(source=types.SimpleNamespace(**v["source"]))
        return v
    if name in ("completion_event_scope_or_refuse", "_async_profile_runtime_scope",
                "async_launch_profile_scope_if_multiplexed"):
        return mat_scope(v)
    if name == "_planned_restart_notification_path":
        return PathDouble(v) if isinstance(v, str) else v
    if name == "adapter.send_final_ledgered":
        if isinstance(v, list):
            return [mat_send_result(x) if i == 0 else x for i, x in enumerate(v)]
        return v
    if name in ("adapter.send", "transport.send"):
        return mat_send_result(v)
    return v


class SeamFn:
    def __init__(self, name, argmap, async_=False):
        self.name = name
        self.argmap = argmap
        self.async_ = async_

    def __call__(self, *a, **k):
        args = self.argmap(a, k)
        v = mat(self.name, materialize(script(self.name, args)))
        return v


def seam(name, argmap, async_=False):
    fn = SeamFn(name, argmap, async_)
    if async_:
        async def wrapper(*a, **k):
            return fn(*a, **k)
        return wrapper
    return fn


def proj_self(sd):
    return {k: denorm(v) for k, v in vars(sd).items() if not callable(v)}


def proj_platform(p):
    return p.value if isinstance(p, Platform) else p


def proj_path(p):
    if isinstance(p, PathDouble):
        return p.path
    if isinstance(p, PurePath):
        return p.as_posix()   # str(Path) flips slashes on Windows; normalize
    return p


ARGMAPS = {
    "self._thread_metadata_for_target": lambda a, k: {
        "platform": proj_platform(a[0]), "chat_id": a[1], "thread_id": a[2],
        "adapter": denorm(k.get("adapter"))},
    "self._note_restart_notice_boot": lambda a, k: {},
    "self._served_home_channel_configs": lambda a, k: {},
    "self._send_home_channel_startup_notifications": lambda a, k: {
        "skip_targets": sorted_targets(k.get("skip_targets") or set())},
    "self._home_channel_transports": lambda a, k: {},
    "self._open_session_db_for_active_scope": lambda a, k: {},
    "self._served_api_server_wake_profile": lambda a, k: {"evt": deepcopy(a[0]), "raw_sid": a[1]},
    "self._resolve_profile_home_for_source": lambda a, k: {"source": denorm(a[0])},
    "self._build_process_event_source": lambda a, k: {"evt": deepcopy(a[0])},
    "self._completion_delivery_identity": lambda a, k: {"evt": deepcopy(a[0])},
    "self._preflight_completion_delivery": lambda a, k: {"evt": deepcopy(a[0])},
    "self._completion_identity_seen": lambda a, k: {"identity": list(a[0]), "claim": k.get("claim", False)},
    "self._inject_watch_notification": lambda a, k: {
        "synth_text": a[0], "evt": deepcopy(a[1]), "raise_not_accepted": k.get("raise_not_accepted", False)},
    "self._mark_completions_delivered_locked": lambda a, k: {"identities": [list(i) for i in a[0]]},
    "self._settle_durable_claim": lambda a, k: {"kind": a[0], "delegation_id": a[1], "claim_id": a[2]},
    "self._record_coalesced_completion_siblings": lambda a, k: {"events": [deepcopy(e) for e in a[0]]},
    "self._deliver_async_delegation_group_scoped": lambda a, k: {"group": deepcopy(a[0])},
    "self._back_off_refused_completions": lambda a, k: {"group": deepcopy(a[0])},
    "self._adapter_for_source": lambda a, k: {"source": denorm(a[0])},
    "self._run_process_watcher": lambda a, k: {"watcher": deepcopy(a[0])},
    "session_store._generate_session_key": lambda a, k: {"source": denorm(a[0])},
    "session_store.lookup_by_session_key": lambda a, k: {"session_key": a[0]},
    "_mark_notify_metadata": lambda a, k: {"metadata": deepcopy(a[0])},
    "_non_conversational_metadata": lambda a, k: {"metadata": deepcopy(a[0]), "platform": proj_platform(k["platform"])},
    "_served_notice_target_key": lambda a, k: {"profile": a[0], "platform_value": a[1], "chat_id": a[2], "thread_id": a[3]},
    "_raw_process_event_session_id": lambda a, k: {"evt": deepcopy(a[0])},
    "_mark_internal_notification": lambda a, k: {"text": a[0]},
    "diagnostic_process_event": lambda a, k: {"evt": deepcopy(a[0])},
    "classify_persistence_error": lambda a, k: {"error": a[0]},
    "profile_cli_selector": lambda a, k: {},
    "get_default_hermes_root": lambda a, k: {},
    "profile_scope": lambda a, k: {},
    "store_location": lambda a, k: {"scope": a[0]},
    "describe_storage_failure": lambda a, k: {"error": a[0]},
    "t": lambda a, k: dict({"key": a[0]}, **{kk: proj_path(vv) for kk, vv in k.items()}),
    "active_authority": lambda a, k: {"self": proj_self(a[0])},
    "committed_route_envelope": lambda a, k: {"authority": denorm(a[0]), "entry": denorm(a[1])},
    "restore_native": lambda a, k: {"envelope": deepcopy(a[0]), "self": proj_self(a[1])},
    "completion_event_scope_or_refuse": lambda a, k: {"self": proj_self(a[0]), "evt": deepcopy(a[1]), "label": a[2]},
    "persist_delegation_delivery": lambda a, k: {
        "adapter": denorm(a[0]), "text": k["text"], "session_id": k["session_id"], "evt": deepcopy(k["evt"])},
    "deliver_wake": lambda a, k: {
        "adapter": denorm(a[0]), "text": k["text"], "session_id": k["session_id"],
        "profile": k["profile"], "notification_category": k["notification_category"]},
    "_async_profile_runtime_scope": lambda a, k: {"home": proj_path(a[0])},
    "async_launch_profile_scope_if_multiplexed": lambda a, k: {},
    "get_hermes_home_override": lambda a, k: {},
    "should_surface_notification": lambda a, k: {"evt": deepcopy(a[0])},
    "restore_completion_route": lambda a, k: {"self": proj_self(a[0]), "session_key": a[1]},
    "follow_detached_pin": lambda a, k: {"self": proj_self(a[0]), "evt": deepcopy(a[1])},
    "completion_admission": lambda a, k: {"self": proj_self(a[0]), "evt": deepcopy(a[1])},
    "warning_notifications_enabled": lambda a, k: {"platform": proj_platform(a[0]), "user_config": a[1] if len(a) > 1 else k.get("user_config")},
    "time.monotonic": lambda a, k: {},
    "safe_schedule_threadsafe": lambda a, k: {
        "coro": denorm(a[0]), "loop": denorm(a[1]), "logger": "logger", "log_message": k["log_message"]},
    "atomic_json_write": lambda a, k: {"path": proj_path(a[0]), "data": _proj_write_data(a[1]), "indent": k.get("indent")},
    "_planned_restart_notification_path": lambda a, k: {},
}
ASYNC_SEAMS = {"follow_detached_pin", "completion_event_scope_or_refuse",
               "self._preflight_completion_delivery", "self._inject_watch_notification",
               "self._deliver_async_delegation_group_scoped",
               "self._send_home_channel_startup_notifications",
               "persist_delegation_delivery", "deliver_wake"}

SELF_SEAMS = {
    "_thread_metadata_for_target": "self._thread_metadata_for_target",
    "_note_restart_notice_boot": "self._note_restart_notice_boot",
    "_served_home_channel_configs": "self._served_home_channel_configs",
    "_send_home_channel_startup_notifications": "self._send_home_channel_startup_notifications",
    "_home_channel_transports": "self._home_channel_transports",
    "_open_session_db_for_active_scope": "self._open_session_db_for_active_scope",
    "_served_api_server_wake_profile": "self._served_api_server_wake_profile",
    "_resolve_profile_home_for_source": "self._resolve_profile_home_for_source",
    "_build_process_event_source": "self._build_process_event_source",
    "_completion_delivery_identity": "self._completion_delivery_identity",
    "_preflight_completion_delivery": "self._preflight_completion_delivery",
    "_completion_identity_seen": "self._completion_identity_seen",
    "_inject_watch_notification": "self._inject_watch_notification",
    "_mark_completions_delivered_locked": "self._mark_completions_delivered_locked",
    "_settle_durable_claim": "self._settle_durable_claim",
    "_record_coalesced_completion_siblings": "self._record_coalesced_completion_siblings",
    "_deliver_async_delegation_group_scoped": "self._deliver_async_delegation_group_scoped",
    "_back_off_refused_completions": "self._back_off_refused_completions",
    "_adapter_for_source": "self._adapter_for_source",
    "_run_process_watcher": "self._run_process_watcher",
}

METHODS = (
    "_send_queued_final_text", "_send_home_channel_message",
    "_replay_pending_planned_restart_notification", "_send_session_db_warning_notifications",
    "_self_post_api_server", "_committed_relay_adapter", "_completion_event_scope",
    "_served_api_server_event_scope", "_deliver_completion_notification_scoped",
    "_settle_batch_waiters", "_deliver_async_delegation_group",
    "_deliver_async_delegation_groups", "arm_process_watcher",
)
STATIC_METHODS = {"_settle_batch_waiters"}
PORTED = set(METHODS)


def _proj_write_data(data):
    """atomic_json_write data arg: delivered_targets is a set-iteration list ->
    sort it (the live set order is hash-randomized; C sorts identically)."""
    if isinstance(data, dict):
        d = {k: deepcopy(v) for k, v in data.items()}
        if isinstance(d.get("delivered_targets"), list):
            d["delivered_targets"] = sorted(d["delivered_targets"],
                                            key=lambda t: json.dumps(t, separators=(",", ":")))
        return d
    return deepcopy(data)


def extract():
    src = open(SRC, encoding="utf-8").read()
    modname = "rn3_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns["__name__"] = modname
    ns["__file__"] = SRC
    ns["asyncio"] = asyncio
    ns["contextlib"] = contextlib
    ns["contextvars"] = contextvars
    ns["dataclasses"] = dataclasses
    ns["logging"] = __import__("logging")
    ns["cast"] = typing.cast
    ns["Any"] = typing.Any
    ns["Optional"] = typing.Optional
    ns["Dict"] = typing.Dict
    ns["Path"] = Path
    ns["json"] = json
    ns["logger"] = RecLogger()
    ns["Platform"] = Platform
    ns["_BUILTIN_PLATFORM_VALUES"] = tuple(PLATFORM_VALUES)
    ns["BasePlatformAdapter"] = BasePlatformAdapter
    ns["MessageEvent"] = MessageEvent
    ns["SessionSource"] = SessionSource
    ns["SessionEntry"] = type("SessionEntry", (), {})
    ns["SessionStore"] = type("SessionStore", (), {})
    ns["MessageType"] = MessageType
    ns["jittered_backoff"] = lambda *a, **k: 0.0
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)):
                continue
        else:
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayNotificationsMixin":
            for sub in node.body:
                if isinstance(sub, ast.ClassDef) and sub.name == "_CompletionClaim":
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec", flags=flags), ns)
                elif isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in PORTED:
                    sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec", flags=flags), ns)
    # the asynccontextmanager method: re-wrap the extracted async generator
    ns["_served_api_server_event_scope"] = contextlib.asynccontextmanager(ns["_served_api_server_event_scope"])
    # real present_notification (gateway/warning_notifications.py)
    wtree = ast.parse(open(WARN_SRC, encoding="utf-8").read())
    for node in wtree.body:
        if isinstance(node, ast.AsyncFunctionDef) and node.name == "present_notification":
            exec(compile(ast.Module(body=[node], type_ignores=[]), WARN_SRC, "exec", flags=flags), ns)
    # real _send_failed / _send_error (gateway/run_shutdown.py)
    stree = ast.parse(open(SHUTDOWN_SRC, encoding="utf-8").read())
    for node in stree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_send_failed", "_send_error"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SHUTDOWN_SRC, "exec", flags=flags), ns)
    need = PORTED | {"_CompletionClaim", "present_notification", "_send_failed", "_send_error",
                     "_SELF_CLEARING_STORAGE_CAUSES"}
    missing = need - set(ns)
    if missing:
        raise RuntimeError("extract failed to bind: %s" % sorted(missing))
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}


def denorm(x):
    if isinstance(x, Platform):
        return x.value
    if isinstance(x, (AdapterDouble, AttrDouble, SessionStoreDouble, TransportDouble, LoopDouble, FutureDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, PathDouble):
        return x.path
    if isinstance(x, QueueDouble):
        return {"id": x.id}
    if isinstance(x, AsyncScopeDouble):
        return deepcopy(x.marker)
    if isinstance(x, contextlib.nullcontext):
        return {"scope": "nullcontext"}
    _ACM = getattr(contextlib, "_AsyncGeneratorContextManager", None)
    if _ACM is not None and isinstance(x, _ACM):
        return {"scope": "served_api_server", "evt": deepcopy(x.args[1]), "raw_sid": x.args[2]}
    if isinstance(x, (NoopLock, asyncio.Lock)):
        return True
    if isinstance(x, RecLogger):
        return "logger"
    if isinstance(x, (MessageEvent, SessionSource)):
        return {k: denorm(v) for k, v in vars(x).items()}
    if isinstance(x, types.SimpleNamespace):
        return {k: denorm(v) for k, v in vars(x).items()}
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return denorm(dataclasses.asdict(x))
    if isinstance(x, PurePosixPath):
        return x.as_posix()
    if isinstance(x, dict):
        return {denorm(k): denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        # set iteration is hash-randomized; project to a sorted list (compact-JSON
        # key) so seam args carrying runner state match the C sorted projection.
        return sorted([denorm(v) for v in x], key=lambda t: json.dumps(t, separators=(",", ":")))
    if isinstance(x, BaseException):
        return {"__raise__": type(x).__name__, "msg": str(x)}
    return x


def stubmod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def install_globals():
    NS["logger"] = RecLogger()
    NS["Platform"] = Platform
    NS["time"] = types.SimpleNamespace(monotonic=seam("time.monotonic", ARGMAPS["time.monotonic"]))
    NS["t"] = seam("t", ARGMAPS["t"])
    NS["_mark_notify_metadata"] = seam("_mark_notify_metadata", ARGMAPS["_mark_notify_metadata"])
    NS["_non_conversational_metadata"] = seam("_non_conversational_metadata", ARGMAPS["_non_conversational_metadata"])
    NS["_served_notice_target_key"] = seam("_served_notice_target_key", ARGMAPS["_served_notice_target_key"])
    NS["_raw_process_event_session_id"] = seam("_raw_process_event_session_id", ARGMAPS["_raw_process_event_session_id"])
    NS["_mark_internal_notification"] = seam("_mark_internal_notification", ARGMAPS["_mark_internal_notification"])
    NS["completion_event_scope_or_refuse"] = seam("completion_event_scope_or_refuse",
                                                  ARGMAPS["completion_event_scope_or_refuse"], async_=True)
    NS["warning_notifications_enabled"] = seam("warning_notifications_enabled", ARGMAPS["warning_notifications_enabled"])
    NS["_notice_target_key"] = lambda *a, **k: None
    NS["_delivery_target_key"] = lambda *a, **k: None
    NS["_log_suppressed"] = contextlib.suppress
    for pkgname in ("gateway", "tools", "agent", "persistence", "persistence.core", "tui_gateway"):
        pkg(pkgname)
    pkg("gateway.platforms")
    stubmod("gateway.run",
            _non_conversational_metadata=NS["_non_conversational_metadata"],
            _planned_restart_notification_path=seam("_planned_restart_notification_path", ARGMAPS["_planned_restart_notification_path"]),
            _async_profile_runtime_scope=seam("_async_profile_runtime_scope", ARGMAPS["_async_profile_runtime_scope"]))
    stubmod("utils", atomic_json_write=seam("atomic_json_write", ARGMAPS["atomic_json_write"]))
    stubmod("hermes_constants",
            get_default_hermes_root=seam("get_default_hermes_root", ARGMAPS["get_default_hermes_root"]),
            profile_cli_selector=seam("profile_cli_selector", ARGMAPS["profile_cli_selector"]),
            get_hermes_home_override=seam("get_hermes_home_override", ARGMAPS["get_hermes_home_override"]))
    stubmod("storage_errors", classify_persistence_error=seam("classify_persistence_error", ARGMAPS["classify_persistence_error"]))
    stubmod("persistence_assembly",
            profile_scope=seam("profile_scope", ARGMAPS["profile_scope"]),
            store_location=seam("store_location", ARGMAPS["store_location"]))
    stubmod("storage_user_copy", describe_storage_failure=seam("describe_storage_failure", ARGMAPS["describe_storage_failure"]))
    stubmod("gateway.warning_notifications",
            present_notification=NS["present_notification"],
            warning_notifications_enabled=seam("warning_notifications_enabled", ARGMAPS["warning_notifications_enabled"]))
    stubmod("gateway.wake",
            deliver_wake=seam("deliver_wake", ARGMAPS["deliver_wake"], async_=True),
            persist_delegation_delivery=seam("persist_delegation_delivery", ARGMAPS["persist_delegation_delivery"], async_=True),
            WakeNotAccepted=WakeNotAccepted)
    stubmod("agent.notification_presentation", diagnostic_process_event=seam("diagnostic_process_event", ARGMAPS["diagnostic_process_event"]))
    stubmod("gateway.session_authorities", active_authority=seam("active_authority", ARGMAPS["active_authority"]))
    stubmod("gateway.session_automation",
            committed_route_envelope=seam("committed_route_envelope", ARGMAPS["committed_route_envelope"]),
            completion_admission=seam("completion_admission", ARGMAPS["completion_admission"]))
    stubmod("gateway.session_envelope", restore_native=seam("restore_native", ARGMAPS["restore_native"]))
    stubmod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal, PersistenceError=PersistenceError)
    stubmod("gateway.session_local_recovery", restore_completion_route=seam("restore_completion_route", ARGMAPS["restore_completion_route"]))
    stubmod("gateway.session_detached", follow_detached_pin=seam("follow_detached_pin", ARGMAPS["follow_detached_pin"], async_=True))
    stubmod("tools.process_registry_notifications", should_surface_notification=seam("should_surface_notification", ARGMAPS["should_surface_notification"]))
    stubmod("tui_gateway.launch_profile_policy",
            async_launch_profile_scope_if_multiplexed=seam("async_launch_profile_scope_if_multiplexed", ARGMAPS["async_launch_profile_scope_if_multiplexed"]))
    stubmod("agent.async_utils", safe_schedule_threadsafe=seam("safe_schedule_threadsafe", ARGMAPS["safe_schedule_threadsafe"]))
    stubmod("gateway.platforms.base", BasePlatformAdapter=BasePlatformAdapter, _mark_notify_metadata=NS["_mark_notify_metadata"])
    stubmod("agent.i18n", t=NS["t"])


install_globals()


def build_self(inp):
    sd = types.SimpleNamespace()
    d = inp.get("self") or {}
    for k, v in d.items():
        if k == "adapters":
            setattr(sd, k, {Platform(pv): mat_adapter(av) for pv, av in v.items()})
        elif k == "config":
            cfg = dict(v)
            if isinstance(cfg.get("platforms"), dict):
                cfg["platforms"] = {Platform(pv): mat_pcfg(pc) for pv, pc in cfg["platforms"].items()}
            setattr(sd, k, AttrDouble(cfg))
        elif k == "_profile_configs":
            setattr(sd, k, {prof: mat_profile_cfg(pc) for prof, pc in v.items()} if isinstance(v, dict) else v)
        elif k == "_profile_adapters":
            setattr(sd, k, {prof: {Platform(pv): mat_adapter(av) for pv, av in av_map.items()}
                            for prof, av_map in v.items()} if isinstance(v, dict) else v)
        elif k == "persistence":
            setattr(sd, k, AttrDouble(v) if isinstance(v, dict) else v)
        elif k == "session_store":
            setattr(sd, k, SessionStoreDouble(v) if isinstance(v, dict) else v)
        elif k == "_completion_deliveries_inflight":
            setattr(sd, k, {tuple(t) for t in v} if isinstance(v, list) else v)
        elif k == "_completion_delivery_lock":
            setattr(sd, k, NoopLock())
        elif k == "_gateway_loop":
            setattr(sd, k, LoopDouble(v) if isinstance(v, dict) else v)
        else:
            setattr(sd, k, deepcopy(v))
    # ported siblings run inline (real)
    for nm in METHODS:
        if nm in STATIC_METHODS:
            setattr(sd, nm, REAL[nm])
        else:
            setattr(sd, nm, types.MethodType(REAL[nm], sd))
    setattr(sd, "_CompletionClaim", NS["_CompletionClaim"])
    # class-attribute default (GatewayNotificationsMixin._planned_restart_notice_lock = None)
    if not hasattr(sd, "_planned_restart_notice_lock"):
        sd._planned_restart_notice_lock = None
    # claimed / foreign self-method seams (instance attributes; no self)
    for live, seamname in SELF_SEAMS.items():
        setattr(sd, live, seam(seamname, ARGMAPS[seamname], async_=seamname in ASYNC_SEAMS))
    return sd


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": state_fn()}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": state_fn()}


def drive(func, inp):
    live = func if func == "arm_process_watcher" else "_" + func
    sd = build_self(inp)
    nostate = lambda: None
    g = inp.get

    if func == "send_queued_final_text":
        adapter = mat_adapter(g("adapter"))
        src = g("source")
        source = types.SimpleNamespace(**src) if isinstance(src, dict) else src
        return run_flow(lambda: asyncio.run(getattr(sd, live)(
            adapter, source, g("text_content", ""), g("metadata"),
            g("event_message_id"), g("session_key"), g("inbound_message_id"))), nostate)
    if func == "send_home_channel_message":
        platform = Platform(g("platform", "telegram"))
        home = AttrDouble(g("home") or {})
        transport = TransportDouble(g("transport") or {})
        return run_flow(lambda: asyncio.run(getattr(sd, live)(
            platform, home, transport, g("message", ""), g("failure_fmt", ""))), nostate)
    if func == "replay_pending_planned_restart_notification":
        def state_fn():
            v = sd.__dict__.get("_planned_restart_notice_lock")
            return {"_planned_restart_notice_lock": denorm(v) if v is not None else None}
        return run_flow(lambda: asyncio.run(getattr(sd, live)()), state_fn)
    if func == "send_session_db_warning_notifications":
        return run_flow(lambda: asyncio.run(getattr(sd, live)()), nostate)
    if func == "self_post_api_server":
        adapter = mat_adapter(g("adapter"))
        return run_flow(lambda: asyncio.run(getattr(sd, live)(
            adapter, g("synth_text", ""), g("raw_sid", ""), g("evt") or {})), nostate)
    if func == "committed_relay_adapter":
        src = g("source")
        source = types.SimpleNamespace(**src) if isinstance(src, dict) else src
        return run_flow(lambda: getattr(sd, live)(source), nostate)
    if func == "completion_event_scope":
        return run_flow(lambda: getattr(sd, live)(g("evt") or {}), nostate)
    if func == "served_api_server_event_scope":
        async def body():
            async with getattr(sd, live)(g("evt") or {}, g("raw_sid", "")):
                pass
        return run_flow(lambda: asyncio.run(body()), nostate)
    if func == "deliver_completion_notification_scoped":
        evt = deepcopy(g("evt") or {})
        siblings = g("sibling_claims") or []

        def state_fn():
            if "_completion_deliveries_inflight" in sd.__dict__:
                w = sd.__dict__["_completion_deliveries_inflight"]
                return {"inflight": sorted_targets(w)}
            return None
        return run_flow(lambda: asyncio.run(getattr(sd, live)(
            g("synth_text", ""), evt, sibling_claims=siblings)), state_fn)
    if func == "settle_batch_waiters":
        entries = []
        for item in (g("entries") or []):
            if isinstance(item, list) and len(item) == 3:
                entries.append([item[0], item[1], FutureDouble(item[2])])
            else:
                entries.append(item)   # malformed: the live unpack raises
        result = g("result")
        return run_flow(lambda: REAL[live](entries, result), lambda: {"entries": denorm(entries)})
    if func == "deliver_async_delegation_group":
        return run_flow(lambda: asyncio.run(getattr(sd, live)(g("group") or [])), nostate)
    if func == "deliver_async_delegation_groups":
        groups = g("groups") or {}
        pr = types.SimpleNamespace(completion_queue=QueueDouble("completion_queue"))
        return run_flow(lambda: asyncio.run(getattr(sd, live)(groups, pr)), nostate)
    if func == "arm_process_watcher":
        return run_flow(lambda: getattr(sd, live)(g("watcher") or {}), nostate)
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
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
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        SEQPOS.clear()
        NS["logger"] = RecLogger()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE run_notifications_flow3: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
