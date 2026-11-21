#!/usr/bin/env python3
"""Oracle for the Matrix-adapter connect / sync / inbound / media / markdown /
crypto flow port (src/gateway/platforms/matrix_adapter_flow6.c).

Drives the LIVE plugins/platforms/matrix/adapter.py UNCLAIMED helpers with
SCRIPTED seams: every module collaborator / self-sibling / in-function import
is a recording stub fed the SAME scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the state echo ("state").

The live defs are AST-extracted from the adapter module body and the
MatrixAdapter class body (decorators excluded by lineno, segments dedented)
and called with plain Python args / a SimpleNamespace self double. The heavy
quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/CancelledError included). Scripted raises round-trip
their spec verbatim; unscripted live raises project {__raise__, message:str(exc)}.
Summary: 'ORACLE matrix_adapter_flow6: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; integral floats normalize to int (canon).
- Attribute carriers are {"__attr__": {...}}; doubles project back on seam args.
- Set-valued state emits as sorted arrays. logger.* calls are seams.
"""
import ast
import asyncio
import builtins
import copy
import inspect as _real_inspect
import json
import os
import re
import sys
import textwrap
import types
import typing
from pathlib import Path, PurePosixPath
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
    "_crypto_db_path": "instance",
    "_abort_connect": "afree",
    "_e2ee_setup_failed": "ainstance",
    "_connect_initial_sync": "ainstance",
    "_extract_reply_context": "ainstance",
    "_check_e2ee_deps": "free",
    "_create_matrix_session": "free",
    "_dispatch_sync": "ainstance",
    "_build_text_message_content": "instance",
    "_absorb_sync": "ainstance",
    "_download_and_cache_media": "ainstance",
    "_background_read_receipt": "instance",
    "_markdown_to_html": "instance",
    "_migrate_legacy_crypto_pickle": "ainstance",
    "_build_inbound_event": "ainstance",
    "_handle_text_message": "ainstance",
}

# EXACTLY the C ma6_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "_get_hermes_dir", "crypto_db.stop", "session.close", "logger.warning",
    "logger.error", "_abort_connect", "sync", "_absorb_sync",
    "matrix_redaction_proxy", "describe_proxy_error", "_extract_reply_fallback",
    "_strip_reply_fallback", "_get_display_name", "gateway_trust_env",
    "ClientSession", "ProxyConnector.from_url", "safe_proxy_url_for_log",
    "handle_sync", "gather", "_extract_outbound_mentions",
    "_has_outbound_room_mention", "_inject_outbound_mention_links",
    "_markdown_to_html", "_content_bytes", "time.time",
    "_invalidate_room_identities", "_warn_encrypted_drops", "put_next_batch",
    "logger.info", "_refresh_dm_cache", "_dispatch_sync",
    "_schedule_pending_invite_joins", "download_mxc_capped",
    "decrypt_attachment", "cache_image", "cache_audio", "cache_document",
    "ensure_future", "_pre_sanitize_matrix_markdown", "_latex_to_tokens",
    "Markdown", "md.preprocessors.deregister", "md.convert", "md.reset",
    "_sanitize_matrix_html", "_markdown_to_html_fallback", "get_account",
    "PgCryptoStore.get_account", "_repickle_crypto_sessions", "put_account",
    "_resolve_message_context", "_extract_reply_context",
    "_normalize_matrix_bang_command", "_is_bare_media_filename", "MessageEvent",
    "pending", "_strip_mention", "_content_mentions_bot", "mark", "settle",
    "claim", "_handle_media_message", "_background_read_receipt",
    "_build_inbound_event", "_enqueue_text_event", "handle_message",
]

