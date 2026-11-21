#!/usr/bin/env python3
"""Oracle for the Matrix-adapter remainder flow port
(src/gateway/platforms/matrix_adapter_flow7.c).

Drives the LIVE plugins/platforms/matrix/adapter.py UNCLAIMED helpers with
SCRIPTED seams: every module collaborator / self-sibling / in-function import
is a recording stub fed the SAME scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the state echo ("state").

The live defs are AST-extracted from the adapter module body and the
MatrixAdapter class body (decorators excluded by lineno, segments dedented)
and called with plain Python args / an attribute-double self. The heavy quarry
import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit/CancelledError included). Scripted raises round-trip
their spec verbatim; unscripted live raises project {__raise__, message:str(exc)}.
Summary: 'ORACLE matrix_adapter_flow7: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are ints; integral floats normalize to int (canon).
- Attribute carriers are {"__attr__": {...}}; doubles project back on seam args.
- logger.* calls are seams recorded with EMPTY args (only call order/name is in
  the contract; long format strings are deliberately not compared).
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
    "_resolve_store_dir": "instance",
    "_get_matrix_recovery_key_output_target": "free",
    "_room_needs_encrypted_upload": "ainstance",
    "_send_content_event": "ainstance",
    "_handle_generated_matrix_recovery_key": "free",
    "_reverify_keys_after_upload": "ainstance",
    "_reset_crypto_store_if_device_changed": "ainstance",
    "_protect_outbound_mention_regions": "instance",
    "_schedule_invite_join": "instance",
    "_repickle_crypto_sessions": "ainstance",
    "_resolve_room_identity": "ainstance",
    "_matrix_voice_metadata_for_file": "free",
    "_on_room_message": "ainstance",
    "_resolve_message_context": "ainstance",
}

# EXACTLY the C ma7_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "_background_read_receipt", "_content_mentions_bot", "_extract_server_ed25519",
    "_get_display_name", "_get_hermes_dir", "_get_room_member_count",
    "_get_room_state_value", "_handle_media_message", "_handle_text_message",
    "_invalidate_room_identities", "_is_allowed_matrix_room_event", "_is_dm_room",
    "_is_duplicate_event", "_is_self_sender", "_is_system_or_bridge_sender",
    "_matrix_event_timestamp_seconds", "_note_late_grace_drop",
    "_query_own_device_keys", "_recovery_key_output_path", "_resolve_room_identity",
    "_run_media_tool", "_split_reply_fallback", "_strip_mention", "_thread_root",
    "_write_matrix_recovery_key_output_file", "build_source",
    "client.send_message_event", "create_task", "delete", "execute", "fetch",
    "from_pickle", "get_device_id", "logger.debug", "logger.error", "logger.info",
    "logger.warning", "mark_async", "path.exists", "path.parent.mkdir",
    "session.pickle", "shutil.which", "state_store.is_encrypted", "time.monotonic",
]

# self.<name> seams bound as staticmethod on the self double (or on collaborator doubles).
SELF_SEAMS = [
    "_query_own_device_keys", "_extract_server_ed25519", "_content_mentions_bot",
    "_strip_mention", "_get_display_name", "_resolve_room_identity", "_is_dm_room",
    "_background_read_receipt", "build_source", "_is_self_sender",
    "_is_system_or_bridge_sender", "_is_allowed_matrix_room_event",
    "_is_duplicate_event", "_note_late_grace_drop", "_handle_media_message",
    "_handle_text_message", "_get_room_state_value", "_get_room_member_count",
    "_invalidate_room_identities", "_join_room_by_id", "_record_dm_room",
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
    mod = new_module("ma7_oracle_adapter")
    ns = mod.__dict__
    ns.update({
        "os": os, "json": json, "re": re, "suppress": suppress,
        "typing": typing, "Any": typing.Any, "Optional": typing.Optional,
        "Dict": typing.Dict, "Set": typing.Set, "List": typing.List,
        "Tuple": typing.Tuple, "array": __import__("array"), "sys": sys,
        "Path": PurePosixPath,
        "RoomID": lambda x: x,
        "EventType": SimpleNamespace(ROOM_MESSAGE="m.room.message"),
        "VoiceGate": object,
        "_STARTUP_GRACE_SECONDS": 5,
        "_MATRIX_VOICE_WAVEFORM_BINS": 30,
    })
    def SendResult(**kw):
        return SimpleNamespace(**kw)
    ns["SendResult"] = SendResult
    def MatrixRoomIdentity(**kw):
        return SimpleNamespace(**kw)
    ns["MatrixRoomIdentity"] = MatrixRoomIdentity
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
        print(f"FATAL oracle_matrix_adapter_flow7: missing: {missing}")
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


def _bytes(v):
    return list(v) if isinstance(v, (bytes, bytearray)) else proj(v)


def build_seams():
    st = {}
    st["_get_hermes_dir"] = rec("_get_hermes_dir",
        lambda legacy_path, new_path: {"legacy_path": legacy_path, "new_path": new_path})
    st["_recovery_key_output_path"] = rec("_recovery_key_output_path", lambda: {},
        build=lambda raw: PathDouble(raw) if isinstance(raw, str) else None)
    st["path.exists"] = rec("path.exists", lambda: {})
    st["path.parent.mkdir"] = rec("path.parent.mkdir",
        lambda parents=False, exist_ok=False: {"parents": parents, "exist_ok": exist_ok})
    st["state_store.is_encrypted"] = arec("state_store.is_encrypted",
        lambda room_id: {"room_id": proj(room_id)})
    st["client.send_message_event"] = arec("client.send_message_event",
        lambda room_id, event_type, msg_content: {"room_id": proj(room_id),
            "event_type": proj(event_type), "msg_content": proj(msg_content)})
    st["logger.debug"] = rec("logger.debug", lambda *a, **k: {})
    st["logger.error"] = rec("logger.error", lambda *a, **k: {})
    st["logger.info"] = rec("logger.info", lambda *a, **k: {})
    st["logger.warning"] = rec("logger.warning", lambda *a, **k: {})
    st["_write_matrix_recovery_key_output_file"] = rec(
        "_write_matrix_recovery_key_output_file", lambda recovery_key: {"recovery_key": recovery_key})
    st["_query_own_device_keys"] = arec("_query_own_device_keys",
        lambda client: {"client": proj(client)}, build=lambda raw: make_obj(raw, {}) if isinstance(raw, dict) else raw)
    st["_extract_server_ed25519"] = rec("_extract_server_ed25519",
        lambda device: {"device": proj(device)})
    st["get_device_id"] = arec("get_device_id", lambda: {})
    st["delete"] = arec("delete", lambda: {})
    st["fetch"] = arec("fetch", lambda query, account_id: {"query": query, "account_id": account_id})
    st["from_pickle"] = rec("from_pickle",
        lambda table, key, data: {"table": table, "key": key, "pickled": _bytes(data)},
        build=lambda raw: PickleSession(raw))
    st["session.pickle"] = rec("session.pickle", lambda key: {"key": key},
        build=lambda raw: raw)
    st["execute"] = arec("execute",
        lambda query, session_data, account_id, session_id: {
            "query": query, "session_data": _bytes(session_data),
            "account_id": account_id, "session_id": session_id})
    st["time.monotonic"] = rec("time.monotonic", lambda: {})
    st["_get_room_state_value"] = arec("_get_room_state_value",
        lambda room_id, event_type, field: {"room_id": room_id, "event_type": event_type, "field": field})
    st["_get_room_member_count"] = arec("_get_room_member_count", lambda room_id: {"room_id": room_id})
    st["_invalidate_room_identities"] = rec("_invalidate_room_identities",
        lambda oldest: {"oldest": proj(oldest)})
    st["shutil.which"] = rec("shutil.which", lambda name: {"name": name})
    st["_run_media_tool"] = rec("_run_media_tool",
        lambda cmd, timeout=None, text=None: {"cmd": proj(cmd), "timeout": timeout, "text": text},
        build=lambda raw: make_obj(raw, {}) if isinstance(raw, dict) else raw)
    st["_is_self_sender"] = rec("_is_self_sender", lambda sender: {"sender": sender})
    st["_is_system_or_bridge_sender"] = rec("_is_system_or_bridge_sender", lambda sender: {"sender": sender})
    st["_is_allowed_matrix_room_event"] = arec("_is_allowed_matrix_room_event",
        lambda room_id: {"room_id": room_id})
    st["_is_duplicate_event"] = rec("_is_duplicate_event", lambda event_id: {"event_id": event_id})
    st["_matrix_event_timestamp_seconds"] = rec("_matrix_event_timestamp_seconds",
        lambda event: {"event": proj(event)})
    st["_note_late_grace_drop"] = rec("_note_late_grace_drop", lambda event_ts: {"event_ts": event_ts})
    st["_handle_media_message"] = arec("_handle_media_message",
        lambda room_id, sender, event_id, event_ts, source_content, relates_to, msgtype: {
            "room_id": room_id, "sender": sender, "event_id": event_id, "event_ts": event_ts,
            "source_content": proj(source_content), "relates_to": proj(relates_to), "msgtype": msgtype})
    st["_handle_text_message"] = arec("_handle_text_message",
        lambda room_id, sender, event_id, event_ts, source_content, relates_to: {
            "room_id": room_id, "sender": sender, "event_id": event_id, "event_ts": event_ts,
            "source_content": proj(source_content), "relates_to": proj(relates_to)})
    st["_resolve_room_identity"] = arec("_resolve_room_identity",
        lambda room_id, force_refresh=False: {"room_id": room_id, "force_refresh": force_refresh},
        build=attr_double)
    st["_is_dm_room"] = arec("_is_dm_room", lambda room_id: {"room_id": room_id})
    st["_thread_root"] = rec("_thread_root", lambda relates_to: {"relates_to": proj(relates_to)})
    st["_content_mentions_bot"] = rec("_content_mentions_bot",
        lambda body, source_content: {"body": body, "source_content": proj(source_content)})
    st["_split_reply_fallback"] = rec("_split_reply_fallback", lambda body: {"body": body},
        build=lambda raw: tuple(raw) if isinstance(raw, list) else raw)
    st["_strip_mention"] = rec("_strip_mention", lambda body: {"body": body})
    st["_get_display_name"] = arec("_get_display_name",
        lambda room_id, sender: {"room_id": room_id, "sender": sender})
    st["build_source"] = rec("build_source",
        lambda chat_id, chat_name, chat_type, user_id, user_name, thread_id, chat_topic,
               guild_id, parent_chat_id, message_id, is_bot: {
            "chat_id": chat_id, "chat_name": proj(chat_name), "chat_type": chat_type,
            "user_id": user_id, "user_name": proj(user_name), "thread_id": proj(thread_id),
            "chat_topic": proj(chat_topic), "guild_id": proj(guild_id),
            "parent_chat_id": proj(parent_chat_id), "message_id": message_id, "is_bot": is_bot})
    st["mark_async"] = arec("mark_async", lambda thread_id: {"thread_id": proj(thread_id)})
    st["_background_read_receipt"] = rec("_background_read_receipt",
        lambda room_id, event_id: {"room_id": room_id, "event_id": event_id})
    return st


class PathDouble(str):
    def exists(self):
        return _CUR.call("path.exists", {})

    @property
    def parent(self):
        return _PathParent()


class _PathParent:
    def mkdir(self, parents=False, exist_ok=False):
        return _CUR.call("path.parent.mkdir", {"parents": parents, "exist_ok": exist_ok})


class PickleSession:
    def __init__(self, raw):
        self._raw = raw

    def pickle(self, key):
        return _CUR.call("session.pickle", {"key": key})


def make_obj(spec, methods):
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    D = type("ObjDouble", (SimpleNamespace,), {k: staticmethod(v) for k, v in methods.items()})
    o = D()
    for k, v in inner.items():
        if k not in methods:
            setattr(o, k, v)
    return o


def install_module_seams(stubs, inp):
    args = inp.get("args") or {}
    for n in ("_get_hermes_dir", "_recovery_key_output_path", "_run_media_tool",
              "_write_matrix_recovery_key_output_file", "_matrix_event_timestamp_seconds",
              "_split_reply_fallback", "_thread_root"):
        NS[n] = stubs[n]
    NS["logger"] = SimpleNamespace(
        debug=stubs["logger.debug"], error=stubs["logger.error"],
        info=stubs["logger.info"], warning=stubs["logger.warning"])
    NS["shutil"] = SimpleNamespace(which=stubs["shutil.which"])
    NS["time"] = SimpleNamespace(monotonic=stubs["time.monotonic"])

    def create_task_rec(coro):
        try:
            coro.close()
        except Exception:
            pass
        return _CUR.call("create_task", {
            "room_id": args.get("room_id", ""), "is_direct": args.get("is_direct", False),
            "inviter": args.get("inviter", "")})
    NS["asyncio"] = SimpleNamespace(create_task=create_task_rec)

    # olm import for _repickle_crypto_sessions.
    def mk_cls(table):
        def from_pickle(data, key):
            return stubs["from_pickle"](table, key, data)
        return type("Sess", (), {"table": table, "from_pickle": staticmethod(from_pickle)})
    olm = SimpleNamespace(
        Session=mk_cls("crypto_olm_session"),
        InboundGroupSession=mk_cls("crypto_megolm_inbound_session"),
        OutboundGroupSession=mk_cls("crypto_megolm_outbound_session"))

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
    stubmod("olm", {"Session": olm.Session,
                    "InboundGroupSession": olm.InboundGroupSession,
                    "OutboundGroupSession": olm.OutboundGroupSession})
    return olm


def make_client(spec, stubs):
    methods = {"send_message_event": stubs["client.send_message_event"]}
    D = type("MatrixClient", (SimpleNamespace,), {k: staticmethod(v) for k, v in methods.items()})
    o = D()
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    for k, v in inner.items():
        if k == "state_store":
            setattr(o, k, make_obj(v, {"is_encrypted": stubs["state_store.is_encrypted"]})
                    if v is not None else None)
        elif k not in methods:
            setattr(o, k, v)
    return o


def make_task(spec):
    inner = spec.get("__attr__", spec) if isinstance(spec, dict) else {}
    done_val = bool(inner.get("done", False))
    o = SimpleNamespace()
    o.done = lambda: done_val
    return o


def make_self(state, stubs):
    attrs = {n: staticmethod(stubs[n]) for n in SELF_SEAMS if n in stubs}
    D = type("MatrixAdapter", (), attrs)
    inst = D()
    for k, v in (state or {}).items():
        if k == "_client":
            setattr(inst, k, make_client(v, stubs) if v is not None else None)
        elif k == "_invite_join_tasks" and isinstance(v, dict):
            setattr(inst, k, {t: make_task(tv) for t, tv in v.items()})
        elif k == "_parked_voices":
            setattr(inst, k, make_obj(v, {"park": (lambda *a: None),
                                          "release": (lambda *a: None)}))
        elif k == "_threads":
            setattr(inst, k, make_obj(v, {"mark_async": stubs["mark_async"]}))
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
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    stubs = build_seams()
    olm = install_module_seams(stubs, inp)
    inst = make_self(state, stubs)
    g = args.get
    try:
        if func == "_resolve_store_dir":
            ret = NS[func](inst)
        elif func == "_get_matrix_recovery_key_output_target":
            ret = NS[func]()
        elif func == "_room_needs_encrypted_upload":
            ret = maybe_run(NS[func](inst, g("room_id", "")))
        elif func == "_send_content_event":
            ret = maybe_run(NS[func](inst, g("room_id", ""), g("msg_content", {})))
        elif func == "_handle_generated_matrix_recovery_key":
            ret = NS[func](g("mxid", ""), g("recovery_key", ""))
        elif func == "_reverify_keys_after_upload":
            client = make_obj(g("client"), {"device_id_attr": None}) if g("client") is not None else None
            if client is None or not hasattr(client, "device_id"):
                client = make_obj(g("client"), {}) if g("client") is not None else None
            ret = maybe_run(NS[func](inst, client, g("local_ed25519", "")))
        elif func == "_reset_crypto_store_if_device_changed":
            store = make_obj(g("crypto_store"), {"get_device_id": stubs["get_device_id"],
                                                 "delete": stubs["delete"]})
            ret = maybe_run(NS[func](inst, store, g("device_id", "")))
        elif func == "_protect_outbound_mention_regions":
            ret = NS[func](inst, g("text", ""))
        elif func == "_schedule_invite_join":
            ret = NS[func](inst, g("room_id", ""), is_direct=g("is_direct", False), inviter=g("inviter", ""))
        elif func == "_repickle_crypto_sessions":
            db = make_obj(g("crypto_db"), {"fetch": stubs["fetch"], "execute": stubs["execute"]})
            ret = maybe_run(NS[func](inst, db, g("acct_id", ""), g("legacy_key", ""), g("pickle_key", "")))
        elif func == "_resolve_room_identity":
            ret = maybe_run(NS[func](inst, g("room_id", ""), force_refresh=g("force_refresh", False)))
        elif func == "_matrix_voice_metadata_for_file":
            ret = NS[func](PurePosixPath(g("path", "")))
        elif func == "_on_room_message":
            ev = make_obj(g("event"), {})
            ret = maybe_run(NS[func](inst, ev))
        elif func == "_resolve_message_context":
            ret = maybe_run(NS[func](inst, g("room_id", ""), g("sender", ""), g("event_id", ""),
                g("body", ""), g("source_content", {}), g("relates_to", {})))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        spec = ctx.spec_for(exc)
        ret = spec if spec is not None else proj_exc(exc)

    st_out = copy.deepcopy(state)
    if func == "_resolve_store_dir" and not (isinstance(ret, dict) and "__raise__" in ret):
        st_out["_store_dir"] = str(ret) if ret is not None else None
    elif func == "_schedule_invite_join":
        tret = _trace_ret(ctx, "create_task")
        if tret is not None and not (isinstance(tret, dict) and "__raise__" in tret):
            st_out.setdefault("_invite_join_tasks", {})[g("room_id", "")] = canon(tret)
    elif func == "_resolve_room_identity" and not (isinstance(ret, dict) and "__raise__" in ret):
        # Stubbed _invalidate_room_identities is a no-op recorder: live does NOT
        # evict any key. Only the full path writes identity / cached_at.
        if any(ev.get("name") == "_get_room_state_value" for ev in ctx.trace):
            rid = g("room_id", "")
            ridir = st_out.setdefault("_room_identities", {})
            cat = st_out.setdefault("_room_identity_cached_at", {})
            mon = _trace_ret(ctx, "time.monotonic")
            ridir[rid] = canon(ret)
            if mon is not None:
                cat[rid] = mon
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(st_out)}


# ── seam-name set cross-check (C / oracle / fixtures / PoP) ──
def _c_src():
    return (_REPO / "src" / "gateway" / "platforms" / "matrix_adapter_flow7.c").read_text(encoding="utf-8")


def cross_check():
    registry = set(SEAM_NAMES)
    csrc = _c_src()
    cseams = set(re.findall(r'ma7_(?:call|log0?)\s*\(\s*ctx\s*,\s*"([^"]+)"', csrc))
    if cseams != registry:
        print(f"FATAL oracle_matrix_adapter_flow7: seam set drift C vs registry: "
              f"extra={sorted(cseams - registry)} missing={sorted(registry - cseams)}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\brec\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\barec\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'\bst\["([^"]+)"\]\s*=\s*(?:a?rec)', osrc))
    if used != registry:
        print(f"FATAL oracle_matrix_adapter_flow7: seam set drift oracle vs registry: "
              f"extra={sorted(used - registry)} missing={sorted(registry - used)}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ma7_\w+\s*@\s*plugins/platforms/matrix/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_matrix_adapter_flow7: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow7.jsonl"
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
            print(f"FATAL oracle_matrix_adapter_flow7: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(registry - fix_keys)} extra={sorted(fix_keys - registry)}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_matrix_adapter_flow7: fixture func set != KINDS: "
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
    print(f"ORACLE matrix_adapter_flow7: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "matrix_adapter_flow7.jsonl"
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
    print(f"SELFTEST matrix_adapter_flow7: {n} cases, {bad} drive/script failures")
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
