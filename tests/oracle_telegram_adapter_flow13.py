#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 13 port
(src/gateway/platforms/telegram_adapter_flow13.c).

Drives the LIVE plugins/platforms/telegram/adapter.py defs with SCRIPTED seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow13: N cases, M mismatches'.
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
    "_drain_polling_connections": "instance",
    "_fail_update_preparation": "instance",
    "_edit_text": "instance",
    "_edit_markdown_or_plain": "instance",
    "_edit_overflow_split": "instance",
    "_edit_result_text": "instance",
    "_edit_md_quiet": "instance",
    "_explicit_bot_mentions_exclude_self": "instance",
    "_ensure_forum_commands": "instance",
    "_enqueue_text_event": "instance",
    "_flush_buffered": "instance",
    "_enqueue_photo_event": "instance",
    "_edit_revision": "static",
    "_text_batch_key": "instance",
    "_mark_connected": "instance",
    "_mark_disconnected": "instance",
}
ORDER = list(KINDS)

# EXACTLY the C ta13_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "req_shutdown", "bounded_request_step", "orphan_and_rebuild_polling_client",
    "req_initialize", "logger_debug",
    "admission_get", "claim_set_failed",
    "normalize_telegram_chat_id", "link_preview_kwargs", "edit_message_text",
    "await_with_thread_deadline",
    "edit_text", "redact", "logger_warning",
    "edit_overflow_split",
    "format_message", "query_edit_message_text",
    "current_bot_username", "foreign_command_target", "leading_bot_handles",
    "foreign_handle_fullmatch", "extract_bot_mention_usernames",
    "schedule_bot_identity_recheck", "chat_id_str", "logger_info",
    "forum_lock_aenter", "menu_max_commands", "to_thread", "bot_commands",
    "set_my_commands", "forum_lock_aexit",
    "note_arrival", "should_drop_delayed_delivery", "hold_inbound_event",
    "text_batch_key", "merged_typed_text", "super_enqueue_text_event",
    "record_typed_text", "accept_update",
    "merge_into_pending", "task_done", "task_cancel", "flush_photo_batch",
    "create_task",
    "current_task", "sleep", "asyncio_wait", "log_fn", "handle_message",
    "edit_timestamp", "sha256_hexdigest",
    "super_text_batch_key", "shared_sender_suffix",
    "super_mark_connected", "schedule_held_inbound_redispatch",
    "super_mark_disconnected",
]

_TEXT_SEND_DEADLINE = 60.0


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


_MOD = new_module("ta13_oracle_adapter")
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
        print(f"FATAL oracle_telegram_adapter_flow13: missing defs: {missing}")
        sys.exit(2)
    return nodes


_NODES = extract()


# ── canonical projection ──
class SeamFn:
    """A callable object (no __get__) so zero-arg super() returns it unbound."""
    def __init__(self, name, args_fn=None):
        self.name = name
        self.args_fn = args_fn or (lambda *a, **k: {})

    def __call__(self, *a, **k):
        return _CUR.call(self.name, self.args_fn(*a, **k))


class _Claim:
    def __init__(self, raw):
        object.__setattr__(self, "_raw", raw)

    def __setattr__(self, k, v):
        _CUR.call("claim_set_failed", {"claim": self._raw, "name": k, "value": v})


class _Task:
    def __init__(self, raw):
        self._raw = raw

    def done(self):
        return _CUR.call("task_done", {"task": self})

    def cancel(self):
        return _CUR.call("task_cancel", {"task": self})

    def __await__(self):
        async def _a():
            return self
        return _a().__await__()


_TASK_REG = {}


def _task_for(raw):
    k = json.dumps(raw, sort_keys=True, ensure_ascii=False)
    if k not in _TASK_REG:
        _TASK_REG[k] = _Task(raw)
    return _TASK_REG[k]


class _Lock:
    def __init__(self, st):
        self._st = st

    async def __aenter__(self):
        return _CUR.call("forum_lock_aenter", {})

    async def __aexit__(self, *a):
        _CUR.call("forum_lock_aexit", {})
        return False


class _Regex:
    def __init__(self, st):
        self._st = st

    def fullmatch(self, handle):
        return bool(_CUR.call("foreign_handle_fullmatch", {"handle": handle}))


def canon(v):
    def conv(x):
        tn = type(x).__name__
        if tn in ("TADouble", "AdapterDouble"):
            return "<self>"
        if tn == "_Claim":
            return conv(x._raw)
        if tn == "_Task":
            return conv(x._raw)
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items() if not callable(val)}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
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
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ta13_call exactly) ──
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


