#!/usr/bin/env python3
"""Oracle for the Matrix-adapter remainder flow port
(src/gateway/platforms/matrix_adapter_flow8.c).

Drives the LIVE plugins/platforms/matrix/adapter.py UNCLAIMED helpers with
SCRIPTED seams: every module collaborator / self-sibling / in-function import
is a recording stub fed the SAME scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the state echo ("state").

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/CancelledError included). Scripted raises round-trip
their spec verbatim; unscripted live raises project {__raise__, message:str(exc)}.
Summary: 'ORACLE matrix_adapter_flow8: N cases, M mismatches'.
"""
import ast
import asyncio
import builtins
import copy
import html
import inspect as _real_inspect
import json
import os
import re
import sys
import textwrap
import types
import typing
from pathlib import Path, PurePosixPath
from urllib.parse import urljoin as _real_urljoin
from contextlib import suppress
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
A_SRC = os.path.join(Q, "plugins", "platforms", "matrix", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

# name -> kind ("free"/"instance" sync, "afree"/"ainstance" coroutine).
KINDS = {
    "_EA_HEADER": "instance",
    "_send_room_message": "ainstance",
    "_connect_authenticate": "ainstance",
    "_connect_setup_e2ee": "ainstance",
    "_download_external_media_with_cap": "ainstance",
    "_handle_media_message": "ainstance",
    "_schedule_reaction_redaction": "instance",
    "_send_exec_approval_prompt": "ainstance",
    "_send_picker": "ainstance",
    "_send_local_file": "ainstance",
    "_send_reaction_prompt": "ainstance",
    "_markdown_to_html_fallback": "free",
    "_standalone_payloads": "free",
    "_standalone_send": "afree",
    "_write_matrix_recovery_key_output_file": "free",
    "create_handoff_thread": "ainstance",
}

# EXACTLY the C ma8_call/ma8_log0 seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "_abort_connect", "_build_inbound_event", "_classify_inbound_media",
    "_download_and_cache_media", "_media_too_large", "_mxc_to_http",
    "_recovery_key_output_path", "_resolve_message_context", "_send_reaction",
    "_upload_and_send", "_voice_may_park", "ClientSession", "CryptoStateStore",
    "Database.create", "OlmMachine", "PgCryptoStore", "check_e2ee_deps",
    "client.login", "client.query_keys", "client.send_message_event",
    "client.whoami", "content_bytes", "create_task", "db.start",
    "emit_media_warning", "e2ee_setup_failed", "file.write", "get_secret",
    "guess_type", "handle_message", "http.get", "is_safe_url",
    "latex_to_tokens", "legacy_pickle.exists", "legacy_pickle.unlink",
    "logger.debug", "logger.error", "logger.info", "logger.warning",
    "make_prompt", "markdown", "mark_async", "migrate_legacy", "olm.load",
    "olm.share_keys", "os.close", "os.open", "parked_voices.begin",
    "parked_voices.release", "path.exists", "path.parent.mkdir",
    "path.read_bytes", "path.stat", "proxy_kwargs", "put", "raise_for_status",
    "read_body_capped", "reset_crypto_store", "resp.json", "resp.text",
    "sanitize_html", "send", "send_error", "send_reaction_prompt",
    "standalone_payloads", "store.open", "store.put_device_id",
    "store_dir.mkdir", "t", "time.monotonic", "truncate_message",
    "verify_cross_signing", "verify_device_keys", "voice_metadata",
]

# self.<name> seams bound as staticmethod on the self double.
SELF_SEAMS = [
    "_abort_connect", "reset_crypto_store", "verify_device_keys",
    "verify_cross_signing", "e2ee_setup_failed", "_classify_inbound_media",
    "_voice_may_park", "_resolve_message_context", "_download_and_cache_media",
    "_mxc_to_http", "_build_inbound_event", "handle_message", "send",
    "_send_reaction", "send_reaction_prompt", "_media_too_large",
    "_upload_and_send", "emit_media_warning",
]


