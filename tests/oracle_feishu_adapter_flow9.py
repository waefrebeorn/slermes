#!/usr/bin/env python3
"""Oracle for the Feishu adapter extraction/media/queue/settings/limit/retry FLOW
port (C sibling: src/gateway/platforms/feishu_adapter_flow9.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa9_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py; the heavy quarry import chain
is never imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case
wrapper catches BaseException (SystemExit included); scripted raises round-trip
their spec. Summary: 'ORACLE feishu_adapter_flow9: N cases, M mismatches'.
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
    "_build_lark_client", "_normalize", "_webhook_reject",
    "_download_remote_image", "_apply_settings", "_check_webhook_rate_limit",
    "_download_feishu_message_resources", "_enqueue_pending_inbound_event",
    "_download_feishu_image", "_download_feishu_message_resource",
    "_extract_message_content", "_enqueue_text_event", "_enqueue_media_event",
    "_feishu_send_with_retry", "_download_remote_document",
    "_drain_pending_inbound_events",
]

FA9_FIELDS = [
    "app_id", "app_secret", "domain_name", "connection_mode", "encrypt_key",
    "verification_token", "group_policy", "allowed_group_users", "bot_open_id",
    "bot_user_id", "bot_name", "dedup_cache_size", "text_batch_delay_seconds",
    "text_batch_split_delay_seconds", "text_batch_max_messages", "text_batch_max_chars",
    "media_batch_delay_seconds", "webhook_host", "webhook_port", "webhook_path",
    "ws_reconnect_nonce", "ws_reconnect_interval", "ws_ping_interval", "ws_ping_timeout",
    "admins", "default_group_policy", "group_rules", "allow_bots",
    "require_mention", "allow_all_dm",
]


def _lit_constant(path, name):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            for t in tgts:
                if isinstance(t, ast.Name) and t.id == name:
                    try:
                        return ast.literal_eval(node.value)
                    except Exception:
                        env = {}
                        return eval(compile(ast.Expression(node.value), path, "eval"), env)
    raise KeyError(name)


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa9_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Literal, Optional, Sequence
    ns.update({
        "Path": PurePosixPath,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
        "Optional": Optional, "Sequence": Sequence,
        "FeishuAdapter": None,
        "FeishuAdapterSettings": types.SimpleNamespace(
            __dataclass_fields__={f: None for f in FA9_FIELDS}),
        "_FEISHU_CONNECT_ATTEMPTS": _lit_constant(SRC, "_FEISHU_SEND_ATTEMPTS"),
        "_FEISHU_SEND_ATTEMPTS": 3,
        "_FEISHU_WEBHOOK_RATE_WINDOW_SECONDS": 60,
        "_FEISHU_WEBHOOK_RATE_LIMIT_MAX": 120,
        "_FEISHU_WEBHOOK_RATE_MAX_KEYS": 4096,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC)
        elif isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if (isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS
                        and ch.name != "_build_lark_client"):
                    c = copy.copy(ch); c.decorator_list = []
                    exec_node(src, c, ns, SRC)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow9: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow9.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa9_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
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


# ── seam argument projections (MUST match fa9_call args in the C port) ──
ARGMAP = {
    "bot_identity": lambda: {},
    "cache_img": lambda url, ext: {"url": url, "ext": ext},
    "guess_ext": lambda url, default: {"url": url, "default": default},
    "json_response": lambda body, status: {"code": body["code"], "msg": body["msg"], "status": status},
    "lark_build": lambda app_id, app_secret, domain: {"app_id": app_id, "app_secret": app_secret, "domain": domain},
    "normalize": lambda message_type, raw_content, mentions, bot: {
        "message_type": message_type, "raw_content": raw_content, "mentions": mentions, "bot": bot},
    "record_anomaly": lambda remote_ip, anomaly: {"remote_ip": remote_ip, "anomaly": anomaly},
    "web_response": lambda status, text: {"status": status, "text": text},
    # phase 2
    "set_field": lambda name, value: {"name": name, "value": value},
    "now": lambda: {},
    "counts_get": lambda key: {"key": key},
    "counts_len": lambda: {},
    "counts_items": lambda: {},
    "counts_set": lambda key, value: {"key": key, "value": value},
    "counts_del": lambda key: {"key": key},
    "log_warn": lambda *a, **k: {},
    "dl_img": lambda message_id, image_key: {"message_id": message_id, "image_key": image_key},
    "dl_res": lambda message_id, file_key, resource_type, fallback_filename: {
        "message_id": message_id, "file_key": file_key,
        "resource_type": resource_type, "fallback_filename": fallback_filename},
    "lock_enter": lambda: {},
    "lock_exit": lambda *a, **k: {},
    "queued_len": lambda: {},
    "queued_pop": lambda: {},
    "queued_append": lambda: {},
    "log_error": lambda fmt, maxd, mid: {"message_id": mid},
    "log_warning": lambda *a, **k: {},
    # phase 3
    "fetch_resource": lambda message_id, file_key, resource_type: {
        "message_id": message_id, "file_key": file_key, "resource_type": resource_type},
    "resp_success": lambda: {},
    "read_binary": lambda response: {},
    "get_header": lambda response, name: {"name": name},
    "guess_file_ext": lambda filename, content_type, default, **k: {
        "filename": filename, "content_type": content_type, "default": default},
    "cache_img_bytes": lambda raw, ext: {"ext": ext},
    "default_img_media": lambda ext: {"ext": ext},
    "norm_media": lambda content_type, default: {
        "content_type": content_type, "default": default},
    "guess_media_name": lambda filename: {"filename": filename},
    "cache_audio_bytes": lambda raw, ext: {"ext": ext},
    "cache_doc_bytes": lambda raw, filename: {"filename": filename},
    "guess_doc_media": lambda filename: {"filename": filename},
    "normalize_msg": lambda message_type, raw_content, mentions: {
        "message_type": message_type, "raw_content": raw_content, "mentions": mentions},
    "fetch_resources": lambda message_id, normalized: {
        "message_id": message_id, "normalized": normalized},
    "resolve_type": lambda normalized, media_types: {
        "normalized": normalized, "media_types": media_types},
    "strip_mentions": lambda text, mentions: {"text": text, "mentions": mentions},
    "caption_is_command": lambda text: {"text": text},
    "extract_doc": lambda url, media_type: {"url": url, "media_type": media_type},
    # phase 4
    "text_key": lambda event: {},
    "batches_get": lambda key: {"key": key},
    "batch_compatible": lambda existing, incoming: {},
    "flush_now": lambda key: {"key": key},
    "counts_get": lambda key, default=1: {"key": key},
    "batches_set": lambda key, value: {"key": key},
    "counts_set": lambda key, value: {"key": key, "value": value},
    "schedule_flush": lambda key: {"key": key},
    "media_key": lambda event: {},
    "mbatches_get": lambda key: {"key": key},
    "mbatches_set": lambda key, value: {"key": key},
    "media_compatible": lambda existing, incoming: {},
    "mflush_now": lambda key: {"key": key},
    "schedule_mflush": lambda key: {"key": key},
    "merge_caption": lambda existing_text, event_text: {
        "existing_text": existing_text, "event_text": event_text},
    # phase 5
    "send_raw": lambda chat_id, msg_type, payload, reply_to, metadata: {
        "chat_id": chat_id, "msg_type": msg_type, "payload": payload,
        "reply_to": reply_to, "metadata": metadata},
    "response_succeeded": lambda response: {},
    "sleep": lambda delay: {"delay": delay},
    # phase 6
    "is_safe": lambda url: {"url": url},
    "client_enter": lambda: {},
    "client_exit": lambda *a, **k: {},
    "stream_enter": lambda *a, **k: {},
    "stream_exit": lambda *a, **k: {},
    "raise_status": lambda: {},
    "headers_get": lambda name, default="": {"name": name},
    "read_body": lambda response, media_type: {},
    "derive_name": lambda file_url, content_type, default_name, default_ext: {
        "url": file_url, "content_type": content_type,
        "default_name": default_name, "default_ext": default_ext},
    "cache_doc": lambda body, filename: {"filename": filename},
    # phase 7
    "running": lambda: {},
    "loop_accepts": lambda loop: {},
    "take_batch": lambda: {},
    "is_empty": lambda: {},
    "handle_event": lambda event: {"event": event},
    "submit_on_loop": lambda loop, data: {},
    "requeue": lambda items: {"items": items},
    "log_info": lambda *a, **k: {},
    "poll_sleep": lambda delay: {"delay": delay},
}

AWAIT_SEAMS = {"cache_img", "dl_img", "dl_res", "fetch_resource", "cache_img_bytes",
               "cache_audio_bytes", "cache_doc_bytes", "extract_doc", "fetch_resources",
               "flush_now", "mflush_now", "send_raw", "sleep", "read_body",
               "cache_doc", "client_enter", "stream_enter"}

# ── per-case script backend (mirrors C fa9_call exactly) ──
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
    """SELF double: attribute writes fire self_setattr; methods installed via _install."""
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)


class _LarkBuilder:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_vals", {})
    def app_id(self, v): self._vals["app_id"] = v; return self
    def app_secret(self, v): self._vals["app_secret"] = v; return self
    def domain(self, v): self._vals["domain"] = v; return self
    def log_level(self, v): return self
    def build(self): return self._ctx.call("lark_build", dict(self._vals))


class _SetSelf:
    """self double whose attribute writes fire the set_field seam."""
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __setattr__(self, name, value):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.call("set_field", {"name": name, "value": canon(value)})
        object.__setattr__(self, name, value)


class _RateMap:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def get(self, key, default=None):
        v = self._ctx.call("counts_get", {"key": key})
        return v if v is not None else default
    def __len__(self): return self._ctx.call("counts_len", {})
    def items(self):
        out = self._ctx.call("counts_items", {}) or []
        return [(k, tuple(v)) for k, v in out]
    def __setitem__(self, k, v):
        return self._ctx.call("counts_set", {"key": k, "value": list(v)})
    def __delitem__(self, k):
        return self._ctx.call("counts_del", {"key": k})
    def __contains__(self, k):
        v = self._ctx.call("counts_get", {"key": k})
        return v is not None


class _Lock:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __enter__(self): return self._ctx.call("lock_enter", {})
    def __exit__(self, *a):
        self._ctx.call("lock_exit", {})
        return False


class _Queue:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __len__(self): return self._ctx.call("queued_len", {})
    def pop(self, i=0): return mat_event(self._ctx.call("queued_pop", {}))
    def append(self, v): return self._ctx.call("queued_append", {})


def mat_event(raw):
    if isinstance(raw, dict):
        return types.SimpleNamespace(**{k: mat_event(v) for k, v in raw.items()})
    if isinstance(raw, list):
        return [mat_event(i) for i in raw]
    return raw


class _Resp:
    def __init__(self, ctx, raw):
        object.__setattr__(self, "_ctx", ctx)
        for k, v in (raw or {}).items():
            object.__setattr__(self, k, mat_event(v))
    def success(self):
        return self._ctx.call("resp_success", {})


def rec_fetch(ctx):
    async def af(*a, **k):
        v = ctx.call("fetch_resource", {
            "message_id": k.get("message_id"), "file_key": k.get("file_key"),
            "resource_type": k.get("resource_type")})
        return _Resp(ctx, v) if isinstance(v, dict) else v
    return af


def rec_norm_msg(ctx):
    def sf(*a, **k):
        v = ctx.call("normalize_msg", {
            "message_type": a[0], "raw_content": a[1], "mentions": a[2]})
        return mat_event(v)
    return sf


def rec_send_raw(ctx):
    async def af(*a, **k):
        v = ctx.call("send_raw", {
            "chat_id": k.get("chat_id"), "msg_type": k.get("msg_type"),
            "payload": k.get("payload"), "reply_to": k.get("reply_to"),
            "metadata": k.get("metadata")})
        return mat_event(v) if isinstance(v, dict) else v
    return af


class _AHeaders:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def get(self, name, default=""):
        return self._ctx.call("headers_get", {"name": name})


class _AResp:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "headers", _AHeaders(ctx))
    def raise_for_status(self): return self._ctx.call("raise_status", {})


class _AStream:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    async def __aenter__(self):
        self._ctx.call("stream_enter", {})
        return _AResp(self._ctx)
    async def __aexit__(self, *a):
        self._ctx.call("stream_exit", {})
        return False


class _AClient:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    async def __aenter__(self):
        self._ctx.call("client_enter", {})
        return self
    async def __aexit__(self, *a):
        self._ctx.call("client_exit", {})
        return False
    def stream(self, *a, **k): return _AStream(self._ctx)


class _DrainQueue:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __getitem__(self, sl):
        return mat_event(self._ctx.call("take_batch", {}))
    def clear(self): return None
    def __bool__(self): return not bool(self._ctx.call("is_empty", {}))
    def __setitem__(self, sl, value):
        return self._ctx.call("requeue", {"items": canon(value)})


class _DrainSelf:
    @property
    def _running(self):
        return object.__getattribute__(self, "_ctx").call("running", {})
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_loop", None)
        object.__setattr__(self, "_pending_inbound_lock", _Lock(ctx))
        object.__setattr__(self, "_pending_inbound_events", _DrainQueue(ctx))
        object.__setattr__(self, "_loop_accepts_callbacks", make_recorder(ctx, "loop_accepts"))
        object.__setattr__(self, "_submit_on_loop", make_recorder(ctx, "submit_on_loop"))
        object.__setattr__(self, "_handle_message_event_data", make_recorder(ctx, "handle_event"))


class _BatchDouble:
    """Inert carrier for a queued text/media batch; reads are preset, writes inert."""
    def __init__(self, ctx, raw):
        object.__setattr__(self, "_ctx", ctx)
        d = raw or {}
        object.__setattr__(self, "text", d.get("text", ""))
        for k in ("media_urls", "media_types", "media_text_inlined"):
            object.__setattr__(self, k, list(d.get(k) or []))
        object.__setattr__(self, "timestamp", d.get("timestamp"))
        object.__setattr__(self, "message_id", d.get("message_id"))
        src = d.get("source") or {}
        object.__setattr__(self, "source", types.SimpleNamespace(
            message_id=src.get("message_id") if isinstance(src, dict) else None))
    def absorb_turn_input(self, ev):
        return None


class _SeamMap:
    def __init__(self, ctx, get_seam, set_seam, wrap=False, drop_value=True):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_get", get_seam)
        object.__setattr__(self, "_set", set_seam)
        object.__setattr__(self, "_wrap", wrap)
        object.__setattr__(self, "_drop", drop_value)
    def get(self, key, default=None):
        v = self._ctx.call(self._get, {"key": key})
        if v is None:
            return default
        return _BatchDouble(self._ctx, v) if self._wrap else v
    def __setitem__(self, k, v):
        args = {"key": k} if self._drop else {"key": k, "value": v}
        return self._ctx.call(self._set, args)


def rec_batches_get(ctx, seam):
    def sf(key):
        v = ctx.call(seam, {"key": key})
        return _BatchDouble(ctx, v) if v is not None else None
    return sf


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
    NS["lark"] = types.SimpleNamespace(
        Client=types.SimpleNamespace(builder=lambda: _LarkBuilder(ctx)),
        LogLevel=types.SimpleNamespace(WARNING="WARNING"))
    SELF = _Self(ctx)
    if target == "_normalize":
        _install(SELF, "_bot_identity", make_recorder(ctx, "bot_identity"))
        NS["normalize_feishu_message"] = make_recorder(ctx, "normalize")
    elif target == "_webhook_reject":
        _install(SELF, "_record_webhook_anomaly", make_recorder(ctx, "record_anomaly"))
        NS["web"] = types.SimpleNamespace(
            json_response=make_recorder(ctx, "json_response"),
            Response=make_recorder(ctx, "web_response"))
    elif target == "_download_remote_image":
        _install(SELF, "_guess_remote_extension", make_recorder(ctx, "guess_ext"))
        NS["cache_image_from_url"] = make_recorder(ctx, "cache_img")
    elif target == "_apply_settings":
        SELF = _SetSelf(ctx)
    elif target == "_check_webhook_rate_limit":
        _install(SELF, "_webhook_rate_counts", _RateMap(ctx))
        NS["time"] = types.SimpleNamespace(time=make_recorder(ctx, "now"))
        NS["logger"] = types.SimpleNamespace(warning=make_recorder(ctx, "log_warn"))
    elif target == "_download_feishu_message_resources":
        _install(SELF, "_download_feishu_image", make_recorder(ctx, "dl_img"))
        _install(SELF, "_download_feishu_message_resource", make_recorder(ctx, "dl_res"))
    elif target == "_enqueue_pending_inbound_event":
        _install(SELF, "_pending_inbound_lock", _Lock(ctx))
        _install(SELF, "_pending_inbound_events", _Queue(ctx))
        NS["logger"] = types.SimpleNamespace(
            error=make_recorder(ctx, "log_error"), warning=make_recorder(ctx, "log_warning"))
    elif target in ("_download_feishu_image", "_download_feishu_message_resource"):
        _install(SELF, "_fetch_message_resource", rec_fetch(ctx))
        _install(SELF, "_read_binary_response", make_recorder(ctx, "read_binary"))
        _install(SELF, "_get_response_header", make_recorder(ctx, "get_header"))
        _install(SELF, "_guess_extension", make_recorder(ctx, "guess_file_ext"))
        _install(SELF, "_normalize_media_type", make_recorder(ctx, "norm_media"))
        _install(SELF, "_default_image_media_type", make_recorder(ctx, "default_img_media"))
        _install(SELF, "_guess_media_type_from_filename", make_recorder(ctx, "guess_media_name"))
        _install(SELF, "_guess_document_media_type", make_recorder(ctx, "guess_doc_media"))
        NS["cache_image_from_bytes_async"] = make_recorder(ctx, "cache_img_bytes")
        NS["cache_audio_from_bytes_async"] = make_recorder(ctx, "cache_audio_bytes")
        NS["cache_document_from_bytes_async"] = make_recorder(ctx, "cache_doc_bytes")
        NS["_IMAGE_EXTENSIONS"] = {".jpg", ".png"}
        NS["_AUDIO_EXTENSIONS"] = {".ogg", ".mp3"}
        NS["_DOCUMENT_MIME_TO_EXT"] = {}
    elif target == "_extract_message_content":
        _install(SELF, "_normalize", rec_norm_msg(ctx))
        _install(SELF, "_download_feishu_message_resources", make_recorder(ctx, "fetch_resources"))
        _install(SELF, "_resolve_normalized_message_type", make_recorder(ctx, "resolve_type"))
        _install(SELF, "_maybe_extract_text_document", make_recorder(ctx, "extract_doc"))
        NS["_strip_edge_self_mentions"] = make_recorder(ctx, "strip_mentions")
        NS["caption_is_command"] = make_recorder(ctx, "caption_is_command")
    elif target == "_enqueue_text_event":
        _install(SELF, "_text_batch_key", make_recorder(ctx, "text_key"))
        _install(SELF, "_pending_text_batches", _SeamMap(ctx, "batches_get", "batches_set", wrap=True))
        _install(SELF, "_text_batch_is_compatible", make_recorder(ctx, "batch_compatible"))
        _install(SELF, "_flush_text_batch_now", make_recorder(ctx, "flush_now"))
        _install(SELF, "_pending_text_batch_counts",
                 _SeamMap(ctx, "counts_get", "counts_set", drop_value=False))
        _install(SELF, "_schedule_text_batch_flush", make_recorder(ctx, "schedule_flush"))
    elif target == "_enqueue_media_event":
        _install(SELF, "_media_batch_key", make_recorder(ctx, "media_key"))
        _install(SELF, "_pending_media_batches", _SeamMap(ctx, "mbatches_get", "mbatches_set", wrap=True))
        _install(SELF, "_media_batch_is_compatible", make_recorder(ctx, "media_compatible"))
        _install(SELF, "_flush_media_batch_now", make_recorder(ctx, "mflush_now"))
        _install(SELF, "_schedule_media_batch_flush", make_recorder(ctx, "schedule_mflush"))
        _install(SELF, "_merge_caption", make_recorder(ctx, "merge_caption"))
    elif target == "_feishu_send_with_retry":
        _install(SELF, "_send_raw_message", rec_send_raw(ctx))
        _install(SELF, "_response_succeeded", make_recorder(ctx, "response_succeeded"))
        _install(SELF, "_domain_name", "feishu")
        NS["logger"] = types.SimpleNamespace(warning=make_recorder(ctx, "log_warn"))
        NS["asyncio"] = types.SimpleNamespace(sleep=make_recorder(ctx, "sleep"))
        NS["_FEISHU_REPLY_FALLBACK_CODES"] = frozenset({230011, 231003})
        NS["_POST_CONTENT_INVALID_RE"] = re.compile(
            r"content format of the post type is incorrect", re.IGNORECASE)
        NS["describe_proxy_error"] = lambda exc, proxy: "desc"
        NS["env_proxy_for_redaction"] = lambda url: "proxy"
        NS["_onboard_open_base_url"] = lambda domain: "url"
    elif target == "_download_remote_document":
        for _pkg in ("gateway", "gateway.platforms", "tools"):
            _m = types.ModuleType(_pkg); _m.__path__ = []; sys.modules[_pkg] = _m
        _base = types.ModuleType("gateway.platforms.base")
        _base.read_httpx_body_with_limit = make_recorder(ctx, "read_body")
        _base.ssrf_redirect_guard = lambda resp: None
        sys.modules["gateway.platforms.base"] = _base
        _us = types.ModuleType("tools.url_safety")
        _us.create_ssrf_safe_async_client = lambda **kw: _AClient(ctx)
        _us.is_safe_url = make_recorder(ctx, "is_safe")
        sys.modules["tools.url_safety"] = _us
        _install(SELF, "_derive_remote_filename", make_recorder(ctx, "derive_name"))
        NS["cache_document_from_bytes_async"] = make_recorder(ctx, "cache_doc")
    elif target == "_drain_pending_inbound_events":
        SELF = _DrainSelf(ctx)
        NS["logger"] = types.SimpleNamespace(
            warning=make_recorder(ctx, "log_warn"), info=make_recorder(ctx, "log_info"),
            error=make_recorder(ctx, "log_error"))
        NS["time"] = types.SimpleNamespace(sleep=make_recorder(ctx, "poll_sleep"))


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
    if func == "_build_lark_client":
        return NS[func](g("app_id", ""), g("app_secret", ""), g("sdk_domain"))
    if func == "_normalize":
        return NS[func](SELF, g("message_type"), g("raw_content"), g("mentions"))
    if func == "_webhook_reject":
        return NS[func](SELF, g("remote_ip", ""), g("anomaly", ""), g("status", 0),
                        text=g("text"), json_msg=g("json_msg"))
    if func == "_download_remote_image":
        return NS[func](SELF, g("image_url", ""))
    if func == "_apply_settings":
        s = g("settings") or {}
        settings = types.SimpleNamespace(**{f: s.get(f, None) for f in FA9_FIELDS})
        return NS[func](SELF, settings)
    if func == "_check_webhook_rate_limit":
        return NS[func](SELF, g("rate_key", ""))
    if func == "_download_feishu_message_resources":
        norm = g("normalized") or {}
        nd = types.SimpleNamespace(**{
            "image_keys": mat_event(norm.get("image_keys")),
            "media_refs": mat_event(norm.get("media_refs"))})
        return NS[func](SELF, message_id=g("message_id", ""), normalized=nd)
    if func == "_enqueue_pending_inbound_event":
        _install(SELF, "_pending_inbound_max_depth", g("max_depth", 0))
        _install(SELF, "_pending_drain_scheduled", g("pending_drain_scheduled", False))
        return NS[func](SELF, g("data"))
    if func == "_download_feishu_image":
        _install(SELF, "_client", "C" if g("client_present", False) else None)
        return NS[func](SELF, message_id=g("message_id", ""), image_key=g("image_key", ""))
    if func == "_download_feishu_message_resource":
        _install(SELF, "_client", "C" if g("client_present", False) else None)
        return NS[func](SELF, message_id=g("message_id", ""), file_key=g("file_key", ""),
                        resource_type=g("resource_type", ""), fallback_filename=g("fallback_filename", ""))
    if func == "_extract_message_content":
        return NS[func](SELF, mat_event(g("message") or {}))
    if func == "_enqueue_text_event":
        _install(SELF, "_text_batch_max_messages", g("max_messages", 0))
        _install(SELF, "_text_batch_max_chars", g("max_chars", 0))
        return NS[func](SELF, mat_event(g("event") or {}))
    if func == "_enqueue_media_event":
        return NS[func](SELF, mat_event(g("event") or {}))
    if func == "_feishu_send_with_retry":
        return NS[func](SELF, chat_id=g("chat_id", ""), msg_type=g("msg_type", ""),
                        payload=g("payload", ""), reply_to=g("reply_to"),
                        metadata=g("metadata"))
    if func == "_download_remote_document":
        return NS[func](SELF, g("file_url", ""),
                        default_ext=g("default_ext", ".bin"),
                        preferred_name=g("preferred_name", "file"))
    if func == "_drain_pending_inbound_events":
        _install(SELF, "_loop", g("loop"))
        return NS[func](SELF)
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
        print(f"FATAL oracle_feishu_adapter_flow9: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow9: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow9.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow9: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow9: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow9: fixture funcs not in TARGETS: {unknown_funcs}")
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
    if shown >= 15:
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
    print(f"ORACLE feishu_adapter_flow9: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow9.jsonl"
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
    print(f"SELFTEST feishu_adapter_flow9: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
