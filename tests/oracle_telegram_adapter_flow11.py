#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 11 port
(src/gateway/platforms/telegram_adapter_flow11.c).

Drives the LIVE plugins/platforms/telegram/adapter.py defs with SCRIPTED seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow11: N cases, M mismatches'.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import enum
import json
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
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "telegram", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

# kind: module | instance | static
KINDS = {
    "_cache_replied_media": "instance",
    "_cache_dm_topic_from_message": "instance",
    "_callback_authorized": "instance",
    "_chat_outbound_slot_remaining": "instance",
    "_cancel_task_attr": "instance",
    "_cache_observed_media": "instance",
    "_apply_yaml_config": "module",
    "_caption_1024": "static",
    "_entity_type": "static",
    "_ext_from_path": "static",
    "_await_cold_start_readiness": "instance",
    "_build_message_event": "instance",
    "_build_ptb_requests": "instance",
    "_cache_inbound_av": "instance",
    "_cache_inbound_document": "instance",
    "_cancel_pending_delivery_tasks": "instance",
}

# EXACTLY the C ta11_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "cache_replied_media", "logger_info", "_is_callback_user_authorized",
    "answer_callback", "normalize_telegram_chat_id", "loop_time",
    "task_done", "task_cancel", "_await_disconnect_step",
    "_download_observed_media", "append_observed_note", "observed_media_note",
    "_attach_cached", "yaml_env_setter", "yaml_env_set",
    "ensure_future", "wait", "_await_with_thread_deadline", "fut_done",
    "fut_cancel", "gather", "progress_is_set", "redact",
    "gateway_chat_type", "effective_message_thread_id", "adopt_created_topic_thread",
    "resolve_topic_binding", "build_source", "reply_context", "resolve_channel_prompt",
    "group_identity_prompt", "group_reply_expected", "expand_link_entities",
    "edit_revision", "MessageEvent",
    "getenv", "platform_ssl_context", "telegram_verify", "env_float_clamped",
    "logger_warning", "discover_fallback_ips", "proxy_target_hosts",
    "resolve_proxy_url", "build_ptb_requests", "instrument_polling_request",
    "telegram_media_size_allowed", "note_instead_of_media", "surface_media_cache_failure",
    "is_permanent_fatal", "salvage_buffered_inbound", "collect_live_tasks",
    "isfuture", "current_task", "clear_task_attrs_except",
]

_INITIAL_POLLING_PROGRESS_TIMEOUT = 30.0
_DISCONNECT_STEP_TIMEOUT = 5.0


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def extract():
    mod = new_module("ta11_oracle_adapter")
    ns = mod.__dict__
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow11: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()


# ── canonical projection ──
class _Task:
    """Task/future double with cancel()/done()/await; projects to its raw JSON."""
    def __init__(self, raw, prefix="task"):
        self._raw = raw
        self._prefix = prefix

    def cancel(self):
        return _CUR.st[self._prefix + "_cancel"](self)

    def done(self):
        return _CUR.st[self._prefix + "_done"](self)

    def __await__(self):
        return _CUR.st[self._prefix + "_await"](self).__await__()


class _Event:
    def __init__(self, raw, kind):
        self._raw = raw
        self._kind = kind

    def wait(self):
        return self._kind + ".wait()"

    def is_set(self):
        return _CUR.call("progress_is_set", {})


def canon(v):
    def conv(x):
        if type(x).__name__ == "AdapterDouble":
            return "<self>"
        if isinstance(x, _Task):
            return conv(x._raw)
        if isinstance(x, _Event):
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


