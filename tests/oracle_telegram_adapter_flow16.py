#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 16 port
(src/gateway/platforms/telegram_adapter_flow16.c).

Drives the LIVE plugins/platforms/telegram/adapter.py defs with SCRIPTED seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow16: N cases, M mismatches'.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import enum
import json
import math
import os
import re
import sys
import textwrap
import types
from pathlib import Path
from types import SimpleNamespace
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "telegram", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

# kind: module | instance | static
KINDS = {
    "_is_user_authorized_from_message": "instance",
    "_legacy_runner_auth_fn": "instance",
    "_log_blocked_user": "instance",
    "_menu_fingerprint": "static",
    "_merge_into_pending": "instance",
    "_message_mentions_bot": "instance",
    "_notify_clarify_expired": "instance",
    "_observe_unmentioned_group_message": "instance",
    "_on_platform_update": "instance",
    "_orphan_and_rebuild_polling_client": "instance",
    "_paged_keyboard": "instance",
    "_persist_dm_topic_thread_id": "instance",
    "_photo_batch_key": "instance",
    "_picker_edit": "instance",
    "_should_attempt_rich_draft": "instance",
    "_restart_task_attr": "instance",
}
ORDER = list(KINDS)

# EXACTLY the C ta16_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "accept_update", "admission_get", "answer_callback", "append_transcript", "auth_env_configured",
    "auth_fn_call", "background_add", "build_client", "build_message_event", "callback_auth_call",
    "coerce_allow_set", "config_exists", "current_bot_username", "edit_html_quiet", "edit_message_text",
    "ensure_future", "entity_sources", "entity_span", "entity_type", "env_allowlist_decision",
    "event_session_key", "extract_mentions", "fail_update_preparation", "format_message", "get_hermes_home",
    "get_language", "get_or_create_session", "handler_call", "has_hook", "html_escape",
    "is_sender_authorized", "legacy_runner_auth_fn", "logger_debug", "logger_info", "logger_log",
    "logger_warning", "merge_batched_event", "normalize_platform_event", "observe_attributed_text",
    "observe_shared_source", "pass_unauthorized_dm", "picker_nav_row", "read_config", "redact_error",
    "refresh_gates", "resolve_routing", "rich_content_ok", "rows_of_two", "sender_suffix", "sha256_hex",
    "source_for_platform_event_auth", "source_from_message", "task_cancel", "task_done",
    "task_done_callback", "time_time", "toast", "translate", "write_config",
]
SEAM_NAMES = list(dict.fromkeys(SEAM_NAMES))


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


_MOD = new_module("ta16_oracle_adapter")
NS = _MOD.__dict__
NS["__builtins__"] = builtins


def extract():
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    nodes = {}
    for n in cls.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in KINDS:
            nodes[n.name] = n
    missing = [k for k in KINDS if k not in nodes]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow16: missing defs: {missing}")
        sys.exit(2)
    return nodes


