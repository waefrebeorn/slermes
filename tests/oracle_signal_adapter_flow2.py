#!/usr/bin/env python3
"""Oracle for the Signal adapter FLOW remainder port
(src/gateway/platforms/signal_adapter_flow2.c).

Drives the LIVE plugins/platforms/signal/adapter.py unclaimed remainder — the
identity / config / quote leaves, the UTF-16 chunking cluster, the reaction-hook
cluster, the typing-backoff cluster, and the recipient-resolution / JSON-RPC /
send-path cluster — with SCRIPTED seams: every SEAMED claimed sibling
(_with_target, _rpc_send, _send_file, _send_reaction_rpc, _resolve_image_path)
and foreign collaborator (client.post, resp.raise_for_status, resp.json,
time.time, time.monotonic, cancel_task, _sig_secret, uuid.UUID,
markdown_to_signal, Path.stat, _is_sender_authorized, _looks_like_e164_number,
_is_signal_rate_limit_error, _extract_retry_after_seconds, logger.*) is a
recording stub fed the same scripts the C harness feeds its universal seam
sig2_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the mutated
state ("state": self and, for on_processing_*, the MessageEvent double).

The live defs are AST-extracted from adapter.py (module constants seeded, Assign
AND AnnAssign, plain-Name targets only; SignalAdapter methods extracted with
decorators stripped; logger re-set silent AFTER seeding). SendResult (base.py)
and utf16_len (base.py, INLINED in C — kept REAL here, no seam) plus
MessageType/ProcessingOutcome (event.py) are exec'd real; SignalRateLimitError +
rate-limit constants come from signal_rate_limit.py. Every exec namespace is a
registered sys.modules module (py3.14). The heavy quarry import chain
(gateway.platforms.base / event / config, httpx, agent.i18n,
gateway.platforms._shared / helpers) is never imported; imported names are seeded
as placeholders / recorders.

Mode table (mirrors the C port): every ported function runs REAL (inline); only
the seamed siblings + foreign collaborators above are SEAMED. utf16_len is REAL
(inlined). A REAL function calls its REAL children directly and its SEAMED
children through the recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec bare
(SignalRateLimitError carries retry_after, whether scripted OR live-constructed).
Summary: 'ORACLE signal_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- The adapter self double is a real type("SignalAdapter", (), clsdict) instance;
  a DIRECT self._attr read with the key absent raises AttributeError
  "'SignalAdapter' object has no attribute 'X'". The MessageEvent / MessageSource
  doubles are real type("MessageEvent"/"MessageSource", (), {}) instances so their
  absent-attr messages match the C port; canon projects them via vars().
- _sent_message_timestamps materializes as OrderedDict[str, None]; the recipient
  maps and typing dicts stay plain dicts; client materializes as a ClientDouble
  (canon projects it back to its marker); _recipient_cache_lock as a no-op
  LockDouble (canon -> marker). tuple returns project to lists; SendResult via
  dataclasses.asdict (all 8 fields); integral floats == ints.
- Fixture corpus restrictions are documented in the C port header (ASCII scope;
  reaction emoji are opaque pass-through literals; resp.json scripts an object;
  _sig_secret scripts a string; int() tokens in _styles_for_chunk are clean).
"""
import ast
import asyncio
import copy
import dataclasses
import enum
import itertools
import json
import logging
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from collections import OrderedDict
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "signal", "adapter.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
EVENT_SRC = os.path.join(Q, "gateway", "platforms", "event.py")
RATE_SRC = os.path.join(Q, "plugins", "platforms", "signal", "signal_rate_limit.py")
_FLAGS = _fut.annotations.compiler_flag
MODNAME = "sig2_oracle_adapter"

