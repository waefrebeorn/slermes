#!/usr/bin/env python3
"""Oracle for openai_routes_flow2.c — gateway/platforms/api_server_openai_routes.py
UNCLAIMED-remainder FLOW (wave 2; LIVE authority: the quarry module).

Drives the LIVE functions with SCRIPTED I/O: the 23 targets are AST-extracted
from the quarry (no heavy gateway/aiohttp import), module constants seeded
(Assign AND AnnAssign, plain-Name targets only), and every foreign helper
import, adapter method, response-store read/write, uuid/time probe and SSE
writer replaced by a recording stub returning the SAME scripted results the
C harness feeds its universal seam. The flow1-CLAIMED siblings reached from
here (_cap_text, _settled, _responses_fingerprint_keys, _matching_replay)
are SEAMED, not re-driven. Internal composition runs REAL on both sides:
_response_messages_turn_start_index inside the transcript cluster,
_message_item/_reasoning_item inside _extract_output_items, envelope inside
terminal_envelope, _history_with_user inside _persist_snapshot,
_persist_snapshot inside _commit_terminal, _chat_request_id inside
_authority_chat_kwargs/_implicit_chat_session, _prepare_sse_response inside
_replay_sse_responses, _replay_sse_responses inside _responses_durable_replay,
and json.loads/json.dumps inside _trim_tool_items.

Compares the ordered DECISION SEQUENCE + return value + mutated state window
(r s_* methods), with ordered-dict comparison (insertion order is semantic).
Per-case BaseException catch (SystemExit/KeyboardInterrupt/CancelledError
included) compared as a raise spec; the BaseException trio PASSES THROUGH
every except Exception site. FAIL-CLOSED: a seam with NO script records and
raises {"__raise__":"RuntimeError","msg":"no script for <name>"}; a scripted
null is a real None result.

Input: stdin JSON lines from t_port_openai_routes_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "st":{...}}}
Summary: ORACLE openai_routes_flow2: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Fixture domain notes (kept identical on both sides):
- request nodes carry ONLY {"headers":{...}}; gateway_runner is absent
  (AttributeError), null, or {"session_authority":...};
- durable_key is null or a 2-item array; authority scripts are null or
  objects with a "persistence" field; store markers are non-null objects;
- integral floats == ints; str() positions use integral ints or non-integral
  floats; uuid4_hex scripts are ASCII; strip probes use ASCII whitespace;
- json.loads blobs use the common libjson/Python grammar; json.dumps numbers
  integral; scripted raises never use KeyError (str(KeyError) reprs its arg);
- events elements are 2-item arrays; tool_call roles are str/null;
- write_event data is an object with an int output_index; state windows never
  carry the injected collaborator keys (_api / response / response_store).
"""
import ast
import asyncio
import builtins
import copy
import json
import os
import sqlite3
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = str(_REPO.parent / "hermes-agent")
SRC = os.path.join(QUARRY, "gateway", "platforms", "api_server_openai_routes.py")

PY = {
    "transformed_notice": "_transformed_notice",
    "message_item": "_message_item",
    "reasoning_item": "_reasoning_item",
    "trim_tool_items": "_trim_tool_items",
    "turn_start_index": "_response_messages_turn_start_index",
    "turn_transcript_messages": "_turn_transcript_messages",
    "turn_reasoning_text": "_turn_reasoning_text",
    "extract_output_items": "_extract_output_items",
    "chat_request_id": "_chat_request_id",
    "authority_chat_kwargs": "_authority_chat_kwargs",
    "response_identity": "_response_identity",
    "implicit_chat_session": "_implicit_chat_session",
    "run_idempotent": "_run_idempotent",
    "prepare_sse_response": "_prepare_sse_response",
    "replay_sse_responses": "_replay_sse_responses",
    "responses_durable_replay": "_responses_durable_replay",
    "rs_item_id": "_item_id",
    "rs_envelope": "envelope",
    "rs_terminal_envelope": "terminal_envelope",
    "rs_history_with_user": "_history_with_user",
    "rs_persist_snapshot": "_persist_snapshot",
    "rs_commit_terminal": "_commit_terminal",
    "rs_write_event": "write_event",
}
WANTED_FUNCS = {
    "_transformed_notice", "_message_item", "_reasoning_item",
    "_trim_tool_items", "_response_identity", "_turn_reasoning_text",
}
WANTED_STATIC = {"_response_messages_turn_start_index", "_extract_output_items"}
WANTED_CLASSMETHODS = {"_turn_transcript_messages"}
WANTED_METHODS = {
    "_chat_request_id", "_authority_chat_kwargs", "_implicit_chat_session",
    "_run_idempotent", "_prepare_sse_response", "_replay_sse_responses",
    "_responses_durable_replay",
}
RS_METHODS = {
    "_item_id", "envelope", "terminal_envelope", "_history_with_user",
    "_persist_snapshot", "_commit_terminal", "write_event",
}
# Seamed flow1-claimed siblings / off_loop worker markers (bare globals).
SEAMED_SIBLINGS = {
    "_cap_text": "cap_text",
    "_settled": "settled",
    "_responses_fingerprint_keys": "responses_fingerprint_keys",
}

