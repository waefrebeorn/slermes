#!/usr/bin/env python3
"""Oracle for the Feishu adapter remainders FLOW port (C sibling:
src/gateway/platforms/feishu_adapter_flow10.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa10_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py; the heavy quarry import chain
is never imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case
wrapper catches BaseException (SystemExit included); scripted raises round-trip
their spec. Summary: 'ORACLE feishu_adapter_flow10: N cases, M mismatches'.
"""
import ast
import asyncio
import copy
import inspect
import json
import os
import re
import sys
import textwrap
import types
from collections import OrderedDict
import __future__ as _fut
from pathlib import PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = __import__("pathlib").Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_fetch_bot_names", "_fetch_last_message_in_thread", "_fetch_message_resource",
    "_fetch_message_text", "_flush_media_batch", "_flush_media_batch_now",
    "_get_audio_duration_ms", "_get_chat_lock", "_get_sdk_executor",
    "_handle_message_event_data", "_handle_message_with_guards",
    "_handle_webhook_request", "_clear_webhook_anomaly", "_ws_link_up",
    "_ws_link_retrying", "_submit_if_ready",
]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa10_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Optional, Sequence
    ns.update({
        "Path": PurePosixPath,
        "json": json,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List,
        "Optional": Optional, "Sequence": Sequence,
        "FeishuAdapter": None,
        "_FEISHU_MESSAGE_TEXT_CACHE_SIZE": 512,
        "_FEISHU_WEBHOOK_MAX_BODY_BYTES": 1048576,
        "_FEISHU_WEBHOOK_BODY_TIMEOUT_SECONDS": 30,
        "_FEISHU_DEDUP_TTL_SECONDS": 86400,
        "_FEISHU_WEBHOOK_ANOMALY_THRESHOLD": 25,
        "_FEISHU_WEBHOOK_ANOMALY_TTL_SECONDS": 21600,
        "_WEBHOOK_EVENT_HANDLERS": {},
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC)
        elif isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch); c.decorator_list = []
                    exec_node(src, c, ns, SRC)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow10: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow10.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa10_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}

# ── canonical projection ──
class _Token:
    def __init__(self, value): object.__setattr__(self, "v", value)
    def __repr__(self): return repr(self.v)

_TO = {}
def tok(value):
    key = json.dumps(canon(value), sort_keys=True, ensure_ascii=False)
    if key not in _TO:
        _TO[key] = _Token(canon(value))
    return _TO[key]


