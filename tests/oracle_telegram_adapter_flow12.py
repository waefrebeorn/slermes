#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 12 port
(src/gateway/platforms/telegram_adapter_flow12.c).

Drives the LIVE plugins/platforms/telegram/adapter.py defs with SCRIPTED seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow12: N cases, M mismatches'.
"""
import ast
import asyncio
import builtins
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

# kind: module | instance | static | classmethod
KINDS = {
    "_clear_reactions": "instance",
    "_clear_task_attrs_except": "instance",
    "_consume_abandoned_task": "module",
    "_claim_callback_state": "instance",
    "_check_polling_stall": "instance",
    "_collect_live_tasks": "static",
    "_create_dm_topic": "instance",
    "_delete_webhook_best_effort": "instance",
    "_download_observed_media": "instance",
    "_drain_general_connections_after_pool_timeout": "instance",
    "_disarm_ptb_retry_loop": "instance",
    "_chat_send_lock": "instance",
    "_compress_image_to_jpeg": "instance",
    "_normalize_chat_type": "static",
    "_env_float_clamped": "static",
    "_notification_kwargs": "instance",
    "_is_id_like": "static",
    "_coerce_bool_extra": "instance",
}

# EXACTLY the C ta12_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "_set_reaction", "task_exception", "logger_debug",
    "_callback_authorized", "answer_callback",
    "recovery_in_flight", "monotonic", "schedule_polling_recovery",
    "task_done",
    "create_forum_topic", "logger_info", "logger_warning", "redact",
    "delete_webhook", "await_with_thread_deadline", "looks_like_network_error",
    "observed_media_source", "int_or_zero", "local_root", "fetch_telegram_file",
    "cache_media_bytes_async",
    "get_general_request_drain_lock", "general_req_shutdown", "general_req_initialize",
    "bounded_request_step",
    "event_is_set", "event_set",
    "normalize_telegram_chat_id", "current_task", "new_lock", "lock_aenter",
    "lock_aexit", "lock_waiters",
    "getsize", "sniff_raster", "pil_import", "mkstemp", "pil_work",
    "getenv",
]

_UPDATER_START_TIMEOUT = 30.0
_POLLING_STALL_TIMEOUT = 150.0
_IMG_COMPRESS_THRESHOLD_BYTES = 1048576


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
    mod = new_module("ta12_oracle_adapter")
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
        print(f"FATAL oracle_telegram_adapter_flow12: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()


# ── canonical projection ──
class _Task:
    """Task/future double; projects to its raw JSON (identity by object)."""
    def __init__(self, raw, prefix="task"):
        self._raw = raw
        self._prefix = prefix

    def exception(self):
        return _CUR.call("task_exception", {"task": self})

    def done(self):
        return _CUR.call("task_done", {"task": self})

    def __await__(self):
        return _CUR.call(self._prefix + "_await", {"task": self}).__await__()


class _Event:
    """asyncio.Event double."""
    def __init__(self, raw):
        self._raw = raw

    def is_set(self):
        return _CUR.call("event_is_set", {})

    def set(self):
        return _CUR.call("event_set", {})


class _Lock:
    """asyncio.Lock double (context manager)."""
    def __init__(self, raw=None):
        self._raw = raw or {}

    @property
    def _waiters(self):
        return _CUR.call("lock_waiters", {})

    async def __aenter__(self):
        return _CUR.call("lock_aenter", {})

    async def __aexit__(self, *a):
        return _CUR.call("lock_aexit", {})


class _CM:
    """Silent async context-manager double for the drain lock."""
    def __init__(self, raw=None):
        self._raw = raw

    async def __aenter__(self):
        return None

    async def __aexit__(self, *a):
        return None


_TASK_REG = {}


def _task_for(raw):
    k = json.dumps(raw, sort_keys=True)
    if k not in _TASK_REG:
        _TASK_REG[k] = _Task(raw, "task")
    return _TASK_REG[k]


class _PollingStallError(Exception):
    pass


class _Image:
    """PIL Image double; only the pil_work seam is observable, the rest is data."""
    def __init__(self, spec):
        self.mode = spec.get("mode", "RGB")
        self.size = tuple(spec.get("size", [10, 10]))
        self._spec = spec

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def convert(self, mode):
        return _Image({"mode": mode, "size": list(self.size)})

    def split(self):
        return [_Image({"mode": "L", "size": list(self.size)}) for _ in self.size]

    def paste(self, *a, **k):
        return None

    def resize(self, size, resample=None):
        return _Image({"mode": self.mode, "size": list(size)})

    def save(self, *a, **k):
        return None


def canon(v):
    def conv(x):
        if type(x).__name__ == "AdapterDouble":
            return "<self>"
        if isinstance(x, (_Task, _Event, _Lock)):
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


# ── per-case script backend (mirrors the C ta12_call exactly) ──
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

    st["_set_reaction"] = arec(
        "_set_reaction",
        lambda self_, chat_id, message_id, reaction: {
            "chat_id": chat_id, "message_id": message_id, "reaction": reaction})

    st["task_exception"] = rec("task_exception", lambda task: {"task": task})
    st["logger_debug"] = rec("logger_debug", lambda *a, **k: {"args": list(a), "kwargs": k})

    st["_callback_authorized"] = arec(
        "_callback_authorized",
        lambda self_, query, cb, denial: {"query": query, "cb": cb, "denial": denial})
    st["answer_callback"] = arec(
        "answer_callback", lambda query, text: {"query": query, "text": text})

    st["recovery_in_flight"] = rec("recovery_in_flight", lambda self_: {})
    st["monotonic"] = rec("monotonic", lambda: {})
    st["schedule_polling_recovery"] = rec(
        "schedule_polling_recovery",
        lambda self_, error, *, reason: {"error": str(error), "reason": reason})

    st["task_done"] = rec("task_done", lambda task: {"task": task})

    def _forum_build(raw):
        return SimpleNamespace(**raw) if isinstance(raw, dict) else raw
    st["create_forum_topic"] = arec(
        "create_forum_topic",
        lambda **kw: dict(kw), build=_forum_build)
    st["logger_info"] = rec("logger_info", lambda *a: {"args": list(a)})
    st["logger_warning"] = rec("logger_warning", lambda *a, **k: {"args": list(a), "kwargs": k})
    st["redact"] = rec("redact", lambda text: {"text": text})

    st["delete_webhook"] = rec(
        "delete_webhook", lambda drop_pending_updates: {"drop_pending_updates": drop_pending_updates})
    st["await_with_thread_deadline"] = arec(
        "await_with_thread_deadline",
        lambda awaitable, *, timeout: {"awaitable": awaitable, "timeout": timeout})
    st["looks_like_network_error"] = rec(
        "looks_like_network_error", lambda self_, error: {"error": error})

    st["observed_media_source"] = rec(
        "observed_media_source", lambda self_, msg: {"msg": msg},
        build=lambda raw: (ns_double(raw[0]), *raw[1:]) if isinstance(raw, list) else raw)
    st["int_or_zero"] = rec("int_or_zero", lambda self_, value: {"value": value})
    st["local_root"] = rec("local_root", lambda self_: {})
    st["fetch_telegram_file"] = arec(
        "fetch_telegram_file",
        lambda source, what, local_root: {"source": source, "what": what, "local_root": local_root},
        build=lambda raw: (ns_double(raw[0]), *raw[1:]) if isinstance(raw, list) else raw)
    st["cache_media_bytes_async"] = arec(
        "cache_media_bytes_async",
        lambda data, *, filename, mime_type, default_kind: {
            "data": data, "filename": filename, "mime_type": mime_type, "default_kind": default_kind})

    st["get_general_request_drain_lock"] = rec(
        "get_general_request_drain_lock", lambda self_: {}, build=_CM)
    st["general_req_shutdown"] = rec("general_req_shutdown", lambda: {})
    st["general_req_initialize"] = rec("general_req_initialize", lambda: {})
    st["bounded_request_step"] = arec(
        "bounded_request_step",
        lambda self_, awaitable, message: {"awaitable": awaitable, "message": message})

    st["event_is_set"] = rec("event_is_set", lambda: {})
    st["event_set"] = rec("event_set", lambda: {})

    st["normalize_telegram_chat_id"] = rec(
        "normalize_telegram_chat_id", lambda chat_id: {"chat_id": chat_id})
    st["current_task"] = rec("current_task", lambda: {}, build=_task_for)
    st["new_lock"] = rec("new_lock", lambda: {},
                         build=lambda raw: _Lock(raw if isinstance(raw, dict) else {}))
    st["lock_aenter"] = arec("lock_aenter", lambda: {})
    st["lock_aexit"] = arec("lock_aexit", lambda: {})
    st["lock_waiters"] = rec("lock_waiters", lambda: {})

    st["getsize"] = rec("getsize", lambda path: {"path": path})
    st["sniff_raster"] = rec("sniff_raster", lambda self_, path: {"path": path})
    st["pil_import"] = rec("pil_import", lambda: {})
    st["mkstemp"] = rec("mkstemp", lambda *, suffix, prefix: {"suffix": suffix, "prefix": prefix})
    st["pil_work"] = rec("pil_work", lambda path: {"path": path})
    st["getenv"] = rec("getenv", lambda key: {"key": key})
    return st


# methods installed on the `self` double (class-level, so not in vars())
METHOD_STUBS = {
    "_set_reaction": "_set_reaction",
    "_callback_authorized": "_callback_authorized",
    "_recovery_in_flight": "recovery_in_flight",
    "_schedule_polling_recovery": "schedule_polling_recovery",
    "_looks_like_network_error": "looks_like_network_error",
    "_observed_media_source": "observed_media_source",
    "_int_or_zero": "int_or_zero",
    "_local_root": "local_root",
    "_get_general_request_drain_lock": "get_general_request_drain_lock",
    "_bounded_request_step": "bounded_request_step",
    "_sniff_raster_format": "sniff_raster",
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
    cls.name = "bot"  # class attr so it is not echoed in vars(inst)
    cls._bot = None
    cls._app = None
    cls.config = SimpleNamespace(extra={})
    cls._IMG_COMPRESS_THRESHOLD_BYTES = _IMG_COMPRESS_THRESHOLD_BYTES
    cls._IMG_MAX_DIMENSION = 1600
    cls._IMG_JPEG_QUALITY = 85
    obj = cls()
    for k, v in (state or {}).items():
        setattr(obj, k, copy.deepcopy(v))
    return obj


def seed_globals(st, state):
    NS["logger"] = SimpleNamespace(
        info=st["logger_info"], debug=st["logger_debug"],
        warning=st["logger_warning"], error=(lambda *a, **k: None))
    NS["_POLLING_STALL_TIMEOUT"] = _POLLING_STALL_TIMEOUT
    NS["_UPDATER_START_TIMEOUT"] = _UPDATER_START_TIMEOUT
    NS["_IMG_COMPRESS_THRESHOLD_BYTES"] = _IMG_COMPRESS_THRESHOLD_BYTES
    NS["_PollingStallError"] = _PollingStallError
    NS["_redact_telegram_error_text"] = st["redact"]
    NS["time"] = SimpleNamespace(monotonic=st["monotonic"])
    NS["_await_with_thread_deadline"] = st["await_with_thread_deadline"]
    NS["answer_callback"] = st["answer_callback"]
    NS["normalize_telegram_chat_id"] = st["normalize_telegram_chat_id"]
    NS["math"] = math

    asyncio_ns = SimpleNamespace(
        CancelledError=asyncio.CancelledError,
        Event=_Event, Lock=(lambda: st["new_lock"]()),
        current_task=st["current_task"])
    NS["asyncio"] = asyncio_ns

    # os.path.getsize is a seam; everything else is silent.
    NS["os"] = SimpleNamespace(
        getenv=st["getenv"],
        getsize=(lambda path: _CUR.call("getsize", {"path": path})),
        close=(lambda fd: None),
        path=SimpleNamespace(
            basename=(lambda p: os.path.basename(p or "")),
            splitext=(lambda p: (p.rsplit(".", 1)[0] if "." in p else p,
                                 ("." + p.rsplit(".", 1)[1]) if "." in p else "")),
            getsize=(lambda p: _CUR.call("getsize", {"path": p}))))


def seed_modules(st, state):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.telegram",
                "gateway", "gateway.platforms"):
        m = sys.modules.get(pkg) or types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m

    mod("gateway.platforms.base",
        cache_media_bytes_async=st["cache_media_bytes_async"])
    NS["fetch_telegram_file"] = st["fetch_telegram_file"]
    NS["local_root"] = st["local_root"]

    class _PILModule(types.ModuleType):
        def __getattr__(self, name):
            if name == "Image":
                _CUR.call("pil_import", {})
                setattr(self, "Image", _Image)  # sticky: one seam per case
                return _Image

    pil = sys.modules.get("PIL")
    if not isinstance(pil, _PILModule):
        pil = _PILModule("PIL")
    sys.modules["PIL"] = pil

    class _Tempfile:
        @staticmethod
        def mkstemp(*, suffix, prefix):
            raw = _CUR.call("mkstemp", {"suffix": suffix, "prefix": prefix})
            return (99, raw)
    NS["tempfile"] = _Tempfile
    sys.modules["tempfile"] = _Tempfile

    # Image.open triggers the single observable pil_work seam.
    _orig_open = getattr(_Image, "_seam_open", None)
    def _open(path):
        _CUR.call("pil_work", {"path": path})
        return _Image(_IMAGE_SPEC[0])
    _Image.open = staticmethod(_open)


_IMAGE_SPEC = [{"mode": "RGB", "size": [10, 10]}]


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

    _IMAGE_SPEC[0] = args.get("image_spec") or {"mode": "RGB", "size": [10, 10]}

    def A(k):
        return ns_double(args.get(k))

    try:
        if func == "_clear_reactions":
            ret = run_call(NS[func], inst, args.get("chat_id"), args.get("message_id"))
        elif func == "_clear_task_attrs_except":
            attrs = args.get("attrs") or []
            cur = _Task(args.get("current") or {"id": "cur"}, "task")
            for k in attrs:
                v = getattr(inst, k, None)
                if isinstance(v, dict) and v == args.get("current"):
                    setattr(inst, k, cur)
            ret = NS[func](inst, cur, *attrs)
        elif func == "_consume_abandoned_task":
            ret = NS[func](_Task(args.get("task") or {"id": "t"}, "task"))
        elif func == "_claim_callback_state":
            ret = run_call(NS[func], inst, A("query"), A("cb"), copy.deepcopy(args.get("state") or {}),
                           args.get("key"), args.get("denial", ""), args.get("resolved", ""),
                           pop=args.get("pop", True))
        elif func == "_check_polling_stall":
            ret = run_call(NS[func], inst)
        elif func == "_collect_live_tasks":
            cands_raw = args.get("candidates") or []
            memo = {}
            cands = []
            for c in cands_raw:
                key = json.dumps(c, sort_keys=True)
                if key not in memo:
                    memo[key] = _Task(c, "task") if c is not None else None
                cands.append(memo[key])
            cur_raw = args.get("current") or {"id": "cur"}
            cur = next((t for t in cands if getattr(t, "_raw", None) == cur_raw), _Task(cur_raw, "task"))
            ret = NS[func](cands, cur)
        elif func == "_create_dm_topic":
            if args.get("bot_none"):
                inst.__class__._bot = None
            else:
                inst.__class__._bot = SimpleNamespace(create_forum_topic=st["create_forum_topic"])
            ret = run_call(NS[func], inst, args.get("chat_id"), args.get("name"),
                           args.get("icon_color"), args.get("icon_custom_emoji_id"))
        elif func == "_delete_webhook_best_effort":
            if args.get("bot_kind") == "none":
                inst.__class__._bot = None
            elif args.get("bot_kind") == "no_method":
                inst.__class__._bot = SimpleNamespace()
            else:
                inst.__class__._bot = SimpleNamespace(delete_webhook=(lambda **kw: st["delete_webhook"](**kw)))
            ret = run_call(NS[func], inst, require_success=args.get("require_success", False))
        elif func == "_download_observed_media":
            ret = run_call(NS[func], inst, A("msg"), args.get("what", "attachment"))
        elif func == "_drain_general_connections_after_pool_timeout":
            req = SimpleNamespace(
                shutdown=(lambda: st["general_req_shutdown"]()),
                initialize=(lambda: st["general_req_initialize"]()))
            if args.get("bot_kind") == "none":
                inst.__class__._bot = None; inst.__class__._app = None
            elif args.get("bot_kind") == "bad_request":
                inst.__class__._bot = SimpleNamespace(_request=[]); inst.__class__._app = None
            else:
                inst.__class__._bot = SimpleNamespace(_request=[None, req])
                inst.__class__._app = None
            ret = run_call(NS[func], inst)
        elif func == "_disarm_ptb_retry_loop":
            if args.get("app_kind") == "none":
                inst.__class__._app = None
            elif args.get("app_kind") == "no_updater":
                inst.__class__._app = SimpleNamespace()
            else:
                upd = SimpleNamespace()
                for a in ("_Updater__polling_task_stop_event", "_polling_task_stop_event"):
                    if a in (args.get("attrs") or []):
                        setattr(upd, a, _Event({"kind": a}))
                inst.__class__._app = SimpleNamespace(updater=upd)
            ret = NS[func](inst)
        elif func == "_chat_send_lock":
            locks = getattr(inst, "_telegram_chat_send_locks", None)
            owners = getattr(inst, "_telegram_chat_send_lock_owners", None)
            if isinstance(locks, dict):
                inst._telegram_chat_send_locks = {k: _Lock(v) for k, v in locks.items()}
            if isinstance(owners, dict):
                inst._telegram_chat_send_lock_owners = {k: _task_for(v) for k, v in owners.items()}

            async def _consume():
                async for _ in NS[func](inst, args.get("chat_id")):
                    pass
            ret = asyncio.run(_consume())
        elif func == "_compress_image_to_jpeg":
            ret = NS[func](inst, args.get("image_path"))
        elif func == "_normalize_chat_type":
            ret = NS[func](args.get("chat_type"), is_forum=args.get("is_forum", False))
        elif func == "_env_float_clamped":
            ret = NS[func](args.get("name"), args.get("default", 0.0),
                           min_value=args.get("min_value"), max_value=args.get("max_value"))
        elif func == "_notification_kwargs":
            ret = NS[func](inst, args.get("metadata"))
        elif func == "_is_id_like":
            ret = NS[func](args.get("value"))
        elif func == "_coerce_bool_extra":
            inst.__class__.config = SimpleNamespace(extra=args.get("extra"))
            ret = NS[func](inst, args.get("key"), args.get("default", False))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


class _ReqAwait:
    def __init__(self, marker):
        self._marker = marker

    def __await__(self):
        async def _a():
            return self._marker
        return _a().__await__()


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow12.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta12_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow12: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'\b(?:rec|arec)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'st\["([^"]+)"\]\s*=\s*(?:a?rec)', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow12: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ta12_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow12: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow12.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow12: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow12: fixture func set != KINDS: "
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
    print(f"ORACLE telegram_adapter_flow12: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow12.jsonl"
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
    print(f"SELFTEST telegram_adapter_flow12: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow12.jsonl"
    out = _REPO / "tests" / "fixtures" / "telegram_adapter_flow12.wants.jsonl"
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
