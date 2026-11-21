#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 14 port
(src/gateway/platforms/telegram_adapter_flow14.c).

Drives the LIVE plugins/platforms/telegram/adapter.py defs with SCRIPTED seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow14: N cases, M mismatches'.
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
    "_should_drop_delayed_delivery": "instance",
    "_send_retry_is_final": "instance",
    "_observed_media_source": "instance",
    "_flush_media_group_event": "instance",
    "_flush_photo_batch": "instance",
    "_flush_text_batch": "instance",
    "_get_dm_topic_info": "instance",
    "_get_general_request_drain_lock": "instance",
    "_go_fatal_network": "instance",
    "_handle_command": "instance",
    "_handle_choice_picker_callback": "instance",
    "_handle_callback_query": "instance",
    "_handle_clarify_callback": "instance",
    "_handle_exec_approval_callback": "instance",
    "_handle_gmail_triage_callback": "instance",
    "_handle_inline_query": "instance",
}
ORDER = list(KINDS)

# EXACTLY the C ta14_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "flush_media_group_event", "flush_buffered", "text_batches_get", "text_batch_delay_for",
    "reload_dm_topics_from_config", "asyncio_lock", "logger_error", "set_fatal_error",
    "handoff_polling_fatal_error", "effective_update_message", "should_process_message",
    "is_user_authorized_from_message", "log_blocked_user", "ensure_forum_commands",
    "build_triggered_event", "root_lobby_key", "enqueue_text_event", "handle_message",
    "answer_or_note", "toast", "answer_callback", "edit_result_text", "choice_callback",
    "translate", "logger_warning", "logger_info", "logger_debug", "accept_update",
    "callback_ctx", "unauthorized", "callback_authorized", "picker_refusal", "refuse_tap",
    "dispatch_handler", "claim_callback_state", "tap_matches_prompt", "clarify_refusal",
    "refusal_text", "handle_multi_token", "mark_awaiting_text", "notify_clarify_expired",
    "edit_html_quiet", "get_pending", "resolve_clarify_choice", "html_escape",
    "approval_refusal", "resolve_approval", "edit_keeping_prompt", "resume_typing",
    "get_hermes_home", "script_exists", "create_subprocess", "wait_for", "communicate",
    "edit_message_text", "is_callback_user_authorized", "inline_cache_time", "inline_answer",
    "fail_update_preparation", "to_thread_inline", "make_article", "make_content",
]
SEAM_NAMES = list(dict.fromkeys(SEAM_NAMES))

_TEXT_SEND_DEADLINE = 60.0
_TOAST_LIMIT = 200


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


_MOD = new_module("ta14_oracle_adapter")
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
        print(f"FATAL oracle_telegram_adapter_flow14: missing defs: {missing}")
        sys.exit(2)
    return nodes


_NODES = extract()


# ── canonical projection ──
class SeamFn:
    """Callable object (no __get__) so zero-arg super() returns it unbound."""
    def __init__(self, name, args_fn=None):
        self.name = name
        self.args_fn = args_fn or (lambda *a, **k: {})

    def __call__(self, *a, **k):
        return _CUR.call(self.name, self.args_fn(*a, **k))


class _BatchMap:
    """A dict-like with a recording .get() (for _flush_text_batch)."""
    def __init__(self, raw):
        self._raw = raw or {}

    def get(self, k, default=None):
        r = _CUR.call("text_batches_get", {"pending": self._raw, "key": k})
        return default if r is None else r


class _Task:
    def __init__(self, raw):
        self._raw = raw

    def __await__(self):
        async def _a():
            return self
        return _a().__await__()


def _task_for(raw):
    return _Task(raw)


class _Lock:
    def __init__(self, st):
        self._st = st


def canon(v):
    def conv(x):
        tn = type(x).__name__
        if tn == "_IntKeyMap":
            return conv(x._raw)
        if isinstance(x, _CallbackMarker):
            return x.marker
        if asyncio.iscoroutine(x):
            return "<awaitable>"
        if isinstance(x, _Bytes):
            return x.s
        if isinstance(x, _CacheConst):
            return "<cache>"
        if tn in ("TADouble", "AdapterDouble"):
            return "<self>"
        if tn == "_Task":
            return conv(x._raw)
        if tn == "_BatchMap":
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


# ── per-case script backend (mirrors the C ta14_call exactly) ──
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


def _marker(x):
    return "<lambda>" if callable(x) else x