def canon(v):
    def conv(x):
        if isinstance(x, _BatchDouble):
            return conv(object.__getattribute__(x, "_raw"))
        if isinstance(x, _Token):
            return conv(x.v)
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception hierarchy ──
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "UnicodeError": UnicodeError, "Warning": Warning, "ArithmeticError": ArithmeticError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
}
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = (BaseException,) if kind in BASE_EXC else (Exception,)
        cls = type(kind, cls, {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections (MUST match fa10_call args in the C port) ──
ARGMAP = {
    "admit": lambda sender, message: {},
    "anomaly_pop": lambda remote_ip, default=None: {"remote_ip": remote_ip},
    "batch_pop": lambda key, default=None: {"key": key},
    "build_get_message_request": lambda message_id: {"message_id": message_id},
    "build_resource_request": lambda message_id, file_key, resource_type: {
        "message_id": message_id, "file_key": file_key, "resource_type": resource_type},
    "cache_contains": lambda message_id: {"message_id": message_id},
    "cache_get": lambda message_id: {"message_id": message_id},
    "cache_len": lambda: {},
    "cache_move_end": lambda message_id: {"message_id": message_id},
    "cache_popitem": lambda last=True: {},
    "cache_set": lambda message_id, value: {"message_id": message_id, "value": value},
    "card_action": lambda payload: {},
    "clear_anomaly": lambda remote_ip: {"remote_ip": remote_ip},
    "delayed_flush": lambda tasks, key, delay, callback: {"key": key, "delay": delay},
    "dispatch_handler": lambda data: {},
    "extract_text": lambda msg_type, raw_content, mentions: {
        "msg_type": msg_type, "raw_content": raw_content, "mentions": mentions},
    "fetch_message": lambda func, request: {},
    "fetch_resource": lambda func, request: {},
    "get_chat_lock": lambda chat_id: {"chat_id": chat_id},
    "handle_guards": lambda event: {"event": canon(event)},
    "handle_message": lambda event: {"event": canon(event)},
    "is_bot_sender": lambda sender: {"sender_id": getattr(sender, "sender_id", "")},
    "is_duplicate": lambda message_id: {"message_id": message_id},
    "list_messages": lambda func, request: {},
    "locks_get": lambda chat_id: {"chat_id": chat_id},
    "locks_items": lambda: {},
    "locks_len": lambda: {},
    "locks_move_end": lambda chat_id: {"chat_id": chat_id},
    "locks_pop": lambda chat_id: {"chat_id": chat_id},
    "locks_set": lambda chat_id, value: {"chat_id": chat_id, "value": canon(value)},
    "loop_accepts": lambda loop: {},
    "mark_connected": lambda: {},
    "namespace": lambda payload: {"payload": canon(payload)},
    "new_executor": lambda max_workers=10, thread_name_prefix="": {
        "max_workers": max_workers, "thread_name_prefix": thread_name_prefix},
    "new_lock": lambda: {},
    "process_inbound": lambda data, message, sender_id, chat_type, message_id, is_bot: {
        "message_id": message_id, "sender_id": sender_id,
        "chat_type": chat_type, "is_bot": is_bot},
    "rate_limit": lambda key: {"key": key},
    "reaction_event": lambda event_type, data: {"event_type": event_type},
    "read_audio": lambda path, mode="rb": {"path": path},
    "read_body": lambda request, limit: {},
    "response_succeeded": lambda response: {},
    "secrets_match": lambda a, b: {"incoming": a, "expected": b},
    "sig_valid": lambda headers, body_bytes: {},
    "submit_on_loop": lambda loop, coro: {},
    "tenant_get_raw": lambda path, queries=None: {"path": path, "queries": queries},
    "warn_allowlist": lambda chat_id: {"chat_id": chat_id},
    "web_response": lambda value: {"value": canon(value)},
    "webhook_reject": lambda remote_ip, anomaly, status, text=None, json_msg=None: {
        "remote_ip": remote_ip, "anomaly": anomaly, "status": status,
        "text": text, "json_msg": json_msg},
    "write_status": lambda label, platform_state=None, error_code=None, error_message=None: {
        "platform_state": platform_state, "error_code": error_code,
        "error_message": error_message},
}

AWAIT_SEAMS = {
    "handle_guards", "handle_message", "is_duplicate", "process_inbound",
    "read_body", "tenant_get_raw", "fetch_message", "fetch_resource", "list_messages",
    "delayed_flush",
}

# ── per-case script backend (mirrors C fa10_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def _install(obj, name, value):
    object.__setattr__(obj, name, value)


class _Self:
    """SELF double: attribute writes are plain; methods installed via _install."""
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)


class _AsyncLock:
    def __init__(self, ctx, tok):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "tok", tok)
    async def __aenter__(self): return self
    async def __aexit__(self, *a): return False


class _MsgResp:
    def __init__(self, ctx, raw):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_raw", mat_event(raw) if raw is not None else None)
    def success(self):
        return self._ctx.call("response_succeeded", {})
    def __getattr__(self, name):
        raw = object.__getattribute__(self, "_raw")
        return getattr(raw, name) if raw is not None else None


class _File:
    def __init__(self, data): object.__setattr__(self, "data", bytes(data or []))
    def read(self): return self.data
    def __enter__(self): return self
    def __exit__(self, *a): return False


class _LockObj:
    def __init__(self, d):
        object.__setattr__(self, "v", bool(d.get("locked", False)) if isinstance(d, dict) else False)
    def locked(self): return self.v


class _SeamMap:
    """Generic dict double firing a get/set/pop seam."""
    def __init__(self, ctx, get_seam, set_seam, pop_seam=None, wrap=None, drop_value=False):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_get", get_seam)
        object.__setattr__(self, "_set", set_seam)
        object.__setattr__(self, "_pop", pop_seam)
        object.__setattr__(self, "_wrap", wrap)
        object.__setattr__(self, "_drop", drop_value)
    def get(self, key, default=None):
        v = self._ctx.call(self._get, {"key": key})
        if v is None: return default
        return self._wrap(self._ctx, v) if self._wrap else v
    def __setitem__(self, k, v):
        args = {"key": k} if self._drop else {"key": k, "value": v}
        return self._ctx.call(self._set, args)
    def pop(self, k, default=None):
        return self._ctx.call(self._pop, {"key": k})


class _TextCache:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
    def __contains__(self, k): return self._ctx.call("cache_contains", {"message_id": k})
    def move_to_end(self, k): return self._ctx.call("cache_move_end", {"message_id": k})
    def __getitem__(self, k): return self._ctx.call("cache_get", {"message_id": k})
    def __setitem__(self, k, v): return self._ctx.call("cache_set", {"message_id": k, "value": v})
    def __len__(self): return self._ctx.call("cache_len", {})
    def popitem(self, last=True): return self._ctx.call("cache_popitem", {})


