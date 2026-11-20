#!/usr/bin/env python3
"""Oracle for the WhatsApp adapter session/token/auth/env FLOW remainder
port (src/gateway/platforms/whatsapp_adapter_flow2.c).

Drives the LIVE plugins/platforms/whatsapp/adapter.py second-wave cluster
with SCRIPTED seams: every SEAMED sibling call (claimed flow1 siblings,
ported flow2 siblings when composed, and every foreign import: aiohttp /
secrets / tempfile / msvcrt / fcntl / psutil / pm.* / tools.* /
gateway.rich_sent_store / os.* / open / logger / print) is a recording stub
fed the same scripts the C harness feeds its universal seam wa2_call.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"}
in "trace") plus the final return ("ret") and state: method targets echo
the fixture-present self keys (mutated in place, on success AND raise);
module targets echo {}.

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; MessageType is a synthetic
enum with the quarry member values; logger re-set AFTER seeding; the real
SendResult dataclass is AST-extracted from gateway/platforms/base.py).
Every exec namespace is a registered sys.modules module (py3.14
dataclasses). The heavy quarry import chain is never imported; in-function
imports bind stub modules installed below. Paths are PathDouble
(PurePosixPath) doubles: resolve()/is_relative_to()/stat()/mkdir() are
seams (no real filesystem); os.path is posixpath (POSIX lexical domain).

Mode table (mirrors the C port): _owner_only_opener is REAL-ALWAYS (passed
as the open(opener=) value, projected by identity); _bridge_url is a real
transcribed leaf bound on the self double; every other ported function is
SEAMED when called by a sibling and REAL when the case target.
_IS_WINDOWS / os.name / hasattr(os,"fchmod") are overridden per case from
args.is_windows / args.os_name / args.has_fchmod.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE whatsapp_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Integral floats == ints (canon folds). Tuples project to lists.
- SendResult projects to its full dataclass field dict.
- Attribute doubles are types.SimpleNamespace instances (py3.14 prefixed
  AttributeError messages); callable instance attrs are stripped in echoes;
  handle doubles project back to their scripted spec ({"__handle__": spec}).
- bytes seam args (handle.write b"\\0") project to {"__bytes__":[0]}.
- The aiohttp module double projects to {"__module__":"aiohttp"}.
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import asyncio
import copy
import dataclasses
import enum
import json
import os
import posixpath
import re
import subprocess
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
SRC = os.path.join(Q, "plugins", "platforms", "whatsapp", "adapter.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_session_dir", "_owner_only_opener", "_open_bridge_log",
    "_terminate_bridge_process", "_is_allowed_bridge_path", "_bridge_token_lock",
    "_load_or_create_bridge_token", "_import_aiohttp", "ensure_aiohttp",
    "_needs_bridge", "_bridge_req", "_authenticated_bridge_health",
    "_bridge_package_hash", "_bridge_env", "_adopted_config_matches",
    "_bridge_died", "_close_bridge_log", "_terminate_bridge",
    "_check_managed_bridge_exit", "_inject_document_text", "_quoted_media",
    "_post_bridge_message", "_apply_yaml_config", "_may_transcribe_at_gate",
    "_log_delivery_status",
]
METHODS = {
    "_bridge_req", "_bridge_package_hash", "_bridge_env", "_adopted_config_matches",
    "_bridge_died", "_close_bridge_log", "_terminate_bridge",
    "_check_managed_bridge_exit", "_inject_document_text", "_quoted_media",
    "_may_transcribe_at_gate", "_log_delivery_status",
}
# seamed instance-method collaborators (installed on the self double);
# _terminate_bridge_process is MODULE-level (composed via the NS global by
# _terminate_bridge), so it is NS-seamed, not self-seamed.
SEAMED_METHODS = {
    "_close_bridge_log", "_bridge_died", "_bridge_env", "_bridge_package_hash",
    "_bridge_req", "_check_managed_bridge_exit",
    "_post_bridge_message", "_set_fatal_error", "_notify_fatal_error",
    "_bridge_unavailable",
}
ASYNC_SEAMS = {"_notify_fatal_error", "_bridge_unavailable", "_post_bridge_message",
               "_check_managed_bridge_exit", "method", "resp.json", "resp.text"}
REAL_ALWAYS = {"_owner_only_opener"}


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


class MessageType(enum.Enum):
    TEXT = "text"
    LOCATION = "location"
    PHOTO = "photo"
    VIDEO = "video"
    AUDIO = "audio"
    VOICE = "voice"
    DOCUMENT = "document"
    STICKER = "sticker"
    COMMAND = "command"


# -- Path double: PurePosixPath + recording file ops (universal seam) --
class PathDouble(PurePosixPath):
    CTX = None

    def exists(self):
        return self.CTX.call("exists", {"path": str(self)})

    def read_text(self, encoding=None, errors=None):
        r = self.CTX.call("read_text", {"path": str(self), "encoding": encoding, "errors": errors})
        if isinstance(r, str) and encoding == "utf-8-sig" and r.startswith("\ufeff"):
            r = r[1:]
        return r

    def write_text(self, text, encoding=None):
        return self.CTX.call("write_text", {"path": str(self), "text": text, "encoding": encoding})

    def unlink(self):
        return self.CTX.call("unlink", {"path": str(self)})

    def stat(self):
        r = self.CTX.call("stat", {"path": str(self)})
        return types.SimpleNamespace(**r) if isinstance(r, dict) else r

    def mkdir(self, parents=False, exist_ok=False):
        return self.CTX.call("mkdir", {"path": str(self), "parents": parents, "exist_ok": exist_ok})

    def resolve(self):
        r = self.CTX.call("resolve", {"path": str(self)})
        return PathDouble(r) if isinstance(r, str) else r

    def is_relative_to(self, other):
        return self.CTX.call("is_relative_to", {"path": str(self), "other": str(other)})


def extract():
    mod = new_module("wa2_oracle_adapter")
    ns = mod.__dict__
    import contextlib
    import functools
    import hashlib
    import hmac
    import logging
    import platform
    from dataclasses import dataclass, field
    from typing import Any, Dict, Optional
    ns.update({
        "re": re, "json": json,
        "os": types.SimpleNamespace(path=posixpath),
        "subprocess": types.SimpleNamespace(DEVNULL=subprocess.DEVNULL,
                                            SubprocessError=subprocess.SubprocessError,
                                            TimeoutExpired=subprocess.TimeoutExpired),
        "signal": types.SimpleNamespace(SIGTERM=15, SIGKILL=9),
        "platform": platform, "logging": logging, "asyncio": asyncio,
        "contextlib": contextlib, "contextmanager": contextlib.contextmanager,
        "suppress": contextlib.suppress, "wraps": functools.wraps,
        "hashlib": hashlib, "hmac": hmac,
        "Path": PathDouble, "MessageType": MessageType,
        "dataclass": dataclass, "field": field,
        "Any": Any, "Dict": Dict, "Optional": Optional,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, SRC)
    # selected WhatsAppAdapter methods (dedented, decorators stripped);
    # _post_bridge_message is a method driven with a minimal self double
    # (no self-field reads, so the harness treats it as a module target)
    wanted_methods = METHODS | {"_bridge_url", "_post_bridge_message"}
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "WhatsAppAdapter":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in wanted_methods:
                    sub2 = copy.copy(sub)
                    sub2.decorator_list = []
                    exec_node(src, sub2, ns, SRC)
    # the real SendResult dataclass from gateway/platforms/base.py
    bsrc = open(BASE_SRC, encoding="utf-8").read()
    btree = ast.parse(bsrc)
    found = False
    for node in btree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(bsrc, node, ns, BASE_SRC)
            found = True
    if not found:
        print("FATAL oracle_whatsapp_adapter_flow2: SendResult missing from base.py")
        sys.exit(2)
    ns["logger"] = _SilentLogger()
    required = TARGETS + ["_bridge_url", "SendResult"] + [
        "_IS_WINDOWS", "_OWNER_REPLY_PREFIX", "_RUN_TEXT", "_SIGTERM_GRACE_S",
        "_BRIDGE_PORT_FREE_TIMEOUT_S", "_SIGKILL", "_BRIDGE_LOG_ROTATE_BYTES",
        "PASSKEY_REQUIRED_EXIT", "_BRIDGE_TOKEN_FILE", "_BRIDGE_PASSTHROUGH_ENV",
        "_BRIDGE_CONSUMED_ENV", "_BRIDGE_FINGERPRINT_FILE", "_TEXT_INJECT_EXTS",
        "_MAX_TEXT_INJECT_BYTES", "_NATIVE_MEDIA_TYPES", "_MEDIA_NEEDLES",
        "_MEDIA_INFO", "_MEDIA_TYPE_BY_BRIDGE_KIND", "_QUOTED_MIME_BY_BRIDGE_KIND",
        "_WA_EXT_MEDIA_TYPE", "_YAML_BRIDGE", "_PLATFORM_HINT", "_SEND_JID_RE",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_whatsapp_adapter_flow2: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# -- stub modules for the in-function imports (attrs swapped per case) --
_psutil = new_module("psutil")
_psutil.NoSuchProcess = type("NoSuchProcess", (Exception,), {})
_gw = new_module("gateway")
_gw.__path__ = []
_rss = new_module("gateway.rich_sent_store")
_gw.rich_sent_store = _rss
_pm = new_module("pm")
_pm.__path__ = []
_pmenv = new_module("pm.environments")
_pm.environments = _pmenv
_pmadopt = new_module("pm.environments_adopt")
_pm.environments_adopt = _pmadopt
_pmextras = new_module("pm.extras")
_pm.extras = _pmextras
_pminst = new_module("pm.install")
_pm.install = _pminst
_pmpaths = new_module("pm.paths")
_pm.paths = _pmpaths
_tools = new_module("tools")
_tools.__path__ = []
_tenv = new_module("tools.environments")
_tools.environments = _tenv
_tlocal = new_module("tools.environments.local")
_tenv.local = _tlocal
_secrets_mod = new_module("secrets")
_tempfile_mod = new_module("tempfile")
_msvcrt_mod = new_module("msvcrt")
_msvcrt_mod.LK_LOCK = 1
_msvcrt_mod.LK_UNLCK = 0
_fcntl_mod = new_module("fcntl")
_fcntl_mod.LOCK_EX = 2
_fcntl_mod.LOCK_UN = 8
# aiohttp stub exception family (registered ONCE: make_exc must raise the
# SAME class object the live except clauses bind)
_ClientError = type("ClientError", (Exception,), {})
_ClientConnectorError = type("ClientConnectorError", (_ClientError,), {})
_ClientResponseError = type("ClientResponseError", (_ClientError,), {})
_ServerTimeoutError = type("ServerTimeoutError", (_ClientError,), {})
_aiohttp_mod = new_module("aiohttp")
_aiohttp_mod.ClientError = _ClientError
_aiohttp_mod.ClientConnectorError = _ClientConnectorError
_aiohttp_mod.ClientResponseError = _ClientResponseError
_aiohttp_mod.ServerTimeoutError = _ServerTimeoutError


# -- SEAMED names: regex-extracted from the C port's wa2_call sites --
def seam_names_from_c():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "whatsapp_adapter_flow2.c",
                encoding="utf-8").read()
    names = set(re.findall(r'wa2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    # variable / ternary call sites the literal regex cannot see, plus the
    # oracle-only install names (seamed instance collaborators that no
    # ported target composes, and the generic logger-level recorders)
    names |= {"session.get", "session.post", "proc.kill", "proc.terminate",
              "child.kill", "child.terminate", "resp.exit", "token_lock.exit"}
    names |= SEAMED_METHODS | {"logger.warning", "logger.debug"}
    return names


SEAMED = seam_names_from_c()
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}


# -- canonical projection --
def canon(v):
    def conv(x):
        if isinstance(x, MessageType):
            return x.value
        if isinstance(x, PurePosixPath):
            return str(x)
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return {f.name: conv(getattr(x, f.name)) for f in dataclasses.fields(x)}
        if isinstance(x, RespCM):
            return conv(x.spec)
        if isinstance(x, HandleDouble):
            return conv(x.__handle__)
        if isinstance(x, types.ModuleType):
            return {"__module__": x.__name__}
        if isinstance(x, types.SimpleNamespace):
            d = vars(x)
            if "__handle__" in d:
                return conv(d["__handle__"])
            return {k: conv(val) for k, val in d.items() if not callable(val)}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# -- exception materialization / projection --
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "IndexError": IndexError,
    "OverflowError": OverflowError, "OSError": OSError,
    "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ConnectionError": ConnectionError, "ConnectionRefusedError": ConnectionRefusedError,
    "SubprocessError": subprocess.SubprocessError, "TimeoutExpired": subprocess.TimeoutExpired,
    "CalledProcessError": subprocess.CalledProcessError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "StopIteration": StopIteration, "NameError": NameError,
    "NoSuchProcess": _psutil.NoSuchProcess,
    "ClientError": _ClientError, "ClientConnectorError": _ClientConnectorError,
    "ClientResponseError": _ClientResponseError, "ServerTimeoutError": _ServerTimeoutError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if cls is KeyError:
        return cls(msg)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- seam arg projections (live call -> trace args, mirrors the C sites) --
def _opener_proj(opener):
    if opener is None:
        return None
    if opener is NS.get("_owner_only_opener"):
        return "owner_only_opener"
    return "other"


def _proc_proj(p):
    if p is None:
        return None
    if isinstance(p, types.SimpleNamespace):
        return {"pid": vars(p).get("pid")}
    return p


ARGMAP = {
    "get_hermes_dir": lambda name, fallback: {"name": name, "fallback": fallback},
    "expanduser": lambda path: {"path": path},
    "abspath": lambda path: {"path": path},
    "os.open": lambda path, flags, mode=0o600: {"path": str(path), "flags": flags, "mode": mode},
    "os.fchmod": lambda fd, mode: {"fd": fd, "mode": mode},
    "os.replace": lambda src, dst: {"src": str(src), "dst": str(dst)},
    "os.unlink": lambda path: {"path": str(path)},
    "os.fdopen": lambda fd, mode, encoding=None: {"fd": fd, "mode": mode, "encoding": encoding},
    "os.fsync": lambda fd: {"fd": fd},
    "open": lambda path, mode, encoding=None, opener=None: {
        "path": str(path), "mode": mode, "encoding": encoding, "opener": _opener_proj(opener)},
    "stat": lambda path: {"path": str(path)},
    "read_text": lambda path, encoding=None, errors=None: {
        "path": str(path), "encoding": encoding, "errors": errors},
    "mkdir": lambda path, parents=False, exist_ok=False: {
        "path": str(path), "parents": parents, "exist_ok": exist_ok},
    "resolve": lambda path: {"path": str(path)},
    "is_relative_to": lambda path, other: {"path": str(path), "other": str(other)},
    "handle.seek": lambda offset, whence=0: {"offset": offset, "whence": whence},
    "handle.write": lambda data: {"data": {"__bytes__": list(data)} if isinstance(data, bytes) else data},
    "handle.flush": lambda: {},
    "handle.fileno": lambda: {},
    "handle.close": lambda: {},
    "fh.close": lambda: {},
    "msvcrt.locking": lambda fd, mode, nbytes: {"fd": fd, "mode": mode, "nbytes": nbytes},
    "fcntl.flock": lambda fd, operation: {"fd": fd, "operation": operation},
    "secrets.token_urlsafe": lambda n: {"n": n},
    "tempfile.mkstemp": lambda prefix=None, dir=None: {
        "prefix": prefix, "dir": str(dir) if dir is not None else None},
    "psutil.Process": lambda pid: {"pid": pid},
    # closure-bound recorders (make_proc/make_parent/make_child carry pid);
    # these ARGMAP shapes document the args and satisfy the cross-check.
    "proc.poll": lambda pid: {"pid": pid},
    "proc.kill": lambda pid: {"pid": pid},
    "proc.terminate": lambda pid: {"pid": pid},
    "child.kill": lambda pid: {"pid": pid},
    "child.terminate": lambda pid: {"pid": pid},
    "psutil.children": lambda pid, recursive=True: {"pid": pid, "recursive": recursive},
    "subprocess.run": lambda argv, **kw: {"argv": list(argv), **kw},
    "print": lambda *a, **k: {"text": a[0] if a else None, **k},
    "logger.info": lambda *a: {"msg": a[0], "args": list(a[1:])},
    "logger.error": lambda *a: {"msg": a[0], "args": list(a[1:])},
    "logger.warning": lambda *a: {"msg": a[0], "args": list(a[1:])},
    "logger.debug": lambda *a: {"msg": a[0], "args": list(a[1:])},
    "voice_mention_gate_enabled": lambda extra: {"extra": extra},
    "_apply_yaml_bridge": lambda cfg, table: {
        "whatsapp_cfg": cfg, "yaml_bridge": [list(r) for r in table]},
    "hermes_subprocess_env": lambda: {},
    "with_hermes_node_path": lambda env=None: {"env": env},
    "repo_root": lambda: {},
    "lazy_installs_allowed": lambda: {},
    "running_from_selected_environment": lambda root: {"root": root},
    "restart_needed": lambda root: {"root": root},
    "ensure_import": lambda extra: {"extra": extra},
    "ClientTimeout": lambda total=None: {"total": total},
    "session.get": lambda url, **kw: {"url": url, **kw},
    "session.post": lambda url, **kw: {"url": url, **kw},
    "resp.enter": lambda: {},
    "resp.json": lambda: {},
    "resp.text": lambda: {},
    "resp.exit": lambda exc: {"exc": exc},
    "token_lock.enter": lambda: {},
    "token_lock.exit": lambda exc: {"exc": exc},
    "method": lambda *a, **k: {"args": list(a[1:]), "kwargs": k},
    "_bridge_unavailable": lambda: {},
    "_set_fatal_error": lambda code, message, retryable=False: {
        "code": code, "message": message, "retryable": retryable},
    "_notify_fatal_error": lambda: {},
    "lookup_media": lambda chat_id, reply_id: {"chat_id": chat_id, "reply_id": reply_id},
    # claimed flow1 siblings
    "_wenv": lambda name, default="": {"name": name, "default": default},
    "_cache_dirs": lambda: {},
    "_file_content_hash": lambda path: {"path": str(path)},
    "_bridge_config_fingerprint": lambda bridge_env, package_hash="": {
        "bridge_env": bridge_env, "package_hash": package_hash},
    "_bridge_auth_proof": lambda token, challenge: {"token": token, "challenge": challenge},
    "_bridge_auth_headers": lambda token: {"token": token},
    # ported flow2 siblings (seamed when composed)
    "_bridge_token_lock": lambda lock_path: {"lock_path": str(lock_path)},
    "_import_aiohttp": lambda: {},
    "_terminate_bridge_process": lambda proc, force=False: {"proc": _proc_proj(proc), "force": force},
    "_is_allowed_bridge_path": lambda url: {"url": url},
    "_close_bridge_log": lambda: {},
    "_bridge_died": lambda detail, code="whatsapp_bridge_exited": {"detail": detail, "code": code},
    "_bridge_env": lambda: {},
    "_bridge_package_hash": lambda: {},
    "_bridge_req": lambda method, path, timeout, **kwargs: {
        "method": method, "path": path, "timeout": timeout, "kwargs": kwargs},
    "_check_managed_bridge_exit": lambda: {},
    "_post_bridge_message": lambda path, payload, timeout: {
        "path": path, "payload": payload, "timeout": timeout},
}


# -- attribute / CM doubles --
class HandleDouble:
    """open()/os.fdopen() result: a sync context manager whose __exit__
    records handle.close (the with-block close rides the same seam the C
    port fires at its close label)."""

    def __init__(self, ctx, spec):
        self.__handle__ = spec
        self._ctx = ctx

    def seek(self, offset, whence=0):
        return self._ctx.call("handle.seek", {"offset": offset, "whence": whence})

    def write(self, data):
        return self._ctx.call("handle.write", {
            "data": {"__bytes__": list(data)} if isinstance(data, bytes) else data})

    def flush(self):
        return self._ctx.call("handle.flush", {})

    def fileno(self):
        return self._ctx.call("handle.fileno", {})

    def close(self):
        return self._ctx.call("handle.close", {})

    def __enter__(self):
        return self

    def __exit__(self, et, ev, tb):
        self.close()
        return False


def make_handle(ctx, spec):
    return HandleDouble(ctx, spec)


def make_fh(ctx, spec):
    d = dict(spec) if isinstance(spec, dict) else {}
    f = types.SimpleNamespace(**d)
    f.close = lambda: ctx.call("fh.close", {})
    return f


def make_proc(ctx, spec):
    if not isinstance(spec, dict):
        return spec
    p = types.SimpleNamespace(**spec)
    p.poll = lambda: ctx.call("proc.poll", {"pid": p.pid})
    p.kill = lambda: ctx.call("proc.kill", {"pid": p.pid})
    p.terminate = lambda: ctx.call("proc.terminate", {"pid": p.pid})
    return p


def make_parent(ctx, spec):
    if not isinstance(spec, dict):
        return spec
    p = types.SimpleNamespace(**spec)
    pid = spec.get("pid")

    def _children(recursive=True):
        raw = ctx.call("psutil.children", {"pid": pid, "recursive": recursive})
        return [make_child(ctx, c) for c in raw] if isinstance(raw, list) else raw

    p.children = _children
    p.kill = lambda: ctx.call("proc.kill", {"pid": pid})
    p.terminate = lambda: ctx.call("proc.terminate", {"pid": pid})
    return p


def make_child(ctx, spec):
    if not isinstance(spec, dict):
        return spec
    c = types.SimpleNamespace(**spec)
    pid = spec.get("pid")
    c.kill = lambda: ctx.call("child.kill", {"pid": pid})
    c.terminate = lambda: ctx.call("child.terminate", {"pid": pid})
    return c


def make_session(ctx, methods):
    s = types.SimpleNamespace(__session__=True)

    def _call(m, url, **kw):
        raw = ctx.call("session." + m, {"url": url, **kw})
        return RespCM(ctx, raw)

    for m in methods:
        setattr(s, m, lambda url, _m=m, **kw: _call(_m, url, **kw))
    return s


def make_resp(ctx, spec):
    if not isinstance(spec, dict):
        return spec
    r = types.SimpleNamespace(**spec)

    async def _json():
        return ctx.call("resp.json", {})

    async def _text():
        return ctx.call("resp.text", {})

    r.json = _json
    r.text = _text
    return r


class RespCM:
    def __init__(self, ctx, spec):
        self.ctx = ctx
        self.spec = spec

    async def __aenter__(self):
        self.ctx.call("resp.enter", {})
        return make_resp(self.ctx, self.spec)

    async def __aexit__(self, et, ev, tb):
        r = self.ctx.call("resp.exit", {"exc": et.__name__ if et else None})
        return bool(r)


class TokenLockCM:
    def __init__(self, ctx):
        self.ctx = ctx

    def __enter__(self):
        self.ctx.call("token_lock.enter", {})
        return None

    def __exit__(self, et, ev, tb):
        r = self.ctx.call("token_lock.exit", {"exc": et.__name__ if et else None})
        return bool(r)


BUILD = {
    "subprocess.run": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "stat": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "open": lambda r: make_handle(BUILD_CTX[0], r),
    "os.fdopen": lambda r: make_handle(BUILD_CTX[0], r),
    "psutil.Process": lambda r: make_parent(BUILD_CTX[0], r),
    "psutil.children": lambda r: [make_child(BUILD_CTX[0], c) for c in r] if isinstance(r, list) else r,
    "tempfile.mkstemp": lambda r: tuple(r) if isinstance(r, list) else r,
    "_cache_dirs": lambda r: tuple(r) if isinstance(r, list) else r,
    "_bridge_token_lock": lambda r: TokenLockCM(BUILD_CTX[0]),
    "_bridge_req": lambda r: RespCM(BUILD_CTX[0], r),
    "session.get": lambda r: RespCM(BUILD_CTX[0], r),
    "session.post": lambda r: RespCM(BUILD_CTX[0], r),
    "resolve": lambda r: PathDouble(r) if isinstance(r, str) else r,
}
BUILD_CTX = [None]  # per-case ctx for the BUILD closures


# -- per-case script backend (mirrors the C wa2_call exactly) --
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


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(raw) if build else raw

    if nm in ASYNC_SEAMS:
        async def af(*a, **k):
            return f(*a, **k)
        return af
    return f


class _LoggerDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        return make_recorder(self._ctx, "logger." + level)


CURRENT = {"sd": None}


def make_self(ctx, target, args):
    d = dict(args.get("self") or {})
    sd = types.SimpleNamespace()
    for k, v in d.items():
        if k == "_bridge_process":
            setattr(sd, k, make_proc(ctx, v) if isinstance(v, dict) else v)
        elif k == "_bridge_log_fh":
            setattr(sd, k, make_fh(ctx, v) if isinstance(v, dict) else v)
        elif k in ("config", "gateway_runner"):
            setattr(sd, k, types.SimpleNamespace(**v) if isinstance(v, dict) else v)
        elif k == "_session_path":
            # live __init__ stores a Path; the / join needs a Path carrier
            setattr(sd, k, PathDouble(v) if isinstance(v, str) else v)
        elif k == "_http_session":
            setattr(sd, k, make_session(ctx, args.get("session_methods") or []))
        else:
            setattr(sd, k, v)
    for nm in sorted(SEAMED_METHODS):
        if nm == target:
            continue
        setattr(sd, nm, make_recorder(ctx, nm))
    import types as _t
    sd._bridge_url = _t.MethodType(NS["_bridge_url"], sd)
    return sd


def install_case(ctx, target, args):
    BUILD_CTX[0] = ctx
    for nm in REAL_DEFS:
        NS[nm] = REAL_DEFS[nm]
    NS["_IS_WINDOWS"] = args.get("is_windows", True)
    PathDouble.CTX = ctx
    NS["logger"] = _LoggerDouble(ctx)
    # os double (POSIX path algorithms; scripted FS probes; per-case flags)
    osdbl = types.SimpleNamespace(
        path=types.SimpleNamespace(abspath=make_recorder(ctx, "abspath"),
                                   expanduser=make_recorder(ctx, "expanduser"),
                                   isabs=posixpath.isabs),
        name=args.get("os_name", "nt"), SEEK_END=2,
        open=make_recorder(ctx, "os.open"),
        fdopen=make_recorder(ctx, "os.fdopen"),
        fsync=make_recorder(ctx, "os.fsync"),
        replace=make_recorder(ctx, "os.replace"),
        unlink=make_recorder(ctx, "os.unlink"),
    )
    if args.get("has_fchmod", True):
        osdbl.fchmod = make_recorder(ctx, "os.fchmod")
    NS["os"] = osdbl
    NS["subprocess"] = types.SimpleNamespace(
        DEVNULL=subprocess.DEVNULL, run=make_recorder(ctx, "subprocess.run"),
        SubprocessError=subprocess.SubprocessError, TimeoutExpired=subprocess.TimeoutExpired)
    NS["open"] = make_recorder(ctx, "open")
    NS["print"] = make_recorder(ctx, "print")
    # in-function import stubs
    _secrets_mod.token_urlsafe = make_recorder(ctx, "secrets.token_urlsafe")
    _tempfile_mod.mkstemp = make_recorder(ctx, "tempfile.mkstemp")
    _msvcrt_mod.locking = make_recorder(ctx, "msvcrt.locking")
    _fcntl_mod.flock = make_recorder(ctx, "fcntl.flock")
    _psutil.Process = make_recorder(ctx, "psutil.Process")
    _rss.lookup_media = make_recorder(ctx, "lookup_media")
    _pmenv.running_from_selected_environment = make_recorder(ctx, "running_from_selected_environment")
    _pmadopt.restart_needed = make_recorder(ctx, "restart_needed")
    _pmextras.ensure_import = make_recorder(ctx, "ensure_import")
    _pminst.lazy_installs_allowed = make_recorder(ctx, "lazy_installs_allowed")
    _pmpaths.repo_root = make_recorder(ctx, "repo_root")
    _tlocal.hermes_subprocess_env = make_recorder(ctx, "hermes_subprocess_env")
    _aiohttp_mod.ClientTimeout = make_recorder(ctx, "ClientTimeout")
    if target == "_import_aiohttp":
        ah = args.get("aiohttp")
        if ah is None:
            sys.modules["aiohttp"] = None
        else:
            m = types.ModuleType("aiohttp")
            if ah.get("client_session"):
                m.ClientSession = type("ClientSession", (), {})
            if "file" in ah:
                m.__file__ = ah["file"]
            if "path" in ah:
                m.__path__ = ah["path"]
            sys.modules["aiohttp"] = m
    else:
        sys.modules["aiohttp"] = _aiohttp_mod
    # bare-global module-top imports (never bound by AST extraction)
    for nm in ("get_hermes_dir", "with_hermes_node_path", "voice_mention_gate_enabled",
               "_apply_yaml_bridge"):
        NS[nm] = make_recorder(ctx, nm)
    # module-level seamed siblings (claimed + ported), except the target
    for nm in SEAMED:
        if nm == target or nm in SEAMED_METHODS or nm in REAL_ALWAYS:
            continue
        if nm in ("os.open", "os.fchmod", "os.replace", "os.unlink", "os.fdopen", "os.fsync",
                  "open", "print", "subprocess.run", "psutil.Process", "secrets.token_urlsafe",
                  "tempfile.mkstemp", "msvcrt.locking", "fcntl.flock", "lookup_media",
                  "running_from_selected_environment", "restart_needed", "ensure_import",
                  "lazy_installs_allowed", "repo_root", "hermes_subprocess_env", "ClientTimeout",
                  "session.get", "session.post", "resp.enter", "resp.json", "resp.text",
                  "resp.exit", "token_lock.enter", "token_lock.exit", "handle.seek",
                  "handle.write", "handle.flush", "handle.fileno", "handle.close", "fh.close",
                  "proc.poll", "proc.kill", "proc.terminate", "child.kill", "child.terminate",
                  "psutil.children", "stat", "read_text", "mkdir", "resolve", "is_relative_to",
                  "exists", "write_text", "unlink", "expanduser", "abspath",
                  "logger.info", "logger.error", "logger.warning", "logger.debug",
                  "get_hermes_dir", "with_hermes_node_path", "voice_mention_gate_enabled",
                  "_apply_yaml_bridge", "method", "_bridge_unavailable",
                  "_set_fatal_error", "_notify_fatal_error"):
            continue  # installed on doubles / stub modules / NS above
        NS[nm] = make_recorder(ctx, nm)
    CURRENT["sd"] = make_self(ctx, target, args) if target in METHODS else None


# -- exception spec projection --
def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# -- per-case live call --
def live_call(func, args, ctx):
    f = NS[func]
    sd = CURRENT["sd"]
    if func == "_session_dir":
        return f(args.get("extra"))
    if func == "_owner_only_opener":
        return f(args["path"], args["flags"])
    if func == "_open_bridge_log":
        return f(PathDouble(args["path"]), private=args["private"])
    if func == "_terminate_bridge_process":
        return f(make_proc(ctx, args.get("proc")), force=args.get("force", False))
    if func == "_is_allowed_bridge_path":
        return f(args["url"])
    if func == "_bridge_token_lock":
        cm = f(PathDouble(args["lock_path"]))
        with cm:
            if args.get("body_raise"):
                raise make_exc(args["body_raise"])
        return None
    if func == "_load_or_create_bridge_token":
        return f(PathDouble(args["session_path"]), rotate=args.get("rotate", False))
    if func == "_import_aiohttp":
        return f()
    if func == "ensure_aiohttp":
        return f()
    if func == "_needs_bridge":
        mrec = make_recorder(ctx, "method")
        guarded = f(mrec)
        gsd = types.SimpleNamespace(_bridge_unavailable=make_recorder(ctx, "_bridge_unavailable"))
        ca = args.get("call_args") or []
        ck = args.get("call_kwargs") or {}
        return asyncio.run(guarded(gsd, *ca, **ck))
    if func == "_bridge_req":
        return f(sd, args["method"], args["path"], args["timeout"], **(args.get("kwargs") or {}))
    if func == "_authenticated_bridge_health":
        session = make_session(ctx, ["get"])
        return asyncio.run(f(session, args["base_url"], args["token"], args.get("timeout")))
    if func == "_bridge_package_hash":
        return f(sd)
    if func == "_bridge_env":
        return f(sd)
    if func == "_adopted_config_matches":
        return f(sd)
    if func == "_bridge_died":
        return f(sd, args["detail"], args.get("code", "whatsapp_bridge_exited"))
    if func == "_close_bridge_log":
        return f(sd)
    if func == "_terminate_bridge":
        return f(sd, force=args["force"])
    if func == "_check_managed_bridge_exit":
        return asyncio.run(f(sd))
    if func == "_inject_document_text":
        return f(sd, args["cached_urls"], args["body"])
    if func == "_quoted_media":
        return f(sd, args["data"], args.get("raw_reply_id"))
    if func == "_post_bridge_message":
        psd = types.SimpleNamespace(_bridge_req=make_recorder(ctx, "_bridge_req"))
        return asyncio.run(f(psd, args["path"], args["payload"], timeout=args["timeout"]))
    if func == "_apply_yaml_config":
        return f(args.get("yaml_cfg"), args["whatsapp_cfg"])
    if func == "_may_transcribe_at_gate":
        return f(sd, args["data"])
    if func == "_log_delivery_status":
        return f(sd, args["data"])
    raise RuntimeError(f"unknown func {func}")


# -- per-case drive --
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_case(ctx, func, args)
    try:
        try:
            ret = live_call(func, args, ctx)
        except BaseException as exc:  # SystemExit included
            ret = exc_spec(exc, ctx)
        if func in METHODS:
            sd = CURRENT["sd"]
            st = {"self": {k: canon(getattr(sd, k)) for k in (args.get("self") or {})}}
        else:
            st = {}
        return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(st)}
    finally:
        sys.modules["aiohttp"] = _aiohttp_mod


# -- three-way seam-name cross-check --
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_whatsapp_adapter_flow2: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_whatsapp_adapter_flow2: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "whatsapp_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_whatsapp_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_whatsapp_adapter_flow2: fixture funcs without a target: {sorted(badf)}")
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
    if want.get("state") != got.get("state"):
        print(f"  state want={json.dumps(want.get('state'), ensure_ascii=False)[:300]}")
        print(f"  state got ={json.dumps(got.get('state'), ensure_ascii=False)[:300]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")


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
    print(f"ORACLE whatsapp_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved).
    Flags drive exceptions, UNINTENTIONAL script gaps, and AttributeError
    families naming a ported method / seam stub (binding-bug signature)."""
    fix = _REPO / "tests" / "fixtures" / "whatsapp_adapter_flow2.jsonl"
    cross_check()
    n = bad = 0
    counts = {}
    names = SEAMED | set(TARGETS)
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
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            hit = [nm for nm in names if f"'{nm}'" in msg]
            if hit:
                bad += 1
                print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg} (names {hit})")
        if isinstance(r, dict) and r.get("__raise__") == "TypeError" and \
                "does not support the context manager protocol" in str(r.get("message", "")):
            bad += 1
            print(f"SELFTEST CM PROTOCOL {func} case {n}: {r['message']}")
    print(f"SELFTEST whatsapp_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    thin = [f for f in TARGETS if counts.get(f, 0) < 5]
    if thin:
        print(f"SELFTEST THIN FUNCS (<5 cases): {thin}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()