_NODES = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        tn = type(x).__name__
        if isinstance(x, _Marker):
            return x.name
        if isinstance(x, _Keyboard):
            return {"rows": conv(x.rows)}
        if isinstance(x, _Path):
            return x._raw
        if isinstance(x, _Bytes):
            return x.s
        if isinstance(x, _HashDouble):
            return "<hash>"
        if isinstance(x, _DoubleBase):
            return type(x).__name__
        if asyncio.iscoroutine(x):
            return "<awaitable>"
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items() if not callable(val)}
        if isinstance(x, dict):
            return {str(k): conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = asyncio.CancelledError if kind == "CancelledError" else getattr(builtins, kind, None)
    if kind == "TelegramError":
        cls = TelegramError
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ta16_call exactly) ──
_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.st = {}
        self.inst = None

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def rec(name, args_fn, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def arec(name, args_fn, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


class _DoubleBase:
    pass


class _Marker(_DoubleBase):
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        return _CUR.call(self.name, {"args": list(a), "kwargs": dict(k)})


class _AuthFnDouble(_DoubleBase):
    """Callable returned by the legacy_runner_auth_fn seam."""
    def __call__(self, source):
        return _CUR.call("auth_fn_call", {"source": source})


class _Bytes(_DoubleBase):
    def __init__(self, s):
        self.s = "" if s is None else (s.decode("utf-8", "replace") if isinstance(s, bytes) else str(s))

    def decode(self, *a, **k):
        return self.s


class _HashDouble(_DoubleBase):
    def __init__(self, payload):
        self.payload = payload

    def hexdigest(self):
        return _CUR.call("sha256_hex", {"payload": self.payload.decode("utf-8", "replace")})


class _Path(_DoubleBase):
    def __init__(self, raw):
        self._raw = "" if raw is None else str(raw)

    def __truediv__(self, part):
        return _Path(self._raw + ("" if self._raw.endswith("/") else "/") + str(part))

    def __str__(self):
        return self._raw


class _Keyboard(_DoubleBase):
    def __init__(self, rows):
        self.rows = rows


class _Admission(_DoubleBase):
    def get(self):
        return _CUR.call("admission_get", {})


class _SessionStore(_DoubleBase):
    def get_or_create_session(self, source):
        raw = _CUR.call("get_or_create_session", {"source": source})
        return ns_double(raw) if raw is not None else None

    def append_to_transcript(self, sid, entry):
        return _CUR.call("append_transcript", {"session_id": sid, "entry": entry})


class _BackgroundTasks(_DoubleBase):
    def add(self, task):
        return _CUR.call("background_add", {"task": task})

    def discard(self, task):
        return None


class _Task(_DoubleBase):
    def done(self):
        return _CUR.call("task_done", {})

    def cancel(self):
        return _CUR.call("task_cancel", {})

    def add_done_callback(self, cb):
        return _CUR.call("task_done_callback", {"cb": cb})


class _Query(_DoubleBase):
    def __init__(self, raw):
        object.__setattr__(self, "_raw", ns_double(raw) if isinstance(raw, dict) else raw)

    def __getattr__(self, k):
        r = object.__getattribute__(self, "_raw")
        if isinstance(r, SimpleNamespace):
            return getattr(r, k)
        return None

    async def edit_message_text(self, **kw):
        args = {"text": kw.get("text"), "parse_mode": kw.get("parse_mode"),
                "reply_markup": kw.get("reply_markup")}
        return _CUR.call("edit_message_text", args)


def _async_noop(*a, **k):
    async def f():
        return None
    return f()


class BaseDouble:
    pass


NS["BaseDouble"] = BaseDouble


def build_seams():
    st = {}
    st["refresh_gates"] = rec("refresh_gates", lambda self_: {})
    st["source_from_message"] = rec("source_from_message", lambda self_, msg: {"message": msg},
                                    build=lambda raw: ns_double(raw) if raw is not None else None)
    st["coerce_allow_set"] = rec("coerce_allow_set", lambda v: {"v": v})
    st["callback_auth_call"] = rec("callback_auth_call",
        lambda uid, **kw: {"user_id": uid, "chat_id": kw.get("chat_id"),
                           "chat_type": kw.get("chat_type"), "thread_id": kw.get("thread_id"),
                           "user_name": kw.get("user_name")})
    st["legacy_runner_auth_fn"] = rec("legacy_runner_auth_fn", lambda self_: {},
        build=lambda raw: _AuthFnDouble() if raw is not None else None)
    st["auth_env_configured"] = rec("auth_env_configured", lambda self_: {})
    st["is_sender_authorized"] = rec("is_sender_authorized",
        lambda self_, uid, *, chat_type, chat_id, is_bot, thread_id: {
            "user_id": uid, "chat_type": chat_type, "chat_id": chat_id,
            "is_bot": is_bot, "thread_id": thread_id})
    st["logger_debug"] = rec("logger_debug", lambda *a, **k: {"args": list(a), "kwargs": dict(k)})
    st["env_allowlist_decision"] = rec("env_allowlist_decision", lambda self_, uid: {"user_id": uid})
    st["pass_unauthorized_dm"] = rec("pass_unauthorized_dm", lambda self_, source: {"source": source})
    st["logger_log"] = rec("logger_log", lambda level, fmt, *a: {"level": level, "args": list(a)})
    st["get_language"] = rec("get_language", lambda: {})
    st["sha256_hex"] = rec("sha256_hex", lambda payload: {"payload": payload})
    st["resolve_routing"] = rec("resolve_routing", lambda self_, event, **kw: {
        "event": event, **({"unbound_again": kw["unbound_again"]} if "unbound_again" in kw else {})})
    st["merge_batched_event"] = rec("merge_batched_event",
        lambda pending, key, event, *, merge_caption, route_key, reroute: {
            "pending": pending, "key": key, "event": event,
            "merge_caption": merge_caption,
            "route_key": _marker(route_key), "reroute": _marker(reroute)})
    st["current_bot_username"] = rec("current_bot_username", lambda self_: {})
    st["entity_sources"] = rec("entity_sources", lambda self_, msg: {"message": msg},
        build=lambda raw: [[ns_double(t) if isinstance(t, (list, tuple)) else t for t in raw] for raw in (raw or [])])
    st["entity_type"] = rec("entity_type", lambda self_, entity: {"entity": entity})
    st["entity_span"] = rec("entity_span", lambda self_, text, entity: {"text": text, "entity": entity})
    st["extract_mentions"] = rec("extract_mentions", lambda self_, msg, uname: {
        "message": msg, "bot_username": uname})
    st["toast"] = rec("toast", lambda key, **kw: {"args": [key], "kwargs": dict(kw)})
    st["answer_callback"] = arec("answer_callback", lambda query, text=None: {"query": query, "text": text})
    st["html_escape"] = rec("html_escape", lambda s: {"text": s})
    st["translate"] = rec("translate", lambda *a, **k: {"args": list(a), "kwargs": dict(k)})
    st["edit_html_quiet"] = arec("edit_html_quiet", lambda self_, query, text: {"query": query, "text": text})
    st["build_message_event"] = rec("build_message_event",
        lambda self_, msg, mtype, *, update_id: {"message": msg, "message_type": mtype, "update_id": update_id},
        build=lambda raw: ns_double(raw) if raw is not None else None)
    st["observe_shared_source"] = rec("observe_shared_source", lambda self_, source: {"source": source})
    st["get_or_create_session"] = rec("get_or_create_session", lambda store, source: {"source": source},
        build=lambda raw: ns_double(raw) if raw is not None else None)
    st["observe_attributed_text"] = rec("observe_attributed_text", lambda self_, event: {"event": event})
    st["accept_update"] = rec("accept_update", lambda self_: {})
    st["append_transcript"] = rec("append_transcript", lambda store, sid, entry: {"session_id": sid, "entry": entry})
    st["logger_info"] = rec("logger_info", lambda *a, **k: {"args": list(a), "kwargs": dict(k)})
    st["fail_update_preparation"] = rec("fail_update_preparation", lambda self_: {})
    st["logger_warning"] = rec("logger_warning", lambda *a, **k: {"args": list(a), "kwargs": dict(k)})
    st["admission_get"] = rec("admission_get", lambda: {})
    st["has_hook"] = rec("has_hook", lambda name: {"name": name})
    st["normalize_platform_event"] = rec("normalize_platform_event", lambda self_, update: {"update": update})
    st["source_for_platform_event_auth"] = rec("source_for_platform_event_auth", lambda self_, update: {"update": update})
    st["handler_call"] = arec("handler_call", lambda event, source: {"event": event, "source": source})

    def _mk_handler():
        async def h(event, source):
            _CUR.call("handler_call", {"event": event, "source": source})
        return h
    st["_handler"] = _mk_handler()
    st["build_client"] = rec("build_client", lambda: {})
    st["ensure_future"] = rec("ensure_future", lambda coro: {})
    st["background_add"] = rec("background_add", lambda tasks, task: {"task": task})
    st["task_done_callback"] = rec("task_done_callback", lambda task, cb: {"cb": _marker(cb)})
    st["task_done"] = rec("task_done", lambda task: {})
    st["task_cancel"] = rec("task_cancel", lambda task: {})
    st["rows_of_two"] = rec("rows_of_two", lambda self_, buttons: {"buttons": buttons})
    st["picker_nav_row"] = rec("picker_nav_row", lambda self_, page, total, prefix: {
        "page": page, "total_pages": total, "nav_prefix": prefix})
    st["get_hermes_home"] = rec("get_hermes_home", lambda: {}, build=lambda raw: _Path(raw))
    st["config_exists"] = rec("config_exists", lambda path: {"path": str(path)})
    st["read_config"] = rec("read_config", lambda path: {"path": str(path)},
        build=lambda raw: copy.deepcopy(raw) if raw is not None else {})
    st["write_config"] = rec("write_config", lambda path, config: {"path": str(path), "config": config})
    st["event_session_key"] = rec("event_session_key", lambda self_, event: {"event": event})
    st["sender_suffix"] = rec("sender_suffix", lambda event: {"event": event})
    st["format_message"] = rec("format_message", lambda self_, text: {"text": text})
    st["redact_error"] = rec("redact_error", lambda err: {"error": err})
    st["rich_content_ok"] = rec("rich_content_ok", lambda self_, content: {"content": content})
    return st


def _marker(x):
    return "<lambda>" if callable(x) else x


def _qet(**kw):
    return _CUR.call("edit_message_text", {"text": kw.get("text"), "parse_mode": kw.get("parse_mode"),
                                           "reply_markup": kw.get("reply_markup")})


QET = _qet


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


# Names referenced in extracted method DEFAULT ARGUMENTS must exist at class-exec time.
NS["logging"] = SimpleNamespace(WARNING=30, INFO=20, DEBUG=10, ERROR=40)
NS["MessageType"] = SimpleNamespace(TEXT="TEXT")

# synthesized class with all extracted methods
_syn = ast.ClassDef(
    name="TADouble", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
    keywords=[], body=[_NODES[n] for n in ORDER], decorator_list=[])
ast.fix_missing_locations(_syn)
_mod_node = ast.Module(body=[_syn], type_ignores=[])
ast.fix_missing_locations(_mod_node)
exec(compile(_mod_node, A_SRC, "exec", flags=_FLAGS, dont_inherit=True), NS)
TADouble = NS["TADouble"]
PORTED = {n: TADouble.__dict__[n] for n in ORDER}
ORIG_METHODS = dict(PORTED)

# exception class used by _picker_edit
TelegramError = type("TelegramError", (Exception,), {})
NS["TelegramError"] = TelegramError


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def seed(st):
    st["edit_message_text"] = QET
    NS["logger"] = SimpleNamespace(
        log=st["logger_log"], info=st["logger_info"], debug=st["logger_debug"],
        warning=st["logger_warning"])
    NS["logging"] = SimpleNamespace(WARNING=30, INFO=20, DEBUG=10, ERROR=40)
    NS["contextlib"] = contextlib
    NS["asyncio"] = SimpleNamespace(
        CancelledError=asyncio.CancelledError, TimeoutError=TimeoutError,
        ensure_future=st["ensure_future"], get_running_loop=lambda: SimpleNamespace(
            time=lambda: _CUR.call("time_time", {})))
    NS["time"] = SimpleNamespace(time=rec("time_time", lambda: {}))
    NS["get_language"] = st["get_language"]
    NS["_coerce_allow_set"] = st["coerce_allow_set"]
    NS["refresh_telegram_gates"] = st["refresh_gates"]
    NS["_telegram_auth_env_configured"] = st["auth_env_configured"]
    NS["_should_pass_unauthorized_dm_for_pairing"] = st["pass_unauthorized_dm"]
    NS["_toast"] = st["toast"]
    NS["answer_callback"] = st["answer_callback"]
    NS["_html"] = SimpleNamespace(escape=st["html_escape"])
    NS["t"] = st["translate"]
    NS["hashlib"] = SimpleNamespace(sha256=lambda b: _HashDouble(b))
    NS["shared_sender_suffix"] = st["sender_suffix"]
    NS["_redact_telegram_error_text"] = st["redact_error"]
    NS["ParseMode"] = SimpleNamespace(MARKDOWN_V2="MARKDOWN_V2")
    NS["Platform"] = SimpleNamespace(TELEGRAM="telegram")
    NS["InlineKeyboardMarkup"] = lambda rows: _Keyboard(rows)
    NS["_consume_abandoned_task"] = _Marker("_consume_abandoned_task")

    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.telegram",
                "hermes_cli", "gateway"):
        m = sys.modules.get(pkg) or types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m
    _mod("hashlib", sha256=lambda b: _HashDouble(b))
    _mod("plugins.platforms.telegram.telegram_batching",
         merge_batched_event=st["merge_batched_event"], resolve_routing=st["resolve_routing"])
    _mod("hermes_cli.lifecycle", has_hook=st["has_hook"])
    _mod("hermes_constants", get_hermes_home=st["get_hermes_home"])
    _mod("hermes_cli.config",
         atomic_config_write=st["write_config"], config_exists=st["config_exists"],
         read_user_config_raw=st["read_config"])
    _mod("telegram", InlineKeyboardMarkup=lambda rows: _Keyboard(rows))
    _mod("telegram.error", TelegramError=TelegramError)


def make_self(state, st, args, func):
    inst = TADouble()
    # Ported-method targets are real (restored by drive); bind seamed siblings.
    binds = {
        "_source_from_message_for_auth": "source_from_message",
        "_telegram_auth_env_configured": "auth_env_configured",
        "_should_pass_unauthorized_dm_for_pairing": "pass_unauthorized_dm",
        "_legacy_runner_auth_fn": "legacy_runner_auth_fn",
        "_is_sender_authorized": "is_sender_authorized",
        "_env_allowlist_decision": "env_allowlist_decision",
        "_current_bot_username": "current_bot_username",
        "_entity_sources": "entity_sources", "_entity_type": "entity_type",
        "_entity_span": "entity_span",
        "_extract_bot_mention_usernames": "extract_mentions",
        "_edit_html_quiet": "edit_html_quiet",
        "_build_message_event": "build_message_event",
        "_telegram_group_observe_shared_source": "observe_shared_source",
        "_telegram_group_observe_attributed_text": "observe_attributed_text",
        "_accept_update": "accept_update",
        "_fail_update_preparation": "fail_update_preparation",
        "_normalize_platform_event": "normalize_platform_event",
        "_source_for_platform_event_auth": "source_for_platform_event_auth",
        "_rows_of_two": "rows_of_two", "_picker_nav_row": "picker_nav_row",
        "_event_session_key": "event_session_key",
        "_rich_content_ok": "rich_content_ok",
        "format_message": "format_message",
    }
    for m, seam in binds.items():
        setattr(TADouble, m, st[seam])
    # Class-level defaults (never in the instance echo).
    defaults = {
        "_bot": None, "_authorization_check": None, "_session_store": None,
        "name": "telegram", "_platform_event_handler": None, "_update_admission": None,
        "_updates_dispatched_total": 0, "_background_tasks": None,
        "_merge_caption": False, "_disable_link_previews": False,
        "_rich_messages_enabled": True, "_rich_drafts_enabled": False,
        "_rich_send_disabled": False, "_rich_draft_disabled": False,
        "config": SimpleNamespace(extra={}),
    }
    for k, v in defaults.items():
        setattr(TADouble, k, v)
    # Double-valued state keys are installed on the CLASS (not the instance), so
    # the instance echo stays the plain fixture state plus real mutations.
    DOUBLE_KEYS = {"_session_store", "_platform_event_handler", "_update_admission",
                   "_background_tasks", "_message_handler"}
    TADouble._session_store = _SessionStore() if (state or {}).get("_session_store") else None
    TADouble._platform_event_handler = staticmethod(st["_handler"]) if (state or {}).get("_platform_event_handler") else None
    TADouble._update_admission = _Admission() if (state or {}).get("_update_admission") else None
    TADouble._background_tasks = _BackgroundTasks()
    TADouble.config = SimpleNamespace(extra=copy.deepcopy((state or {}).get("config_extra", {})))
    TADouble.name = (state or {}).get("name", "telegram")
    TADouble._authorization_check = (state or {}).get("_authorization_check", None)
    TADouble._bot = ns_double((state or {}).get("_bot")) if (state or {}).get("_bot") is not None else None
    if (state or {}).get("_is_callback_user_authorized"):
        inst.__dict__["_is_callback_user_authorized"] = st["callback_auth_call"]
    for k, v in (state or {}).items():
        if k in DOUBLE_KEYS or k == "_bot" or k == "_is_callback_user_authorized":
            continue
        setattr(inst, k, ns_double(v))
    return inst


def run_call(fn, *a, **kw):
    r = fn(*a, **kw)
    if asyncio.iscoroutine(r):
        return asyncio.run(r)
    return r


def drive(func, inp):
    global _CUR
    args = copy.deepcopy(inp.get("args") or {})
    state = copy.deepcopy(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    for n, m in ORIG_METHODS.items():
        setattr(TADouble, n, m)
    st = build_seams()
    ctx.st = st
    seed(st)
    inst = make_self(state, st, args, func)
    ctx.inst = inst
    fn = PORTED[func]
    A = lambda k: ns_double(args.get(k))

    try:
        if func == "_is_user_authorized_from_message":
            ret = fn(inst, A("message"))

        elif func == "_legacy_runner_auth_fn":
            runner_raw = (state or {}).get("_runner_auth")
            runner = SimpleNamespace(_is_user_authorized=_Marker("auth_fn")) if runner_raw else None
            TADouble._message_handler = SimpleNamespace(__self__=runner)
            ret = fn(inst)

        elif func == "_log_blocked_user":
            msg = SimpleNamespace(
                from_user=ns_double(args.get("from_user")),
                chat=ns_double(args.get("chat")))
            ret = fn(inst, msg, level=args.get("level", 30), what=args.get("what", "unauthorized user"))

        elif func == "_menu_fingerprint":
            ret = run_call(fn, copy.deepcopy(args.get("menu_commands") or []))

        elif func == "_merge_into_pending":
            ret = fn(inst, copy.deepcopy(args.get("pending") or {}), args.get("key"),
                     A("event"))

        elif func == "_message_mentions_bot":
            ret = fn(inst, A("message"))

        elif func == "_notify_clarify_expired":
            ret = run_call(fn, inst, _Query(args.get("query")), args.get("user_display", ""))

        elif func == "_observe_unmentioned_group_message":
            ev = A("event")
            ret = fn(inst, A("message"), args.get("msg_type", "TEXT"),
                     update_id=args.get("update_id"), event=ev)

        elif func == "_on_platform_update":
            ret = run_call(fn, inst, A("update"), A("context"))

        elif func == "_orphan_and_rebuild_polling_client":
            pr = SimpleNamespace(
                _client=ns_double((args.get("polling_req") or {}).get("_client")),
                _build_client=_Marker("build_client") if (args.get("polling_req") or {}).get("_build_client") else None)
            ret = fn(inst, pr)

        elif func == "_paged_keyboard":
            ret = fn(inst, copy.deepcopy(args.get("buttons") or []),
                     copy.deepcopy(args.get("page_meta") or {}), args.get("nav_prefix", ""),
                     copy.deepcopy(args.get("tail_row") or []), columns=args.get("columns", 2))

        elif func == "_persist_dm_topic_thread_id":
            ret = fn(inst, args.get("chat_id"), args.get("topic_name"), args.get("thread_id"),
                     replace_existing=bool(args.get("replace_existing", False)))

        elif func == "_photo_batch_key":
            ret = fn(inst, A("event"), A("msg"))

        elif func == "_picker_edit":
            ret = run_call(fn, inst, _Query(args.get("query")), args.get("text_md", ""),
                           args.get("keyboard"), toast=args.get("toast"))

        elif func == "_should_attempt_rich_draft":
            ret = fn(inst, args.get("content", ""))

        elif func == "_restart_task_attr":
            attr = args.get("attr", "_t")
            prior_raw = (state or {}).get(attr)
            if prior_raw:
                setattr(inst, attr, _Task())
            ret = fn(inst, attr, _Marker("coro"))

        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)

    echo_state = {k: v for k, v in vars(inst).items()
                  if not callable(v) and not isinstance(v, _DoubleBase)}
    for k, v in (state or {}).items():
        if k not in echo_state:
            echo_state[k] = v
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo_state)}


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow16.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta16_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow16: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'(?:rec|arec|SeamFn)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow16: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(
        r'PoP:\s*ta16_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow16: PoP/KINDS drift: "
              f"c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow16.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        if fix_keys != set(SEAM_NAMES):
            print(f"FATAL oracle_telegram_adapter_flow16: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow16: fixture func set != KINDS: "
                  f"missing={sorted(set(KINDS) - fix_funcs)} extra={sorted(fix_funcs - set(KINDS))}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 30:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:600]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:600]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:800]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:800]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:4]:
            print(f"    {k}: want={json.dumps(ws.get(k), ensure_ascii=False)[:300]} got={json.dumps(gs.get(k), ensure_ascii=False)[:300]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE telegram_adapter_flow16: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow16.jsonl"
    n = bad = 0
    counts = {}
    for line in fix.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            r = drive(func, case.get("in") or {})
            if isinstance(r.get("ret"), dict) and r["ret"].get("__raise__") == "AttributeError":
                print(f"SELFTEST ATTR RAISE {func} case {n}: {r['ret']}")
                bad += 1
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST telegram_adapter_flow16: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