# ── seam stubs ──
def build_seams():
    st = {}
    st["flush_media_group_event"] = arec(
        "flush_media_group_event",
        lambda self_, mid: {"self": "<self>", "media_group_id": mid})
    st["flush_buffered"] = arec(
        "flush_buffered",
        lambda self_, pending, tasks, key, delay, where, log_fn=None, pause=None: {
            "pending": pending, "tasks": tasks, "key": key, "delay": delay,
            "where": where, "log_fn": _marker(log_fn), "pause": _marker(pause)})
    st["text_batch_delay_for"] = rec(
        "text_batch_delay_for", lambda self_, existing: {"existing": existing})
    st["reload_dm_topics_from_config"] = rec(
        "reload_dm_topics_from_config", lambda self_: {})
    st["asyncio_lock"] = rec("asyncio_lock", lambda: {})
    st["logger_error"] = rec("logger_error", lambda *a, **k: {"args": list(a)})
    st["set_fatal_error"] = rec(
        "set_fatal_error", lambda self_, kind, message, *, retryable=False: {
            "kind": kind, "message": message, "retryable": retryable})
    st["handoff_polling_fatal_error"] = arec(
        "handoff_polling_fatal_error", lambda self_: {})
    st["effective_update_message"] = rec(
        "effective_update_message", lambda self_, update: {"update": update},
        build=lambda raw: ns_double(raw) if raw is not None else None)
    st["should_process_message"] = rec(
        "should_process_message", lambda self_, msg, *, is_command=False: {
            "msg": msg, "is_command": is_command})
    st["is_user_authorized_from_message"] = rec(
        "is_user_authorized_from_message", lambda self_, msg: {"msg": msg})
    st["log_blocked_user"] = rec("log_blocked_user", lambda self_, msg: {"msg": msg})
    st["ensure_forum_commands"] = arec(
        "ensure_forum_commands", lambda self_, msg: {"msg": msg})
    st["build_triggered_event"] = arec(
        "build_triggered_event",
        lambda self_, msg, update, mtype: {"msg": msg, "update": update, "message_type": mtype},
        build=lambda raw: ns_double(raw) if raw is not None else None)
    st["root_lobby_key"] = rec("root_lobby_key", lambda self_, msg: {"msg": msg})
    st["enqueue_text_event"] = rec("enqueue_text_event", lambda self_, event: {"event": event})
    st["handle_message"] = arec("handle_message", lambda self_, event: {"event": event})

    st["toast"] = rec("toast", lambda key, **kw: {"args": [key], "kwargs": dict(kw)})
    st["answer_callback"] = arec(
        "answer_callback", lambda query, text=None, **kw: {
            "query": query, "text": text, **({} if not kw else {"kwargs": dict(kw)})})
    st["answer_or_note"] = arec(
        "answer_or_note", lambda self_, query, text: {
            "self": "<self>", "query": query, "text": text})
    st["edit_result_text"] = arec(
        "edit_result_text", lambda self_, query, text: {"query": query, "text": text})
    st["choice_callback"] = arec(
        "choice_callback", lambda chat_id, value: {"chat_id": chat_id, "value": value})
    st["translate"] = rec("translate", lambda *a, **kw: {"args": list(a), "kwargs": dict(kw)})
    st["logger_warning"] = rec("logger_warning", lambda *a, **k: {"args": list(a)})
    st["logger_info"] = rec("logger_info", lambda *a, **k: {"args": list(a)})
    st["logger_debug"] = rec("logger_debug", lambda *a, **k: {"args": list(a)})

    st["accept_update"] = rec("accept_update", lambda self_: {})
    st["callback_ctx"] = rec("callback_ctx", lambda self_, query: {"query": query})
    st["unauthorized"] = rec("unauthorized", lambda: {})
    st["callback_authorized"] = arec(
        "callback_authorized",
        lambda self_, query, cb, unauth: {"query": query, "cb": cb, "unauth": unauth})
    st["picker_refusal"] = rec(
        "picker_refusal", lambda callback, user_id, *, extra=None: {
            "callback": callback, "user_id": user_id, "extra": extra or {}})
    st["refuse_tap"] = arec("refuse_tap", lambda query, refusal: {
        "query": query, "refusal": refusal})
    st["dispatch_handler"] = arec(
        "dispatch_handler",
        lambda handler, query, data, third: {
            "which": getattr(handler, "__name__", str(handler)),
            "query": query, "data": data, "third": third})

    st["claim_callback_state"] = arec(
        "claim_callback_state",
        lambda self_, query, cb, state, cid, unauth, resolved, *, pop=False: {
            "query": query, "cb": cb, "state": state, "clarify_id": cid,
            "unauth": unauth, "resolved": resolved, "pop": pop})
    st["tap_matches_prompt"] = arec(
        "tap_matches_prompt",
        lambda query, cid, prompt, toast_text: {
            "query": query, "clarify_id": cid, "prompt": prompt, "toast": toast_text})
    st["clarify_refusal"] = rec(
        "clarify_refusal", lambda cid, user_id, *, extra=None: {
            "clarify_id": cid, "user_id": user_id, "extra": extra or {}})
    st["refusal_text"] = rec("refusal_text", lambda requester: {"requester": requester})
    st["handle_multi_token"] = arec(
        "handle_multi_token",
        lambda self_, query, cid, token, prompt, display: {
            "self": "<self>", "query": query, "clarify_id": cid, "choice_token": token,
            "prompt": prompt, "user_display": display})
    st["mark_awaiting_text"] = rec("mark_awaiting_text", lambda cid: {"clarify_id": cid})
    st["notify_clarify_expired"] = arec(
        "notify_clarify_expired",
        lambda self_, query, display: {"query": query, "user_display": display})
    st["edit_html_quiet"] = arec(
        "edit_html_quiet", lambda self_, query, text: {"query": query, "text": text})
    st["get_pending"] = rec("get_pending", lambda cid: {"clarify_id": cid})
    st["resolve_clarify_choice"] = arec(
        "resolve_clarify_choice",
        lambda self_, query, cid, prompt, resolved, display: {
            "query": query, "clarify_id": cid, "prompt": prompt,
            "resolved_text": resolved, "user_display": display})
    st["html_escape"] = rec("html_escape", lambda text: {"text": text})

    st["approval_refusal"] = rec(
        "approval_refusal",
        lambda session_key, user_id, *, extra=None, request_id=None, choice=None: {
            "session_key": session_key, "user_id": user_id, "extra": extra or {},
            "request_id": request_id, "choice": choice})
    st["resolve_approval"] = rec(
        "resolve_approval",
        lambda session_key, choice, request_id, *, actor_id=None, extra=None: {
            "session_key": session_key, "choice": choice, "request_id": request_id,
            "actor_id": actor_id, "extra": extra or {}})
    st["edit_keeping_prompt"] = arec(
        "edit_keeping_prompt", lambda self_, query, text: {"query": query, "text": text})
    st["resume_typing"] = rec("resume_typing", lambda self_, chat_id: {"chat_id": chat_id})

    st["get_hermes_home"] = rec("get_hermes_home", lambda: {},
                                build=lambda raw: _Path(raw))
    st["script_exists"] = rec("script_exists", lambda path: {"path": path})
    st["create_subprocess"] = arec(
        "create_subprocess",
        lambda *argv, stdin, stdout, stderr, env: {
            "argv": list(argv), "stdin": stdin, "stdout": stdout, "stderr": stderr,
            "env": env},
        build=lambda raw: _Proc(raw))
    st["wait_for"] = arec("wait_for", lambda awaitable, *, timeout: {
        "awaitable": awaitable, "timeout": timeout},
        build=lambda raw: tuple(_Bytes(x) for x in raw) if isinstance(raw, list) else raw)
    st["communicate"] = arec("communicate", lambda proc: {"proc": proc})
    st["edit_message_text"] = arec(
        "edit_message_text", lambda *, text, **kw: {
            "text": text, **({} if not kw else {"kwargs": dict(kw)})})

    st["is_callback_user_authorized"] = rec(
        "is_callback_user_authorized",
        lambda self_, user_id, *, chat_id, chat_type, user_name: {
            "user_id": user_id, "chat_id": chat_id, "chat_type": chat_type,
            "user_name": user_name})
    st["inline_cache_time"] = rec("inline_cache_time", lambda: {})
    st["to_thread_inline"] = arec(
        "to_thread_inline", lambda func, query, *, offset: {"query": query, "offset": offset})
    st["make_content"] = rec("make_content", lambda text: {"message_text": text})
    st["make_article"] = rec(
        "make_article", lambda *, id, title, description, input_message_content: {
            "id": id, "title": title, "description": description,
            "input_message_content": input_message_content})
    st["fail_update_preparation"] = rec("fail_update_preparation", lambda self_: {})
    return st