# module-level functions ported REAL
MODULE_TARGETS = ["_is_signal_service_id", "validate_signal_config"]
# SignalAdapter methods ported REAL: name -> kind
METHOD_KINDS = {
    "_remember_recipient_identifiers": "method",
    "_extract_contact_uuid": "method",
    "_quote_references_own_message": "method",
    "_extract_reaction_target": "method",
    "format_message": "method",
    "_reactions_enabled": "method",
    "send_reaction": "method",
    "remove_reaction": "method",
    "on_processing_start": "method",
    "on_processing_complete": "method",
    "_stop_typing_indicator": "method",
    "stop_typing": "method",
    "send_typing": "method",
    "_resolve_recipient": "method",
    "_rpc": "method",
    "get_chat_info": "method",
    "send_image": "method",
    "_send_attachment": "method",
    "send": "method",
    "_styles_for_chunk": "staticmethod",
    "_utf16_offsets": "staticmethod",
    "_split_signal_formatted_message": "classmethod",
}
TARGETS = MODULE_TARGETS + list(METHOD_KINDS)
ASYNC_METHODS = {
    "send_reaction", "remove_reaction", "on_processing_start", "on_processing_complete",
    "_stop_typing_indicator", "stop_typing", "send_typing", "_resolve_recipient",
    "_rpc", "get_chat_info", "send_image", "_send_attachment", "send",
}
# seamed claimed siblings (REAL flow ports in signal_adapter_flow.c; NOT re-ported)
SEAMED_SIBLINGS = ["_with_target", "_rpc_send", "_send_file", "_send_reaction_rpc", "_resolve_image_path"]
SELF_EMIT = {nm for nm, k in METHOD_KINDS.items() if k == "method"}
EVENT_FUNCS = {"_extract_reaction_target", "_reactions_enabled", "on_processing_start", "on_processing_complete"}
EVENT_EMIT = {"on_processing_start", "on_processing_complete"}


def seg(src, node):
    start = node.lineno
    if isinstance(node, ast.ClassDef):
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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def _seed_constants(src, tree, ns, path):
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    import time as _time
    import uuid as _uuid
    from contextlib import suppress
    from dataclasses import dataclass, field
    from typing import Any, Optional, Dict, List, Tuple
    ns.update({"re": re, "json": json, "os": os, "time": _time, "logging": logging,
               "asyncio": asyncio, "itertools": itertools, "uuid": _uuid, "suppress": suppress,
               "OrderedDict": OrderedDict, "dataclass": dataclass, "field": field,
               "Any": Any, "Optional": Optional, "Dict": Dict, "List": List,
               "Tuple": Tuple, "enum": enum, "Enum": enum.Enum, "logger": _SilentLogger()})
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # real MessageType / ProcessingOutcome enums (for module-constant seeding)
    esrc = open(EVENT_SRC, encoding="utf-8").read()
    for node in ast.parse(esrc).body:
        if isinstance(node, ast.ClassDef) and node.name in ("MessageType", "ProcessingOutcome"):
            exec_node(esrc, node, ns, EVENT_SRC)
    # real SendResult dataclass + utf16_len (INLINED in C, REAL here)
    bsrc = open(BASE_SRC, encoding="utf-8").read()
    btree = ast.parse(bsrc)
    for node in btree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(bsrc, node, ns, BASE_SRC)
        if isinstance(node, ast.FunctionDef) and node.name == "utf16_len":
            exec_node(bsrc, node, ns, BASE_SRC)
    # SignalRateLimitError + rate-limit constants
    rsrc = open(RATE_SRC, encoding="utf-8").read()
    rtree = ast.parse(rsrc)
    for node in rtree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SignalRateLimitError":
            exec_node(rsrc, node, ns, RATE_SRC)
    _seed_constants(rsrc, rtree, ns, RATE_SRC)
    # adapter module constants (_OUTCOME_REACTION needs the real enums above)
    _seed_constants(src, tree, ns, SRC)
    # module-level functions ported REAL
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_TARGETS:
            exec_node(src, node, ns, SRC)
    # SignalAdapter methods (decorators stripped; re-wrapped by make_double)
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SignalAdapter")
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in METHOD_KINDS:
            clean = copy.copy(node)
            clean.decorator_list = []
            exec_node(src, clean, ns, SRC)
    ns["logger"] = _SilentLogger()
    required = TARGETS + [
        "SendResult", "SignalRateLimitError", "utf16_len", "ProcessingOutcome",
        "_OUTCOME_REACTION", "_QUOTE_AUTHOR_KEYS", "SIGNAL_MAX_ATTACHMENT_SIZE",
        "SIGNAL_RATE_LIMIT_MAX_ATTEMPTS", "MAX_MESSAGE_LENGTH",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_signal_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in TARGETS}


# ── doubles whose AttributeError type name must match the C port ──
MessageEventDouble = type("MessageEvent", (), {"__project__": True})
MessageSourceDouble = type("MessageSource", (), {"__project__": True})


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, enum.Enum):
            return x.value
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, (ClientDouble, LockDouble)):
            return conv(x._marker)
        if getattr(type(x), "__project__", False):
            return conv(vars(x))
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