class _ChatLocks:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_last_items", [])
    def get(self, k):
        return self._ctx.call("locks_get", {"chat_id": k})
    def move_to_end(self, k): return self._ctx.call("locks_move_end", {"chat_id": k})
    def __len__(self): return self._ctx.call("locks_len", {})
    def items(self):
        raw = self._ctx.call("locks_items", {}) or []
        pairs = [(k, _LockObj(v)) for k, v in raw]
        object.__setattr__(self, "_last_items", [k for k, _ in pairs])
        return pairs
    def __iter__(self):
        return iter(object.__getattribute__(self, "_last_items"))
    def pop(self, k): return self._ctx.call("locks_pop", {"chat_id": k})
    def __setitem__(self, k, v): return self._ctx.call("locks_set", {"chat_id": k, "value": canon(v)})


class _MediaBatchMap:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def pop(self, k, default=None):
        v = self._ctx.call("batch_pop", {"key": k})
        return _BatchDouble(self._ctx, v) if v else v


class _AnomalyMap:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def pop(self, ip, default=None):
        return self._ctx.call("anomaly_pop", {"remote_ip": ip})


class _BatchDouble:
    def __init__(self, ctx, raw):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_raw", raw or {})
        d = raw or {}
        for k in ("media_urls", "media_types", "media_text_inlined"):
            object.__setattr__(self, k, list(d.get(k) or []))
        object.__setattr__(self, "text", d.get("text", ""))


def _client_double():
    return types.SimpleNamespace(im=types.SimpleNamespace(v1=types.SimpleNamespace(
        message=types.SimpleNamespace(get=lambda: None, list=lambda: None),
        message_resource=types.SimpleNamespace(get=lambda: None))))


def mat_event(raw):
    if isinstance(raw, dict):
        return types.SimpleNamespace(**{k: mat_event(v) for k, v in raw.items()})
    if isinstance(raw, list):
        return [mat_event(i) for i in raw]
    return raw


SELF = None


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return ctx.call(nm, argfn(*a, **k))
        return af
    def sf(*a, **k):
        return ctx.call(nm, argfn(*a, **k))
    return sf