CUR = {"trace": [], "scripts": {}, "idx": {}, "stwin": None}


# ── exception construction (real builtins / sqlite3 / registered stubs) ──

class PersistenceError(Exception):
    pass


class TransientStoreError(PersistenceError):
    pass


class PermanentStoreError(PersistenceError):
    pass


class StoreUnavailable(PersistenceError):
    pass


class StoreReplaced(PersistenceError, RuntimeError):
    pass


class AuthorityRefusal(PersistenceError, ValueError):
    pass


STORE_FAILURES = (TransientStoreError, PermanentStoreError, StoreUnavailable, StoreReplaced)
CUSTOM_EXC = {c.__name__: c for c in (
    PersistenceError, TransientStoreError, PermanentStoreError,
    StoreUnavailable, StoreReplaced, AuthorityRefusal)}


def _exc_class(name):
    if name in CUSTOM_EXC:
        return CUSTOM_EXC[name]
    cls = getattr(builtins, name, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls
    cls = getattr(sqlite3, name, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls
    if name == "CancelledError":
        return asyncio.CancelledError
    return type(name, (Exception,), {})


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        e = _exc_class(r["__raise__"])(r.get("msg", ""))
        for k, v in r.items():
            if k not in ("__raise__", "msg"):
                try:
                    setattr(e, k, v)
                except Exception:
                    pass
        raise e
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    args = json.loads(json.dumps(args))
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))
    # Trace keeps a PRISTINE copy; the caller gets its own copy to mutate.
    CUR["trace"].append({"ev": name, "args": args, "ret": json.loads(json.dumps(r))})
    return maybe_raise(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (StreamResponseDouble, JsonResponseDouble)):
        return norm(o.proj())
    if isinstance(o, StoreDouble):
        return {"__store__": "response"}
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    """Ordered, type-tagged canonical form: dict insertion order is semantic."""
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── doubles ──

def proj_request(request):
    return {"headers": dict(request.headers)}


def proj_runner(runner):
    if runner is None:
        return None
    if isinstance(runner, types.SimpleNamespace):
        return dict(vars(runner))
    return runner


class StreamResponseDouble:
    def __init__(self, status=200, headers=None):
        self.status = status
        self.headers = dict(headers) if headers else {}
        self.prepared = False

    async def prepare(self, request):
        self.prepared = True
        return seam("response.prepare", {"request": proj_request(request)})

    async def write(self, data):
        return seam("response.write", {"data": data})

    def proj(self):
        return {"__response__": "stream", "status": self.status,
                "headers": self.headers, "prepared": self.prepared}


class JsonResponseDouble:
    def __init__(self, body, status, headers):
        self.body = body
        self.status = status
        self.headers = dict(headers) if headers else {}

    def proj(self):
        return {"__response__": "json", "body": self.body,
                "status": self.status, "headers": self.headers}


def _store_arg_proj(a):
    if isinstance(a, StoreDouble):
        return {"__store__": "response"}
    return a