# ── seam stubs ──
def build_seams():
    st = {}
    st["req_shutdown"] = rec("req_shutdown", lambda: {})
    st["req_initialize"] = rec("req_initialize", lambda: {})
    st["bounded_request_step"] = arec(
        "bounded_request_step",
        lambda self_, awaitable, message: {"awaitable": awaitable, "message": message})
    st["orphan_and_rebuild_polling_client"] = rec(
        "orphan_and_rebuild_polling_client", lambda self_, req: {"req": req})
    st["logger_debug"] = rec("logger_debug", lambda *a: {"args": list(a)})

    st["admission_get"] = rec(
        "admission_get", lambda: {},
        build=lambda raw: _Claim(raw) if not isinstance(raw, type(None)) else raw)
    st["claim_set_failed"] = rec(
        "claim_set_failed",
        lambda claim, name, value: {"claim": claim, "name": name, "value": value})

    st["normalize_telegram_chat_id"] = rec(
        "normalize_telegram_chat_id", lambda chat_id: {"chat_id": chat_id})
    st["link_preview_kwargs"] = rec("link_preview_kwargs", lambda self_: {})
    st["edit_message_text"] = rec("edit_message_text", lambda **kw: dict(kw))
    st["await_with_thread_deadline"] = arec(
        "await_with_thread_deadline",
        lambda awaitable, *, timeout, label, dump_on_blocked_loop: {
            "awaitable": awaitable, "timeout": timeout, "label": label,
            "dump_on_blocked_loop": dump_on_blocked_loop})

    def _edit_text_args(self_, chat_id, message_id, text, parse_mode=None):
        d = {"chat_id": chat_id, "message_id": message_id, "text": text}
        if parse_mode is not None:
            d["parse_mode"] = parse_mode
        return d
    st["edit_text"] = arec("edit_text", _edit_text_args)
    st["redact"] = rec("redact", lambda text: {"text": text})
    st["logger_warning"] = rec("logger_warning", lambda *a: {"args": list(a)})

    st["edit_overflow_split"] = arec(
        "edit_overflow_split",
        lambda self_, chat_id, message_id, content, *, finalize, metadata: {
            "self": "<self>", "chat_id": chat_id, "message_id": message_id,
            "content": content, "finalize": finalize, "metadata": metadata})

    st["format_message"] = rec("format_message", lambda self_, content: {"content": content})
    st["query_edit_message_text"] = arec(
        "query_edit_message_text",
        lambda query, *, text, parse_mode, reply_markup: {
            "query": query, "text": text, "parse_mode": parse_mode,
            "reply_markup": reply_markup})

    st["current_bot_username"] = rec("current_bot_username", lambda self_: {})
    st["foreign_command_target"] = rec(
        "foreign_command_target",
        lambda message, bot_username: {"message": message, "bot_username": bot_username})

    def _leading(message, pred):
        raw = _CUR.call("leading_bot_handles", {"message": message})
        return [h for h in (raw or []) if pred(h)]
    st["leading_bot_handles"] = _leading
    st["foreign_handle_fullmatch"] = rec(
        "foreign_handle_fullmatch", lambda handle: {"handle": handle})
    st["extract_bot_mention_usernames"] = rec(
        "extract_bot_mention_usernames",
        lambda self_, message, bot_username: {"message": message, "bot_username": bot_username})
    st["schedule_bot_identity_recheck"] = rec(
        "schedule_bot_identity_recheck", lambda self_: {})
    st["chat_id_str"] = rec("chat_id_str", lambda self_, message: {"message": message})
    st["logger_info"] = rec("logger_info", lambda *a: {"args": list(a)})

    st["forum_lock_aenter"] = arec("forum_lock_aenter", lambda: {})
    st["forum_lock_aexit"] = arec("forum_lock_aexit", lambda: {})
    st["menu_max_commands"] = rec("menu_max_commands", lambda: {})
    st["to_thread"] = arec(
        "to_thread",
        lambda func, *, max_commands: {
            "func": "telegram_menu_commands", "max_commands": max_commands})
    st["bot_commands"] = rec(
        "bot_commands", lambda self_, menu_commands: {"menu_commands": menu_commands})
    st["set_my_commands"] = arec(
        "set_my_commands",
        lambda commands, *, scope: {"commands": commands, "scope": scope})

    st["note_arrival"] = rec("note_arrival", lambda self_, event: {"event": event})
    st["should_drop_delayed_delivery"] = rec("should_drop_delayed_delivery", lambda self_: {})
    st["hold_inbound_event"] = rec(
        "hold_inbound_event", lambda self_, event, *, where: {"event": event, "where": where})
    st["text_batch_key"] = rec("text_batch_key", lambda self_, event: {"event": event})
    st["merged_typed_text"] = rec(
        "merged_typed_text", lambda existing, event: {"existing": existing, "event": event})
    st["super_enqueue_text_event"] = SeamFn(
        "super_enqueue_text_event", lambda event: {"event": event})
    st["record_typed_text"] = rec(
        "record_typed_text", lambda existing, text: {"existing": existing, "text": text})
    st["accept_update"] = rec("accept_update", lambda self_: {})

    st["merge_into_pending"] = rec(
        "merge_into_pending",
        lambda self_, pending, batch_key, event: {
            "pending": pending, "batch_key": batch_key, "event": event})
    st["task_done"] = rec("task_done", lambda task: {"task": task})
    st["task_cancel"] = rec("task_cancel", lambda task: {"task": task})
    st["flush_photo_batch"] = rec(
        "flush_photo_batch", lambda self_, batch_key: {"batch_key": batch_key})
    st["create_task"] = rec("create_task", lambda coro: {"coro": coro})

    st["current_task"] = rec("current_task", lambda: {}, build=lambda raw: _task_for(raw))
    st["sleep"] = arec("sleep", lambda delay: {"delay": delay})
    st["asyncio_wait"] = arec("asyncio_wait", lambda tasks: {"tasks": tasks})
    st["log_fn"] = rec("log_fn", lambda event: {"event": event})
    st["handle_message"] = arec("handle_message", lambda self_, event: {"event": event})

    st["edit_timestamp"] = rec("edit_timestamp", lambda: {})
    st["sha256_hexdigest"] = rec("sha256_hexdigest", lambda text: {"text": text})

    st["super_text_batch_key"] = SeamFn(
        "super_text_batch_key", lambda event: {"event": event})
    st["shared_sender_suffix"] = rec(
        "shared_sender_suffix", lambda event: {"event": event})
    st["super_mark_connected"] = SeamFn("super_mark_connected", lambda: {})
    st["schedule_held_inbound_redispatch"] = rec(
        "schedule_held_inbound_redispatch", lambda self_: {})
    st["super_mark_disconnected"] = SeamFn("super_mark_disconnected", lambda: {})
    return st