# self.<LIVE attr> -> seam name, for names that differ from the seam channel.
SELF_BINDINGS = {
    "_send_reaction_prompt": "send_reaction_prompt",
    "_reset_crypto_store_if_device_changed": "reset_crypto_store",
    "_verify_device_keys_on_server": "verify_device_keys",
    "_verify_or_bootstrap_cross_signing": "verify_cross_signing",
    "_e2ee_setup_failed": "e2ee_setup_failed",
    "_migrate_legacy_crypto_pickle": "migrate_legacy",
}


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def extract():
    mod = new_module("ma8_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "os": os, "json": json, "re": re, "html": html, "suppress": suppress,
        "typing": typing, "Any": typing.Any, "Optional": typing.Optional,
        "Dict": typing.Dict, "Set": typing.Set, "List": typing.List,
        "Tuple": typing.Tuple, "sys": sys, "Path": PurePosixPath,
        "_html_escape": html.escape,
        "RoomID": lambda x: x,
        "EventType": SimpleNamespace(ROOM_MESSAGE="m.room.message"),
        "UserID": lambda x: x,
        "TrustState": SimpleNamespace(UNVERIFIED="unverified"),
        "_E2EE_INSTALL_HINT": "pip install mautrix",
        "_MAX_CONTENT_BYTES": 4096,
        "MATRIX_MAX_MESSAGE_LENGTH_CEILING": 4096,
    })

    def SendResult(**kw):
        return SimpleNamespace(**kw)
    ns["SendResult"] = SendResult

    class MatrixMediaTooLarge(Exception):
        pass
    ns["MatrixMediaTooLarge"] = MatrixMediaTooLarge

    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, A_SRC)
            except Exception:
                pass
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "MatrixAdapter")
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            n2 = copy.copy(node); n2.decorator_list = []
            exec_node(src, n2, ns, A_SRC); found.add(node.name)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            n2 = copy.copy(node); n2.decorator_list = []
            exec_node(src, n2, ns, A_SRC); found.add(node.name)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_matrix_adapter_flow8: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()
# Preserve the extracted live functions: some seam names collide with module
# globals that other ported defs call (notably _standalone_payloads).
REAL_FUNCS = dict(NS)