# ── exception materialization / projection (FULL builtin hierarchy) ──
BUILTIN = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "Exception": Exception, "TypeError": TypeError, "ValueError": ValueError,
    "AttributeError": AttributeError, "KeyError": KeyError, "IndexError": IndexError,
    "LookupError": LookupError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "OSError": OSError,
    "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
    "TimeoutError": TimeoutError, "ConnectionError": ConnectionError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "ArithmeticError": ArithmeticError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "FloatingPointError": FloatingPointError, "AssertionError": AssertionError,
    "BufferError": BufferError, "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "MemoryError": MemoryError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "ReferenceError": ReferenceError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "RecursionError": RecursionError,
    "Warning": Warning, "UserWarning": UserWarning,
    "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "SyntaxWarning": SyntaxWarning, "RuntimeWarning": RuntimeWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning,
    "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "ResourceWarning": ResourceWarning, "EncodingWarning": EncodingWarning,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "ProcessLookupError": ProcessLookupError,
    "FileExistsError": FileExistsError, "ExceptionGroup": ExceptionGroup,
    "BaseExceptionGroup": BaseExceptionGroup,
    "JSONDecodeError": json.JSONDecodeError,
    "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "SignalRateLimitError":
        return NS["SignalRateLimitError"](msg, retry_after=spec.get("retry_after"))
    cls = BUILTIN.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    if cls in (UnicodeDecodeError, UnicodeEncodeError, UnicodeError):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, NS["SignalRateLimitError"]):
        d["retry_after"] = exc.retry_after
    return d


# ── per-case script backend (mirrors the C sig2_call exactly) ──
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
                for k, v in rs.items():
                    if k not in ("type", "message"):
                        spec[k] = v
                self._raise(name, args, spec, make_exc(rs))
            ret = copy.deepcopy(used.get("ret"))
        else:
            ret = copy.deepcopy(used)
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def rec(ctx, name, argfn):
    def f(*a, **k):
        return ctx.call(name, argfn(*a, **k))
    return f


def rec_async(ctx, name, argfn):
    async def f(*a, **k):
        return ctx.call(name, argfn(*a, **k))
    return f


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


class RecLogger:
    """logger.<level>(fmt, *args) and logger.log(level, fmt, *args) seams."""
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log

    def log(self, level, fmt, *a, **k):
        return self._ctx.call("logger.log", {"level": level, "fmt": fmt, "args": list(a)})


# ── attribute / collaborator doubles ──
class RespDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def raise_for_status(self):
        return self._ctx.call("resp.raise_for_status", {})

    def json(self):
        return self._ctx.call("resp.json", {})


class ClientDouble:
    def __init__(self, marker, ctx):
        self._marker = marker
        self._ctx = ctx

    async def post(self, url, json=None, timeout=None):
        self._ctx.call("client.post", {"url": url, "json": json, "timeout": timeout})
        return RespDouble(self._ctx)


class LockDouble:
    def __init__(self, marker, ctx):
        self._marker = marker
        self._ctx = ctx

    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False


class PathDouble:
    def __init__(self, path, ctx):
        self.path = path
        self._ctx = ctx

    def stat(self):
        r = self._ctx.call("Path.stat", {"path": self.path})
        return types.SimpleNamespace(st_size=r["st_size"])


def mat_self_attr(k, v, ctx):
    if k == "_sent_message_timestamps" and isinstance(v, dict):
        return OrderedDict((kk, None) for kk in v)
    if k == "client":
        return None if v is None else ClientDouble(v, ctx)
    if k == "_recipient_cache_lock":
        return LockDouble(v, ctx)
    return v


def mat_event(ev, ctx):
    e = MessageEventDouble()
    if isinstance(ev, dict):
        for k, v in ev.items():
            if k == "source" and isinstance(v, dict):
                s = MessageSourceDouble()
                for sk, sv in v.items():
                    setattr(s, sk, copy.deepcopy(sv))
                setattr(e, k, s)
            else:
                setattr(e, k, copy.deepcopy(v))
    return e


# ── seamed sibling recorders (bound on the double) ──
def make_with_target_seam(ctx):
    async def _with_target(self, params, chat_id, *, resolve=True):
        return ctx.call("_with_target", {"params": params, "chat_id": chat_id, "resolve": resolve})
    return _with_target


def make_rpc_send_seam(ctx):
    async def _rpc_send(self, params, fail_error):
        return ctx.call("_rpc_send", {"params": params, "fail_error": fail_error})
    return _rpc_send


def make_send_file_seam(ctx):
    async def _send_file(self, chat_id, file_path, caption, fail_error):
        return ctx.call("_send_file", {"chat_id": chat_id, "file_path": file_path,
                                       "caption": caption, "fail_error": fail_error})
    return _send_file