# ── per-case script backend (mirrors the C ta11_call exactly) ──
_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.st = {}
        self.inst = None
        self.event = None

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

    def _task(raw):
        t = _Task(raw, "task")
        return t

    st["cache_replied_media"] = arec("cache_replied_media",
                                     lambda self_, msg, event: {"msg": msg, "event": event})
    st["logger_info"] = rec("logger_info", lambda *a: {"args": list(a)})
    st["_is_callback_user_authorized"] = rec(
        "_is_callback_user_authorized",
        lambda self_, uid, *, chat_id, chat_type, thread_id, user_name: {
            "user_id": uid, "chat_id": chat_id, "chat_type": chat_type,
            "thread_id": thread_id, "user_name": user_name})
    st["answer_callback"] = arec("answer_callback",
                                 lambda q, text: {"query": q, "text": text})
    st["normalize_telegram_chat_id"] = rec("normalize_telegram_chat_id",
                                           lambda chat_id: {"chat_id": chat_id})
    st["loop_time"] = rec("loop_time", lambda: {})
    st["task_done"] = rec("task_done", lambda task: {})
    st["task_cancel"] = rec("task_cancel", lambda task: {})
    st["_await_disconnect_step"] = arec(
        "_await_disconnect_step",
        lambda self_, task, timeout, step: {"awaitable": task, "timeout": timeout, "step": step})

    def _dl_build(raw):
        if isinstance(raw, (list, tuple)):
            return tuple(raw)
        return raw
    st["_download_observed_media"] = arec(
        "_download_observed_media",
        lambda self_, msg, label: {"msg": msg, "label": label}, build=_dl_build)
    st["append_observed_note"] = rec("append_observed_note",
                                     lambda self_, existing, note: {"existing": existing, "note": note})
    st["observed_media_note"] = rec("observed_media_note", lambda cached: {"cached": cached})
    st["_attach_cached"] = rec(
        "_attach_cached",
        lambda self_, event, cached, note, log_fmt: {
            "event": event, "note": note, "log_fmt": log_fmt})

    # yaml bridge: yaml_env_setter() fires a seam then returns a setter closure.
    def yaml_env_setter():
        _CUR.call("yaml_env_setter", {})
        def setter(env, value):
            _CUR.call("yaml_env_set", {"env": env, "value": value})
        return setter
    st["yaml_env_setter"] = yaml_env_setter
    st["yaml_env_set"] = None  # seam name only; fired directly in the setter

    # cold-start readiness
    st["ensure_future"] = rec("ensure_future", lambda aw: {"awaitable": aw},
                              build=lambda raw: _Task(raw, "fut"))
    st["wait"] = rec("wait", lambda tasks, *, return_when: {
        "return_when": return_when, "n": len(tasks)})
    st["_await_with_thread_deadline"] = arec(
        "_await_with_thread_deadline",
        lambda awaitable, *, timeout: {"awaitable": awaitable, "timeout": timeout})
    st["fut_done"] = rec("fut_done", lambda task: {})
    st["fut_cancel"] = rec("fut_cancel", lambda task: {})
    st["gather"] = arec("gather", lambda *tasks, return_exceptions: {
        "n": len(tasks), "return_exceptions": return_exceptions})
    st["progress_is_set"] = rec("progress_is_set", lambda: {})
    st["redact"] = rec("redact", lambda text: {"text": text})

    # build_message_event
    st["gateway_chat_type"] = rec("gateway_chat_type",
                                  lambda self_, chat, *, is_forum: {"chat": chat, "is_forum": is_forum})
    st["effective_message_thread_id"] = rec("effective_message_thread_id",
                                            lambda self_, message: {"message": message})
    st["adopt_created_topic_thread"] = rec(
        "adopt_created_topic_thread",
        lambda self_, message, chat_type, thread_id: {
            "message": message, "chat_type": chat_type, "thread_id": thread_id})
    st["resolve_topic_binding"] = rec(
        "resolve_topic_binding",
        lambda self_, message, chat_type, thread_id: {
            "message": message, "chat_type": chat_type, "thread_id": thread_id},
        build=lambda raw: tuple(raw) if isinstance(raw, list) else raw)
    st["build_source"] = rec(
        "build_source",
        lambda self_, **kw: dict(kw),
        build=lambda raw: SimpleNamespace(**raw) if isinstance(raw, dict) else raw)
    st["reply_context"] = rec("reply_context", lambda self_, message: {"message": message},
                              build=lambda raw: tuple(raw) if isinstance(raw, list) else raw)
    st["resolve_channel_prompt"] = rec(
        "resolve_channel_prompt",
        lambda extra, key, chat_id: {"extra": extra, "key": key, "chat_id": chat_id})
    st["group_identity_prompt"] = rec(
        "group_identity_prompt",
        lambda self_, message, prompt: {"message": message, "prompt": prompt})
    st["group_reply_expected"] = rec(
        "group_reply_expected",
        lambda self_, message, chat_type: {"message": message, "chat_type": chat_type})
    st["expand_link_entities"] = rec("expand_link_entities", lambda message: {"message": message})
    st["edit_revision"] = rec("edit_revision",
                              lambda self_, message, update_id: {"message": message, "update_id": update_id})
    st["MessageEvent"] = rec("MessageEvent", lambda **kw: dict(kw))

    # build_ptb_requests
    st["getenv"] = rec("getenv", lambda key, default="": {"key": key, "default": default})
    st["platform_ssl_context"] = rec("platform_ssl_context", lambda: {})
    st["telegram_verify"] = rec("telegram_verify", lambda extra, default: {"extra": extra, "default": default})
    st["env_float_clamped"] = rec(
        "env_float_clamped", lambda self_, name, default, *, min_value=None: {
            "name": name, "default": default, "min_value": min_value})
    st["logger_warning"] = rec("logger_warning", lambda *a, **k: {"args": list(a), "kwargs": k})
    st["discover_fallback_ips"] = arec("discover_fallback_ips", lambda *a: {"args": list(a)})
    st["proxy_target_hosts"] = rec("proxy_target_hosts",
                                   lambda base_url, ips: {"base_url": base_url, "fallback_ips": ips})
    st["resolve_proxy_url"] = rec(
        "resolve_proxy_url",
        lambda key, *, target_hosts, configured: {
            "key": key, "target_hosts": target_hosts, "configured": configured})
    st["build_ptb_requests"] = rec(
        "build_ptb_requests",
        lambda httpx, *, fallback_ips, proxy_url, verify: {
            "httpx": httpx, "fallback_ips": fallback_ips, "proxy_url": proxy_url, "verify": verify},
        build=lambda raw: tuple(raw) if isinstance(raw, list) else raw)
    st["instrument_polling_request"] = rec(
        "instrument_polling_request", lambda self_, request: {"request": request})

    # cache_inbound_av / document
    st["telegram_media_size_allowed"] = rec(
        "telegram_media_size_allowed", lambda self_, source, label: {"source": source, "label": label},
        build=lambda raw: tuple(raw) if isinstance(raw, list) else raw)
    st["note_instead_of_media"] = rec(
        "note_instead_of_media", lambda self_, event, text: {"event": event, "text": text})
    st["surface_media_cache_failure"] = arec(
        "surface_media_cache_failure", lambda *a, **k: {"args": list(a), "kwargs": k})

    # cancel_pending_delivery_tasks
    st["collect_live_tasks"] = rec(
        "collect_live_tasks", lambda self_, tasks, current: {"tasks": tasks, "current_task": current},
        build=lambda raw: [_Task(x, "task") for x in raw] if isinstance(raw, list) else raw)
    st["isfuture"] = rec("isfuture", lambda task: {"task": task})
    st["current_task"] = rec("current_task", lambda: {})
    st["is_permanent_fatal"] = rec("is_permanent_fatal", lambda self_: {})
    st["salvage_buffered_inbound"] = rec("salvage_buffered_inbound", lambda self_: {})
    st["clear_task_attrs_except"] = rec(
        "clear_task_attrs_except", lambda self_, current, *keep: {
            "current_task": current, "keep": list(keep)})
    return st