class _CacheConst:
    pass


class _IntKeyMap:
    """State map whose int/str keys are interchangeable (JSON keys are strings)."""
    def __init__(self, raw):
        self._raw = dict(raw or {})

    def get(self, k, default=None):
        return self._raw.get(str(k), default)

    def pop(self, k, default=None):
        return self._raw.pop(str(k), default)


class _CallbackMarker:
    def __init__(self, marker):
        self.marker = marker

    async def __call__(self, chat_id, value):
        return _CUR.call("choice_callback", {"chat_id": chat_id, "value": value})


class _Bytes:
    def __init__(self, s):
        self.s = "" if s is None else (s.decode("utf-8", "replace") if isinstance(s, bytes) else str(s))

    def decode(self, *a, **k):
        return self.s


CACHE_CONST = _CacheConst()


class _CacheFn:
    """Callable sentinel used where live reads CACHE_TIME_SECONDS as a value."""
    def __call__(self):
        return _CUR.call("inline_cache_time", {})


NS["CACHE_CONST"] = CACHE_CONST


class _Path:
    def __init__(self, raw):
        self._raw = "" if raw is None else str(raw)

    def __truediv__(self, part):
        return _Path(self._raw + ("" if self._raw.endswith("/") else "/") + str(part))

    def exists(self):
        return bool(_CUR.call("script_exists", {"path": self._raw}))

    def __str__(self):
        return self._raw