# ── synthetic class hosting the ported methods (super() resolves to BaseDouble) ──
class BaseDouble:
    pass


BaseDouble._text_batch_key = SeamFn(
    "super_text_batch_key", lambda event: {"event": event})
BaseDouble._mark_connected = SeamFn("super_mark_connected", lambda: {})
BaseDouble._mark_disconnected = SeamFn("super_mark_disconnected", lambda: {})
BaseDouble._enqueue_text_event = SeamFn(
    "super_enqueue_text_event", lambda event: {"event": event})
NS["BaseDouble"] = BaseDouble

_syn = ast.ClassDef(
    name="TADouble", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
    keywords=[], body=[_NODES[n] for n in ORDER], decorator_list=[])
ast.fix_missing_locations(_syn)
_mod_node = ast.Module(body=[_syn], type_ignores=[])
ast.fix_missing_locations(_mod_node)
exec(compile(_mod_node, A_SRC, "exec", flags=_FLAGS, dont_inherit=True), NS)
TADouble = NS["TADouble"]
PORTED = {n: TADouble.__dict__[n] for n in ORDER}


# ── seeding ──
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def seed(st):
    NS["logger"] = SimpleNamespace(
        info=st["logger_info"], debug=st["logger_debug"],
        warning=st["logger_warning"], error=(lambda *a, **k: None))
    NS["ParseMode"] = SimpleNamespace(MARKDOWN_V2="MARKDOWN_V2")
    NS["_TEXT_SEND_DEADLINE"] = _TEXT_SEND_DEADLINE
    NS["_redact_telegram_error_text"] = st["redact"]
    NS["normalize_telegram_chat_id"] = st["normalize_telegram_chat_id"]
    NS["_await_with_thread_deadline"] = st["await_with_thread_deadline"]
    NS["merged_typed_text"] = st["merged_typed_text"]
    NS["record_typed_text"] = st["record_typed_text"]
    NS["shared_sender_suffix"] = st["shared_sender_suffix"]
    NS["foreign_command_target"] = st["foreign_command_target"]
    NS["leading_bot_handles"] = st["leading_bot_handles"]
    NS["contextlib"] = contextlib
    NS["asyncio"] = SimpleNamespace(
        CancelledError=asyncio.CancelledError,
        sleep=st["sleep"], current_task=st["current_task"],
        to_thread=st["to_thread"], create_task=st["create_task"],
        wait=st["asyncio_wait"])

    class _Hash:
        def __init__(self, data):
            self._data = data

        def hexdigest(self):
            return st["sha256_hexdigest"](self._data.decode("utf-8", "replace"))
    NS["hashlib"] = SimpleNamespace(sha256=lambda data: _Hash(data))

    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.telegram",
                "hermes_cli"):
        m = sys.modules.get(pkg) or types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m
    _mod("plugins.platforms.telegram.telegram_overflow_edit",
         edit_overflow_split=st["edit_overflow_split"])
    _mod("plugins.platforms.telegram.telegram_addressing",
         foreign_command_target=st["foreign_command_target"],
         leading_bot_handles=st["leading_bot_handles"])
    _mod("telegram",
         BotCommandScopeChat=(lambda *, chat_id: SimpleNamespace(chat_id=chat_id)))
    _mod("hermes_cli.commands_platforms",
         telegram_menu_commands=(lambda **kw: None),
         telegram_menu_max_commands=st["menu_max_commands"])


