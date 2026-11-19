#!/usr/bin/env python3
"""Oracle for the Signal adapter FLOW port
(src/gateway/platforms/signal_adapter_flow.c).

Drives the LIVE plugins/platforms/signal/adapter.py inbound SSE-line dispatch /
sync-message unwrap / group-mention gating / attachment-collection cluster, the
outbound recipient-routing / send-RPC / image-resolution / rate-limited
attachment-batch chain, and the sent-timestamp cache / send-validation /
quote-author / mime leaves with SCRIPTED seams: every SEAMED sibling method
(_rpc, _handle_envelope, _fetch_attachment, _resolve_recipient) and foreign
collaborator (client.aclose, time.time, time.monotonic, json.loads,
cache_image_from_url, Path.exists, Path.stat, _signal_send_timeout,
scheduler.acquire/feedback/report_rpc_duration, asyncio.sleep, logger.*) is a
recording stub fed the same scripts the C harness feeds its universal seam
sig_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the mutated
state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded, Assign
AND AnnAssign, plain-Name targets only; the SignalAdapter methods extracted with
decorators stripped; logger re-set silent AFTER seeding). SendResult (base.py)
and MessageType/ProcessingOutcome (event.py) are exec'd as the real
dataclass/enums; SignalRateLimitError + SIGNAL_RATE_LIMIT_MAX_ATTEMPTS come from
signal_rate_limit.py; mime_for_ext + DEFAULT_EXT_TO_MIME from media_cache.py.
Every exec namespace is a registered sys.modules module (py3.14). The heavy
quarry import chain (gateway.platforms.base / event / config, httpx, agent.i18n,
gateway.platforms._shared / helpers) is never imported; imported names are seeded
as placeholders / recorders.

Mode table (mirrors the C port): every ported function runs REAL (inline); only
the seamed siblings + foreign collaborators above are SEAMED. A REAL function
calls its REAL children directly and its SEAMED children through the recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec bare
(SignalRateLimitError carries retry_after). Summary:
'ORACLE signal_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- The adapter self double is a real type("SignalAdapter", (), clsdict) instance;
  a DIRECT self._attr read with the key absent raises AttributeError
  "'SignalAdapter' object has no attribute 'X'".
- _recent_sent_timestamps materializes as OrderedDict[int, float];
  _sent_message_timestamps as OrderedDict[str, None]; group/dm_allow_from stay
  lists (membership/truthiness only). client materializes as a ClientDouble
  (canon projects it back to its marker). tuple returns project to lists;
  SendResult via dataclasses.asdict (all 8 fields); integral floats == ints.
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
from collections import OrderedDict
from pathlib import Path
from urllib.parse import unquote as _real_unquote

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
MEDIA_SRC = os.path.join(Q, "gateway", "platforms", "media_cache.py")
_FLAGS = _fut.annotations.compiler_flag
MODNAME = "sig_oracle_adapter"

# module-level function ported REAL
MODULE_TARGETS = ["_ext_to_mime"]
# SignalAdapter methods ported REAL: name -> kind
METHOD_KINDS = {
    "_close_client": "method",
    "_handle_sse_line": "method",
    "_unwrap_sync_message": "method",
    "_apply_group_mention_rules": "method",
    "_collect_attachments": "method",
    "_group_allowed": "method",
    "_with_target": "method",
    "_rpc_send": "method",
    "_resolve_image_path": "method",
    "_send_attachment_batch": "method",
    "_send_file": "method",
    "_send_reaction_rpc": "method",
    "_validate_send_result": "method",
    "_track_sent_timestamp": "method",
    "_remember_sent_message_timestamp": "method",
    "_consume_sent_timestamp": "method",
    "_extract_quote_author": "staticmethod",
}
TARGETS = MODULE_TARGETS + list(METHOD_KINDS)
ASYNC_METHODS = {
    "_close_client", "_handle_sse_line", "_collect_attachments", "_with_target",
    "_rpc_send", "_resolve_image_path", "_send_attachment_batch", "_send_file",
    "_send_reaction_rpc",
}
# seamed sibling methods (claimed elsewhere; NOT ported here)
SEAMED_SIBLINGS = ["_rpc", "_handle_envelope", "_fetch_attachment", "_resolve_recipient"]
# every instance-method target echoes the self double
SELF_EMIT = {nm for nm, k in METHOD_KINDS.items() if k == "method"}


def seg(src, node):
    start = node.lineno
    if isinstance(node, ast.ClassDef):  # keep @dataclass etc. on classes
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
    import logging
    import time as _time
    from dataclasses import dataclass, field
    from typing import Any, Optional, Dict, List, Tuple
    ns.update({"re": re, "json": json, "os": os, "time": _time, "logging": logging,
               "asyncio": asyncio, "OrderedDict": OrderedDict, "dataclass": dataclass,
               "field": field, "Any": Any, "Optional": Optional, "Dict": Dict,
               "List": List, "Tuple": Tuple, "enum": enum, "Enum": enum.Enum,
               "unquote": _real_unquote, "logger": _SilentLogger()})
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # real MessageType / ProcessingOutcome enums (for module-constant seeding)
    esrc = open(EVENT_SRC, encoding="utf-8").read()
    for node in ast.parse(esrc).body:
        if isinstance(node, ast.ClassDef) and node.name in ("MessageType", "ProcessingOutcome"):
            exec_node(esrc, node, ns, EVENT_SRC)
    # real SendResult dataclass
    bsrc = open(BASE_SRC, encoding="utf-8").read()
    for node in ast.parse(bsrc).body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(bsrc, node, ns, BASE_SRC)
    # SignalRateLimitError + rate-limit constants
    rsrc = open(RATE_SRC, encoding="utf-8").read()
    rtree = ast.parse(rsrc)
    for node in rtree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SignalRateLimitError":
            exec_node(rsrc, node, ns, RATE_SRC)
    _seed_constants(rsrc, rtree, ns, RATE_SRC)
    # mime_for_ext + DEFAULT_EXT_TO_MIME (used by the REAL _ext_to_mime)
    msrc = open(MEDIA_SRC, encoding="utf-8").read()
    mtree = ast.parse(msrc)
    _seed_constants(msrc, mtree, ns, MEDIA_SRC)
    for node in mtree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "mime_for_ext":
            exec_node(msrc, node, ns, MEDIA_SRC)
    # adapter module constants
    _seed_constants(src, tree, ns, SRC)
    # module-level functions ported REAL
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_TARGETS:
            exec_node(src, node, ns, SRC)
    # SignalAdapter methods (decorators stripped; staticmethod re-wrapped by make_double)
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SignalAdapter")
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in METHOD_KINDS:
            clean = copy.copy(node)
            clean.decorator_list = []
            exec_node(src, clean, ns, SRC)
    ns["logger"] = _SilentLogger()
    required = TARGETS + [
        "SendResult", "SignalRateLimitError", "mime_for_ext", "DEFAULT_EXT_TO_MIME",
        "_QUOTE_AUTHOR_KEYS", "SIGNAL_MAX_ATTACHMENT_SIZE", "SIGNAL_RATE_LIMIT_MAX_ATTEMPTS",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_signal_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
# keep the REAL ported defs so per-case setup can restore them
REAL = {nm: NS[nm] for nm in TARGETS}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, enum.Enum):
            return x.value
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, ClientDouble):
            return conv(x._marker)
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
        e = NS["SignalRateLimitError"](msg, retry_after=spec.get("retry_after"))
        return e
    cls = BUILTIN.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C sig_call exactly) ──
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


class RecLogger:
    """logger.<level>(fmt, *args, **kw) -> scripted seam event; exc_info dropped."""
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log


# ── attribute / collaborator doubles ──
class ClientDouble:
    def __init__(self, marker, ctx):
        self._marker = marker
        self._ctx = ctx

    async def aclose(self):
        return self._ctx.call("client.aclose", {})


class SchedulerDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    async def acquire(self, n):
        return self._ctx.call("scheduler.acquire", {"n": n})

    def feedback(self, retry_after, n):
        return self._ctx.call("scheduler.feedback", {"retry_after": retry_after, "n": n})

    async def report_rpc_duration(self, duration, n):
        return self._ctx.call("scheduler.report_rpc_duration", {"duration": duration, "n": n})


class PathDouble:
    def __init__(self, path, ctx):
        self.path = path
        self._ctx = ctx

    def exists(self):
        return self._ctx.call("Path.exists", {"path": self.path})

    def stat(self):
        r = self._ctx.call("Path.stat", {"path": self.path})
        return types.SimpleNamespace(st_size=r["st_size"])


def mat_self_attr(k, v, ctx):
    if k == "_recent_sent_timestamps" and isinstance(v, dict):
        return OrderedDict((int(kk), vv) for kk, vv in v.items())
    if k == "_sent_message_timestamps" and isinstance(v, dict):
        return OrderedDict((kk, None) for kk in v)
    if k == "client":
        return None if v is None else ClientDouble(v, ctx)
    return v


# ── seamed sibling recorders (bound on the double) ──
def make_rpc_seam(ctx):
    async def _rpc(self, method, params, rpc_id=None, *, log_failures=True,
                   raise_on_rate_limit=False, timeout=30.0):
        return ctx.call("_rpc", {"method": method, "params": params, "rpc_id": rpc_id,
                                 "log_failures": log_failures,
                                 "raise_on_rate_limit": raise_on_rate_limit, "timeout": timeout})
    return _rpc


def make_handle_envelope_seam(ctx):
    async def _handle_envelope(self, envelope):
        return ctx.call("_handle_envelope", {"envelope": envelope})
    return _handle_envelope


def make_fetch_attachment_seam(ctx):
    async def _fetch_attachment(self, attachment_id):
        return ctx.call("_fetch_attachment", {"attachment_id": attachment_id})
    return _fetch_attachment


def make_resolve_recipient_seam(ctx):
    async def _resolve_recipient(self, chat_id):
        return ctx.call("_resolve_recipient", {"chat_id": chat_id})
    return _resolve_recipient


def make_double(state, ctx):
    clsdict = {}
    for name, kind in METHOD_KINDS.items():
        fn = NS[name]
        clsdict[name] = staticmethod(fn) if kind == "staticmethod" else fn
    clsdict["_rpc"] = make_rpc_seam(ctx)
    clsdict["_handle_envelope"] = make_handle_envelope_seam(ctx)
    clsdict["_fetch_attachment"] = make_fetch_attachment_seam(ctx)
    clsdict["_resolve_recipient"] = make_resolve_recipient_seam(ctx)
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
    NS["json"] = types.SimpleNamespace(
        loads=rec(ctx, "json.loads", lambda s, **k: {"data": s}),
        JSONDecodeError=json.JSONDecodeError)
    NS["asyncio"] = types.SimpleNamespace(
        sleep=rec_async(ctx, "asyncio.sleep", lambda delay: {"delay": delay}))
    NS["Path"] = lambda p: PathDouble(p, ctx)
    NS["cache_image_from_url"] = rec_async(
        ctx, "cache_image_from_url", lambda image_url: {"image_url": image_url})
    NS["_signal_send_timeout"] = rec(
        ctx, "_signal_send_timeout", lambda num_attachments: {"num_attachments": num_attachments})
    D, inst = make_double(state, ctx)
    return ctx, D, inst


# ── per-case live call ──
def live_call(func, args, D, inst, ctx):
    if func == "_ext_to_mime":
        return NS[func](args.get("ext"))
    if func == "_extract_quote_author":
        return D._extract_quote_author(args.get("quote_data"))
    if func == "_close_client":
        return asyncio.run(inst._close_client())
    if func == "_handle_sse_line":
        return asyncio.run(inst._handle_sse_line(args["line"]))
    if func == "_unwrap_sync_message":
        return inst._unwrap_sync_message(args.get("envelope_data"))
    if func == "_apply_group_mention_rules":
        return inst._apply_group_mention_rules(args.get("text"), args.get("data_message"))
    if func == "_collect_attachments":
        return asyncio.run(inst._collect_attachments(args.get("attachments_data")))
    if func == "_group_allowed":
        return inst._group_allowed(args.get("group_id"))
    if func == "_with_target":
        return asyncio.run(inst._with_target(args["params"], args["chat_id"],
                                             resolve=args.get("resolve", True)))
    if func == "_rpc_send":
        return asyncio.run(inst._rpc_send(args.get("params"), args.get("fail_error")))
    if func == "_resolve_image_path":
        return asyncio.run(inst._resolve_image_path(args["image_url"]))
    if func == "_send_attachment_batch":
        sched = args.get("scheduler")
        if sched is not None:
            sched = SchedulerDouble(ctx)
        return asyncio.run(inst._send_attachment_batch(sched, args.get("params"),
                                                       args.get("n"), args.get("label")))
    if func == "_send_file":
        return asyncio.run(inst._send_file(args["chat_id"], args["file_path"],
                                           args.get("caption"), args.get("fail_error")))
    if func == "_send_reaction_rpc":
        return asyncio.run(inst._send_reaction_rpc(args["chat_id"], args["params"]))
    if func == "_validate_send_result":
        return inst._validate_send_result(args.get("result"))
    if func == "_track_sent_timestamp":
        return inst._track_sent_timestamp(args.get("rpc_result"))
    if func == "_remember_sent_message_timestamp":
        return inst._remember_sent_message_timestamp(args.get("timestamp"))
    if func == "_consume_sent_timestamp":
        return inst._consume_sent_timestamp(args.get("ts"))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = copy.deepcopy(inp.get("args") or {})
    ctx, D, inst = setup_case(func, inp)
    state_out = {}
    try:
        ret = live_call(func, args, D, inst, ctx)
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    if func in SELF_EMIT:
        state_out["self"] = vars(inst)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
SEAM_NAMES = [
    "client.aclose", "time.time", "time.monotonic", "json.loads",
    "_handle_envelope", "_fetch_attachment", "_resolve_recipient", "_rpc",
    "cache_image_from_url", "Path.exists", "Path.stat", "_signal_send_timeout",
    "scheduler.acquire", "scheduler.feedback", "scheduler.report_rpc_duration",
    "asyncio.sleep", "logger.debug", "logger.warning", "logger.error",
    "logger.info", "logger.exception",
]
LOGGER_LEVELS = {"logger.debug", "logger.warning", "logger.error", "logger.info", "logger.exception"}


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "signal_adapter_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'sig_call\(\s*ctx,\s*"([^"]+)"', csrc))
    # logger.* is emitted dynamically via sig_log; account for the levels used
    c_names |= {f"logger.{lv}" for lv in re.findall(r'sig_log\(\s*ctx,\s*"([a-z]+)"', csrc)}
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_signal_adapter_flow: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    unknown = c_names - registry
    if unknown:
        print(f"FATAL oracle_signal_adapter_flow: C seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "signal_adapter_flow.jsonl"
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
            print(f"FATAL oracle_signal_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_signal_adapter_flow: fixture funcs without a target: {sorted(badf)}")
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
    print(f"ORACLE signal_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    fix = _REPO / "tests" / "fixtures" / "signal_adapter_flow.jsonl"
    n = bad = 0
    counts = {}
    extraction_names = set(TARGETS) | set(SEAM_NAMES) | set(SEAMED_SIBLINGS)
    attr_whitelist = {"client", "_account_normalized", "require_mention", "group_allow_from",
                      "account", "_recent_sent_timestamps", "_sent_message_timestamps",
                      "_recent_sent_ttl_seconds", "_max_recent_timestamps",
                      "_max_sent_message_timestamps", "_recipient_uuid_by_number"}
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
            for nm in extraction_names:
                if f"'{nm}'" in msg and nm not in attr_whitelist:
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST signal_adapter_flow: {n} cases, {bad} drive/script failures")
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