class _Proc:
    def __init__(self, raw):
        object.__setattr__(self, "_raw", raw if isinstance(raw, dict) else {})

    @property
    def returncode(self):
        return self._raw.get("returncode")

    def communicate(self):
        raw = _CUR.call("communicate", {"proc": self._raw})

        async def _r():
            return raw
        return _r()


# ── synthetic class hosting the ported methods ──
class BaseDouble:
    pass


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
ORIG_METHODS = dict(PORTED)

DISPATCH_NAMES = [
    "_handle_model_picker_callback", "_handle_choice_picker_callback",
    "_handle_gmail_triage_callback", "_handle_exec_approval_callback",
    "_handle_slash_confirm_callback", "_handle_clarify_callback",
    "_handle_update_prompt_callback",
]


def _dispatch_stub(name):
    async def h(self_, query, data, third):
        return _CUR.call("dispatch_handler", {
            "which": name, "query": query, "data": data, "third": third})
    h.__name__ = name
    return h


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
        info=st["logger_info"], debug=st["logger_debug"], warning=st["logger_warning"],
        error=st["logger_error"])
    NS["_TOAST_LIMIT"] = _TOAST_LIMIT
    NS["MessageType"] = SimpleNamespace(COMMAND="COMMAND")
    NS["t"] = st["translate"]
    NS["contextlib"] = contextlib
    NS["asyncio"] = SimpleNamespace(
        CancelledError=asyncio.CancelledError, TimeoutError=TimeoutError,
        Lock=st["asyncio_lock"], to_thread=st["to_thread_inline"],
        create_subprocess_exec=st["create_subprocess"], wait_for=st["wait_for"],
        subprocess=SimpleNamespace(DEVNULL="DEVNULL", PIPE="PIPE"))
    NS["_unauthorized"] = st["unauthorized"]
    NS["_toast"] = st["toast"]
    NS["answer_callback"] = st["answer_callback"]
    NS["answer_or_note"] = st["answer_or_note"]
    NS["refuse_tap"] = st["refuse_tap"]
    NS["picker_refusal"] = st["picker_refusal"]
    NS["tap_matches_prompt"] = st["tap_matches_prompt"]
    NS["handle_multi_token"] = st["handle_multi_token"]
    NS["resolve_clarify_choice"] = st["resolve_clarify_choice"]
    NS["edit_keeping_prompt"] = st["edit_keeping_prompt"]
    NS["flush_media_group_event"] = st["flush_media_group_event"]
    NS["_html"] = SimpleNamespace(escape=st["html_escape"])

    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.telegram",
                "hermes_cli", "gateway", "tools", "tools.environments"):
        m = sys.modules.get(pkg) or types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m
    _mod("gateway.approval_actor",
         clarify_refusal=st["clarify_refusal"], refusal_text=st["refusal_text"],
         approval_refusal=st["approval_refusal"])
    _mod("tools.clarify_gateway", mark_awaiting_text=st["mark_awaiting_text"],
         get_pending=st["get_pending"])
    _mod("tools.approval", resolve_gateway_approval_card=st["resolve_approval"])
    _mod("tools.environments.local", build_subprocess_env=lambda **kw: "<env>")
    _mod("hermes_constants", get_hermes_home=st["get_hermes_home"])
    _mod("plugins.platforms.telegram.inline_picker",
         CACHE_TIME_SECONDS=CACHE_CONST, build_inline_results="build_inline_results")
    _mod("telegram",
         InlineQueryResultArticle=lambda **kw: st["make_article"](**kw),
         InputTextMessageContent=lambda text: st["make_content"](text))


