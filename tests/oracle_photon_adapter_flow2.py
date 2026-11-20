#!/usr/bin/env python3
"""Oracle for the Photon adapter FLOW remainder port
(src/gateway/platforms/photon_adapter_flow2.c).

Drives the LIVE plugins/platforms/photon/adapter.py second-wave clusters with
SCRIPTED seams: every SEAMED claimed flow1 sibling (_normalize_chat_key,
_reply_inner, _carries_voice, _reactions_enabled, _markdown_enabled,
_sidecar_try, _sidecar_send) and foreign import (get_hermes_home,
atomic_json_write, read_text, unlink, pid_exists, os.kill,
load_project_credentials, get_scoped_secret, seed_extra_from_env, coerce_port,
send_error, strip_markdown, voice_mention_gate_enabled,
rich_sent_store.record_async, rich_sent_store.lookup, guess_type, resp.json,
time.monotonic, time.time, task.done, task.cancel) is a recording stub fed the
same scripts the C harness feeds its universal seam ph2_call. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and the adapter state echo ("state").

The live defs are AST-extracted from adapter.py (module constants seeded; the
PhotonAdapter methods stripped of decorators and dedented; PhotonSidecarError
exec'd as a real class). Every exec namespace is a registered sys.modules module
(py3.14). The heavy quarry import chain (httpx / gateway.* / pm) is never
imported; in-function imports resolve per-case stub modules.

All 33 ported functions are REAL (run inline / as real methods on a PhotonAdapter
double class). Async methods drive with asyncio.run; their awaited seams are
async recorders. Paths are FakePath doubles (PurePosixPath algebra + seamed
unlink/read_text). Logger calls are SILENT on both sides.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec.
Summary: 'ORACLE photon_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc), ...}.
- Paths project to POSIX strings; tuples to JSON arrays.
- PhotonSidecarError (returned) projects to its field dict + str(e).
- Integral floats == ints (canon folds). dict comparison order-insensitive;
  trace event LIST order is semantic.
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import asyncio
import copy
import dataclasses
import enum
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "photon", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

MODULE_TARGETS = [
    "_runtime_record_path", "_legacy_runtime_record_path", "_current_runtime_record_path",
    "_write_runtime_record", "_read_runtime_record", "_delete_runtime_record",
    "_sidecar_pid_alive", "_sidecar_error_from_response", "_is_timeout_error",
    "validate_config", "is_connected", "_env_enablement", "_sent_text_key",
    "_record_sent_text_async", "_lookup_sent_text", "_guess_mime",
    "_standalone_error", "_standalone_token_from_record",
]
KINDS = {
    "_sidecar_url": "instance", "_sidecar_headers": "instance",
    "_note_upstream_activity": "instance", "_cancel_pending_fffc": "instance",
    "_gate_stt_chat": "instance", "_add_reaction": "instance",
    "_remove_reaction": "instance", "add_reaction": "instance",
    "remove_reaction": "instance", "on_processing_start": "instance",
    "get_chat_info": "instance", "format_message": "instance",
    "send_typing": "instance", "stop_typing": "instance",
    "_send_plain_fallback": "instance",
}
ASYNC_METHODS = {"_add_reaction", "_remove_reaction", "add_reaction", "remove_reaction",
                 "on_processing_start", "get_chat_info", "send_typing", "stop_typing",
                 "_send_plain_fallback"}
ASYNC_MODULE = {"_record_sent_text_async"}
TARGETS = MODULE_TARGETS + list(KINDS)
CLASS_CONSTS = ["MAX_MESSAGE_LENGTH"]


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    start = node.lineno
    if isinstance(node, ast.ClassDef):
        for d in getattr(node, "decorator_list", []):
            start = min(start, d.lineno)
    text = textwrap.dedent("\n".join(src.splitlines()[start - 1:node.end_lineno]))
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


def _seed_constants(src, tree, ns):
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass


# ── stub modules for in-function imports (created once, recorders per case) ──
HC_MOD = new_module("hermes_constants")
GW_MOD = new_module("gateway")
GW_MOD.__path__ = []
RSS_MOD = new_module("gateway.rich_sent_store")
STATUS_MOD = new_module("gateway.status")
GW_MOD.rich_sent_store = RSS_MOD
GW_MOD.status = STATUS_MOD
MIME_MOD = new_module("mimetypes")


class HttpxTimeoutException(Exception):
    pass


class HttpxConnectTimeout(HttpxTimeoutException):
    pass


class HttpxReadTimeout(HttpxTimeoutException):
    pass


class HttpxWriteTimeout(HttpxTimeoutException):
    pass


class HttpxPoolTimeout(HttpxTimeoutException):
    pass


def extract():
    mod = new_module("ph2_oracle_adapter")
    ns = mod.__dict__
    import logging
    import contextlib
    from typing import Any, Optional, Callable, Dict, List, Tuple
    ns.update({"re": re, "json": json, "os": os, "logging": logging,
               "contextlib": contextlib, "asyncio": asyncio,
               "Any": Any, "Optional": Optional, "Callable": Callable,
               "Dict": Dict, "List": List, "Tuple": Tuple,
               "logger": _SilentLogger(), "Path": PurePosixPath})
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "PhotonSidecarError":
            exec_node(src, node, ns, SRC)
    _seed_constants(src, tree, ns)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name in MODULE_TARGETS:
                exec_node(src, node, ns, SRC)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "PhotonAdapter")
    for node in cls.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(nm in CLASS_CONSTS for nm in names):
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, SRC)
    _seed_constants(src, tree, ns)
    ns["logger"] = _SilentLogger()
    required = TARGETS + CLASS_CONSTS + [
        "PhotonSidecarError", "_RUNTIME_RECORD_NAME", "_TARGET_NOT_ALLOWED_MESSAGE",
        "_DEFAULT_SIDECAR_PORT", "_DEFAULT_SIDECAR_BIND", "_TYPING_COOLDOWN_SECONDS",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_photon_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── attribute / path / task / resp doubles ──
class FakePath:
    def __init__(self, s):
        self.p = PurePosixPath(s if s is not None else "")
        self.s = str(self.p)

    def __truediv__(self, name):
        return FakePath(str(self.p / name))

    @property
    def parent(self):
        return FakePath(str(self.p.parent))

    @property
    def name(self):
        return self.p.name

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s


class TaskDouble:
    def __init__(self, tid, ctx):
        self.tid = tid
        self._ctx = ctx

    def done(self):
        return self._ctx.call("task.done", {"task": self.tid})

    def cancel(self):
        return self._ctx.call("task.cancel", {"task": self.tid})


class RespDouble:
    def __init__(self, d, ctx):
        self.status_code = d.get("status_code", 200)
        self.text = d.get("text", "")
        self._ctx = ctx

    def json(self):
        return self._ctx.call("resp.json", {})


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, FakePath):
            return str(x)
        if isinstance(x, TaskDouble):
            return x.tid
        if isinstance(x, PurePosixPath):
            return str(x)
        if type(x).__name__ == "PhotonSidecarError":
            return {"__exc__": "PhotonSidecarError", "message": str(x), "path": x.path,
                    "status_code": x.status_code, "error": x.error,
                    "error_class": x.error_class, "retryable": x.retryable}
        if isinstance(x, enum.Enum):
            return x.value
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
BUILTIN = {
    "Exception": Exception, "BaseException": BaseException, "OSError": OSError,
    "TimeoutError": TimeoutError, "asyncio.TimeoutError": TimeoutError,
    "ConnectionError": ConnectionError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "AttributeError": AttributeError,
    "TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "KeyError": KeyError,
    "IndexError": IndexError, "LookupError": LookupError, "OverflowError": OverflowError,
    "StopIteration": StopIteration, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "UnicodeError": UnicodeError, "JSONDecodeError": json.JSONDecodeError,
    "CancelledError": type("CancelledError", (Exception,), {}),
    "TimeoutException": HttpxTimeoutException, "ConnectTimeout": HttpxConnectTimeout,
    "ReadTimeout": HttpxReadTimeout, "WriteTimeout": HttpxWriteTimeout,
    "PoolTimeout": HttpxPoolTimeout,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = BUILTIN.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ph2_call exactly) ──
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
            ret = used.get("ret")
        else:
            ret = used
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


# ── seam argument projections (one per C ph2_call name) ──
ARGMAP = {
    "get_hermes_home": lambda: {},
    "unlink": lambda path, missing_ok=False: {"path": str(path), "missing_ok": missing_ok},
    "atomic_json_write": lambda path, data, *, indent=2, mode=None, **k: {
        "path": str(path), "data": data, "indent": indent, "mode": mode},
    "read_text": lambda path, encoding=None, **k: {"path": str(path), "encoding": encoding},
    "pid_exists": lambda pid: {"pid": pid},
    "os.kill": lambda pid, sig: {"pid": pid, "sig": sig},
    "load_project_credentials": lambda: {},
    "get_scoped_secret": lambda name, default=None, **k: {"name": name, "default": default},
    "seed_extra_from_env": lambda spec, *, home_env=None, home_default="": {
        "spec": list(spec), "home_env": home_env},
    "coerce_port": lambda value, default: {"value": value, "default": default},
    "send_error": lambda message, *, nothing_sent=False: {"message": message},
    "normalize_chat_key": lambda chat_id: {"chat_id": chat_id},
    "reply_inner": lambda content: {"content": content},
    "carries_voice": lambda content: {"content": content},
    "voice_mention_gate_enabled": lambda extra: {"extra": extra},
    "reactions_enabled": lambda: {},
    "markdown_enabled": lambda: {},
    "sidecar_try": lambda path, body, what: {"path": path, "body": body, "what": what},
    "sidecar_send": lambda space_id, text, *, richlink=True, markdown=True: {
        "space_id": space_id, "text": text, "richlink": richlink, "markdown": markdown},
    "strip_markdown": lambda text, *, keep_link_targets=False: {
        "text": text, "keep_link_targets": keep_link_targets},
    "rich_sent_store.record_async": lambda chat_id, message_id, text: {
        "chat_id": chat_id, "message_id": message_id, "text": text},
    "rich_sent_store.lookup": lambda chat_id, message_id: {
        "chat_id": chat_id, "message_id": message_id},
    "guess_type": lambda path, strict=True: {"path": path},
    "resp.json": lambda: {},
    "time.monotonic": lambda: {},
    "time.time": lambda: {},
    "task.done": lambda: {},
    "task.cancel": lambda: {},
}
SEAMED = set(ARGMAP)


def install_recorders(ctx, args):
    NS["_get_scoped_secret"] = rec(ctx, "get_scoped_secret", ARGMAP["get_scoped_secret"])
    NS["_seed_extra_from_env"] = rec(ctx, "seed_extra_from_env", ARGMAP["seed_extra_from_env"])
    NS["_coerce_port"] = rec(ctx, "coerce_port", ARGMAP["coerce_port"])
    NS["send_error"] = rec(ctx, "send_error", ARGMAP["send_error"])
    NS["strip_markdown"] = rec(ctx, "strip_markdown", ARGMAP["strip_markdown"])
    NS["voice_mention_gate_enabled"] = rec(ctx, "voice_mention_gate_enabled",
                                           ARGMAP["voice_mention_gate_enabled"])
    NS["load_project_credentials"] = rec(ctx, "load_project_credentials",
                                         ARGMAP["load_project_credentials"])
    NS["atomic_json_write"] = rec(ctx, "atomic_json_write", ARGMAP["atomic_json_write"])
    NS["_reply_inner"] = rec(ctx, "reply_inner", ARGMAP["reply_inner"])
    NS["_carries_voice"] = rec(ctx, "carries_voice", ARGMAP["carries_voice"])
    NS["_markdown_enabled"] = rec(ctx, "markdown_enabled", ARGMAP["markdown_enabled"])
    NS["os"] = types.SimpleNamespace(
        name=args.get("os_name", "nt"), kill=rec(ctx, "os.kill", ARGMAP["os.kill"]))
    NS["time"] = types.SimpleNamespace(
        monotonic=rec(ctx, "time.monotonic", ARGMAP["time.monotonic"]),
        time=rec(ctx, "time.time", ARGMAP["time.time"]))
    NS["HTTPX_AVAILABLE"] = args.get("httpx_available", True)
    NS["httpx"] = types.SimpleNamespace(TimeoutException=HttpxTimeoutException)

    def get_hermes_home_rec():
        raw = ctx.call("get_hermes_home", {})
        return FakePath(raw)
    HC_MOD.get_hermes_home = get_hermes_home_rec
    RSS_MOD.record_async = rec_async(ctx, "rich_sent_store.record_async",
                                     ARGMAP["rich_sent_store.record_async"])
    RSS_MOD.lookup = rec(ctx, "rich_sent_store.lookup", ARGMAP["rich_sent_store.lookup"])
    STATUS_MOD._pid_exists = rec(ctx, "pid_exists", ARGMAP["pid_exists"])
    MIME_MOD.guess_type = rec(ctx, "guess_type", ARGMAP["guess_type"])


def make_double(state, ctx):
    clsdict = {}
    for name, kind in KINDS.items():
        fn = NS[name]
        if kind == "classmethod":
            clsdict[name] = classmethod(fn)
        elif kind == "staticmethod":
            clsdict[name] = staticmethod(fn)
        else:
            clsdict[name] = fn
    for cname in CLASS_CONSTS:
        clsdict[cname] = NS[cname]
    # claimed flow1 siblings -> seams
    clsdict["_normalize_chat_key"] = staticmethod(rec(ctx, "normalize_chat_key",
                                                      ARGMAP["normalize_chat_key"]))
    clsdict["_reactions_enabled"] = staticmethod(rec(ctx, "reactions_enabled",
                                                     ARGMAP["reactions_enabled"]))
    clsdict["_sidecar_try"] = staticmethod(rec_async(ctx, "sidecar_try", ARGMAP["sidecar_try"]))
    clsdict["_sidecar_send"] = staticmethod(rec_async(ctx, "sidecar_send", ARGMAP["sidecar_send"]))
    D = type("PhotonAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        if k == "_pending_fffc":
            mat = {}
            for ck, entry in (v or {}).items():
                ts, tid = entry[0], entry[1]
                mat[ck] = [ts, TaskDouble(tid, ctx) if tid else None]
            setattr(inst, k, mat)
        elif k == "config":
            setattr(inst, k, types.SimpleNamespace(extra=copy.deepcopy((v or {}).get("extra"))))
        else:
            setattr(inst, k, copy.deepcopy(v))
    return D, inst


def cfg_double(v):
    return types.SimpleNamespace(extra=copy.deepcopy((v or {}).get("extra")))


def event_double(v):
    v = v or {}
    src = v.get("source")
    src_d = types.SimpleNamespace(**(src or {})) if isinstance(src, dict) else src
    return types.SimpleNamespace(source=src_d, message_id=v.get("message_id"))


# ── per-case live dispatch ──
def live_module(func, args):
    f = NS[func]
    if func in ("_runtime_record_path", "_legacy_runtime_record_path",
                "_current_runtime_record_path", "_read_runtime_record",
                "_delete_runtime_record", "_env_enablement"):
        return f()
    if func == "_write_runtime_record":
        return f(args.get("port", 0), args.get("token", ""), args.get("pid", 0))
    if func == "_sidecar_pid_alive":
        return f(args.get("pid"))
    if func == "_sidecar_error_from_response":
        return f(args.get("path", ""), args.get("status_code", 0), args.get("text", ""),
                 args.get("data"))
    if func == "_is_timeout_error":
        return f(make_exc(args.get("exc") or {}))
    if func in ("validate_config", "is_connected"):
        return f(cfg_double(args.get("cfg")))
    if func == "_sent_text_key":
        return f(args.get("chat_id"))
    if func == "_record_sent_text_async":
        return asyncio.run(f(args.get("chat_id"), args.get("message_id"), args.get("text")))
    if func == "_lookup_sent_text":
        return f(args.get("chat_id"), args.get("message_id"))
    if func == "_guess_mime":
        return f(args.get("path", ""))
    if func == "_standalone_error":
        return f(RespDouble(args.get("resp") or {}, _CUR_CTX[0]))
    if func == "_standalone_token_from_record":
        return f(args.get("port", 8789))
    raise RuntimeError(f"unknown module func {func}")


def live_method(func, args, D, inst):
    if func == "_sidecar_url":
        return inst._sidecar_url(args.get("path", ""))
    if func == "_sidecar_headers":
        return inst._sidecar_headers()
    if func == "_note_upstream_activity":
        return inst._note_upstream_activity()
    if func == "_cancel_pending_fffc":
        return inst._cancel_pending_fffc(args.get("chat_key", ""))
    if func == "_gate_stt_chat":
        return inst._gate_stt_chat(args.get("event") or {})
    if func == "_add_reaction":
        return asyncio.run(inst._add_reaction(args.get("chat_id", ""), args.get("message_id", ""),
                                              args.get("emoji", "")))
    if func == "_remove_reaction":
        return asyncio.run(inst._remove_reaction(args.get("chat_id", ""), args.get("message_id", "")))
    if func == "add_reaction":
        return asyncio.run(inst.add_reaction(args.get("chat_id", ""), args.get("emoji", ""),
                                             args.get("message_id")))
    if func == "remove_reaction":
        return asyncio.run(inst.remove_reaction(args.get("chat_id", ""), args.get("message_id")))
    if func == "on_processing_start":
        return asyncio.run(inst.on_processing_start(event_double(args.get("event"))))
    if func == "get_chat_info":
        return asyncio.run(inst.get_chat_info(args.get("chat_id", "")))
    if func == "format_message":
        return inst.format_message(args.get("content"))
    if func == "send_typing":
        return asyncio.run(inst.send_typing(args.get("chat_id", "")))
    if func == "stop_typing":
        return asyncio.run(inst.stop_typing(args.get("chat_id", "")))
    if func == "_send_plain_fallback":
        return asyncio.run(inst._send_plain_fallback(args.get("chat_id", ""), args.get("content"),
                                                     reply_to=args.get("reply_to"),
                                                     metadata=args.get("metadata")))
    raise RuntimeError(f"unknown method {func}")


_CUR_CTX = [None]


def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX[0] = ctx
    install_recorders(ctx, args)
    D, inst = make_double(state, ctx)
    NS["PhotonAdapter"] = D
    state_out = {}
    if func in MODULE_TARGETS:
        try:
            ret = live_module(func, args)
        except BaseException as exc:
            ret = exc_spec(exc, ctx)
    else:
        try:
            ret = live_method(func, args, D, inst)
        except BaseException as exc:
            ret = exc_spec(exc, ctx)
        state_out.update({k: getattr(inst, k) for k in state})
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "photon_adapter_flow2.c",
                encoding="utf-8").read()
    c_seams = set(re.findall(r'ph2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != SEAMED:
        print(f"FATAL oracle_photon_adapter_flow2: C seams {sorted(c_seams)} != ARGMAP {sorted(SEAMED)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "photon_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_photon_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(TARGETS)
        if unbound:
            print(f"FATAL oracle_photon_adapter_flow2: fixture funcs missing from TARGETS: {sorted(unbound)}")
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
    print(f"ORACLE photon_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "photon_adapter_flow2.jsonl"
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError" and \
                any(f"'{nm}'" in str(r.get("message", "")) for nm in TARGETS):
            bad += 1
            print(f"SELFTEST BINDING GAP {func} case {n}: {r['message']}")
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not (case.get("in") or {}).get("expect_fail_closed") and not case.get("fc"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST photon_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = set(TARGETS) - set(counts)
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {sorted(missing)}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()