METHOD_STUBS = {
    "_note_arrival": "note_arrival",
    "_should_drop_delayed_delivery": "should_drop_delayed_delivery",
    "_hold_inbound_event": "hold_inbound_event",
    "_merge_into_pending": "merge_into_pending",
    "_accept_update": "accept_update",
    "_extract_bot_mention_usernames": "extract_bot_mention_usernames",
    "_schedule_bot_identity_recheck": "schedule_bot_identity_recheck",
    "_chat_id_str": "chat_id_str",
    "_current_bot_username": "current_bot_username",
    "_bot_commands": "bot_commands",
    "_bounded_request_step": "bounded_request_step",
    "_orphan_and_rebuild_polling_client": "orphan_and_rebuild_polling_client",
    "_edit_text": "edit_text",
    "_text_batch_key": "text_batch_key",
    "_flush_photo_batch": "flush_photo_batch",
    "format_message": "format_message",
    "_schedule_held_inbound_redispatch": "schedule_held_inbound_redispatch",
    "handle_message": "handle_message",
}


def make_self(state, st):
    inst = TADouble()
    for k, v in (state or {}).items():
        setattr(inst, k, copy.deepcopy(v))
    TADouble.name = "bot"
    TADouble._bot = None
    TADouble._app = None
    TADouble._pending_text_batches = {}
    TADouble._pending_photo_batches = {}
    TADouble._pending_photo_batch_tasks = {}
    TADouble.config = SimpleNamespace(extra={})
    TADouble._FOREIGN_BOT_HANDLE_RE = _Regex(st)
    for m, seam in METHOD_STUBS.items():
        setattr(TADouble, m, st[seam])
    return inst


def run_call(fn, *a, **kw):
    r = fn(*a, **kw)
    if asyncio.iscoroutine(r):
        return asyncio.run(r)
    return r


def _query(raw):
    q = ns_double(raw) if isinstance(raw, dict) else SimpleNamespace()
    seam = _CUR.st["query_edit_message_text"]

    async def _edit(*a, **kw):
        return await seam(q, *a, **kw)
    q.edit_message_text = _edit
    return q