class StoreDouble:
    """Response-store double: every method is one scripted seam call."""

    def get(self, key):
        return seam("store.get", {"key": key})

    def put(self, key, value):
        return seam("store.put", {"key": key, "value": value})

    def get_conversation(self, conversation):
        return seam("store.get_conversation", {"conversation": conversation})

    def set_conversation(self, conversation, response_id):
        return seam("store.set_conversation",
                    {"conversation": conversation, "response_id": response_id})

    def request_created_at(self, key):
        return seam("store.request_created_at", {"key": key})

    async def off_loop(self, fn, *args):
        marker = getattr(fn, "__or2_fn__", getattr(fn, "__name__", str(fn)))
        return seam("off_loop", {"fn": marker, "args": [_store_arg_proj(a) for a in args]})


StoreDouble.request_created_at.__or2_fn__ = "request_created_at"


class RecLogger:
    def error(self, msg, *args, exc_info=False, **kw):
        return seam("logger.error", {
            "message": msg,
            "args": [str(a) if isinstance(a, BaseException) else a for a in args],
            "exc_info": exc_info})

    def warning(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def debug(self, *a, **k):
        pass


# ── extraction ──

def extract():
    import logging
    import re
    import time
    import uuid
    from contextlib import suppress
    from typing import Any, Dict, List, Optional
    import __future__

    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("openai_routes2_extracted")
    sys.modules["openai_routes2_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "openai_routes2_extracted",
        "json": json, "os": os, "sys": sys, "re": re, "copy": copy,
        "time": time, "uuid": uuid, "logging": logging, "suppress": suppress,
        "Any": Any, "Dict": Dict, "List": List, "Optional": Optional,
        "asyncio": asyncio,
        "t": lambda key: seam("t", {"key": key}),
        "web": types.SimpleNamespace(
            Response=object, Request=object,
            StreamResponse=lambda *, status=200, headers=None, **kw: (
                seam("stream_response", {"status": status, "headers": headers}),
                StreamResponseDouble(status, headers))[1],
            json_response=lambda data, *, status=200, headers=None, **kw: (
                seam("json_response", {"body": data, "status": status, "headers": headers}),
                JsonResponseDouble(data, status, headers))[1]),
    })
    tree = ast.parse(src)
    # Pass 1: module constants (Assign AND AnnAssign, plain-Name targets).
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)):
                continue
        else:
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # The module's `logger = logging.getLogger(...)` Assign overwrote the
    # seeded recorder — re-set it AFTER the constant pass.
    ns["logger"] = RecLogger()
    # Pass 2: module-level target defs (future-annotations keeps annotations
    # strings so def-exec never evaluates web.Request etc.).
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
    # Pass 3: mixin members (decorators stripped).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "OpenAICompatRoutesMixin":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in (
                        WANTED_STATIC | WANTED_CLASSMETHODS | WANTED_METHODS):
                    sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec",
                                 flags=__future__.annotations.compiler_flag), ns)
    # Pass 4: _ResponsesStream methods (decorators stripped).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_ResponsesStream":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in RS_METHODS:
                    sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec",
                                 flags=__future__.annotations.compiler_flag), ns)
    missing = (WANTED_FUNCS | WANTED_STATIC | WANTED_CLASSMETHODS |
               WANTED_METHODS | RS_METHODS) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)

    # Seamed flow1-claimed siblings (bare globals in the extracted defs).
    for live, sname in SEAMED_SIBLINGS.items():
        ns[live] = (lambda sn: lambda *a, **k: seam(sn, _sibling_args(sn, a, k)))(sname)
    # _matching_replay rides off_loop as a worker marker (never called).
    def _matching_replay_marker(*a, **k):
        raise AssertionError("matching_replay must run via off_loop")
    _matching_replay_marker.__or2_fn__ = "matching_replay"
    ns["_matching_replay"] = _matching_replay_marker

    # The mixin class double carries the ported turn-boundary staticmethod
    # and the seamed _message_response projection.
    class MixinDouble:
        pass
    MixinDouble._response_messages_turn_start_index = staticmethod(
        ns["_response_messages_turn_start_index"])
    MixinDouble._message_response = staticmethod(
        lambda msg: seam("message_response", {"message": msg}))
    ns["OpenAICompatRoutesMixin"] = MixinDouble
    ns["MixinDouble"] = MixinDouble
    return ns