def install_recorders(ctx, target):
    global SELF
    for nm in SEAMED:
        NS[nm] = make_recorder(ctx, nm)
    NS["FeishuAdapter"] = None
    NS["logger"] = types.SimpleNamespace(
        warning=lambda *a, **k: None, error=lambda *a, **k: None,
        info=lambda *a, **k: None, debug=lambda *a, **k: None)
    NS["asyncio"] = types.SimpleNamespace(
        Lock=make_recorder(ctx, "new_lock"),
        wait_for=lambda coro, timeout: coro)
    NS["threading"] = types.SimpleNamespace(Lock=make_recorder(ctx, "new_lock"))
    NS["concurrent"] = types.SimpleNamespace(
        futures=types.SimpleNamespace(ThreadPoolExecutor=make_recorder(ctx, "new_executor")))
    SELF = _Self(ctx)

    if target == "_fetch_bot_names":
        _install(SELF, "_tenant_get_raw", make_recorder(ctx, "tenant_get_raw"))
        _install(SELF, "_client", "C")
    elif target == "_fetch_last_message_in_thread":
        _install(SELF, "_client", _client_double())
        _install(SELF, "_response_succeeded", make_recorder(ctx, "response_succeeded"))
        async def run_blocking(func, request):
            return mat_event(ctx.call("list_messages", {"thread_id": getattr(request, "thread_id", "")}))
        _install(SELF, "_run_blocking", run_blocking)
        class _ReqBuilder:
            def __init__(self): self.thread_id = ""
            def container_id_type(self, v): return self
            def container_id(self, v): self.thread_id = v; return self
            def page_size(self, v): return self
            def build(self): return self
        _lm = types.SimpleNamespace(builder=_ReqBuilder)
        for m in ("lark_oapi", "lark_oapi.api", "lark_oapi.api.im", "lark_oapi.api.im.v1"):
            mod = types.ModuleType(m); mod.__path__ = []; sys.modules[m] = mod
        sys.modules["lark_oapi.api.im.v1"].ListMessageRequest = _lm
    elif target == "_fetch_message_resource":
        _install(SELF, "_build_message_resource_request", make_recorder(ctx, "build_resource_request"))
        async def fetch_rb(func, req):
            return ctx.call("fetch_resource", {})
        _install(SELF, "_run_blocking", fetch_rb)
        _install(SELF, "_client", types.SimpleNamespace(
            im=types.SimpleNamespace(v1=types.SimpleNamespace(
                message_resource=types.SimpleNamespace(get=lambda: None)))))
    elif target == "_fetch_message_text":
        _install(SELF, "_client", types.SimpleNamespace(
            im=types.SimpleNamespace(v1=types.SimpleNamespace(message=types.SimpleNamespace(get=lambda: None)))))
        _install(SELF, "_message_text_cache", _TextCache(ctx))
        _install(SELF, "_build_get_message_request", make_recorder(ctx, "build_get_message_request"))
        async def fetch_msg_rb(func, req):
            return _MsgResp(ctx, ctx.call("fetch_message", {}))
        _install(SELF, "_run_blocking", fetch_msg_rb)
        _install(SELF, "_extract_text_from_raw_content", make_recorder(ctx, "extract_text"))
    elif target == "_flush_media_batch":
        _install(SELF, "_pending_media_batch_tasks", {})
        _install(SELF, "_flush_media_batch_now", lambda ev=None: None)
        _install(SELF, "_delayed_flush", make_recorder(ctx, "delayed_flush"))
    elif target == "_flush_media_batch_now":
        _install(SELF, "_pending_media_batches", _MediaBatchMap(ctx))
        _install(SELF, "_handle_message_with_guards", make_recorder(ctx, "handle_guards"))
    elif target == "_get_audio_duration_ms":
        def fake_open(path, mode="rb", *a, **k):
            v = ctx.call("read_audio", {"path": path})
            return _File(v)
        NS["open"] = fake_open
    elif target == "_get_chat_lock":
        _install(SELF, "_chat_locks", _ChatLocks(ctx))
    elif target == "_get_sdk_executor":
        class _DummyLock:
            def __enter__(self): return self
            def __exit__(self, *a): return False
        NS["threading"] = types.SimpleNamespace(Lock=_DummyLock)
        # reads SELF attrs installed in live_call; new_executor is a global seam
    elif target == "_handle_message_event_data":
        _install(SELF, "_is_duplicate", make_recorder(ctx, "is_duplicate"))
        _install(SELF, "_admit", make_recorder(ctx, "admit"))
        _install(SELF, "_warn_once_empty_allowlist_deny", make_recorder(ctx, "warn_allowlist"))
        _install(SELF, "_process_inbound_message", make_recorder(ctx, "process_inbound"))
        NS["_is_bot_sender"] = make_recorder(ctx, "is_bot_sender")
    elif target == "_handle_message_with_guards":
        def get_lock(chat_id):
            return _AsyncLock(ctx, ctx.call("get_chat_lock", {"chat_id": chat_id}))
        _install(SELF, "_get_chat_lock", get_lock)
        _install(SELF, "handle_message", make_recorder(ctx, "handle_message"))
    elif target == "_handle_webhook_request":
        def reject(remote_ip, anomaly, status, text=None, json_msg=None):
            return ctx.call("webhook_reject", {
                "remote_ip": remote_ip, "anomaly": anomaly, "status": status,
                "text": text, "json_msg": json_msg})
        _install(SELF, "_webhook_reject", reject)
        async def read_limited(request, limit):
            v = ctx.call("read_body", {})
            return bytes(v) if isinstance(v, list) else (v.encode("utf-8") if isinstance(v, str) else b"")
        NS["_read_limited_feishu_webhook_body"] = read_limited
        NS["secrets_match"] = make_recorder(ctx, "secrets_match")
        NS["web"] = types.SimpleNamespace(json_response=make_recorder(ctx, "web_response"))
        _install(SELF, "_is_webhook_signature_valid", make_recorder(ctx, "sig_valid"))
        _install(SELF, "_check_webhook_rate_limit", make_recorder(ctx, "rate_limit"))
        _install(SELF, "_clear_webhook_anomaly", make_recorder(ctx, "clear_anomaly"))
        _install(SELF, "_on_webhook_card_action", make_recorder(ctx, "card_action"))
        _install(SELF, "_namespace_from_mapping", make_recorder(ctx, "namespace"))
        _install(SELF, "_on_reaction_event", make_recorder(ctx, "reaction_event"))
        _install(SELF, "_WEBHOOK_EVENT_HANDLERS", NS.get("_WEBHOOK_EVENT_HANDLERS") or {})
    elif target == "_clear_webhook_anomaly":
        _install(SELF, "_webhook_anomaly_counts", _AnomalyMap(ctx))
    elif target == "_ws_link_up":
        _install(SELF, "_mark_connected", make_recorder(ctx, "mark_connected"))
    elif target == "_ws_link_retrying":
        _install(SELF, "_write_runtime_status_safe", make_recorder(ctx, "write_status"))
    elif target == "_submit_if_ready":
        _install(SELF, "_loop_accepts_callbacks", make_recorder(ctx, "loop_accepts"))
        _install(SELF, "_submit_on_loop", make_recorder(ctx, "submit_on_loop"))


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def _run(x):
    if inspect.isawaitable(x):
        return asyncio.run(x)
    return x