GT_VERB_DISPATCH = {
    "send": ("send-draft.sh", [], "done_send", False),
    "archive": ("archive.sh", [], "done_archive", False),
    "draft": ("draft-blank.sh", [], "done_draft", False),
    "spam": ("spam.sh", [], "done_spam", False),
    "mute": ("mute-add.sh", ["email"], "done_mute", True),
    "mute-domain": ("mute-add.sh", ["domain"], "done_mute_domain", True),
    "trust": ("trusted-ops-add.sh", ["email"], "done_trust_domain", True),
    "vip": ("vip-add.sh", ["email"], "done_vip", True),
}


def make_self(state, st):
    inst = TADouble()
    for k, v in (state or {}).items():
        setattr(inst, k, _IntKeyMap(v) if k in ("_approval_state",) else copy.deepcopy(v))
    TADouble.name = "bot"
    TADouble._FOREIGN_BOT_HANDLE_RE = None
    TADouble.config = SimpleNamespace(extra={})
    TADouble._SPLIT_THRESHOLD = 4000
    TADouble._GT_VERB_DISPATCH = dict(GT_VERB_DISPATCH)
    for attr in ("_pending_photo_batches", "_pending_photo_batch_tasks",
                 "_pending_text_batches", "_pending_text_batch_tasks",
                 "_choice_picker_state", "_clarify_state", "_clarify_multi",
                 "_approval_state", "_approval_request_ids", "_model_picker_state"):
        if not hasattr(TADouble, attr):
            setattr(TADouble, attr, {})
    for attr in ("_dm_topics",):
        if not hasattr(TADouble, attr):
            setattr(TADouble, attr, {})
    if not hasattr(TADouble, "_dm_topics_config"):
        TADouble._dm_topics_config = []
    if not hasattr(TADouble, "_media_batch_delay_seconds"):
        TADouble._media_batch_delay_seconds = 0.0
    # bind method-style seams
    for m, seam in {
        "_accept_update": "accept_update", "_callback_ctx": "callback_ctx",
        "_callback_authorized": "callback_authorized", "_claim_callback_state": "claim_callback_state",
        "_notify_clarify_expired": "notify_clarify_expired", "_edit_html_quiet": "edit_html_quiet",
        "_edit_result_text": "edit_result_text", "_reload_dm_topics_from_config": "reload_dm_topics_from_config",
        "resume_typing_for_chat": "resume_typing", "_set_fatal_error": "set_fatal_error",
        "_handoff_polling_fatal_error": "handoff_polling_fatal_error",
        "_effective_update_message": "effective_update_message",
        "_should_process_message": "should_process_message",
        "_is_user_authorized_from_message": "is_user_authorized_from_message",
        "_log_blocked_user": "log_blocked_user", "_ensure_forum_commands": "ensure_forum_commands",
        "_build_triggered_event": "build_triggered_event", "_root_lobby_key": "root_lobby_key",
        "_enqueue_text_event": "enqueue_text_event", "_is_callback_user_authorized": "is_callback_user_authorized",
        "_fail_update_preparation": "fail_update_preparation",
        "_flush_buffered": "flush_buffered", "_text_batch_delay_for": "text_batch_delay_for",
        "handle_message": "handle_message",
    }.items():
        setattr(TADouble, m, st[seam])
    return inst


def run_call(fn, *a, **kw):
    r = fn(*a, **kw)
    if asyncio.iscoroutine(r):
        return asyncio.run(r)
    return r


