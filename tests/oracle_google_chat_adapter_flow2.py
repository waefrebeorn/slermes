#!/usr/bin/env python3
"""Oracle for the UNCLAIMED remainder FLOW port of
plugins/platforms/google_chat/adapter.py
(src/gateway/platforms/google_chat_adapter_flow2.c, second wave after
google_chat_adapter_flow claimed the module decision chains + inbound/typing
leaves).

Drives the LIVE GoogleChatAdapter / _ThreadCountStore methods and the module
SA-credential loader with SCRIPTED seams: every sibling method, module helper,
asyncio primitive, foreign import, logger call, scoped-secret read, filesystem
probe and Chat API call is a recording stub fed the same scripts the C harness
feeds its universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the mutated
adapter / store state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; logger re-set to the recorder
AFTER seeding). Every exec namespace is a registered sys.modules module
(py3.14). SendResult is the REAL dataclass AST-extracted from
gateway/platforms/base.py; ParkedRouteRefused from
gateway/parked_route_refusal.py; MessageType a real enum with the live values.
The heavy quarry import chain is never imported.

Claimed flow1 siblings and unported collaborators are SEAMED; the 24 ported
targets stay LIVE and compose through scripted seams (a ported sibling called
by another ported target is seamed, not run live).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE google_chat_adapter_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds errno when set, message "[Errno N] msg"; _SACredentialError
  adds kind + detail (a raise envelope or null)). BaseException kinds
  (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError,
  BaseExceptionGroup) pass through every except-Exception site.
- Exception doubles (args.exc): {"cls": name, "msg": str(exc), "attrs": {...}}.
- Attribute carriers (config / message / event / source) travel as
  {"__obj__": {attrs}}; a plain JSON object IS a Python dict.
- SendResult returns project to the FULL 8-field dataclass dict (asdict);
  tuple returns project to arrays; MessageType members project to .value.
- logger seams record {"fmt": <literal>, "args": [<raw args>]}; exc_info
  kwargs drop on both sides (but the exc_info expression's seams still fire).
- asyncio.to_thread is ONE seam {"fn": <site marker>, "args": {...}}; the
  callable is NEVER invoked (its inner Chat API chain is out of seam scope).
  Site markers: members.list / oauth.build_user_chat / get_subscription /
  messages.delete / messages.patch / media.download / download_uri.fetch /
  oauth.load_user_credentials / oauth.list_emails / <fixture callable marker>.
- Seamed async siblings return a SeamCoro: the seam fires on AWAIT; a
  never-awaited construction (self._submit_on_loop(self._dispatch_message(...)))
  fires NO seam and projects to {"__coro__": name, "args": {...}}.
- In-function imports fire ONE seam "import.<module>" {"fromlist": [...]};
  the imported names are per-case recorders / callable markers.
- Path modeling POSIX: path.exists / path.read_text / path.write_text /
  path.mkdir / os.replace seams; read_text(utf-8-sig) strips ONE leading BOM
  AFTER the seam call (trace ret carries the raw scripted text).
- json.loads is SEAMED (its JSONDecodeError message is observable in the SA
  error templates); json.dumps is REAL (pure). Attachment bytes travel as
  ASCII str markers; cache seams receive the marker verbatim.
- ASCII scope: strip/lower/upper fold ASCII; integral numbers only in the
  json.dumps / redact-dump corpus; the edit_message ellipsis literal is U+2026.
"""
import ast
import asyncio as _asyncio
import builtins
import contextlib
import copy
import enum
import json
import os
import re
import sys
import textwrap
import types
import warnings
from dataclasses import asdict, dataclass, field, is_dataclass
from pathlib import Path
import __future__ as _fut

warnings.filterwarnings("ignore", message="coroutine .* was never awaited")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "google_chat", "adapter.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
PARK_SRC = os.path.join(Q, "gateway", "parked_route_refusal.py")

_FLAGS = _fut.annotations.compiler_flag
MODNAME = "google_chat_adapter_flow2_oracle"
PKG = "plugins.platforms.google_chat"


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


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


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


# ── ported targets ──
MODULE_FUNCS = ["_load_sa_credentials_from"]
STORE_FUNCS = ["load", "get", "incr", "decr", "_save"]
METHOD_FUNCS = [
    "_load_sa_credentials", "_bot_id_cache_path", "_save_cached_bot_id",
    "_resolve_bot_user_id", "_load_user_oauth", "_check_subscription",
    "_handle_pubsub_message", "dispatch_http_event", "_dispatch_message",
    "_build_message_event", "_download_attachment", "send", "send_card",
    "edit_message", "delete_message", "_patch_message", "_create_message",
    "_call_with_retry",
]
ALL_PORTED = MODULE_FUNCS + ["store." + f for f in STORE_FUNCS] + METHOD_FUNCS
ASYNC_FUNCS = {
    "_resolve_bot_user_id", "_load_user_oauth", "_check_subscription",
    "dispatch_http_event", "_dispatch_message", "_build_message_event",
    "_download_attachment", "send", "send_card", "edit_message",
    "delete_message", "_patch_message", "_create_message", "_call_with_retry",
}