def drive(func, inp):
    global _CUR
    args = copy.deepcopy(inp.get("args") or {})
    state = copy.deepcopy(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    st = build_seams()
    ctx.st = st
    seed(st)
    inst = make_self(state, st)
    ctx.inst = inst
    fn = PORTED[func]

    def A(k):
        return ns_double(args.get(k))

    try:
        if func == "_drain_polling_connections":
            ak = args.get("app_kind")
            if ak == "none":
                TADouble._app = None
            elif ak == "no_bot":
                TADouble._app = SimpleNamespace(bot=None)
            else:
                req = SimpleNamespace(**(args.get("req") or {}))
                req.shutdown = st["req_shutdown"]
                req.initialize = st["req_initialize"]
                reqs = [] if args.get("req_kind") == "missing" else [req]
                TADouble._app = SimpleNamespace(bot=SimpleNamespace(_request=reqs))
            ret = run_call(fn, inst)

        elif func == "_fail_update_preparation":
            if getattr(inst, "_update_admission", None) is not None:
                adm = SimpleNamespace()
                adm.get = st["admission_get"]
                inst._update_admission = adm
            ret = fn(inst)

        elif func == "_edit_text":
            TADouble._bot = SimpleNamespace(edit_message_text=st["edit_message_text"])
            TADouble._link_preview_kwargs = st["link_preview_kwargs"]
            ret = run_call(fn, inst, args.get("chat_id"), args.get("message_id"),
                           args.get("text"), args.get("parse_mode"))

        elif func == "_edit_markdown_or_plain":
            ret = run_call(fn, inst, args.get("chat_id"), args.get("message_id"),
                           args.get("formatted"), args.get("plain"), args.get("warn_fmt"))

        elif func == "_edit_overflow_split":
            ret = run_call(fn, inst, args.get("chat_id"), args.get("message_id"),
                           args.get("content"), finalize=args.get("finalize", False),
                           metadata=args.get("metadata"))

        elif func == "_edit_result_text":
            ret = run_call(fn, inst, _query(args.get("query")), args.get("result_text"))

        elif func == "_edit_md_quiet":
            ret = run_call(fn, inst, _query(args.get("query")), args.get("text_md"))

        elif func == "_explicit_bot_mentions_exclude_self":
            TADouble._bot = object() if args.get("bot_present", True) else None
            TADouble._FOREIGN_BOT_HANDLE_RE = _Regex(st)
            ret = fn(inst, A("message"), log_skip=args.get("log_skip", True))

        elif func == "_ensure_forum_commands":
            TADouble._forum_lock = _Lock(st)
            TADouble._bot = SimpleNamespace(set_my_commands=st["set_my_commands"])
            inst._forum_command_registered = set(state.get("_forum_command_registered") or [])
            ret = run_call(fn, inst, A("message"))

        elif func == "_enqueue_text_event":
            batches = getattr(inst, "_pending_text_batches", None)
            if "_pending_text_batches" in state and isinstance(batches, dict):
                inst._pending_text_batches = {
                    k: (SimpleNamespace(**v) if isinstance(v, dict) else v)
                    for k, v in batches.items()}
            ret = fn(inst, A("event"))

        elif func == "_flush_buffered":
            tasks = {k: _task_for(v) for k, v in (args.get("tasks") or {}).items()}
            cm = getattr(inst, "_committed_flush_tasks", None)
            if isinstance(cm, dict):
                inst._committed_flush_tasks = {
                    tuple(k.split("|", 1)): _task_for(v) for k, v in cm.items()}
            lf = st["log_fn"] if args.get("log_fn") else None
            ret = run_call(fn, inst, copy.deepcopy(args.get("pending") or {}), tasks,
                           args.get("key"), args.get("delay", 0), args.get("where", "w"),
                           log_fn=lf, pause=None)

        elif func == "_enqueue_photo_event":
            if "_pending_photo_batch_tasks" not in state:
                inst._pending_photo_batch_tasks = {}
            ptasks = getattr(inst, "_pending_photo_batch_tasks", None)
            if "_pending_photo_batch_tasks" in state and isinstance(ptasks, dict):
                inst._pending_photo_batch_tasks = {
                    k: _task_for(v) for k, v in ptasks.items()}
            ret = fn(inst, args.get("batch_key"), A("event"))

        elif func == "_edit_revision":
            ed = args.get("edit_date")
            msg = SimpleNamespace()
            if isinstance(ed, dict) and "__ts__" in ed:
                ts = SimpleNamespace()
                ts.timestamp = st["edit_timestamp"]
                msg.edit_date = ts
            else:
                msg.edit_date = ed
            msg.text = args.get("text")
            msg.caption = args.get("caption")
            ret = fn(msg, args.get("update_id"))

        elif func == "_text_batch_key":
            ret = fn(inst, A("event"))

        elif func == "_mark_connected":
            ret = fn(inst)

        elif func == "_mark_disconnected":
            ret = fn(inst)

        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)

    echo_state = {k: v for k, v in vars(inst).items() if not callable(v)}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo_state)}


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow13.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta13_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow13: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'(?:rec|arec|SeamFn|AsyncSeamFn)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow13: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(
        r'PoP:\s*ta13_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow13: PoP/KINDS drift: "
              f"c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow13.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow13: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow13: fixture func set != KINDS: "
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
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:800]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:800]}")
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
    print(f"ORACLE telegram_adapter_flow13: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow13.jsonl"
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
            drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST telegram_adapter_flow13: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow13.jsonl"
    out = _REPO / "tests" / "fixtures" / "telegram_adapter_flow13.wants.jsonl"
    n = 0
    with out.open("w", encoding="utf-8", newline="\n") as f:
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            try:
                w = drive(case["func"], case.get("in") or {})
            except BaseException as exc:
                w = {"ret": proj_exc(exc), "trace": [], "state": {}, "_drive_error": True}
            f.write(json.dumps({"func": case["func"], "in": case.get("in"), "out": w},
                               ensure_ascii=False) + "\n")
            n += 1
    print(f"dumped {n} wants to {out}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--dump":
        dump()
        sys.exit(0)
    main()
