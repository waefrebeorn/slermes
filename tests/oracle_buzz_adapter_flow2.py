#!/usr/bin/env python3
"""Oracle for the Buzz adapter FLOW remainder port
(src/gateway/platforms/buzz_adapter_flow2.c).

Drives the LIVE plugins/platforms/buzz/adapter.py media-ref / attachment
cluster, identity / CLI-resolution cluster, websocket URL / task leaves,
send-result / connect-failure leaves and the async CLI plumbing flows with
SCRIPTED seams: every SEAMED collaborator (flow1-claimed siblings, urllib
urlsplit/urlunsplit, shutil.which, Path ops, _nostr_auth.parse_auth_tag,
_shared_scoped_secret, logger, asyncio task ops, _exec_buzz) is a recording
stub fed the same scripts the C harness feeds its universal seam ba2_call.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the mutated self double ("state").

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; BuzzAdapter methods extracted
with decorators stripped; logger re-set AFTER seeding). The bech32 helpers
(_BECH32_CHARSET / _BECH32_GENERATOR / _bech32_polymod / _bech32_hrp_expand /
_convertbits) are extracted from gateway/authz_mixin.py; SendResult from
gateway/platforms/base.py (dataclass). Every exec namespace is a registered
sys.modules module (py3.14). The heavy quarry import chain is never imported.

Async methods are driven with asyncio.run over async recording stubs; the C
port is synchronous (seams model awaited results). Trace order is identical.

Mode table (mirrors the C port): flow2 siblings run REAL (inline); flow1
siblings + foreign collaborators are SEAMED (regex-extracted from C ba2_call
sites). Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper
catches BaseException (SystemExit included); scripted raises round-trip bare.
Summary: 'ORACLE buzz_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- The adapter self double is a real type("BuzzAdapter", (), clsdict) instance;
  a DIRECT self._attr read with the key absent raises AttributeError.
- urlsplit seam results materialize as ParsedDouble (.port property raises
  ValueError on port_error); args.parsed (_effective_port) is the same shape.
- SendResult projects via dataclasses.asdict to its full field dict.
- tuple/generator returns project to lists. Integral floats == ints (canon).
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import asyncio
import contextlib
import copy
import dataclasses
import inspect
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from collections import OrderedDict
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "buzz", "adapter.py")
AUTHZ_SRC = os.path.join(Q, "gateway", "authz_mixin.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
_FLAGS = _fut.annotations.compiler_flag

# module-level functions ported REAL
MODULE_TARGETS = [
    "_get_scoped_secret", "_escape_unresolved_presentation_mention",
    "_safe_attachment_filename", "_attachment_origin", "_effective_port",
    "_is_relay_media_url", "_find_relay_media_refs", "_replace_media_refs",
    "hex_to_npub", "_resolve_cli_path", "_configured_cli_path",
    "_resolve_auth_tag", "_consume_ws_read_task",
]
# BuzzAdapter methods ported REAL: name -> kind
METHOD_KINDS = {
    "_parse_imeta_attachments": "staticmethod",
    "_imeta_attachments": "staticmethod",
    "_websocket_url": "method",
    "_send_result": "method",
    "_connect_failed": "method",
    "_run_cli": "method",
    "_cli_json": "method",
    "send_reaction": "method",
    "edit_message": "method",
    "delete_message": "method",
    "get_chat_info": "method",
    "_resolve_user_name": "method",
}
TARGETS = MODULE_TARGETS + list(METHOD_KINDS)
ASYNC_TARGETS = {"_run_cli", "_cli_json", "send_reaction", "edit_message",
                 "delete_message", "get_chat_info", "_resolve_user_name"}
# seamed method collaborators bound on the double class (flow1 / base)
SEAMED_METHODS = {"_mark_seen": "self._mark_seen", "_set_fatal_error": "self._set_fatal_error"}

# state-emission flags (mirror the harness): bit0 self
SELF_EMIT = {nm for nm, k in METHOD_KINDS.items() if k == "method"}

BECH32_NAMES = ["_BECH32_CHARSET", "_BECH32_GENERATOR", "_bech32_polymod",
                "_bech32_hrp_expand", "_convertbits"]


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


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module("ba2_oracle_adapter")
    ns = mod.__dict__
    import logging
    import time as _time
    import shutil as _shutil
    ns.update({"re": re, "json": json, "os": os, "time": _time, "logging": logging,
               "asyncio": asyncio, "contextlib": contextlib, "shutil": _shutil,
               "OrderedDict": OrderedDict, "Path": PurePosixPath,
               "dataclass": dataclasses.dataclass, "field": dataclasses.field,
               "logger": _SilentLogger()})
    # imported names used by ported defs (seeded; recorders installed per case)
    ns["npub_to_hex"] = None
    ns["_profile_scoped"] = None
    ns["_shared_scoped_secret"] = None
    ns["_nostr_auth"] = None
    ns["Any"] = object
    ns["Optional"] = object
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module constants (plain-Name Assign / AnnAssign targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # module-level functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, SRC)
    # BuzzAdapter methods
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "BuzzAdapter":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHOD_KINDS:
                    clean = copy.copy(sub)
                    clean.decorator_list = []
                    exec_node(src, clean, ns, SRC)
    ns["logger"] = _SilentLogger()
    # bech32 helpers from gateway/authz_mixin.py
    asrc = open(AUTHZ_SRC, encoding="utf-8").read()
    atree = ast.parse(asrc)
    for node in atree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in BECH32_NAMES for t in node.targets):
            exec_node(asrc, node, ns, AUTHZ_SRC)
        if isinstance(node, ast.FunctionDef) and node.name in BECH32_NAMES:
            exec_node(asrc, node, ns, AUTHZ_SRC)
    # SendResult dataclass from gateway/platforms/base.py (keep decorator)
    bsrc = open(BASE_SRC, encoding="utf-8").read()
    btree = ast.parse(bsrc)
    for node in btree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(bsrc, node, ns, BASE_SRC)
    required = TARGETS + BECH32_NAMES + [
        "_UNRESOLVED_MENTION_ERROR_RE", "_BUZZ_PRESENTATION_MENTION_SEPARATOR",
        "_HEX64_RE", "_MAX_ATTACHMENT_FILENAME_BYTES", "_MEDIA_URL_PATTERN",
        "_MARKDOWN_MEDIA_RE", "_BARE_MEDIA_RE", "_MEDIA_PATH_RE",
        "_MAX_INBOUND_ATTACHMENTS", "_MAX_INBOUND_ATTACHMENT_BYTES", "SendResult",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_buzz_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── SEAMED names: regex-extracted from the C port's ba2_call sites ──
def seam_names_from_c():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "buzz_adapter_flow2.c",
                encoding="utf-8").read()
    return set(re.findall(r'ba2_call\(\s*ctx,\s*"([^"]+)"', csrc))


SEAMED = seam_names_from_c()
FOREIGN_SEAMS = {
    "urlsplit", "urlunsplit", "shutil.which", "expanduser", "is_file", "Path.home",
    "parse_auth_tag", "task.cancelled", "task.exception", "logger.error",
    "logger.debug", "_exec_buzz", "_shared_scoped_secret",
    "self._mark_seen", "self._set_fatal_error",
}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if isinstance(x, ParsedDouble):
            return conv(x._d)
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection (FULL builtin hierarchy) ──
EXC_CLASSES = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError, "BaseExceptionGroup": BaseExceptionGroup,
    "Exception": Exception, "ExceptionGroup": ExceptionGroup,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError, "SystemError": SystemError,
    "TypeError": TypeError, "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "RuntimeWarning": RuntimeWarning,
    "SyntaxWarning": SyntaxWarning, "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning, "JSONDecodeError": json.JSONDecodeError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ba2_call exactly) ──
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


# ── parsed (urlsplit) double ──
class ParsedDouble:
    def __init__(self, d):
        self._d = d
        for k in ("scheme", "netloc", "path", "query", "fragment",
                  "hostname", "username", "password"):
            setattr(self, k, d.get(k))

    @property
    def port(self):
        pe = self._d.get("port_error")
        if pe is not None:
            raise ValueError(pe)
        return self._d.get("port")


def mat_parsed(d):
    return ParsedDouble(d)


def mat_state(sv):
    out = {}
    if "chat_type" in sv:
        out["chat_type"] = sv["chat_type"]
    if "last_ts" in sv:
        out["last_ts"] = sv["last_ts"]
    if "seen" in sv:
        out["seen"] = OrderedDict((k, None) for k in sv["seen"])
    if "event_meta" in sv:
        out["event_meta"] = OrderedDict(
            (k, tuple(x) if isinstance(x, list) else x) for k, x in sv["event_meta"].items())
    return out


def mat_self_attr(k, v):
    if k == "_channel_state" and isinstance(v, dict):
        return {cid: mat_state(sv) for cid, sv in v.items()}
    if k == "_thread_roots" and isinstance(v, dict):
        return OrderedDict(v.items())
    return v


# ── seam argument projections / result builders ──
ARGMAP = {
    "_shared_scoped_secret": lambda name, default=None, external_fallback=False: {
        "name": name, "default": default, "external_fallback": external_fallback},
    "urlsplit": lambda url: {"url": url},
    "urlunsplit": lambda parts: {"parts": list(parts)},
    "shutil.which": lambda name: {"name": name},
    "expanduser": lambda self: {"path": str(self)},
    "is_file": lambda self: {"path": str(self)},
    "Path.home": lambda: {},
    "parse_auth_tag": lambda raw, label: {"raw": raw, "label": label},
    "task.cancelled": lambda: {},
    "task.exception": lambda: {},
    "logger.error": lambda fmt, *a: {"fmt": fmt, "args": list(a)},
    "logger.debug": lambda fmt, *a: {"fmt": fmt, "args": list(a)},
    "_exec_buzz": lambda cli_path, args, *, relay_url, private_key, auth_tag="",
                         input_text=None, timeout=30.0: {
        "cli_path": cli_path, "args": args, "relay_url": relay_url,
        "private_key": private_key, "auth_tag": auth_tag, "input_text": input_text},
    "_scoped_platform_setting": lambda env_name, extra, key: {
        "env_name": env_name, "extra": extra, "key": key},
    "_cli_error_message": lambda stderr, returncode, *, redact_path=None: {
        "stderr": stderr, "returncode": returncode, "redact_path": redact_path},
    "_parse_send_receipt": lambda stdout: {"stdout": stdout},
    "_json_or": lambda text, default: {"text": text, "default": default},
    "_parse_json_list": lambda stdout: {"stdout": stdout},
    "_resolve_private_key": lambda extra=None: {"extra": extra},
    "_resolve_credentials_data": lambda extra=None: {"extra": extra},
    "_configured_credentials_file": lambda extra: {"extra": extra},
    "self._mark_seen": lambda channel_id, event_id: {"channel_id": channel_id, "event_id": event_id},
    "self._set_fatal_error": lambda code, detail, retryable=False: {
        "code": code, "detail": detail, "retryable": retryable},
}
BUILD = {
    "urlsplit": lambda r, ctx: mat_parsed(r),
}


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(raw, ctx) if build else raw
    return f


def make_async_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    async def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(raw, ctx) if build else raw
    return f


class RecLogger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        nm = f"logger.{level}"
        if nm in SEAMED:
            return make_recorder(self._ctx, nm)
        def silent(*a, **k):
            return None
        return silent


def install_recorders(ctx, target):
    for nm in SEAMED:
        if nm == "urlsplit":
            NS["urlsplit"] = make_recorder(ctx, nm)
        elif nm == "urlunsplit":
            NS["urlunsplit"] = make_recorder(ctx, nm)
        elif nm == "shutil.which":
            NS["shutil"] = types.SimpleNamespace(which=make_recorder(ctx, nm))
        elif nm == "expanduser":
            PurePosixPath.expanduser = lambda self: PurePosixPath(ctx.call("expanduser", {"path": str(self)}))
        elif nm == "is_file":
            PurePosixPath.is_file = lambda self: ctx.call("is_file", {"path": str(self)})
        elif nm == "Path.home":
            PurePosixPath.home = staticmethod(lambda: PurePosixPath(ctx.call("Path.home", {})))
        elif nm == "parse_auth_tag":
            NS["_nostr_auth"] = types.SimpleNamespace(parse_auth_tag=make_recorder(ctx, nm))
        elif nm in ("task.cancelled", "task.exception"):
            continue  # bound on the task double in live_call
        elif nm in ("logger.error", "logger.debug"):
            continue  # RecLogger bound below
        elif nm == "_exec_buzz":
            NS["_exec_buzz"] = make_async_recorder(ctx, nm)
        elif nm.startswith("self."):
            continue  # bound on the adapter double class
        elif nm in NS and nm != target:
            NS[nm] = make_recorder(ctx, nm)
    NS["logger"] = RecLogger(ctx)


# ── adapter double ──
def make_double(state, ctx):
    clsdict = {}
    for name, kind in METHOD_KINDS.items():
        fn = NS[name]
        clsdict[name] = staticmethod(fn) if kind == "staticmethod" else fn
    for live_nm, seam_nm in SEAMED_METHODS.items():
        clsdict[live_nm] = staticmethod(make_recorder(ctx, seam_nm))
    D = type("BuzzAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_self_attr(k, copy.deepcopy(v)))
    return D, inst


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── per-case live call ──
def live_call(func, args, D, inst, ctx):
    if func in MODULE_TARGETS:
        f = NS[func]
        if func == "_get_scoped_secret":
            return f(args["name"], args.get("default"))
        if func == "_escape_unresolved_presentation_mention":
            return f(args["content"], args["error"])
        if func == "_safe_attachment_filename":
            return f(args["value"])
        if func == "_attachment_origin":
            return f(args["value"])
        if func == "_effective_port":
            return f(mat_parsed(args["parsed"]))
        if func == "_is_relay_media_url":
            return f(args["url"], args["relay_url"])
        if func == "_find_relay_media_refs":
            return f(args["text"], args["relay_url"])
        if func == "_replace_media_refs":
            return f(args["text"], [tuple(r) for r in args["replacements"]])
        if func == "hex_to_npub":
            return f(args["pubkey_hex"])
        if func == "_resolve_cli_path":
            return f(args.get("configured", ""))
        if func == "_configured_cli_path":
            return f(args["extra"])
        if func == "_resolve_auth_tag":
            return f(args.get("extra"))
        if func == "_consume_ws_read_task":
            task = types.SimpleNamespace(
                cancelled=make_recorder(ctx, "task.cancelled"),
                exception=make_recorder(ctx, "task.exception"))
            return f(task)
    # methods
    if func == "_parse_imeta_attachments":
        return D._parse_imeta_attachments(args["event"])
    if func == "_imeta_attachments":
        return D._imeta_attachments(args["event"])
    if func == "_websocket_url":
        return inst._websocket_url()
    if func == "_send_result":
        return inst._send_result(args["chat_id"], args["code"], args["out"], args["err"],
                                 redact_path=args.get("redact_path"))
    if func == "_connect_failed":
        return inst._connect_failed(args["code"], args["detail"], args["log"],
                                    *args.get("log_args", []), retryable=args.get("retryable", False))
    if func == "_run_cli":
        return inst._run_cli(args["args"], input_text=args.get("input_text"))
    if func == "_cli_json":
        return inst._cli_json(args["args"], args["default"])
    if func == "send_reaction":
        return inst.send_reaction(args["chat_id"], args["message_id"], args["emoji"])
    if func == "edit_message":
        return inst.edit_message(args["chat_id"], args["message_id"], args["content"],
                                 finalize=args.get("finalize", False))
    if func == "delete_message":
        return inst.delete_message(args["chat_id"], args["message_id"])
    if func == "get_chat_info":
        return inst.get_chat_info(args["chat_id"])
    if func == "_resolve_user_name":
        return inst._resolve_user_name(args["pubkey"])
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = copy.deepcopy(inp.get("args") or {})
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    D, inst = make_double(inp.get("state"), ctx)
    NS["BuzzAdapter"] = D  # _imeta_attachments reads BuzzAdapter._parse_imeta_attachments
    state_out = {}
    try:
        ret = live_call(func, args, D, inst, ctx)
        if inspect.iscoroutine(ret):
            ret = asyncio.run(ret)
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    if func in SELF_EMIT:
        state_out["self"] = vars(inst)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_buzz_adapter_flow2: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_buzz_adapter_flow2: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    missing_def = sorted(nm for nm in SEAMED if nm not in FOREIGN_SEAMS and nm not in NS)
    if missing_def:
        print(f"FATAL oracle_buzz_adapter_flow2: seams without live def: {missing_def}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "buzz_adapter_flow2.jsonl"
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
            print(f"FATAL oracle_buzz_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_buzz_adapter_flow2: fixture funcs without a target: {sorted(badf)}")
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
    print(f"ORACLE buzz_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "buzz_adapter_flow2.jsonl"
    n = bad = 0
    counts = {}
    extraction_names = set(TARGETS) | SEAMED
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
        if isinstance(r, dict) and r.get("__raise__") == "NameError":
            bad += 1
            print(f"SELFTEST NAMEERROR {func} case {n}: {r.get('message')}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in extraction_names:
                if f"'{nm}'" in msg and nm not in (
                        "_self_pubkey", "_self_npub", "_display_name", "_channel_meta",
                        "_channel_state", "_channel_names", "_thread_roots",
                        "_restored_cursors", "_reply_to_mode", "_private_key", "_auth_tag",
                        "_extra", "cli_path", "relay_url", "_user_names"):
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST buzz_adapter_flow2: {n} cases, {bad} drive/script failures")
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