def _sibling_args(sname, a, k):
    if sname == "cap_text":
        return {"text": a[0] if a else k.get("text"), "keep": a[1] if len(a) > 1 else k.get("keep")}
    if sname == "settled":
        return {"record": a[0] if a else k.get("record")}
    if sname == "responses_fingerprint_keys":
        return {"body": a[0] if a else k.get("body")}
    return {}


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _register(name, mod):
    parts = name.split(".")
    parent = _pkg(".".join(parts[:-1]))
    setattr(parent, parts[-1], mod)
    sys.modules[name] = mod


def install_stubs(ns):
    """The lazy in-function imports resolve to these recording stubs (same
    scripts as C). uuid/time are module-top imports seeded as doubles."""
    _pkg("gateway")
    _pkg("gateway.platforms")

    api = types.ModuleType("gateway.platforms.api_server")

    def _er(message, status, *, err_type="invalid_request_error", param=None,
            code=None, headers=None):
        return seam("error_response", {"message": message, "status": status,
                                       "err_type": err_type})
    api._error_response = _er

    def _oe(message, err_type="invalid_request_error", param=None, code=None):
        return seam("openai_error", {"message": message, "err_type": err_type,
                                     "param": param, "code": code})
    api._openai_error = _oe

    def _redact(value, *, limit=None):
        return seam("redact_api_error_text", {"value": value})
    api._redact_api_error_text = _redact

    def _mrf(body, keys=None):
        return seam("make_request_fingerprint", {"body": body, "keys": keys})
    api._make_request_fingerprint = _mrf

    async def _get_or_set(key, fp, compute):
        return seam("idem_get_or_set", {"key_parts": key.split("\0"), "fingerprint": fp})
    api._idem_cache = types.SimpleNamespace(get_or_set=_get_or_set)

    def _sse_frame(data, *, event=None, ensure_ascii=True, id=None):
        return seam("sse_frame", {"data": data, "event": event})
    api._sse_frame = _sse_frame
    _register("gateway.platforms.api_server", api)

    tb = types.ModuleType("gateway.platforms.api_server_turn_boundary")
    tb.response_turn_start_index = lambda ch, um, result: seam(
        "turn_start_index",
        {"conversation_history": ch, "user_message": um, "result": result})
    _register("gateway.platforms.api_server_turn_boundary", tb)

    ri = types.ModuleType("gateway.platforms.api_server_response_identity")
    ri.durable_response_id = lambda request_key: seam("durable_response_id", {"key": request_key})

    def _next_item_id(response_id, counters, kind, *, call_id=None, commentary=False):
        return seam("next_item_id", {"response_id": response_id, "identity_counts": counters,
                                     "kind": kind, "call_id": call_id, "commentary": commentary})
    ri.next_item_id = _next_item_id
    ri.identify_output = lambda items, response_id: seam(
        "identify_output", {"items": items, "response_id": response_id})
    _register("gateway.platforms.api_server_response_identity", ri)

    ra = types.ModuleType("gateway.platforms.api_server_response_admissions")
    ra.request_binding = lambda adapter, request, durable_key: seam(
        "request_binding", {"request": proj_request(request), "durable_key": durable_key})
    _register("gateway.platforms.api_server_response_admissions", ra)

    ar = types.ModuleType("gateway.platforms.api_server_authority_runs")
    ar.store_error_response = lambda _openai_error, exc: seam(
        "store_error_response", {"exc": {"kind": type(exc).__name__, "msg": str(exc)}})
    _register("gateway.platforms.api_server_authority_runs", ar)

    sa = types.ModuleType("gateway.session_api")

    def _implicit_api_session(store, session_id, request_id=None, *, persistence=None):
        raise AssertionError("implicit_api_session must run via to_thread")
    _implicit_api_session.__or2_fn__ = "implicit_api_session"
    sa.implicit_api_session = _implicit_api_session
    _register("gateway.session_api", sa)

    sar = types.ModuleType("gateway.session_authorities")

    def _active_authority(runner):
        r = seam("active_authority", {"gateway_runner": proj_runner(runner)})
        # authority.persistence is an ATTRIBUTE read — materialize the double.
        if isinstance(r, dict):
            return types.SimpleNamespace(**r)
        return r
    sar.active_authority = _active_authority
    _register("gateway.session_authorities", sar)

    pers = types.ModuleType("persistence")
    pers.AuthorityRefusal = AuthorityRefusal
    pers.STORE_FAILURES = STORE_FAILURES
    pers.PersistenceError = PersistenceError
    _register("persistence", pers)

    # uuid / time module doubles (module-top imports the seeder never sees).
    ns["uuid"] = types.SimpleNamespace(
        uuid4=lambda: types.SimpleNamespace(hex=seam("uuid4_hex", {})))
    ns["time"] = types.SimpleNamespace(time=lambda: seam("time", {}))

    # asyncio.to_thread is the worker boundary seam.
    async def _to_thread(fn, *args):
        marker = getattr(fn, "__or2_fn__", getattr(fn, "__name__", str(fn)))
        return seam("to_thread", {"fn": marker, "args": list(args)})
    asyncio.to_thread = _to_thread