def live_call(func, args, ctx):
    g = args.get
    if func == "_fetch_bot_names":
        _install(SELF, "_client", "C" if g("client", False) else None)
        return NS[func](SELF, list(g("bot_ids") or []))
    if func == "_fetch_last_message_in_thread":
        _install(SELF, "_client", _client_double() if g("client", False) else None)
        return NS[func](SELF, g("thread_id", ""))
    if func == "_fetch_message_resource":
        return NS[func](SELF, message_id=g("message_id", ""), file_key=g("file_key", ""),
                        resource_type=g("resource_type", ""))
    if func == "_fetch_message_text":
        _install(SELF, "_client", _client_double() if g("client", False) else None)
        return NS[func](SELF, g("message_id", ""))
    if func == "_flush_media_batch":
        _install(SELF, "_media_batch_delay_seconds", g("delay", 0.0))
        return NS[func](SELF, g("key", ""))
    if func == "_flush_media_batch_now":
        return NS[func](SELF, g("key", ""))
    if func == "_get_audio_duration_ms":
        return NS[func](g("path", ""))
    if func == "_get_chat_lock":
        _install(SELF, "CHAT_LOCK_MAX_SIZE", g("max_size", 2))
        return NS[func](SELF, g("chat_id", ""))
    if func == "_get_sdk_executor":
        _install(SELF, "_sdk_executor_closing", g("closing", False))
        ex = g("executor", None)
        _install(SELF, "_sdk_executor", mat_event(ex) if ex is not None else None)
        return NS[func](SELF)
    if func == "_handle_message_event_data":
        return NS[func](SELF, mat_event(g("data") or {}))
    if func == "_handle_message_with_guards":
        return NS[func](SELF, mat_event(g("event") or {}))
    if func == "_handle_webhook_request":
        _install(SELF, "_verification_token", g("verification_token", ""))
        _install(SELF, "_encrypt_key", g("encrypt_key", ""))
        _install(SELF, "_app_id", g("app_id", ""))
        _install(SELF, "_webhook_path", g("webhook_path", ""))
        NS["_FEISHU_WEBHOOK_MAX_BODY_BYTES"] = g("max_body", 1048576)
        NS["_FEISHU_WEBHOOK_BODY_TIMEOUT_SECONDS"] = g("body_timeout", 30)
        NS["_WEBHOOK_EVENT_HANDLERS"] = g("handlers") or {}
        _install(SELF, "_WEBHOOK_EVENT_HANDLERS", NS["_WEBHOOK_EVENT_HANDLERS"])
        rq = g("request") or {}
        req = types.SimpleNamespace(
            remote=rq.get("remote"), headers=rq.get("headers") or {},
            content_length=rq.get("content_length"))
        return NS[func](SELF, req)
    if func == "_clear_webhook_anomaly":
        return NS[func](SELF, g("remote_ip", ""))
    if func in ("_ws_link_up", "_ws_link_retrying"):
        cl = mat_event(g("client")) if g("client") is not None else None
        wc = mat_event(g("ws_client")) if "ws_client" in args else cl
        _install(SELF, "_running", g("running", False))
        _install(SELF, "_ws_client", wc)
        return NS[func](SELF, cl)
    if func == "_submit_if_ready":
        _install(SELF, "_loop", g("loop"))
        return NS[func](SELF, g("label", "task"), lambda: None)
    raise RuntimeError(f"unknown func {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    try:
        ret = _run(live_call(func, args, ctx))
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── cross-checks ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow10: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow10: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow10.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_feishu_adapter_flow10: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow10: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow10: fixture funcs not in TARGETS: {unknown_funcs}")
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
    if shown >= 200:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:500]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:500]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:500]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:500]}")


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
    print(f"ORACLE feishu_adapter_flow10: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow10.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if case.get("_note") == "fail-closed":
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in TARGETS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST feishu_adapter_flow10: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