# methods installed on the `self` double (class-level, so not in vars())
METHOD_STUBS = {
    "_is_callback_user_authorized": "_is_callback_user_authorized",
    "_cache_replied_media": "cache_replied_media",
    "_download_observed_media": "_download_observed_media",
    "_append_observed_note": "append_observed_note",
    "_attach_cached": "_attach_cached",
    "_await_disconnect_step": "_await_disconnect_step",
    "_await_with_thread_deadline": "_await_with_thread_deadline",
    "gateway_chat_type": "gateway_chat_type",
    "_gateway_chat_type": "gateway_chat_type",
    "_effective_message_thread_id": "effective_message_thread_id",
    "_adopt_created_topic_thread": "adopt_created_topic_thread",
    "_resolve_topic_binding": "resolve_topic_binding",
    "build_source": "build_source",
    "_reply_context": "reply_context",
    "_edit_revision": "edit_revision",
    "_instrument_polling_request": "instrument_polling_request",
    "_fallback_ips": "noop_list",
    "_env_float_clamped": "env_float_clamped",
    "_telegram_media_size_allowed": "telegram_media_size_allowed",
    "_note_instead_of_media": "note_instead_of_media",
    "_surface_media_cache_failure": "surface_media_cache_failure",
    "_collect_live_tasks": "collect_live_tasks",
    "_is_permanent_fatal": "is_permanent_fatal",
    "_salvage_buffered_inbound": "salvage_buffered_inbound",
    "_clear_task_attrs_except": "clear_task_attrs_except",
    "noop": "noop",
}


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def make_inst(state, st):
    methods = {}
    for m, seam in METHOD_STUBS.items():
        methods[m] = st.get(seam, (lambda *a, **k: None))
    cls = type("AdapterDouble", (), methods)
    obj = cls()
    for k, v in (state or {}).items():
        setattr(obj, k, copy.deepcopy(v))
    return obj