# ── drive ──

def mk_request(inp):
    req = inp.get("request") or {}
    h = req.get("headers")
    if not isinstance(h, dict):
        h = {}
    return types.SimpleNamespace(headers=dict(h))


def mk_mixin_self(ns, inp):
    sd = types.SimpleNamespace()
    if "gateway_runner" in inp:
        gw = inp["gateway_runner"]
        if gw is None:
            sd.gateway_runner = None
        elif isinstance(gw, dict):
            sd.gateway_runner = types.SimpleNamespace(**gw)
        else:
            sd.gateway_runner = gw
    sd._chat_request_id = types.MethodType(ns["_chat_request_id"], sd)
    sd._prepare_sse_response = types.MethodType(ns["_prepare_sse_response"], sd)
    sd._replay_sse_responses = types.MethodType(ns["_replay_sse_responses"], sd)
    sd._run_idempotency_scope = lambda request: seam(
        "run_idempotency_scope", {"request": proj_request(request)})
    sd._sse_headers = lambda request: seam(
        "sse_headers", {"request": proj_request(request)})

    def _crs():
        r = seam("current_response_store", {})
        return StoreDouble() if r is not None else None
    sd._current_response_store = _crs
    return sd


def mk_rs_self(ns, inp):
    state = copy.deepcopy(inp.get("state") or {})
    slf = types.SimpleNamespace(**state)
    # Injected collaborators (always present live; never echoed as state).
    slf._api = types.SimpleNamespace(
        _sse_frame=lambda data, *, event=None, ensure_ascii=True, id=None: seam(
            "sse_frame", {"data": data, "event": event}),
        _responses_usage_payload=lambda usage: seam(
            "responses_usage_payload", {"usage": usage}),
        _redact_api_error_text=lambda value, *, limit=None: seam(
            "redact_api_error_text", {"value": value}))
    slf.response = StreamResponseDouble()
    slf.response_store = StoreDouble()
    for m in RS_METHODS:
        setattr(slf, m, types.MethodType(ns[m], slf))
    CUR["stwin"] = (slf, list(state.keys()))
    return slf