def make_send_reaction_rpc_seam(ctx):
    async def _send_reaction_rpc(self, chat_id, params):
        return ctx.call("_send_reaction_rpc", {"chat_id": chat_id, "params": params})
    return _send_reaction_rpc


def make_resolve_image_path_seam(ctx):
    async def _resolve_image_path(self, image_url):
        return ctx.call("_resolve_image_path", {"image_url": image_url})
    return _resolve_image_path


def make_is_sender_authorized_seam(ctx):
    def _is_sender_authorized(self, sender, chat_type, chat_id, *, is_bot=False):
        return ctx.call("_is_sender_authorized", {"sender": sender, "chat_type": chat_type,
                                                  "chat_id": chat_id, "is_bot": is_bot})
    return _is_sender_authorized


def make_double(state, ctx):
    clsdict = {}
    for name, kind in METHOD_KINDS.items():
        fn = NS[name]
        if kind == "staticmethod":
            clsdict[name] = staticmethod(fn)
        elif kind == "classmethod":
            clsdict[name] = classmethod(fn)
        else:
            clsdict[name] = fn
    clsdict["_with_target"] = make_with_target_seam(ctx)
    clsdict["_rpc_send"] = make_rpc_send_seam(ctx)
    clsdict["_send_file"] = make_send_file_seam(ctx)
    clsdict["_send_reaction_rpc"] = make_send_reaction_rpc_seam(ctx)
    clsdict["_resolve_image_path"] = make_resolve_image_path_seam(ctx)
    clsdict["_is_sender_authorized"] = make_is_sender_authorized_seam(ctx)
    D = type("SignalAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_self_attr(k, copy.deepcopy(v), ctx))
    return D, inst


# ── per-case namespace installation ──
def setup_case(func, inp):
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    for nm, fn in REAL.items():
        NS[nm] = fn
    NS["logger"] = RecLogger(ctx)
    NS["time"] = types.SimpleNamespace(
        time=rec(ctx, "time.time", lambda: {}),
        monotonic=rec(ctx, "time.monotonic", lambda: {}))
    NS["uuid"] = types.SimpleNamespace(UUID=rec(ctx, "uuid.UUID", lambda value: {"value": value}))
    NS["Path"] = lambda p: PathDouble(p, ctx)
    NS["cancel_task"] = rec_async(ctx, "cancel_task", lambda task: {"task": task})
    NS["_sig_secret"] = rec(ctx, "_sig_secret", lambda name, default=None: {"name": name, "default": default})
    NS["markdown_to_signal"] = rec(ctx, "markdown_to_signal", lambda text: {"text": text})
    NS["_looks_like_e164_number"] = rec(ctx, "_looks_like_e164_number", lambda value: {"value": value})
    NS["_is_signal_rate_limit_error"] = rec(ctx, "_is_signal_rate_limit_error", lambda err: {"err": err})
    NS["_extract_retry_after_seconds"] = rec(ctx, "_extract_retry_after_seconds", lambda err: {"err": err})
    D, inst = make_double(state, ctx)
    return ctx, D, inst


# ── per-case live call ──
def live_call(func, args, D, inst, ctx, ev):
    if func == "_is_signal_service_id":
        return NS[func](args.get("value"))
    if func == "validate_signal_config":
        cfg = args.get("config")
        cfgd = types.SimpleNamespace(**cfg) if isinstance(cfg, dict) else types.SimpleNamespace()
        return NS[func](cfgd)
    if func == "_remember_recipient_identifiers":
        return inst._remember_recipient_identifiers(args.get("number"), args.get("service_id"))
    if func == "_extract_contact_uuid":
        return inst._extract_contact_uuid(args.get("contact"), args.get("phone_number"))
    if func == "_quote_references_own_message":
        return inst._quote_references_own_message(args.get("reply_to_id"), args.get("reply_to_author"))
    if func == "_extract_reaction_target":
        return inst._extract_reaction_target(ev)
    if func == "format_message":
        return inst.format_message(args.get("content"))
    if func == "_styles_for_chunk":
        return D._styles_for_chunk(args.get("text_styles"), args.get("chunk_start"), args.get("chunk_end"))
    if func == "_utf16_offsets":
        return D._utf16_offsets(args.get("text"))
    if func == "_split_signal_formatted_message":
        return D._split_signal_formatted_message(args.get("plain_text"), args.get("text_styles"), args.get("max_length"))
    if func == "_reactions_enabled":
        return inst._reactions_enabled(ev)
    if func == "send_reaction":
        return asyncio.run(inst.send_reaction(args.get("chat_id"), args.get("emoji"),
                                              args.get("target_author"), args.get("target_timestamp")))
    if func == "remove_reaction":
        return asyncio.run(inst.remove_reaction(args.get("chat_id"), args.get("target_author"),
                                                args.get("target_timestamp")))
    if func == "on_processing_start":
        return asyncio.run(inst.on_processing_start(ev))
    if func == "on_processing_complete":
        outcome = NS["ProcessingOutcome"](args.get("outcome"))
        return asyncio.run(inst.on_processing_complete(ev, outcome))
    if func == "_stop_typing_indicator":
        return asyncio.run(inst._stop_typing_indicator(args.get("chat_id")))
    if func == "stop_typing":
        return asyncio.run(inst.stop_typing(args.get("chat_id")))
    if func == "send_typing":
        return asyncio.run(inst.send_typing(args.get("chat_id"), args.get("metadata")))
    if func == "_resolve_recipient":
        return asyncio.run(inst._resolve_recipient(args.get("chat_id")))
    if func == "_rpc":
        return asyncio.run(inst._rpc(args.get("method"), args.get("params"), args.get("rpc_id"),
                                     log_failures=args.get("log_failures", True),
                                     raise_on_rate_limit=args.get("raise_on_rate_limit", False),
                                     timeout=args.get("timeout", 30.0)))
    if func == "get_chat_info":
        return asyncio.run(inst.get_chat_info(args.get("chat_id")))
    if func == "send_image":
        return asyncio.run(inst.send_image(args.get("chat_id"), args.get("image_url"), args.get("caption")))
    if func == "_send_attachment":
        return asyncio.run(inst._send_attachment(args.get("chat_id"), args.get("file_path"),
                                                 args.get("media_label"), args.get("caption")))
    if func == "send":
        return asyncio.run(inst.send(args.get("chat_id"), args.get("content"),
                                     args.get("reply_to"), args.get("metadata")))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = copy.deepcopy(inp.get("args") or {})
    ctx, D, inst = setup_case(func, inp)
    ev = mat_event(args.get("event"), ctx) if func in EVENT_FUNCS else None
    state_out = {}
    try:
        ret = live_call(func, args, D, inst, ctx, ev)
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    if func in SELF_EMIT:
        state_out["self"] = vars(inst)
    if func in EVENT_EMIT:
        state_out["event"] = vars(ev)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
SEAM_NAMES = [
    "uuid.UUID", "_send_reaction_rpc", "cancel_task", "_with_target", "time.monotonic",
    "time.time", "client.post", "resp.raise_for_status", "resp.json",
    "_is_signal_rate_limit_error", "_extract_retry_after_seconds", "_looks_like_e164_number",
    "_resolve_image_path", "_send_file", "markdown_to_signal", "Path.stat", "_rpc_send",
    "logger.debug", "logger.warning", "logger.info", "logger.log",
    "_sig_secret", "_is_sender_authorized",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "signal_adapter_flow2.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'sig2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    c_names |= {f"logger.{lv}" for lv in re.findall(r'sig2_log\(\s*ctx,\s*"([a-z]+)"', csrc)}
    if "sig2_log_level(" in csrc:
        c_names.add("logger.log")
    if "sig2_sig_secret(" in csrc:
        c_names.add("_sig_secret")
    if "sig2_is_sender_authorized(" in csrc:
        c_names.add("_is_sender_authorized")
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_signal_adapter_flow2: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    unknown = c_names - registry
    if unknown:
        print(f"FATAL oracle_signal_adapter_flow2: C seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "signal_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_signal_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_signal_adapter_flow2: fixture funcs without a target: {sorted(badf)}")
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
    if shown >= 12:
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
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE signal_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "signal_adapter_flow2.jsonl"
    n = bad = 0
    counts = {}
    extraction_names = set(TARGETS) | set(SEAM_NAMES) | set(SEAMED_SIBLINGS)
    attr_whitelist = {"client", "http_url", "account", "_sent_message_timestamps",
                      "_account_normalized", "_recipient_uuid_by_number", "_recipient_number_by_uuid",
                      "dm_allow_from", "group_allow_from", "_typing_tasks", "_typing_failures",
                      "_typing_skip_until", "MAX_MESSAGE_LENGTH", "_recipient_cache_lock"}
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
        io = (case.get("in") or {}).get("io") or {}
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if io:
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            if "'SignalAdapter'" in msg:
                for nm in extraction_names:
                    if f"'{nm}'" in msg and nm not in attr_whitelist:
                        bad += 1
                        print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                        break
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST signal_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = set(TARGETS) - set(counts)
    if missing:
        print(f"SELFTEST MISSING FUNCS: {sorted(missing)}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()