def seed_globals(st, state):
    NS["logger"] = SimpleNamespace(
        info=st["logger_info"], debug=(lambda *a, **k: None),
        warning=st["logger_warning"], error=(lambda *a, **k: None))
    NS["_BOT_COMMAND_DESCRIPTION_LIMIT"] = 256
    NS["_INITIAL_POLLING_PROGRESS_TIMEOUT"] = _INITIAL_POLLING_PROGRESS_TIMEOUT
    NS["_DISCONNECT_STEP_TIMEOUT"] = _DISCONNECT_STEP_TIMEOUT
    NS["_TELEGRAM_IMAGE_MIME_TO_EXT"] = {"image/png": ".png", "image/jpeg": ".jpg"}
    NS["SUPPORTED_DOCUMENT_TYPES"] = {".png": "image/png", ".jpg": "image/jpeg"}
    NS["SUPPORTED_VIDEO_TYPES"] = {".mp4": "video/mp4"}
    NS["SUPPORTED_IMAGE_DOCUMENT_TYPES"] = {".png": "image/png"}
    NS["_TEXT_INJECT_EXTENSIONS"] = {".txt"}
    NS["_TELEGRAM_IMAGE_EXTENSIONS"] = {".png", ".jpg"}
    NS["_TELEGRAM_IMAGE_EXT_TO_MIME"] = {".png": "image/png"}
    NS["SEED_FALLBACK_IPS"] = ["1.1.1.1"]
    NS["HTTPXRequest"] = "HTTPXRequest"
    NS["MessageType"] = SimpleNamespace(PHOTO="PHOTO", VIDEO="VIDEO", AUDIO="AUDIO")
    NS["safe_proxy_url_for_log"] = lambda u: u

    def utf16_len(s):
        return len(s)
    NS["utf16_len"] = utf16_len
    NS["_consume_abandoned_task"] = lambda t: None
    NS["_await_with_thread_deadline"] = st["_await_with_thread_deadline"]
    NS["normalize_telegram_chat_id"] = st["normalize_telegram_chat_id"]
    NS["_redact_telegram_error_text"] = st["redact"]
    NS["answer_callback"] = st["answer_callback"]

    asyncio_ns = SimpleNamespace(
        CancelledError=asyncio.CancelledError, TimeoutError=TimeoutError,
        FIRST_COMPLETED="FIRST_COMPLETED",
        ensure_future=st["ensure_future"], wait=st["wait"], gather=st["gather"],
        isfuture=st["isfuture"], iscoroutine=(lambda t: _CUR.call("isfuture", {"task": t})),
        current_task=st["current_task"],
        get_running_loop=(lambda: SimpleNamespace(time=st["loop_time"])))
    NS["asyncio"] = asyncio_ns