def drive(ns, func, inp):
    """Returns ret; ret is a raise spec on ANY BaseException."""
    name = PY[func]
    g = inp.get
    try:
        if func == "transformed_notice":
            ret = ns[name]()
        elif func == "message_item":
            ret = ns[name](g("text"))
        elif func == "reasoning_item":
            ret = ns[name](g("text"))
        elif func == "trim_tool_items":
            items = copy.deepcopy(g("items"))
            ret = ns[name](items)
        elif func == "turn_start_index":
            ret = ns[name](g("conversation_history"), g("user_message"), g("result"))
        elif func == "turn_transcript_messages":
            _ch = g("conversation_history"); _um = g("user_message"); _res = g("result")
            ret = ns[name](ns["MixinDouble"], _ch, _um, _res)
        elif func == "turn_reasoning_text":
            ret = ns[name](g("conversation_history"), g("user_message"), g("result"))
        elif func == "extract_output_items":
            _si = g("start_index", 0)
            _rid = g("response_id")
            ret = ns[name](g("result"), _si, response_id=_rid)
        elif func == "chat_request_id":
            sd = mk_mixin_self(ns, inp)
            req = mk_request(inp)
            ret = ns[name](sd, req)
        elif func == "authority_chat_kwargs":
            sd = mk_mixin_self(ns, inp)
            req = mk_request(inp)
            _psi = g("provided_session_id")
            ret = ns[name](sd, req, _psi)
        elif func == "response_identity":
            ret = asyncio.run(ns[name](StoreDouble(), g("durable_key")))
        elif func == "implicit_chat_session":
            sd = mk_mixin_self(ns, inp)
            req = mk_request(inp)
            _sid = g("session_id")
            ret = asyncio.run(ns[name](sd, req, _sid))
        elif func == "run_idempotent":
            sd = mk_mixin_self(ns, inp)
            req = mk_request(inp)
            _body = g("body"); _ll = g("log_label")
            _fk = g("fingerprint_keys"); _rt = g("route")

            async def compute():
                return seam("compute", {})
            ret = asyncio.run(ns[name](sd, req, _body, compute,
                                       log_label=_ll, fingerprint_keys=_fk, route=_rt))
        elif func == "prepare_sse_response":
            sd = mk_mixin_self(ns, inp)
            req = mk_request(inp)
            _sid = g("session_id"); _gsk = g("gateway_session_key")
            ret = asyncio.run(ns[name](sd, req, _sid, _gsk))
        elif func == "replay_sse_responses":
            sd = mk_mixin_self(ns, inp)
            req = mk_request(inp)
            _replay = copy.deepcopy(g("replay"))
            ret = asyncio.run(ns[name](sd, req, _replay))
        elif func == "responses_durable_replay":
            sd = mk_mixin_self(ns, inp)
            req = mk_request(inp)
            _body = g("body"); _stream = g("stream")
            ret = asyncio.run(ns[name](sd, req, _body, _stream))
        elif func == "rs_item_id":
            slf = mk_rs_self(ns, inp)
            _kind = g("kind"); _cid = g("call_id"); _com = g("commentary", False)
            ret = slf._item_id(_kind, call_id=_cid, commentary=_com)
        elif func == "rs_envelope":
            slf = mk_rs_self(ns, inp)
            ret = slf.envelope(g("status"))
        elif func == "rs_terminal_envelope":
            slf = mk_rs_self(ns, inp)
            _st = g("status"); _out = g("output"); _err = g("error")
            ret = slf.terminal_envelope(_st, _out, error=_err)
        elif func == "rs_history_with_user":
            slf = mk_rs_self(ns, inp)
            ret = slf._history_with_user()
        elif func == "rs_persist_snapshot":
            slf = mk_rs_self(ns, inp)
            _env = g("response_env"); _hist = g("history"); _sid = g("session_id")
            ret = slf._persist_snapshot(_env, _hist, _sid)
        elif func == "rs_commit_terminal":
            slf = mk_rs_self(ns, inp)
            _env = g("env"); _fh = g("full_history")
            _sid = g("session_id"); _rec = g("record")
            ret = slf._commit_terminal(_env, _fh, _sid, _rec)
        elif func == "rs_write_event":
            slf = mk_rs_self(ns, inp)
            _et = g("event_type"); _data = copy.deepcopy(g("data"))
            ret = asyncio.run(slf.write_event(_et, _data))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit/KeyboardInterrupt/CancelledError included
        ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def main():
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["stwin"] = None
        if func not in PY:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret = drive(ns, func, inp)
        st = {}
        if CUR["stwin"] is not None:
            slf, keys = CUR["stwin"]
            st = {k: norm(getattr(slf, k)) for k in keys}
        want = {"ret": norm(ret), "trace": CUR["trace"], "st": st}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE openai_routes_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE openai_routes_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()