def _int(s):
    try:
        return int(s)
    except (ValueError, TypeError):
        return None


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
    inst = make_self(state, st)
    ctx.inst = inst
    fn = PORTED[func]

    def A(k):
        return ns_double(args.get(k))

    try:
        if func == "_should_drop_delayed_delivery":
            ret = fn(inst)

        elif func == "_send_retry_is_final":
            res = dict(args.get("result") or {})
            res.setdefault("raw_response", None)
            ret = fn(inst, SimpleNamespace(raw_response=res["raw_response"]))

        elif func == "_observed_media_source":
            d = {"photo": None, "video": None, "video_note": None,
                 "voice": None, "audio": None, "document": None}
            d.update(args.get("msg") or {})
            ret = fn(inst, ns_double(d))

        elif func == "_flush_media_group_event":
            ret = run_call(fn, inst, args.get("media_group_id"))

        elif func == "_flush_photo_batch":
            ret = run_call(fn, inst, args.get("batch_key"))

        elif func == "_flush_text_batch":
            inst._pending_text_batches = _BatchMap(state.get("_pending_text_batches"))
            ret = run_call(fn, inst, args.get("key"))

        elif func == "_get_dm_topic_info":
            ret = fn(inst, args.get("chat_id"), args.get("thread_id"))

        elif func == "_get_general_request_drain_lock":
            ret = fn(inst)

        elif func == "_go_fatal_network":
            ret = run_call(fn, inst, args.get("message"), args.get("log_message"),
                           *(args.get("log_args") or []))

        elif func == "_handle_command":
            TADouble._SPLIT_THRESHOLD = args.get("split_threshold", 4000)
            ret = run_call(fn, inst, ns_double(args.get("update")), ns_double(args.get("context")))

        elif func == "_handle_choice_picker_callback":
            cps = getattr(inst, "_choice_picker_state", None)
            if isinstance(cps, dict):
                for v in cps.values():
                    if isinstance(v, dict) and "on_choice_selected" in v:
                        v["on_choice_selected"] = _CallbackMarker(v["on_choice_selected"])
            ret = run_call(fn, inst, ns_double(args.get("query")), args.get("data"),
                           args.get("chat_id"))

        elif func == "_handle_callback_query":
            for hname in DISPATCH_NAMES:
                setattr(TADouble, hname, _dispatch_stub(hname))
            up = dict(args.get("update") or {})
            q = dict(up.get("callback_query") or {}) if up.get("callback_query") is not None else None
            if q is not None:
                q.setdefault("from_user", {"id": 7})
                q.setdefault("message", {"chat_id": 9, "message_id": 42})
            up["callback_query"] = q
            ret = run_call(fn, inst, ns_double(up), ns_double(args.get("context")))

        elif func == "_handle_clarify_callback":
            ret = run_call(fn, inst, ns_double(args.get("query")), args.get("data"),
                           ns_double(args.get("cb")))

        elif func == "_handle_exec_approval_callback":
            ret = run_call(fn, inst, ns_double(args.get("query")), args.get("data"),
                           args.get("cb"))

        elif func == "_handle_gmail_triage_callback":
            q = ns_double(args.get("query"))
            q.edit_message_text = arec(
                "edit_message_text", lambda *, text, **kw: {
                    "text": text, **({} if not kw else {"kwargs": dict(kw)})})
            ret = run_call(fn, inst, q, args.get("data"), ns_double(args.get("cb")))

        elif func == "_handle_inline_query":
            upd = ns_double(args.get("update"))
            iq = getattr(upd, "inline_query", None)
            if iq is not None:
                def _answer(results, **kw):
                    ct = kw.get("cache_time")
                    if isinstance(ct, _CacheConst):
                        ct = _CUR.call("inline_cache_time", {})
                        kw = dict(kw); kw["cache_time"] = ct
                    a = {"query": args.get("update", {}).get("inline_query"),
                         "results": results, "cache_time": kw.get("cache_time"),
                         "is_personal": kw.get("is_personal")}
                    if "next_offset" in kw:
                        a["next_offset"] = kw["next_offset"]
                    return arec("inline_answer", lambda *x, **y: a)(results, **kw)
                iq.answer = _answer
            ret = run_call(fn, inst, upd, ns_double(args.get("context")))

        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)

    echo_state = {k: v for k, v in vars(inst).items() if not callable(v)}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo_state)}


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow14.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta14_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow14: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'(?:rec|arec|SeamFn)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow14: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(
        r'PoP:\s*ta14_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow14: PoP/KINDS drift: "
              f"c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow14.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow14: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow14: fixture func set != KINDS: "
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
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:900]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:900]}")
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
    print(f"ORACLE telegram_adapter_flow14: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow14.jsonl"
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
    print(f"SELFTEST telegram_adapter_flow14: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow14.jsonl"
    out = _REPO / "tests" / "fixtures" / "telegram_adapter_flow14.wants.jsonl"
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