def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (bytes, bytearray)):
            return list(x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, PurePosixPath):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def proj(v):
    if isinstance(v, types.SimpleNamespace):
        return {"__attr__": {k: proj(vv) for k, vv in vars(v).items()}}
    if isinstance(v, dict):
        return {k: proj(vv) for k, vv in v.items()}
    if isinstance(v, (list, tuple)):
        return [proj(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(proj(x) for x in v)
    if isinstance(v, (bytes, bytearray)):
        return list(v)
    if isinstance(v, float) and v.is_integer():
        return int(v)
    if isinstance(v, BaseException):
        return str(v)
    return v


def attr_double(raw):
    if not isinstance(raw, dict):
        return raw
    inner = raw.get("__attr__", raw)
    ns = SimpleNamespace()
    for k, v in inner.items():
        setattr(ns, k, v)
    return ns


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


_MISSING = object()
_CUR = None
_TASK_ARGS = {}


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _record_raise(self, name, args, exc, spec=None):
        if spec is None:
            spec = proj_exc(exc)
        self.scripted.append((exc, spec))
        self.trace.append({"name": name, "args": args, "ret": spec})
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
                sp = used["raise"]
                spec = {"__raise__": sp.get("type", "RuntimeError"),
                        "message": sp.get("message", "")}
                self._record_raise(name, args, make_exc(sp), spec)
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret

    def spec_for(self, exc):
        for e, spec in self.scripted:
            if e is exc:
                return spec
        return None


def rec(name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def arec(name, args_fn=None, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def build_seams():
    st = {}
    st["t"] = rec("t", lambda key: {"key": key})
    st["client.send_message_event"] = arec("client.send_message_event",
        lambda room_id, event_type, msg_content: {"room_id": proj(room_id),
            "event_type": proj(event_type), "msg_content": proj(msg_content)})
    st["client.whoami"] = arec("client.whoami", lambda: {}, build=attr_double)
    st["logger.debug"] = rec("logger.debug", lambda *a, **k: {})
    st["logger.error"] = rec("logger.error", lambda *a, **k: {})
    st["logger.info"] = rec("logger.info", lambda *a, **k: {})
    st["logger.warning"] = rec("logger.warning", lambda *a, **k: {})
    st["_abort_connect"] = arec("_abort_connect", lambda *a, **k: {})
    st["client.query_keys"] = arec("client.query_keys",
        lambda q: {"mxid": str(next(iter(q))) if q else ""}, build=attr_double)
    st["client.login"] = arec("client.login",
        lambda identifier, password, device_name, device_id: {
            "identifier": proj(identifier), "password": proj(password),
            "device_name": device_name, "device_id": proj(device_id)}, build=attr_double)
    st["check_e2ee_deps"] = rec("check_e2ee_deps", lambda: {})
    st["store_dir.mkdir"] = rec("store_dir.mkdir",
        lambda parents=False, exist_ok=False: {"parents": parents, "exist_ok": exist_ok})
    st["legacy_pickle.exists"] = rec("legacy_pickle.exists", lambda: {})
    st["legacy_pickle.unlink"] = rec("legacy_pickle.unlink", lambda: {})
    st["Database.create"] = rec("Database.create",
        lambda url, upgrade_table=None: {"url": url},
        build=lambda raw: EntityDouble(raw, {"start": "db.start"}))
    st["db.start"] = arec("db.start", lambda: {})
    st["PgCryptoStore"] = rec("PgCryptoStore",
        lambda account_id, pickle_key, db: {"account_id": account_id,
            "pickle_key": pickle_key}, build=lambda raw: EntityDouble(raw, {
                "open": "store.open", "put_device_id": "store.put_device_id"}))
    st["store.open"] = arec("store.open", lambda: {})
    st["reset_crypto_store"] = arec("reset_crypto_store",
        lambda store, device_id: {"device_id": device_id})
    st["store.put_device_id"] = arec("store.put_device_id", lambda device_id: {"device_id": device_id})
    st["migrate_legacy"] = arec("migrate_legacy", lambda *a: {})
    st["CryptoStateStore"] = rec("CryptoStateStore",
        lambda state_store, joined_rooms, client: {"joined_rooms": proj(joined_rooms)}, build=attr_double)
    st["OlmMachine"] = rec("OlmMachine", lambda *a: {}, build=lambda raw: OlmDouble())
    st["olm.load"] = arec("olm.load", lambda: {})
    st["verify_device_keys"] = arec("verify_device_keys", lambda client, olm: {})
    st["olm.share_keys"] = arec("olm.share_keys", lambda: {})
    st["verify_cross_signing"] = arec("verify_cross_signing", lambda olm, client: {})
    st["e2ee_setup_failed"] = arec("e2ee_setup_failed",
        lambda phase, exc, api: {"phase": phase})
    st["is_safe_url"] = rec("is_safe_url", lambda url: {"url": url})
    st["proxy_kwargs"] = rec("proxy_kwargs", lambda proxy_url: {"proxy_url": proj(proxy_url)})
    st["ClientSession"] = rec("ClientSession", lambda *a, **k: {}, build=lambda raw: SessionDouble(st))
    st["http.get"] = rec("http.get",
        lambda url, timeout=None, allow_redirects=False, **kw: {
            "url": url, "allow_redirects": allow_redirects},
        build=lambda raw: AsyncCM(RespDouble(raw)))
    st["raise_for_status"] = rec("raise_for_status", lambda: {})
    st["read_body_capped"] = arec("read_body_capped",
        lambda headers, chunks, max_bytes: {"max_bytes": max_bytes})
    st["_classify_inbound_media"] = rec("_classify_inbound_media",
        lambda msgtype, mimetype, content: {"msgtype": msgtype, "mimetype": mimetype},
        build=lambda raw: tuple(raw) if isinstance(raw, list) else raw)
    st["_voice_may_park"] = rec("_voice_may_park",
        lambda room_id, body, content, relates, mention: {
            "room_id": room_id, "body": body, "mention_claimed": mention})
    st["parked_voices.begin"] = rec("parked_voices.begin",
        lambda room_id, sender: {"room_id": room_id, "sender": sender})
    st["parked_voices.release"] = rec("parked_voices.release",
        lambda room_id, sender, gate: {"room_id": room_id, "sender": sender, "gate": proj(gate)})
    st["_resolve_message_context"] = arec("_resolve_message_context",
        lambda room_id, sender, event_id, body, source_content, relates_to,
        mention_claimed=False, voice_gate=None: {
            "room_id": room_id, "sender": sender, "event_id": event_id, "body": body,
            "mention_claimed": mention_claimed, "voice_gate": proj(voice_gate)},
        build=lambda raw: attr_double(raw) if isinstance(raw, dict) else None)
    st["_download_and_cache_media"] = arec("_download_and_cache_media",
        lambda url, event_id, file, msg_type, media_type, is_voice, body: {
            "url": url, "event_id": event_id, "msg_type": msg_type,
            "media_type": media_type, "is_voice": is_voice, "body": body})
    st["_mxc_to_http"] = rec("_mxc_to_http", lambda url: {"url": url})
    st["_build_inbound_event"] = arec("_build_inbound_event",
        lambda room_id, sender, event_id, body, source_content, relates_to, ctx=None,
        message_type=None, media_urls=None, media_types=None, media_msgtype=None: {
            "room_id": room_id, "sender": sender, "event_id": event_id, "body": body,
            "message_type": message_type, "media_msgtype": media_msgtype},
        build=lambda raw: attr_double(raw) if isinstance(raw, dict) else None)
    st["handle_message"] = arec("handle_message", lambda event: {"event": proj(event)})
    st["create_task"] = rec("create_task", lambda coro=None: dict(_TASK_ARGS),
                            build=lambda raw: TaskDouble())
    st["send"] = arec("send",
        lambda chat_id, text, metadata=None: {"chat_id": chat_id, "text": text,
            "metadata": proj(metadata)}, build=attr_double)
    st["make_prompt"] = rec("make_prompt",
        lambda message_id, requester, expires_at: {"message_id": message_id,
            "requester": requester, "expires_at": expires_at}, build=attr_double)
    st["time.monotonic"] = rec("time.monotonic", lambda: {})
    st["_send_reaction"] = arec("_send_reaction",
        lambda chat_id, message_id, emoji: {"chat_id": chat_id,
            "message_id": message_id, "emoji": emoji})
    st["send_reaction_prompt"] = arec("send_reaction_prompt",
        lambda chat_id, text, metadata, make_prompt, registry, emojis, label: {
            "chat_id": chat_id, "text": text, "emojis": proj(list(emojis)), "label": label},
        build=attr_double)
    st["voice_metadata"] = rec("voice_metadata", lambda path: {"path": str(path)})
    st["path.exists"] = rec("path.exists", lambda: {})
    st["path.stat"] = rec("path.stat", lambda: {}, build=attr_double)
    st["path.read_bytes"] = rec("path.read_bytes", lambda: {})
    st["_media_too_large"] = rec("_media_too_large", lambda size: {"size": size}, build=attr_double)
    st["guess_type"] = rec("guess_type", lambda path: {"path": path},
        build=lambda raw: tuple(raw) if isinstance(raw, list) else raw)
    st["_upload_and_send"] = arec("_upload_and_send",
        lambda room_id, data, fname, mime, msgtype, caption, reply_to, metadata,
        is_voice, voice_metadata: {
            "room_id": room_id, "fname": fname, "mime": mime, "msgtype": msgtype,
            "is_voice": is_voice}, build=attr_double)
    st["emit_media_warning"] = arec("emit_media_warning",
        lambda room_id, text, caption=None, reply_to=None, metadata=None: {
            "room_id": room_id, "text": text}, build=attr_double)
    st["mark_async"] = arec("mark_async", lambda thread_id: {"thread_id": proj(thread_id)})
    st["_recovery_key_output_path"] = rec("_recovery_key_output_path", lambda: {},
        build=lambda raw: FlowPath(raw) if isinstance(raw, str) else None)
    st["path.parent.mkdir"] = rec("path.parent.mkdir",
        lambda parents=False, exist_ok=False: {"parents": parents, "exist_ok": exist_ok})
    st["os.open"] = rec("os.open",
        lambda path, flags, mode: {"path": str(path), "flags": flags, "mode": mode})
    st["file.write"] = rec("file.write", lambda data: {"data": data})
    st["os.close"] = rec("os.close", lambda fd: {"fd": fd})
    st["send_error"] = rec("send_error", lambda message: {"message": message})
    st["get_secret"] = rec("get_secret", lambda key, default="": {"key": key, "default": default})
    st["standalone_payloads"] = rec("standalone_payloads", lambda message: {"message": message})
    st["put"] = rec("put",
        lambda url, headers=None, json=None: {"payload": proj(json)},
        build=lambda raw: AsyncCM(RespDouble(raw, put=True)))
    st["resp.text"] = arec("resp.text", lambda: {})
    st["resp.json"] = arec("resp.json", lambda: {})
    st["truncate_message"] = rec("truncate_message",
        lambda message, limit, len_fn: {"message": message, "limit": limit})
    st["markdown"] = rec("markdown",
        lambda text, extensions=None: {"text": text, "extensions": proj(extensions)})
    st["latex_to_tokens"] = rec("latex_to_tokens", lambda text: {"text": text})
    st["sanitize_html"] = rec("sanitize_html",
        lambda html_fragment, tex_store: {"html": html_fragment})
    st["content_bytes"] = rec("content_bytes", lambda payload: {"payload": proj(payload)})
    return st


class FlowPath(PurePosixPath):
    def expanduser(self):
        return self

    def exists(self):
        return _CUR.call("path.exists", {})

    def stat(self):
        return attr_double(_CUR.call("path.stat", {}))

    def read_bytes(self):
        return _CUR.call("path.read_bytes", {})

    @property
    def parent(self):
        return _PathParent()


class _PathParent:
    def mkdir(self, parents=False, exist_ok=False):
        return _CUR.call("path.parent.mkdir", {"parents": parents, "exist_ok": exist_ok})


class FileDouble:
    def __init__(self, fd):
        self.fd = fd

    def write(self, data):
        return _CUR.call("file.write", {"data": data})

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class _Headers(dict):
    def get(self, key, default=None):
        return super().get(key, default)


class RespDouble:
    def __init__(self, spec, put=False):
        inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
        self.status = inner.get("status", 200)
        self._headers = _Headers()
        if inner.get("location") is not None:
            self._headers["Location"] = inner["location"]
        for k, v in (inner.get("headers") or {}).items():
            self._headers[k] = v
        self.headers = self._headers
        self.content_type = inner.get("content_type", "")
        self.content = SimpleNamespace(iter_chunked=lambda n: [])

    def raise_for_status(self):
        return _CUR.call("raise_for_status", {})

    def headers_get(self, key, default=None):
        return self._headers.get(key, default)

    async def text(self):
        return _CUR.call("resp.text", {})

    async def json(self):
        return _CUR.call("resp.json", {})


class AsyncCM:
    def __init__(self, value):
        self.value = value

    async def __aenter__(self):
        return self.value

    async def __aexit__(self, *a):
        return False


class TaskDouble:
    def add_done_callback(self, fn):
        return None


class EntityDouble(SimpleNamespace):
    def __init__(self, spec, methods):
        inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
        super().__init__(**inner)
        for meth, seam in methods.items():
            setattr(self, meth, arec(seam, lambda *a, **k: {}))


class OlmDouble(SimpleNamespace):
    def __init__(self):
        super().__init__()
        self.share_keys_min_trust = ""
        self.send_keys_min_trust = ""

    async def load(self):
        return _CUR.call("olm.load", {})

    async def share_keys(self):
        return _CUR.call("olm.share_keys", {})


class SessionDouble:
    def __init__(self, stubs):
        self._stubs = stubs

    def get(self, url, timeout=None, allow_redirects=False, **kw):
        return self._stubs["http.get"](url, timeout=timeout,
                                       allow_redirects=allow_redirects, **kw)

    def put(self, url, headers=None, json=None):
        return self._stubs["put"](url, headers=headers, json=json)

    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False


class StoreDir(PurePosixPath):
    def mkdir(self, parents=False, exist_ok=False):
        return _CUR.call("store_dir.mkdir", {"parents": parents, "exist_ok": exist_ok})

    def __truediv__(self, other):
        return LegacyPickle(str(self) + "/" + str(other))


class LegacyPickle:
    def __init__(self, path):
        self.path = path

    def exists(self):
        return _CUR.call("legacy_pickle.exists", {})

    def unlink(self):
        return _CUR.call("legacy_pickle.unlink", {})


def make_obj(spec, methods):
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    D = type("ObjDouble", (SimpleNamespace,), {k: staticmethod(v) for k, v in methods.items()})
    o = D()
    for k, v in inner.items():
        if k not in methods:
            setattr(o, k, v)
    return o


def stubmod(fullname, attrs):
    parts = fullname.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            m = types.ModuleType(pn); m.__path__ = []; sys.modules[pn] = m
    m = types.ModuleType(fullname)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[fullname] = m
    return m


def install_module_seams(stubs, inp, args):
    for n in ("t", "_recovery_key_output_path", "standalone_payloads",
              "send_error", "get_secret"):
        NS[n] = stubs[n]
    NS["logger"] = SimpleNamespace(
        debug=stubs["logger.debug"], error=stubs["logger.error"],
        info=stubs["logger.info"], warning=stubs["logger.warning"])
    NS["time"] = SimpleNamespace(monotonic=stubs["time.monotonic"], time=lambda: 1000.0)
    NS["_check_e2ee_deps"] = stubs["check_e2ee_deps"]
    NS["proxy_kwargs_for_aiohttp"] = stubs["proxy_kwargs"]
    NS["read_body_capped"] = stubs["read_body_capped"]
    NS["urljoin"] = _real_urljoin
    NS["_utf8_len"] = lambda s: len(s.encode("utf-8"))
    NS["Path"] = FlowPath
    NS["_latex_to_tokens"] = stubs["latex_to_tokens"]
    NS["_sanitize_matrix_html"] = stubs["sanitize_html"]
    NS["_content_bytes"] = stubs["content_bytes"]
    stubmod("markdown", {"markdown": stubs["markdown"]})

    def create_task_rec(coro):
        try:
            coro.close()
        except Exception:
            pass
        return stubs["create_task"]()
    NS["asyncio"] = SimpleNamespace(create_task=create_task_rec)

    async def wait_for(coro, timeout=None):
        return await coro

    async def to_thread(fn, path):
        return stubs["voice_metadata"](str(path))
    NS["asyncio"].wait_for = wait_for
    NS["asyncio"].to_thread = to_thread
    NS["_matrix_voice_metadata_for_file"] = lambda p: None

    NS["_recovery_key_output_path"] = stubs["_recovery_key_output_path"]
    NS["os"] = SimpleNamespace(
        open=stubs["os.open"],
        fdopen=lambda fd, *a, **k: FileDouble(fd),
        close=stubs["os.close"],
        urandom=lambda n: b"\x01\x02\x03\x04"[:n],
        O_WRONLY=1, O_CREAT=2, O_EXCL=4)
    NS["mimetypes"] = SimpleNamespace(guess_type=stubs["guess_type"])
    NS["BasePlatformAdapter"] = SimpleNamespace(truncate_message=stubs["truncate_message"])
    NS["_MAX_CONTENT_BYTES"] = 4096
    NS["MATRIX_MAX_MESSAGE_LENGTH_CEILING"] = 4096
    NS["MatrixAdapter"] = SimpleNamespace(_sanitize_link_url=lambda u: u.strip().replace('"', "&quot;"))

    # _standalone_send helpers.
    # NS["_standalone_payloads"] is chosen per-drive (it is both a seam and a
    # ported live def -- see drive()).
    NS["send_error"] = stubs["send_error"]
    NS["get_secret"] = stubs["get_secret"]
    NS["describe_proxy_error"] = lambda exc, proxy: str(exc) or ""
    NS["matrix_redaction_proxy"] = lambda a, b: "<redacted>"

    class _SendFile:
        def __init__(self, fd):
            self.fd = fd
    NS["_SendFile"] = _SendFile

    # _handle_media_message helpers.
    NS["_media_too_large"] = stubs["_media_too_large"]
    NS["MatrixMediaTooLarge"] = NS.get("MatrixMediaTooLarge", type("MatrixMediaTooLarge", (Exception,), {}))

    # In-function imports.
    stubmod("tools", {})
    stubmod("tools.url_safety", {
        "is_safe_url": stubs["is_safe_url"],
        "create_ssrf_safe_async_client": lambda **kw: None,
        "ssrf_redirect_guard": lambda *a: None})
    stubmod("pm", {})
    stubmod("pm.extras", {})
    stubmod("urllib", {})
    stubmod("urllib.parse", {"quote": lambda s, safe="": s})

    aiohttp_mod = stubmod("aiohttp", {})
    class ClientTimeout:
        def __init__(self, *a, **k):
            pass
    aiohttp_mod.ClientTimeout = ClientTimeout
    aiohttp_mod.ClientSession = lambda *a, **k: stubs["ClientSession"]()

    # E2EE in-function import chain.
    mautrix = stubmod("mautrix", {})
    stubmod("mautrix.types", {"TrustState": SimpleNamespace(UNVERIFIED="unverified")})
    stubmod("mautrix.crypto", {"OlmMachine": lambda *a, **k: stubs["OlmMachine"]()})
    stubmod("mautrix.crypto.store", {})
    class PgCryptoStoreCls:
        upgrade_table = object()
        def __new__(cls, account_id, pickle_key, db):
            return stubs["PgCryptoStore"](account_id, pickle_key, db)
    stubmod("mautrix.crypto.store.asyncpg", {"PgCryptoStore": PgCryptoStoreCls})
    class DatabaseCls:
        @staticmethod
        def create(url, upgrade_table=None):
            return stubs["Database.create"](url, upgrade_table)
    stubmod("mautrix.util", {})
    stubmod("mautrix.util.async_db", {"Database": DatabaseCls})
    stubmod("plugins", {})
    stubmod("plugins.platforms", {})
    stubmod("plugins.platforms.matrix", {})
    stubmod("plugins.platforms.matrix.adapter_crypto",
            {"_CryptoStateStore": lambda *a: stubs["CryptoStateStore"](*a)})
    NS["_E2EE_INSTALL_HINT"] = "pip install mautrix"


def make_client(spec, stubs):
    D = type("MatrixClient", (SimpleNamespace,), {
        "send_message_event": staticmethod(stubs["client.send_message_event"]),
        "whoami": staticmethod(stubs["client.whoami"]),
        "query_keys": staticmethod(stubs["client.query_keys"]),
        "login": staticmethod(stubs["client.login"])})
    o = D()
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    o.mxid = inner.get("mxid", "@me:s")
    o.device_id = inner.get("device_id", "")
    for k, v in inner.items():
        if k in ("mxid", "device_id"):
            continue
        setattr(o, k, v)
    return o


def make_self(state, stubs):
    attrs = {n: staticmethod(stubs[n]) for n in SELF_SEAMS if n in stubs}
    for attr, seam in SELF_BINDINGS.items():
        attrs[attr] = staticmethod(stubs[seam])
    D = type("MatrixAdapter", (), attrs)
    inst = D()
    if not hasattr(inst, "name"):
        setattr(inst, "name", "matrix")
    for k, v in (state or {}).items():
        if k == "_client":
            setattr(inst, k, make_client(v, stubs) if v is not None else None)
        elif k == "_parked_voices":
            setattr(inst, k, make_obj(v, {
                "begin": stubs["parked_voices.begin"],
                "release": stubs["parked_voices.release"]}) if v is not None else None)
        elif k == "_threads":
            setattr(inst, k, make_obj(v, {"mark_async": stubs["mark_async"]}) if v is not None else None)
        else:
            setattr(inst, k, v)
    return inst


def maybe_run(x):
    if _real_inspect.isawaitable(x):
        return asyncio.run(x)
    return x


def _trace_ret(ctx, name):
    for ev in reversed(ctx.trace):
        if ev.get("name") == name:
            return ev.get("ret")
    return None


def drive(func, inp):
    global _CUR, _TASK_ARGS
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    stubs = build_seams()
    install_module_seams(stubs, inp, args)
    NS["_standalone_payloads"] = (stubs["standalone_payloads"]
        if func == "_standalone_send" else REAL_FUNCS["_standalone_payloads"])
    inst = make_self(state, stubs)
    if func == "_schedule_reaction_redaction":
        inst._reaction_redaction_tasks = set()
    if func == "_send_reaction_prompt":
        inst._approval_timeout_seconds = 0
    if func == "_send_exec_approval_prompt":
        inst._approval_prompts_by_event = {}
    if func == "create_handoff_thread" and not hasattr(inst, "_threads"):
        inst._threads = make_obj({}, {"mark_async": stubs["mark_async"]})
    g = args.get
    try:
        if func == "_EA_HEADER":
            ret = NS[func](inst)
        elif func == "_send_room_message":
            client = make_client(g("client", {"__attr__": {}}), stubs)
            inst._client = client
            ret = maybe_run(NS[func](inst, g("chat_id", ""), g("msg_content", {})))
        elif func == "_connect_authenticate":
            client = make_client(g("client", {"__attr__": {}}), stubs)
            api = attr_double(g("api", {"__attr__": {}}))
            ret = maybe_run(NS[func](inst, client, api))
        elif func == "_connect_setup_e2ee":
            client = make_client(g("client", {"__attr__": {}}), stubs)
            api = attr_double(g("api", {"__attr__": {}}))
            ss = attr_double(g("state_store", {"__attr__": {}}))
            inst._store_dir = StoreDir("/store")
            ret = maybe_run(NS[func](inst, client, api, ss))
        elif func == "_download_external_media_with_cap":
            ret = maybe_run(NS[func](inst, g("url", "")))
        elif func == "_handle_media_message":
            ret = maybe_run(NS[func](inst, g("room_id", ""), g("sender", ""),
                g("event_id", ""), g("event_ts", 0.0), g("source_content", {}),
                g("relates_to", {}), g("msgtype", ""), g("mention_claimed", False)))
        elif func == "_schedule_reaction_redaction":
            _TASK_ARGS = {"room_id": g("room_id", ""), "reaction_event_id": g("reaction_event_id", ""),
                          "reason": g("reason", ""),
                          "delay": (state or {}).get("_reaction_redaction_delay_seconds", 0)}
            ret = NS[func](inst, g("room_id", ""), g("reaction_event_id", ""), g("reason", ""))
        elif func == "_send_exec_approval_prompt":
            prompt = attr_double(g("prompt", {"__attr__": {}}))
            ret = maybe_run(NS[func](inst, prompt))
        elif func == "_send_picker":
            ret = maybe_run(NS[func](inst, g("chat_id", ""), list(g("lines", [])),
                g("choices", {}), g("session_key", ""), g("on_selected"), g("metadata"),
                g("registry", {}), g("label", "")))
        elif func == "_send_local_file":
            ret = maybe_run(NS[func](inst, g("room_id", ""), g("file_path", ""),
                g("msgtype", ""), g("caption"), g("reply_to"), g("file_name"),
                g("metadata"), g("is_voice", False)))
        elif func == "_send_reaction_prompt":
            mk = stubs["make_prompt"]
            ret = maybe_run(NS[func](inst, g("chat_id", ""), g("text", ""), g("metadata"),
                mk, g("registry", {}), list(g("emojis", [])), g("label", "")))
        elif func == "_markdown_to_html_fallback":
            ret = NS[func](g("text", ""))
        elif func == "_standalone_payloads":
            ret = NS[func](g("message", ""))
        elif func == "_standalone_send":
            pconfig = attr_double(g("pconfig", {"__attr__": {}}))
            ret = maybe_run(NS[func](pconfig, g("chat_id", ""), g("message", "")))
        elif func == "_write_matrix_recovery_key_output_file":
            ret = NS[func](g("recovery_key", ""))
        elif func == "create_handoff_thread":
            ret = maybe_run(NS[func](inst, g("parent_chat_id", ""), g("name", "")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        spec = ctx.spec_for(exc)
        ret = spec if spec is not None else proj_exc(exc)

    st_out = copy.deepcopy(state)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(st_out)}


# ── seam-name set cross-check (C / oracle / fixtures / PoP) ──
def _c_src():
    return (_REPO / "src" / "gateway" / "platforms" / "matrix_adapter_flow8.c").read_text(encoding="utf-8")


def cross_check():
    registry = set(SEAM_NAMES)
    csrc = _c_src()
    cseams = set(re.findall(r'ma8_(?:call|log0?)\s*\(\s*ctx\s*,\s*"([^"]+)"', csrc))
    if cseams != registry:
        print(f"FATAL oracle_matrix_adapter_flow8: seam set drift C vs registry: "
              f"extra={sorted(cseams - registry)} missing={sorted(registry - cseams)}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\brec\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\barec\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\bst\["([^"]+)"\]\s*=\s*(?:a?rec)', osrc))
    if used != registry:
        print(f"FATAL oracle_matrix_adapter_flow8: seam set drift oracle vs registry: "
              f"extra={sorted(used - registry)} missing={sorted(registry - used)}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ma8_\w+\s*@\s*plugins/platforms/matrix/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_matrix_adapter_flow8: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow8.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        if fix_keys != registry:
            print(f"FATAL oracle_matrix_adapter_flow8: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(registry - fix_keys)} extra={sorted(fix_keys - registry)}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_matrix_adapter_flow8: fixture func set != KINDS: "
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
    if shown >= 60:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")


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
    print(f"ORACLE matrix_adapter_flow8: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow8.jsonl"
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
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if not case.get("fc") and isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in list(KINDS) + SELF_SEAMS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING {func} case {n}: {msg}")
    print(f"SELFTEST matrix_adapter_flow8: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    low = [f for f, c in counts.items() if c < 6]
    if low:
        print("WARNING funcs with <6 cases:", low)
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