# self.<name> seams bound as staticmethod on the self double.
SELF_SEAMS = [
    "_get_display_name", "_absorb_sync", "_abort_connect",
    "_extract_outbound_mentions", "_has_outbound_room_mention",
    "_inject_outbound_mention_links", "_markdown_to_html",
    "_invalidate_room_identities", "_warn_encrypted_drops", "_refresh_dm_cache",
    "_dispatch_sync", "_schedule_pending_invite_joins",
    "_resolve_message_context", "_extract_reply_context",
    "_strip_mention", "_content_mentions_bot", "_repickle_crypto_sessions",
    "_handle_media_message", "_background_read_receipt", "_build_inbound_event",
    "_enqueue_text_event", "handle_message", "_markdown_to_html_fallback",
]


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
    mod = new_module("ma6_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "os": os, "json": json, "re": re, "suppress": suppress,
        "typing": typing, "Any": typing.Any, "Optional": typing.Optional,
        "Dict": typing.Dict, "Set": typing.Set, "List": typing.List,
        "Tuple": typing.Tuple,
        "MessageType": SimpleNamespace(
            TEXT="text", LOCATION="location", PHOTO="photo", VIDEO="video",
            AUDIO="audio", VOICE="voice", DOCUMENT="document", STICKER="sticker",
            COMMAND="command"),
        "Path": PurePosixPath,
        "_MAX_CONTENT_BYTES": 45000,
        "_E2EE_INSTALL_HINT": "Install with: pip install 'mautrix[encryption]' asyncpg aiosqlite  (requires libolm C library)",
    })
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
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "MatrixAdapter")
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
        print(f"FATAL oracle_matrix_adapter_flow6: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()


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
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
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
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
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


def _logger_args(msg, *a):
    return {"msg": msg, "args": [proj(str(x) if isinstance(x, BaseException) else x) for x in a]}


def build_seams():
    st = {}
    st["_get_hermes_dir"] = rec("_get_hermes_dir",
        lambda legacy_path, new_path: {"legacy_path": legacy_path, "new_path": new_path},
        build=lambda raw: PurePosixPath(raw) if isinstance(raw, str) else raw)
    st["crypto_db.stop"] = arec("crypto_db.stop", lambda: {})
    st["session.close"] = arec("session.close", lambda: {})
    st["logger.warning"] = rec("logger.warning", _logger_args)
    st["logger.error"] = rec("logger.error", _logger_args)
    st["logger.info"] = rec("logger.info", _logger_args)
    st["_abort_connect"] = arec("_abort_connect", lambda api: {"api": proj(api)})
    st["sync"] = arec("sync", lambda timeout, full_state: {"timeout": timeout, "full_state": full_state})
    st["_absorb_sync"] = arec("_absorb_sync",
        lambda client, sync_data, initial: {"client": proj(client), "sync_data": proj(sync_data), "initial": initial})
    st["matrix_redaction_proxy"] = rec("matrix_redaction_proxy",
        lambda proxy_url, homeserver: {"proxy_url": proxy_url, "homeserver": homeserver})
    st["describe_proxy_error"] = rec("describe_proxy_error",
        lambda exc, proxy: {"exc": str(exc) if isinstance(exc, BaseException) else proj(exc), "proxy": proj(proxy)})
    st["_extract_reply_fallback"] = rec("_extract_reply_fallback", lambda body: {"body": body})
    st["_strip_reply_fallback"] = rec("_strip_reply_fallback", lambda body: {"body": body})
    st["_get_display_name"] = arec("_get_display_name",
        lambda room_id, user_id: {"room_id": room_id, "user_id": user_id})
    st["gateway_trust_env"] = rec("gateway_trust_env", lambda: {})
    st["ClientSession"] = rec("ClientSession", lambda **kw: {k: proj(v) for k, v in kw.items()})
    st["ProxyConnector.from_url"] = rec("ProxyConnector.from_url",
        lambda url, rdns=False: {"url": url, "rdns": rdns})
    st["safe_proxy_url_for_log"] = rec("safe_proxy_url_for_log", lambda url: {"url": url})
    st["handle_sync"] = rec("handle_sync", lambda sync_data: {"sync_data": proj(sync_data)})
    st["gather"] = arec("gather", lambda *t, **kw: {"tasks": [proj(x) for x in t]},
        build=lambda raw: [make_exc(x) if isinstance(x, dict) and x.get("__raise__") else x for x in raw] if isinstance(raw, list) else raw)
    st["_extract_outbound_mentions"] = rec("_extract_outbound_mentions", lambda text: {"text": text})
    st["_has_outbound_room_mention"] = rec("_has_outbound_room_mention", lambda text: {"text": text})
    st["_inject_outbound_mention_links"] = rec("_inject_outbound_mention_links", lambda text: {"text": text})
    st["_markdown_to_html"] = rec("_markdown_to_html", lambda text: {"text": text})
    st["_content_bytes"] = rec("_content_bytes", lambda content: {"content": proj(content)})
    st["time.time"] = rec("time.time", lambda: {})
    st["_invalidate_room_identities"] = rec("_invalidate_room_identities", lambda: {})
    st["_warn_encrypted_drops"] = rec("_warn_encrypted_drops",
        lambda rooms_join, client: {"rooms_join": proj(rooms_join), "client": proj(client)})
    st["put_next_batch"] = arec("put_next_batch", lambda next_batch: {"next_batch": next_batch})
    st["_refresh_dm_cache"] = arec("_refresh_dm_cache", lambda: {})
    st["_dispatch_sync"] = arec("_dispatch_sync", lambda sync_data: {"sync_data": proj(sync_data)})
    st["_schedule_pending_invite_joins"] = rec("_schedule_pending_invite_joins",
        lambda sync_data: {"sync_data": proj(sync_data)})
    st["download_mxc_capped"] = arec("download_mxc_capped",
        lambda client, url, max_bytes: {"url": url, "max_bytes": max_bytes})
    st["decrypt_attachment"] = rec("decrypt_attachment",
        lambda file_bytes, key, hash, iv: {"file_bytes": proj(file_bytes), "key": key, "hash": hash, "iv": iv})
    st["cache_image"] = arec("cache_image", lambda data, ext: {"file_bytes": proj(data), "ext": ext})
    st["cache_audio"] = arec("cache_audio", lambda data, ext: {"file_bytes": proj(data), "ext": ext})
    st["cache_document"] = arec("cache_document", lambda data, filename: {"file_bytes": proj(data), "filename": filename})
    st["_pre_sanitize_matrix_markdown"] = rec("_pre_sanitize_matrix_markdown", lambda text: {"text": text})
    st["_latex_to_tokens"] = rec("_latex_to_tokens", lambda text: {"text": text})
    st["md.convert"] = rec("md.convert", lambda text: {"text": text})
    st["md.reset"] = rec("md.reset", lambda: {})
    st["md.preprocessors.deregister"] = rec("md.preprocessors.deregister", lambda *a: {})
    st["_sanitize_matrix_html"] = rec("_sanitize_matrix_html",
        lambda html, tex_store: {"html": html, "tex_store": proj(tex_store)})
    st["_markdown_to_html_fallback"] = rec("_markdown_to_html_fallback", lambda text: {"text": text})
    st["get_account"] = arec("get_account", lambda: {})
    st["_repickle_crypto_sessions"] = arec("_repickle_crypto_sessions",
        lambda db, acct_id, legacy_key, pickle_key: {"db": proj(db), "acct_id": acct_id,
            "legacy_key": legacy_key, "pickle_key": pickle_key})
    st["put_account"] = arec("put_account", lambda account: {"account": proj(account)})
    st["_resolve_message_context"] = arec("_resolve_message_context",
        lambda room_id, sender, event_id, body, source_content, relates_to: {
            "room_id": room_id, "sender": sender, "event_id": event_id, "body": body,
            "source_content": proj(source_content), "relates_to": proj(relates_to)})
    st["_extract_reply_context"] = arec("_extract_reply_context",
        lambda room_id, body, relates_to: {"room_id": room_id, "body": body, "relates_to": proj(relates_to)})
    st["_normalize_matrix_bang_command"] = rec("_normalize_matrix_bang_command", lambda body: {"body": body})
    st["_is_bare_media_filename"] = rec("_is_bare_media_filename",
        lambda media_msgtype, body: {"media_msgtype": media_msgtype, "body": body})
    st["MessageEvent"] = rec("MessageEvent", lambda **kw: {k: proj(v) for k, v in kw.items()})
    st["pending"] = rec("pending", lambda room_id, sender: {"room_id": room_id, "sender": sender})
    st["_strip_mention"] = rec("_strip_mention", lambda body: {"body": body})
    st["_content_mentions_bot"] = rec("_content_mentions_bot",
        lambda body, content: {"body": body, "content": proj(content)})
    st["mark"] = rec("mark", lambda: {})
    st["settle"] = arec("settle", lambda room_id, sender: {"room_id": room_id, "sender": sender})
    st["claim"] = rec("claim", lambda room_id, sender, before: {"room_id": room_id, "sender": sender, "before": before},
        build=lambda raw: list(raw) if isinstance(raw, list) else raw)
    st["_handle_media_message"] = arec("_handle_media_message",
        lambda room_id, sender, event_id, event_ts, source_content, relates_to, msgtype, mention_claimed=False: {
            "room_id": room_id, "sender": sender, "event_id": event_id, "event_ts": event_ts,
            "source_content": proj(source_content), "relates_to": proj(relates_to),
            "msgtype": msgtype, "mention_claimed": mention_claimed})
    st["_background_read_receipt"] = rec("_background_read_receipt",
        lambda room_id, event_id: {"room_id": room_id, "event_id": event_id})
    st["_build_inbound_event"] = arec("_build_inbound_event",
        lambda room_id, sender, event_id, body, source_content, relates_to, **kw: {
            "room_id": room_id, "sender": sender, "event_id": event_id, "body": body,
            "source_content": proj(source_content), "relates_to": proj(relates_to), **{k: proj(v) for k, v in kw.items()}},
        build=attr_double)
    st["_enqueue_text_event"] = rec("_enqueue_text_event", lambda event: {"event": proj(event)})
    st["handle_message"] = arec("handle_message", lambda event: {"event": proj(event)})
    return st


def ensure_future_rec(coro):
    try:
        coro.close()
    except Exception:
        pass
    return _CUR.call("ensure_future", {})


class PgCryptoStoreDouble:
    def __init__(self, account_id, pickle_key, db, stub):
        self._args = {"account_id": account_id, "pickle_key": pickle_key, "db": proj(db)}
        self._stub = stub

    async def get_account(self):
        return _CUR.call("PgCryptoStore.get_account", self._args)


def install_module_seams(stubs, inp):
    args = inp.get("args") or {}
    # module-level collaborators
    for n in ("_get_hermes_dir", "matrix_redaction_proxy", "describe_proxy_error",
              "_extract_reply_fallback", "_strip_reply_fallback", "gateway_trust_env",
              "safe_proxy_url_for_log", "_content_bytes", "_pre_sanitize_matrix_markdown",
              "_latex_to_tokens", "_sanitize_matrix_html", "_markdown_to_html_fallback",
              "_normalize_matrix_bang_command", "_is_bare_media_filename", "MessageEvent",
              "download_mxc_capped"):
        NS[n] = stubs[n]
    # logger
    NS["logger"] = SimpleNamespace(
        warning=stubs["logger.warning"], error=stubs["logger.error"], info=stubs["logger.info"])
    # asyncio / inspect / time (module-level imports)
    NS["asyncio"] = SimpleNamespace(gather=stubs["gather"], ensure_future=ensure_future_rec)
    NS["inspect"] = SimpleNamespace(isawaitable=lambda x: bool(args.get("tasks_awaitable", False)))
    NS["time"] = SimpleNamespace(time=stubs["time.time"])
    # in-function import stubs
    def stubmod(fullname, attrs, blocked=False):
        parts = fullname.split(".")
        for i in range(1, len(parts)):
            pn = ".".join(parts[:i])
            if pn not in sys.modules:
                m = types.ModuleType(pn); m.__path__ = []; sys.modules[pn] = m
        m = types.ModuleType(fullname)
        if not blocked:
            for k, v in attrs.items():
                setattr(m, k, v)
        sys.modules[fullname] = None if blocked else m
        return m
    stubmod("aiohttp", {"ClientSession": stubs["ClientSession"], "ClientTimeout": lambda **k: k})
    stubmod("aiohttp_socks", {"ProxyConnector": SimpleNamespace(from_url=stubs["ProxyConnector.from_url"])},
            blocked=not bool(args.get("socks_available", True)))
    stubmod("mautrix.crypto.attachments", {"decrypt_attachment": stubs["decrypt_attachment"]})
    stubmod("gateway.platforms.base", {
        "cache_image_from_bytes_async": stubs["cache_image"],
        "cache_audio_from_bytes_async": stubs["cache_audio"],
        "cache_document_from_bytes_async": stubs["cache_document"]})
    # markdown import gate
    md_blocked = not bool(args.get("markdown_available", True))
    def markdown_factory(extensions):
        _CUR.call("Markdown", {"extensions": proj(extensions)})
        has_hb = bool(args.get("html_block_present", False))
        class PP:
            def __contains__(self, k):
                return has_hb and k == "html_block"
            def deregister(self, k):
                return stubs["md.preprocessors.deregister"](k)
        class MD:
            def __init__(self):
                self.preprocessors = PP()
            def convert(self, text):
                return stubs["md.convert"](text)
            def reset(self):
                return stubs["md.reset"]()
        return MD()
    stubmod("markdown", {"Markdown": markdown_factory}, blocked=md_blocked)
    # mautrix PgCryptoStore (in-function import for _migrate)
    def pgs(**kw):
        return PgCryptoStoreDouble(kw.get("account_id"), kw.get("pickle_key"), kw.get("db"), stubs)
    stubmod("mautrix.crypto.store.asyncpg", {"PgCryptoStore": pgs})
    # E2EE deps import gate (for _check_e2ee_deps)
    blocked = set(args.get("blocked") or [])
    stubmod("mautrix.crypto", {"OlmMachine": object}, blocked="mautrix.crypto" in blocked)
    stubmod("asyncpg", {}, blocked="asyncpg" in blocked)
    stubmod("aiosqlite", {}, blocked="aiosqlite" in blocked)


def make_self(state, stubs):
    attrs = {n: staticmethod(stubs[n]) for n in SELF_SEAMS}
    D = type("MatrixAdapter", (), attrs)
    inst = D()
    for k, v in (state or {}).items():
        if k == "_parked_voices":
            setattr(inst, k, make_parked(v, stubs))
        elif k == "_client":
            setattr(inst, k, make_client(v, stubs) if v is not None else None)
        elif k == "_joined_rooms" and isinstance(v, list):
            setattr(inst, k, set(v))
        elif k == "_store_dir" and isinstance(v, str) and v:
            setattr(inst, k, PurePosixPath(v))
        else:
            setattr(inst, k, v)
    return inst


def make_parked(spec, stubs):
    spec = spec or {}
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    D = type("ParkedVoices", (), {
        "pending": staticmethod(stubs["pending"]), "mark": staticmethod(stubs["mark"]),
        "settle": staticmethod(stubs["settle"]), "claim": staticmethod(stubs["claim"])})
    o = D()
    for k, v in inner.items():
        if k not in ("pending", "mark", "settle", "claim"):
            setattr(o, k, v)
    return o


def make_client(spec, stubs):
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    D = type("MatrixClient", (SimpleNamespace,), {
        "sync": staticmethod(stubs["sync"]), "handle_sync": staticmethod(stubs["handle_sync"]),
        "sync_store": SimpleNamespace(put_next_batch=stubs["put_next_batch"])})
    c = D()
    for k, v in inner.items():
        if k not in ("sync", "handle_sync", "sync_store"):
            setattr(c, k, v)
    return c


def make_obj(spec, methods):
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    D = type("ObjDouble", (SimpleNamespace,), {k: staticmethod(v) for k, v in methods.items()})
    o = D()
    for k, v in inner.items():
        if k not in methods:
            setattr(o, k, v)
    return o


def maybe_run(x):
    if _real_inspect.isawaitable(x):
        return asyncio.run(x)
    return x


def drive(func, inp):
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    stubs = build_seams()
    install_module_seams(stubs, inp)
    inst = make_self(state, stubs)
    client = make_client(args.get("client"), stubs)
    api = make_obj(args.get("api"), {"session": make_obj(None, {"close": stubs["session.close"]})})
    crypto_db = make_obj(args.get("crypto_db"), {"stop": stubs["crypto_db.stop"]})
    store = make_obj(args.get("crypto_store"), {"get_account": stubs["get_account"], "put_account": stubs["put_account"]})
    g = args.get
    try:
        if func == "_crypto_db_path":
            ret = NS[func](inst)
        elif func == "_abort_connect":
            cdb = g("crypto_db")
            if cdb is not None:
                cdb = make_obj(cdb, {"stop": stubs["crypto_db.stop"]})
            ret = maybe_run(NS[func](api, cdb))
        elif func == "_e2ee_setup_failed":
            ret = maybe_run(NS[func](inst, g("what"), Exception(g("exc", "")), api))
        elif func == "_connect_initial_sync":
            ret = maybe_run(NS[func](inst, make_client(g("client"), stubs)))
        elif func == "_extract_reply_context":
            ret = maybe_run(NS[func](inst, g("room_id"), g("body"), g("relates_to")))
        elif func == "_check_e2ee_deps":
            ret = NS[func]()
        elif func == "_create_matrix_session":
            ret = NS[func](g("proxy_url"))
        elif func == "_dispatch_sync":
            ret = maybe_run(NS[func](inst, g("sync_data")))
        elif func == "_build_text_message_content":
            ret = NS[func](inst, g("text"), g("msgtype", "m.text"))
        elif func == "_absorb_sync":
            ret = maybe_run(NS[func](inst, make_client(g("client"), stubs), g("sync_data"), initial=g("initial", False)))
        elif func == "_download_and_cache_media":
            ret = maybe_run(NS[func](inst, g("url"), g("event_id"), g("encrypted_file"),
                g("msg_type"), g("media_type"), g("is_voice_message", False), g("body", "")))
        elif func == "_background_read_receipt":
            def _ef(coro, _rid=g("room_id"), _eid=g("event_id")):
                try:
                    coro.close()
                except Exception:
                    pass
                return _CUR.call("ensure_future", {"room_id": _rid, "event_id": _eid})
            NS["asyncio"] = SimpleNamespace(ensure_future=_ef, gather=stubs["gather"])
            ret = NS[func](inst, g("room_id"), g("event_id"))
        elif func == "_markdown_to_html":
            ret = NS[func](inst, g("text"))
        elif func == "_migrate_legacy_crypto_pickle":
            ret = maybe_run(NS[func](inst, store, g("crypto_db", crypto_db), g("acct_id"), g("pickle_key")))
        elif func == "_build_inbound_event":
            ret = maybe_run(NS[func](inst, g("room_id"), g("sender"), g("event_id"), g("body"),
                g("source_content"), g("relates_to"), ctx=g("ctx"), **{k: v for k, v in (g("extra") or {}).items()}))
        elif func == "_handle_text_message":
            ret = maybe_run(NS[func](inst, g("room_id"), g("sender"), g("event_id"), g("event_ts", 0),
                g("source_content"), g("relates_to")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        spec = ctx.spec_for(exc)
        ret = spec if spec is not None else proj_exc(exc)
    st_out = copy.deepcopy(inp.get("state") or {})
    if "_joined_rooms" in vars(inst) and isinstance(inst._joined_rooms, set):
        st_out["_joined_rooms"] = sorted(inst._joined_rooms)
    if "_encryption" in vars(inst):
        st_out["_encryption"] = inst._encryption
    if "_last_sync_ts" in vars(inst):
        st_out["_last_sync_ts"] = inst._last_sync_ts
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(st_out)}


# ── seam-name set cross-check (C / oracle / fixtures / PoP) ──
def _c_seams():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "matrix_adapter_flow6.c").read_text(encoding="utf-8")
    return set(re.findall(r'ma6_(?:call|log)\s*\(\s*ctx\s*,\s*"([^"]+)"', csrc))


def cross_check():
    registry = set(SEAM_NAMES)
    cseams = _c_seams()
    if cseams != registry:
        print(f"FATAL oracle_matrix_adapter_flow6: seam set drift C vs registry: "
              f"extra={sorted(cseams - registry)} missing={sorted(registry - cseams)}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\brec\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\barec\(\s*"([^"]+)"', osrc))
    # recorder names also appear inside build_seams via st["name"]
    used |= set(re.findall(r'\bst\["([^"]+)"\]\s*=\s*(?:a?rec)', osrc))
    if used != registry:
        print(f"FATAL oracle_matrix_adapter_flow6: seam set drift oracle vs registry: "
              f"extra={sorted(used - registry)} missing={sorted(registry - used)}")
        sys.exit(2)
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "matrix_adapter_flow6.c").read_text(encoding="utf-8")
    pops = set(re.findall(r'PoP:\s*ma6_\w+\s*@\s*plugins/platforms/matrix/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_matrix_adapter_flow6: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow6.jsonl"
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
            print(f"FATAL oracle_matrix_adapter_flow6: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(registry - fix_keys)} extra={sorted(fix_keys - registry)}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_matrix_adapter_flow6: fixture func set != KINDS: "
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
    print(f"ORACLE matrix_adapter_flow6: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow6.jsonl"
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
    print(f"SELFTEST matrix_adapter_flow6: {n} cases, {bad} drive/script failures")
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