# seamed module siblings (claimed flow1 leaves + the SA loader when composed)
MODULE_SEAMED = {
    "_http_status", "_exc_text", "_proxy_error", "_is_retryable_error",
    "_is_google_owned_host", "_mime_for_message_type", "_create_kwargs",
    "_thread_body", "_adc_would_borrow_foreign_credentials",
    "_load_sa_credentials_from",
}
# seamed instance-method siblings (claimed flow1 + unported collaborators +
# ported targets when composed). name -> (is_async, args_fn key)
SELF_SEAMED = [
    "_resolve_thread_id", "_chunk_text", "_note_rate_limit",
    "_track_outbound_thread", "_release_unadmitted", "_connect_failed",
    "_prepare_inbound", "_save_cached_bot_id", "_submit_on_loop",
    "_dispatch_message", "_build_message_event", "_download_attachment",
    "_patch_message", "_create_message", "_call_with_retry",
    "_handle_setup_files_command", "format_message", "pause_typing_for_chat",
    "resume_typing_for_chat", "_is_sender_authorized", "handle_message",
    "build_source", "_set_fatal_error", "_bot_id_cache_path",
]
SELF_SEAMED_ASYNC = {
    "_dispatch_message", "_build_message_event", "_download_attachment",
    "_patch_message", "_create_message", "_call_with_retry",
    "_handle_setup_files_command", "handle_message",
}


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "os": os, "re": re, "json": json, "contextlib": contextlib,
        "MessageType": MessageType, "enum": enum,
        "logging": __import__("logging"), "threading": __import__("threading"),
        "Any": object, "Optional": object, "Callable": object,
        "Dict": dict, "List": list, "Tuple": tuple,
        "dataclass": dataclass, "field": field, "asdict": asdict,
        "__package__": PKG, "__name__": MODNAME,
    })
    src = open(A_SRC, encoding="utf-8").read()
    # cache_* names must exist before _MEDIA_CACHERS seeds (rebuilt per case)
    for nm in ("cache_image_from_bytes_async", "cache_audio_from_bytes_async",
               "cache_video_from_bytes_async", "cache_document_from_bytes_async"):
        ns[nm] = lambda *a, **k: None
    ns["service_account"] = types.SimpleNamespace(
        Credentials=types.SimpleNamespace(from_service_account_info=lambda *a, **k: None))
    ns["pubsub_v1"] = types.SimpleNamespace(SubscriberClient=lambda *a, **k: None)
    ns["gax_exceptions"] = types.SimpleNamespace()
    seed_constants(src, ns, A_SRC)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, A_SRC)
        if isinstance(node, ast.ClassDef) and node.name == "_SACredentialError":
            exec_node(src, node, ns, A_SRC)
        if isinstance(node, ast.ClassDef) and node.name == "_ThreadCountStore":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in STORE_FUNCS:
                    sub = copy.copy(sub)
                    sub.decorator_list = []
                    exec_node(src, sub, ns, A_SRC)
        if isinstance(node, ast.ClassDef) and node.name == "GoogleChatAdapter":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHOD_FUNCS:
                    sub = copy.copy(sub)
                    sub.decorator_list = []
                    exec_node(src, sub, ns, A_SRC)
    # SendResult real dataclass
    bsrc = open(BASE_SRC, encoding="utf-8").read()
    for node in ast.parse(bsrc).body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(bsrc, node, ns, BASE_SRC)
    # ParkedRouteRefused real class
    psrc = open(PARK_SRC, encoding="utf-8").read()
    for node in ast.parse(psrc).body:
        if isinstance(node, ast.ClassDef) and node.name == "ParkedRouteRefused":
            exec_node(psrc, node, ns, PARK_SRC)
    ns["logger"] = _SilentLogger()  # re-set AFTER the Assign seeding loop
    # tuple-target Assign skipped by seed_constants; seed the retry constants
    ns["_RETRY_MAX_ATTEMPTS"] = 3
    ns["_RETRY_BASE_DELAY"] = 1.0
    ns["_RETRY_MAX_DELAY"] = 8.0
    ns["_RETRY_JITTER"] = 0.3
    required = MODULE_FUNCS + STORE_FUNCS + METHOD_FUNCS + [
        "_SACredentialError", "SendResult", "ParkedRouteRefused",
        "_SA_ERROR_MESSAGES", "_CHAT_SCOPES", "_MAX_TEXT_LENGTH",
        "_RETRY_MAX_ATTEMPTS", "_RETRY_BASE_DELAY", "_RETRY_MAX_DELAY",
        "_RETRY_JITTER", "_RETRYABLE_HTTP_STATUSES", "_TYPING_CONSUMED_SENTINEL",
        "_MEDIA_CACHERS", "_MIME_MESSAGE_TYPES",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_google_chat_adapter_flow2: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {}
for nm in MODULE_FUNCS + METHOD_FUNCS:
    REAL[nm] = NS[nm]
for nm in STORE_FUNCS:
    REAL["store." + nm] = NS[nm]
SACredError = NS["_SACredentialError"]
SendResult = NS["SendResult"]
ParkedRouteRefused = NS["ParkedRouteRefused"]


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items() if k != "__spec__"})
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
HttpError = type("HttpError", (Exception,), {})
NotFound = type("NotFound", (Exception,), {})
PermissionDenied = type("PermissionDenied", (Exception,), {})
Unauthenticated = type("Unauthenticated", (Exception,), {})
InstallError = type("InstallError", (Exception,), {})
EXC_CLASSES = {
    "OSError": OSError, "IOError": OSError, "TimeoutError": TimeoutError,
    "PermissionError": PermissionError, "FileNotFoundError": FileNotFoundError,
    "FileExistsError": FileExistsError, "BlockingIOError": BlockingIOError,
    "ConnectionError": ConnectionError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "ChildProcessError": ChildProcessError,
    "ProcessLookupError": ProcessLookupError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": _asyncio.CancelledError,
    "JSONDecodeError": json.JSONDecodeError,
    "HttpError": HttpError, "NotFound": NotFound,
    "PermissionDenied": PermissionDenied, "Unauthenticated": Unauthenticated,
    "InstallError": InstallError, "ParkedRouteRefused": ParkedRouteRefused,
    "_SACredentialError": SACredError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if kind == "ParkedRouteRefused":
        return cls()
    if kind == "_SACredentialError":
        e = cls(spec.get("kind", msg))
        e.kind = spec.get("kind", msg)
        det = spec.get("detail")
        e.detail = make_exc({"type": det.get("__raise__", "Exception"),
                             "message": det.get("message", "")}) if isinstance(det, dict) else None
        return e
    en = spec.get("errno")
    if en is not None and isinstance(cls, type) and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    if isinstance(exc, SACredError):
        d["kind"] = exc.kind
        d["detail"] = proj_exc(exc.detail) if exc.detail is not None else None
    return d


def attr_double(spec):
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            setattr(d, k, attr_double(v))
        d.__spec__ = spec
        return d
    return spec


def obj_double(spec):
    if isinstance(spec, dict) and "__obj__" in spec:
        d = types.SimpleNamespace()
        for k, v in spec["__obj__"].items():
            setattr(d, k, obj_double(v))
        d.__spec__ = spec
        return d
    return spec


def proj(x):
    if hasattr(x, "__spec__"):
        return x.__spec__
    if isinstance(x, BaseException):
        return {"cls": type(x).__name__, "msg": str(x), "attrs": {}}
    return x


def make_exc_obj(spec):
    spec = spec or {}
    cls_name = spec.get("cls", "Exception")
    cls = EXC_CLASSES.get(cls_name)
    if cls is None:
        cls = type(cls_name, (Exception,), {})
    e = cls(spec.get("msg", ""))
    for k, v in (spec.get("attrs") or {}).items():
        setattr(e, k, attr_double(v))
    e.__spec__ = spec
    return e


# ── per-case script backend (mirrors the C gca2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
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
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def method_rec(ctx, name, args_fn=None, build=None):
    def f(self, *a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


class SeamCoro:
    """Awaitable seam: fires on await; a never-awaited construction carries a
    coroutine projection for the _submit_on_loop hand-off."""
    def __init__(self, ctx, name, args):
        self._ctx, self._name, self._args = ctx, name, args
        self.__coro_proj__ = {"__coro__": name, "args": args}

    def __await__(self):
        ctx, name, args = self._ctx, self._name, self._args
        build = getattr(self, "_build", None)
        async def _run():
            raw = ctx.call(name, args)
            return build(raw) if build else raw
        return _run().__await__()


def method_rec_async(ctx, name, args_fn=None, build=None):
    def f(self, *a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        coro = SeamCoro(ctx, name, args)
        if build:
            coro._build = build
        return coro
    return f


def rec_async(ctx, name, args_fn=None, build=None):
    """Plain (no self) async recorder for setattr-bound instance attributes and
    module-level async helpers: the seam fires on AWAIT."""
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        coro = SeamCoro(ctx, name, args)
        if build:
            coro._build = build
        return coro
    return f


def proj_coro(x):
    if isinstance(x, SeamCoro):
        return x.__coro_proj__
    if hasattr(x, "cr_frame"):
        return {"__coro__": x.cr_code.co_name, "args": canon(dict(x.cr_frame.f_locals))}
    return x


class RecLogger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log


class CallableDouble:
    def __init__(self, marker, args=None):
        self.__gca2_marker__ = marker
        self.__gca2_args__ = args or {}

    def __call__(self, *a, **k):
        return None


def proj_callable(fn):
    if isinstance(fn, CallableDouble):
        return {"__callable__": fn.__gca2_marker__, "args": fn.__gca2_args__}
    if hasattr(fn, "__gca2_marker__"):
        return {"__callable__": fn.__gca2_marker__, "args": {}}
    return {"__callable__": getattr(fn, "__name__", "fn"), "args": {}}


class PathDouble:
    """POSIX path carrier; filesystem ops are seams. _path is a str."""
    def __init__(self, path, ctx):
        self._path = path if isinstance(path, str) else ""
        self._ctx = ctx

    def __str__(self):
        return self._path

    def __fspath__(self):
        return self._path

    def __truediv__(self, name):
        p = self._path
        if p == "":
            return PathDouble(str(name), self._ctx)
        return PathDouble(p.rstrip("/") + "/" + str(name), self._ctx)

    @property
    def parent(self):
        p = self._path
        if "/" not in p:
            return PathDouble(".", self._ctx)
        head = p.rsplit("/", 1)[0]
        return PathDouble(head if head else "/", self._ctx)

    @property
    def suffix(self):
        base = self._path.rsplit("/", 1)[-1]
        if "." in base[1:]:
            return "." + base.rsplit(".", 1)[-1]
        return ""

    def with_suffix(self, s):
        p = self._path
        base = p.rsplit("/", 1)[-1]
        head = p[:len(p) - len(base)]
        if "." in base[1:]:
            base = base[:base.rfind(".")]
        return PathDouble(head + base + s, self._ctx)

    def exists(self):
        return self._ctx.call("path.exists", {"path": self._path})

    def read_text(self, encoding=None, errors=None):
        raw = self._ctx.call("path.read_text", {"path": self._path, "encoding": encoding})
        if encoding == "utf-8-sig" and isinstance(raw, str) and raw.startswith("\ufeff"):
            return raw[1:]
        return raw

    def write_text(self, text, encoding=None, errors=None):
        return self._ctx.call("path.write_text",
                              {"path": self._path, "text": text, "encoding": encoding})

    def mkdir(self, parents=False, exist_ok=False):
        return self._ctx.call("path.mkdir",
                              {"path": self._path, "parents": parents, "exist_ok": exist_ok})


def message_double(spec, ctx):
    attrs = spec.get("__obj__", {}) if isinstance(spec, dict) else {}
    m = types.SimpleNamespace()
    data = attrs.get("data")
    m.data = data.encode("utf-8") if isinstance(data, str) else (data or b"")
    m.attributes = attrs.get("attributes")
    m.nack = rec(ctx, "message.nack", lambda: {})
    m.ack = rec(ctx, "message.ack", lambda: {})
    return m


# ── to_thread site identification ──
# Built from the AST: (co_name, relative_firstlineno) -> marker. The relative
# line is lambda.lineno - def_start + 1 (the extraction compiles each def from
# its own first line). Nested defs (_fetch_media/_fetch_uri) likewise.
def build_site_table():
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    table = {}
    markers = {
        ("_resolve_bot_user_id", "<lambda>"): "members.list",
        ("_load_user_oauth", "<lambda>"): "oauth.build_user_chat",
        ("_check_subscription", "<lambda>"): "get_subscription",
        ("delete_message", "<lambda>"): "messages.delete",
        ("_patch_message", "<lambda>"): "messages.patch",
        ("_download_attachment", "_fetch_media"): "media.download",
        ("_download_attachment", "_fetch_uri"): "download_uri.fetch",
    }
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in markers_scope():
            start = node.lineno
            for d in getattr(node, "decorator_list", []):
                start = min(start, d.lineno)
            for sub in ast.walk(node):
                if isinstance(sub, ast.Call) and isinstance(sub.func, ast.Attribute) \
                        and sub.func.attr == "to_thread" and sub.args:
                    a0 = sub.args[0]
                    if isinstance(a0, ast.Lambda):
                        key = (node.name, "<lambda>")
                        if key in markers:
                            table[( "<lambda>", a0.lineno - start + 1)] = markers[key]
                    elif isinstance(a0, ast.Name) and a0.id in ("_fetch_media", "_fetch_uri"):
                        # find the nested def line
                        for nd in ast.walk(node):
                            if isinstance(nd, ast.FunctionDef) and nd.name == a0.id:
                                table[(a0.id, nd.lineno - start + 1)] = markers[(node.name, a0.id)]
    return table


def markers_scope():
    return {"_resolve_bot_user_id", "_load_user_oauth", "_check_subscription",
            "delete_message", "_patch_message", "_download_attachment"}


SITE_TABLE = build_site_table()


def identify_fn(fn):
    if hasattr(fn, "__gca2_marker__"):
        return fn.__gca2_marker__, {}
    code = getattr(fn, "__code__", None)
    if code is not None:
        free = {}
        if code.co_freevars and fn.__closure__:
            free = dict(zip(code.co_freevars, (c.cell_contents for c in fn.__closure__)))
        # freevar names are unique per call site (robust to relative-line collisions)
        if "subscription_path" in free:
            return "get_subscription", {"request": {"subscription": free["subscription_path"]}}
        if "user_creds" in free:
            return "oauth.build_user_chat", {"creds": free["user_creds"]}
        if "message_id" in free:
            return "messages.delete", {"name": free["message_id"]}
        if "patch_body" in free:
            return "messages.patch", {"name": free.get("message_name"),
                                      "updateMask": free.get("update_mask"),
                                      "body": free["patch_body"]}
        if "resource_name" in free:
            return "media.download", {"resourceName": free["resource_name"]}
        if "download_uri" in free:
            return "download_uri.fetch", {"url": free["download_uri"], "timeout": 30}
        if code.co_name == "<lambda>" and fn.__defaults__:
            return "members.list", {"parent": fn.__defaults__[0], "pageSize": 50}
    raise RuntimeError(f"unmarked to_thread callable {fn!r}")


# ── import registry (in-function imports fire one seam each) ──
def import_module_double(full, ctx):
    if full == "google.auth":
        return types.SimpleNamespace(
            default=rec(ctx, "google_auth.default", lambda scopes=None: {"scopes": scopes}))
    if full == PKG + ".oauth":
        return types.SimpleNamespace(
            load_user_credentials=CallableDouble("oauth.load_user_credentials"),
            build_user_chat_service=CallableDouble("oauth.build_user_chat_service"),
            list_authorized_emails=CallableDouble("oauth.list_emails"))
    if full == "agent.redact":
        return types.SimpleNamespace(
            redact_sensitive_text=rec(ctx, "redact_sensitive_text", lambda text: {"text": text}))
    if full == "gateway.parked_route_refusal":
        def factory():
            raw = ctx.call("retryable_ingress", {})
            refused = raw.get("refused", False) if isinstance(raw, dict) else False
            scope = types.SimpleNamespace(refused=refused, open=True)
            class _CM:
                def __enter__(self):
                    return scope
                def __exit__(self, *a):
                    return False
            return _CM()
        return types.SimpleNamespace(ParkedRouteRefused=ParkedRouteRefused,
                                     retryable_ingress=factory)
    if full == "hermes_constants":
        return types.SimpleNamespace(
            get_hermes_home=rec(ctx, "get_hermes_home", lambda: {},
                                build=lambda r: PathDouble(r, ctx)))
    raise RuntimeError(f"unmodeled import {full}")


IMPORT_SEAMS = {
    "google.auth": "import.google.auth",
    PKG + ".oauth": "import.oauth",
    "agent.redact": "import.agent.redact",
    "gateway.parked_route_refusal": "import.gateway.parked_route_refusal",
    "hermes_constants": "import.hermes_constants",
}


# ── seamed sibling argmaps ──
MODULE_ARGMAP = {
    "_http_status": lambda exc: {"exc": proj(exc)},
    "_exc_text": lambda exc, *targets: {"exc": proj(exc), "targets": list(targets)},
    "_proxy_error": lambda exc, *targets: {"exc": proj(exc), "targets": list(targets)},
    "_is_retryable_error": lambda exc: {"exc": proj(exc)},
    "_is_google_owned_host": lambda url: {"url": url},
    "_mime_for_message_type": lambda mime: {"mime": mime},
    "_create_kwargs": lambda chat_id, body: {"chat_id": chat_id, "body": body},
    "_thread_body": lambda text, thread_id: {"text": text, "thread_id": thread_id},
    "_adc_would_borrow_foreign_credentials": lambda: {},
    "_load_sa_credentials_from": lambda sa_value: {"sa_value": sa_value},
}
SELF_ARGMAP = {
    "_resolve_thread_id": lambda reply_to, metadata, chat_id=None: {
        "reply_to": reply_to, "metadata": metadata, "chat_id": chat_id},
    "_chunk_text": lambda text: {"text": text},
    "_note_rate_limit": lambda chat_id: {"chat_id": chat_id},
    "_track_outbound_thread": lambda chat_id, resp: {"chat_id": chat_id, "resp": resp},
    "_release_unadmitted": lambda msg, envelope: {"msg": msg, "envelope": envelope},
    "_connect_failed": lambda log_fmt, exc, code, *, retryable: {
        "log_fmt": log_fmt, "exc": proj(exc), "code": code, "retryable": retryable},
    "_prepare_inbound": lambda envelope, ce_type=None: {"envelope": envelope, "ce_type": ce_type},
    "_save_cached_bot_id": lambda bot_user_id: {"bot_user_id": bot_user_id},
    "_submit_on_loop": lambda coro: {"coro": proj_coro(coro)},
    "_dispatch_message": lambda msg, envelope: {"msg": msg, "envelope": envelope},
    "_build_message_event": lambda msg, envelope: {"msg": msg, "envelope": envelope},
    "_download_attachment": lambda attachment: {"attachment": attachment},
    "_patch_message": lambda message_name, body: {"message_name": message_name, "body": body},
    "_create_message": lambda chat_id, body: {"chat_id": chat_id, "body": body},
    "_call_with_retry": lambda sync_fn, *, op_name="chat-api-call": {
        "sync_fn": proj_callable(sync_fn), "op_name": op_name},
    "_handle_setup_files_command": lambda chat_id, thread_id, raw_text, sender_email=None: {
        "chat_id": chat_id, "thread_id": thread_id, "raw_text": raw_text, "sender_email": sender_email},
    "format_message": lambda content: {"content": content},
    "pause_typing_for_chat": lambda chat_id: {"chat_id": chat_id},
    "resume_typing_for_chat": lambda chat_id: {"chat_id": chat_id},
    "_is_sender_authorized": lambda user_id, chat_type, chat_id, thread_id=None: {
        "user_id": user_id, "chat_type": chat_type, "chat_id": chat_id, "thread_id": thread_id},
    "handle_message": lambda event: {"event": proj(event)},
    "build_source": lambda **kw: kw,
    "_set_fatal_error": lambda **kw: kw,
    "_bot_id_cache_path": lambda: {},
}


def sr_build(raw):
    if raw is None:
        return None
    if isinstance(raw, SendResult):
        return raw
    if isinstance(raw, dict):
        return SendResult(**raw)
    return raw


def setup_case(func, inp, state):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    for nm, fn in REAL.items():
        if nm.startswith("store."):
            NS[nm[6:]] = fn
        else:
            NS[nm] = fn
    NS["logger"] = RecLogger(ctx)
    NS["json"] = types.SimpleNamespace(
        loads=rec(ctx, "json.loads", lambda s: {"s": s}),
        dumps=json.dumps, JSONDecodeError=json.JSONDecodeError)
    NS["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(exists=rec(ctx, "os.path.exists", lambda p: {"path": p})),
        replace=rec(ctx, "os.replace", lambda src, dst: {"src": str(src), "dst": str(dst)}),
        environ=types.SimpleNamespace(get=lambda *a: None))
    NS["random"] = types.SimpleNamespace(random=rec(ctx, "random.random", lambda: {}))

    async def _to_thread(fn, *a, **k):
        marker, siteargs = identify_fn(fn)
        return ctx.call("asyncio.to_thread", {"fn": marker, "args": siteargs})

    async def _sleep(seconds):
        return ctx.call("asyncio.sleep", {"seconds": seconds})

    NS["asyncio"] = types.SimpleNamespace(to_thread=_to_thread, sleep=_sleep)
    NS["_get_scoped_secret"] = rec(
        ctx, "get_scoped_secret", lambda name, default=None: {"name": name, "default": default})
    NS["service_account"] = types.SimpleNamespace(Credentials=types.SimpleNamespace(
        from_service_account_info=rec(
            ctx, "service_account.Credentials.from_service_account_info",
            lambda info, scopes=None: {"info": info, "scopes": scopes})))
    NS["pubsub_v1"] = types.SimpleNamespace(
        SubscriberClient=rec(ctx, "pubsub_v1.SubscriberClient",
                             lambda credentials=None: {"credentials": credentials}))
    NS["gax_exceptions"] = types.SimpleNamespace(
        NotFound=NotFound, PermissionDenied=PermissionDenied, Unauthenticated=Unauthenticated)
    NS["_MEDIA_CACHERS"] = (
        ("image/", rec_async(ctx, "cache_image_from_bytes_async",
                             lambda data, ext=None: {"data": data, "ext": ext}), ".jpg"),
        ("audio/", rec_async(ctx, "cache_audio_from_bytes_async",
                             lambda data, ext=None: {"data": data, "ext": ext}), ".ogg"),
        ("video/", rec_async(ctx, "cache_video_from_bytes_async",
                             lambda data, ext=None: {"data": data, "ext": ext}), ".mp4"),
    )
    NS["cache_document_from_bytes_async"] = rec_async(
        ctx, "cache_document_from_bytes_async", lambda data, filename: {"data": data, "filename": filename})
    NS["MessageEvent"] = rec(ctx, "MessageEvent", lambda **kw: kw)
    # seamed module siblings (not the target)
    for nm in MODULE_SEAMED:
        if nm != func:
            NS[nm] = rec(ctx, nm, MODULE_ARGMAP[nm])
    # adapter self double
    slf = types.SimpleNamespace()
    slf.config = obj_double(state.get("config"))
    slf._LEGACY_USER_IDENTITY = "__legacy__"
    slf._dedup = types.SimpleNamespace(
        is_duplicate=rec(ctx, "dedup.is_duplicate", lambda name: {"name": name}),
        discard=rec(ctx, "dedup.discard", lambda name: {"name": name}))
    slf._thread_count_store = types.SimpleNamespace(
        incr=rec(ctx, "thread_count_store.incr",
                 lambda chat_id, thread_name: {"chat_id": chat_id, "thread_name": thread_name}),
        decr=rec(ctx, "thread_count_store.decr",
                 lambda chat_id, thread_name: {"chat_id": chat_id, "thread_name": thread_name}),
        load=CallableDouble("thread_count_store.load"))
    # per-method state attrs
    for key in ("_shutting_down", "_debug_raw", "_bot_user_id", "_bootstrap_spaces",
                "_user_credentials", "_user_chat_api", "_subscriber"):
        setattr(slf, key, state.get(key))
    slf._typing_messages = state.setdefault("_typing_messages", {})
    slf._last_sender_by_chat = state.setdefault("_last_sender_by_chat", {})
    slf._last_inbound_thread = state.setdefault("_last_inbound_thread", {})
    # bind seamed instance siblings (PLAIN functions: SimpleNamespace attrs are
    # not descriptor-bound, so the recorder must NOT take self)
    for nm in SELF_SEAMED:
        if nm == func:
            continue
        if nm == "_bot_id_cache_path":
            setattr(slf, nm, rec(ctx, nm, SELF_ARGMAP[nm], build=lambda r: PathDouble(r, ctx)))
        elif nm == "_build_message_event":
            setattr(slf, nm, rec_async(ctx, nm, SELF_ARGMAP[nm], build=obj_double))
        elif nm in SELF_SEAMED_ASYNC:
            setattr(slf, nm, rec_async(ctx, nm, SELF_ARGMAP[nm],
                                       build=sr_build if nm in ("_patch_message", "_create_message") else None))
        else:
            setattr(slf, nm, rec(ctx, nm, SELF_ARGMAP[nm]))
    # bind the live target
    if func in METHOD_FUNCS:
        setattr(slf, func, types.MethodType(REAL[func], slf))
    # store double
    store = types.SimpleNamespace()
    store._path = PathDouble(state.get("_path", ""), ctx)
    store._counts = state.setdefault("_counts", {})
    for nm in STORE_FUNCS:
        if "store." + nm == func:
            setattr(store, nm, types.MethodType(REAL["store." + nm], store))
        elif nm == "_save":
            setattr(store, nm, rec(ctx, "store._save", lambda: {}))
        else:
            setattr(store, nm, types.MethodType(REAL["store." + nm], store))
    slf._thread_count_store_real = store
    return ctx, slf, store


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf, store = setup_case(func, inp, state)
    real_import = builtins.__import__

    def case_import(name, globals=None, locals=None, fromlist=(), level=0):
        if level:
            pkg = (globals or {}).get("__package__") or ""
            full = pkg + "." + name
        else:
            full = name
        if full in IMPORT_SEAMS:
            ctx.call(IMPORT_SEAMS[full], {"fromlist": list(fromlist or ())})
            return import_module_double(full, ctx)
        return real_import(name, globals, locals, fromlist, level)

    builtins.__import__ = case_import
    try:
        ret = dispatch(func, args, slf, store, ctx)
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    finally:
        builtins.__import__ = real_import
    # sync mutated state
    if func.startswith("store."):
        state["_counts"] = store._counts
    else:
        state["_typing_messages"] = slf._typing_messages
        state["_last_sender_by_chat"] = slf._last_sender_by_chat
        state["_last_inbound_thread"] = slf._last_inbound_thread
        for key in ("_shutting_down", "_debug_raw", "_bot_user_id",
                    "_user_credentials", "_user_chat_api", "_subscriber"):
            state[key] = getattr(slf, key)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


def dispatch(func, args, slf, store, ctx):
    g = args.get
    if func == "_load_sa_credentials_from":
        return NS[func](g("sa_value"))
    if func == "store.load":
        return store.load()
    if func == "store.get":
        return store.get(g("chat_id"), g("thread_name"))
    if func == "store.incr":
        return store.incr(g("chat_id"), g("thread_name"))
    if func == "store.decr":
        return store.decr(g("chat_id"), g("thread_name"))
    if func == "store._save":
        return store._save()
    if func == "_load_sa_credentials":
        return slf._load_sa_credentials()
    if func == "_bot_id_cache_path":
        return slf._bot_id_cache_path()
    if func == "_save_cached_bot_id":
        return slf._save_cached_bot_id(g("bot_user_id"))
    if func == "_resolve_bot_user_id":
        return _asyncio.run(slf._resolve_bot_user_id())
    if func == "_load_user_oauth":
        return _asyncio.run(slf._load_user_oauth())
    if func == "_check_subscription":
        return _asyncio.run(slf._check_subscription(g("subscription_path"), g("credentials")))
    if func == "_handle_pubsub_message":
        return slf._handle_pubsub_message(message_double(g("message"), ctx))
    if func == "dispatch_http_event":
        return _asyncio.run(slf.dispatch_http_event(g("envelope")))
    if func == "_dispatch_message":
        return _asyncio.run(slf._dispatch_message(g("msg"), g("envelope")))
    if func == "_build_message_event":
        return _asyncio.run(slf._build_message_event(g("msg"), g("envelope")))
    if func == "_download_attachment":
        return _asyncio.run(slf._download_attachment(g("attachment")))
    if func == "send":
        return _asyncio.run(slf.send(g("chat_id"), g("content"), g("reply_to"), g("metadata")))
    if func == "send_card":
        return _asyncio.run(slf.send_card(g("chat_id"), g("card"), g("metadata")))
    if func == "edit_message":
        return _asyncio.run(slf.edit_message(g("chat_id"), g("message_id"), g("content"),
                                             finalize=g("finalize", False)))
    if func == "delete_message":
        return _asyncio.run(slf.delete_message(g("chat_id"), g("message_id")))
    if func == "_patch_message":
        return _asyncio.run(slf._patch_message(g("message_name"), g("body")))
    if func == "_create_message":
        return _asyncio.run(slf._create_message(g("chat_id"), g("body")))
    if func == "_call_with_retry":
        sf = g("sync_fn") or {}
        fn = CallableDouble(sf.get("__callable__", "op"), sf.get("args") or {})
        return _asyncio.run(slf._call_with_retry(fn, op_name=g("op_name", "chat-api-call")))
    raise RuntimeError(f"unknown func {func}")


# ── three-way seam-name cross-check ──
SEAM_NAMES = [
    "logger.debug", "logger.warning", "logger.error", "logger.exception", "logger.info",
    "asyncio.to_thread", "asyncio.sleep", "random.random",
    "json.loads", "path.exists", "path.read_text", "path.write_text", "path.mkdir",
    "os.replace", "os.path.exists",
    "import.google.auth", "import.oauth", "import.agent.redact",
    "import.gateway.parked_route_refusal", "import.hermes_constants",
    "google_auth.default", "service_account.Credentials.from_service_account_info",
    "get_hermes_home", "get_scoped_secret", "pubsub_v1.SubscriberClient",
    "retryable_ingress", "redact_sensitive_text",
    "message.nack", "message.ack",
    "cache_image_from_bytes_async", "cache_audio_from_bytes_async",
    "cache_video_from_bytes_async", "cache_document_from_bytes_async",
    "MessageEvent", "store._save",
    "thread_count_store.incr", "thread_count_store.decr",
    "dedup.is_duplicate", "dedup.discard",
] + sorted(MODULE_SEAMED) + sorted(SELF_SEAMED)


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "google_chat_adapter_flow2.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'gca2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_google_chat_adapter_flow2: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= set(IMPORT_SEAMS.values())
    used |= {f"logger.{lv}" for lv in ("debug", "warning", "error", "exception", "info")}
    used |= set(MODULE_ARGMAP) | set(SELF_ARGMAP) | {"store._save"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_google_chat_adapter_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_google_chat_adapter_flow2: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "google_chat_adapter_flow2.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case["func"])
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_google_chat_adapter_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(ALL_PORTED)
        if unbound:
            print(f"FATAL oracle_google_chat_adapter_flow2: fixture funcs not ported: {sorted(unbound)}")
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
    print(f"ORACLE google_chat_adapter_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    fix = _REPO / "tests" / "fixtures" / "google_chat_adapter_flow2.jsonl"
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
        if not case.get("fc") and isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            if any(f"'{nm}'" in msg for nm in ALL_PORTED + list(SELF_SEAMED) + ["_dedup", "_thread_count_store"]):
                bad += 1
                print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST google_chat_adapter_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = [f for f in ALL_PORTED if f not in counts]
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {missing}")
        bad += 1
    few = [f for f in ALL_PORTED if 0 < counts.get(f, 0) < 5]
    if few:
        print(f"SELFTEST FUNCS WITH <5 CASES: {few}")
        bad += 1
    HAND = [
        ("store.get", {"args": {"chat_id": "c", "thread_name": "t"},
                       "state": {"_counts": {"c": {"t": 3}}}}, 3),
        ("store.get", {"args": {"chat_id": "c", "thread_name": "t"}, "state": {"_counts": {}}}, 0),
        ("store.incr", {"args": {"chat_id": "c", "thread_name": "t"},
                        "state": {"_counts": {"c": {"t": 2}}}, "io": {"store._save": None}}, 2),
        ("store.decr", {"args": {"chat_id": "c", "thread_name": "t"},
                        "state": {"_counts": {"c": {"t": 2}}}, "io": {"store._save": None}}, None),
        ("_load_sa_credentials_from", {"args": {"sa_value": None},
                                       "io": {"_adc_would_borrow_foreign_credentials": False,
                                              "import.google.auth": None,
                                              "google_auth.default": {"__obj__": {"token": "T"}}}},
         {"__obj__": {"token": "T"}}),
        ("_bot_id_cache_path", {"io": {"import.hermes_constants": None, "get_hermes_home": "/home/u"}},
         "/home/u/google_chat_bot_id.json"),
        ("_patch_message", {"args": {"message_name": "spaces/A/messages/M", "body": {"text": "hi"}},
                            "io": {"asyncio.to_thread": {"name": "spaces/A/messages/M2"}}},
         {"success": True, "message_id": "spaces/A/messages/M2", "error": None,
          "raw_response": None, "retryable": False, "retry_after": None,
          "continuation_message_ids": [], "error_kind": None}),
        ("edit_message", {"args": {"chat_id": "c", "message_id": "", "content": "x"}},
         {"success": False, "message_id": None, "error": "missing message_id",
          "raw_response": None, "retryable": False, "retry_after": None,
          "continuation_message_ids": [], "error_kind": None}),
        ("delete_message", {"args": {"chat_id": "c", "message_id": ""}}, False),
    ]
    for func, inp, want in HAND:
        out = drive(func, inp)
        if out["ret"] != canon(want):
            bad += 1
            print(f"SELFTEST HAND CHECK FAILED {func}: want={want!r} got={out['ret']!r}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()