def seed_modules(st, state):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.telegram",
                "hermes_cli", "gateway", "gateway.platforms"):
        m = sys.modules.get(pkg) or types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m

    def yaml_env_setter():
        return st["yaml_env_setter"]()
    mod("gateway.platforms._shared", yaml_env_setter=yaml_env_setter)
    mod("gateway.platforms.base", resolve_channel_prompt=st["resolve_channel_prompt"],
        cache_media_bytes_async=(arec("noop_cache", lambda *a, **k: {})))
    mod("plugins.platforms.telegram.telegram_context",
        group_identity_prompt=st["group_identity_prompt"],
        group_reply_expected=st["group_reply_expected"],
        group_trigger_text=(lambda *a, **k: None))
    mod("hermes_cli.gateway", get_env_value=(rec("getenv", lambda key: {"key": key})))

    def platform_ssl_context():
        return st["platform_ssl_context"]()
    def telegram_verify(extra, default):
        return st["telegram_verify"](extra, default)
    def discover_fallback_ips(*a):
        return st["discover_fallback_ips"](*a)
    def proxy_target_hosts(base_url, ips):
        return st["proxy_target_hosts"](base_url, ips)
    def resolve_proxy_url(key, *, target_hosts=None, configured=None):
        return st["resolve_proxy_url"](key, target_hosts=target_hosts, configured=configured)
    def build_ptb_requests(httpx, **kw):
        return st["build_ptb_requests"](httpx, **kw)
    NS["platform_ssl_context"] = platform_ssl_context
    NS["telegram_verify"] = telegram_verify
    NS["discover_fallback_ips"] = discover_fallback_ips
    NS["proxy_target_hosts"] = proxy_target_hosts
    NS["resolve_proxy_url"] = resolve_proxy_url
    NS["build_ptb_requests"] = build_ptb_requests
    NS["os"] = SimpleNamespace(getenv=st["getenv"], path=SimpleNamespace(
        splitext=lambda p: (p.rsplit(".", 1)[0] if "." in p else p,
                            ("." + p.rsplit(".", 1)[1]) if "." in p else "")))
    NS["re"] = re
    NS["expand_link_entities"] = st["expand_link_entities"]
    NS["cache_replied_media"] = st["cache_replied_media"]
    NS["observed_media_note"] = st["observed_media_note"]
    NS["fetch_telegram_file"] = arec("noop_fetch", lambda *a, **k: {})
    NS["is_image_document"] = lambda doc: False
    NS["caption_is_command"] = lambda text: False
    NS["inline_item_text"] = lambda event, text: None
    NS["MessageEvent"] = st["MessageEvent"]


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
    st = build_seams()
    ctx.st = st
    seed_globals(st, state)
    seed_modules(st, state)
    inst = make_inst(state, st)
    ctx.inst = inst

    def A(k):
        return ns_double(args.get(k))

    try:
        if func == "_cache_replied_media":
            ret = run_call(NS[func], inst, A("msg"), A("event"))
        elif func == "_cache_dm_topic_from_message":
            ret = NS[func](inst, args.get("chat_id"), args.get("thread_id"), args.get("topic_name"))
        elif func == "_callback_authorized":
            q = A("query")
            ret = run_call(NS[func], inst, q, args.get("cb"), args.get("denial_text", ""))
        elif func == "_chat_outbound_slot_remaining":
            ret = NS[func](inst, args.get("chat_id"))
        elif func == "_cancel_task_attr":
            attr = args.get("attr")
            if attr in vars(inst) and vars(inst)[attr] is not None:
                setattr(inst, attr, _Task(vars(inst)[attr], "task"))
            ret = run_call(NS[func], inst, attr, args.get("label", ""))
        elif func == "_cache_observed_media":
            ev = A("event")
            inst.event = ev
            ctx.event = ev
            ret = run_call(NS[func], inst, A("msg"), ev)
        elif func == "_apply_yaml_config":
            ret = NS[func](args.get("yaml_cfg"), args.get("telegram_cfg"))
        elif func == "_caption_1024":
            ret = NS[func](args.get("caption"))
        elif func == "_entity_type":
            ret = NS[func](A("entity"))
        elif func == "_ext_from_path":
            ret = NS[func](args.get("file_path"), args.get("candidates"), args.get("default"))
        elif func == "_await_cold_start_readiness":
            prog = _Event({"kind": "progress"}, "progress")
            err = _Event({"kind": "error"}, "error")
            se = args.get("strict_error") or []
            se = [make_exc(x) if isinstance(x, dict) else x for x in se]
            ret = run_call(NS[func], inst, prog, err, se)
        elif func == "_build_message_event":
            cfg = state.get("config") or {}
            inst.config = SimpleNamespace(extra=cfg.get("extra", {}))
            ret = NS[func](inst, A("message"), args.get("msg_type"), args.get("update_id"))
        elif func == "_build_ptb_requests":
            cfg = state.get("config") or {}
            inst.config = SimpleNamespace(extra=cfg.get("extra", {}))
            ret = run_call(NS[func], inst)
        elif func == "_cache_inbound_av":
            ev = A("event")
            src = A("source")
            ret = run_call(NS[func], inst, A("msg"), ev, src, args.get("label", ""),
                           args.get("kind", ""), args.get("ext", ""), args.get("mime", ""))
        elif func == "_cache_inbound_document":
            ev = A("event")
            inst.event = ev
            ctx.event = ev
            ret = run_call(NS[func], inst, A("msg"), ev)
        elif func == "_cancel_pending_delivery_tasks":
            ret = run_call(NS[func], inst)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow11.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta11_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow11: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'\b(?:rec|arec)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'st\["([^"]+)"\]\s*=\s*(?:a?rec)', osrc))
    # explicit _CUR.call("name" patterns
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow11: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ta11_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow11: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow11.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow11: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow11: fixture func set != KINDS: "
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
    if shown >= 25:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:600]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:600]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:600]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:600]}")
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
    print(f"ORACLE telegram_adapter_flow11: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow11.jsonl"
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
    print(f"SELFTEST telegram_adapter_flow11: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow11.jsonl"
    out = _REPO / "tests" / "fixtures" / "telegram_adapter_flow11.wants.jsonl"
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
