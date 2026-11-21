#!/usr/bin/env python3
"""Oracle for the third-wave UNCLAIMED-remainder FLOW port of
plugins/platforms/google_chat/adapter.py (src/gateway/platforms/google_chat_flow2.c).

Drives the LIVE adapter/module defs with SCRIPTED seams: every sibling method,
asyncio primitive, foreign import, logger, translation and constructor call is
a recording stub fed the same scripts the C harness feeds its universal seam.
Compares the ordered decision trace plus the final return and mutated state.

Summary: 'ORACLE google_chat_flow2: N cases, M mismatches'.
Fail-closed: missing defs, seam-set drift, or 0 cases => exit 2.
"""
import ast
import asyncio as _asyncio
import builtins
import copy
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
Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "google_chat", "adapter.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
_FLAGS = _fut.annotations.compiler_flag
MODNAME = "google_chat_flow2_oracle"
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


# ── ported targets ──
MODULE_FUNCS = ["_get_google_id_token_request", "_verify_google_id_token", "_load_google_modules"]
METHOD_FUNCS = [
    "format_message", "send_animation", "send_document", "send_image_file",
    "send_video", "send_voice", "_send_file_reply", "_new_authed_http",
    "_handle_setup_files_command", "_refresh_or_none", "_patch_quietly",
    "_on_pubsub_message", "_post_attachment_fallback",
    "_consume_typing_card_with_text", "_acquire_user_chat_api",
    "_load_per_user_chat_api", "_submit_on_loop",
]
ALL_PORTED = MODULE_FUNCS + METHOD_FUNCS
ASYNC_FUNCS = {
    "send_animation", "send_document", "send_image_file", "send_video",
    "send_voice", "_send_file_reply", "_handle_setup_files_command",
    "_refresh_or_none", "_patch_quietly", "_post_attachment_fallback",
    "_consume_typing_card_with_text", "_acquire_user_chat_api",
    "_load_per_user_chat_api",
}
PLAIN_FUNCS = {"format_message", "_refresh_or_none"}  # classmethod / staticmethod

DICT_KEYS = ["_typing_messages", "_user_chat_api_by_email", "_user_creds_by_email"]
SCALAR_KEYS = [
    "_user_chat_api", "_user_credentials", "_credentials", "_scope_ctx",
    "_loop", "_google_id_token_request", "GOOGLE_CHAT_AVAILABLE",
    "_google_modules_loaded",
]
MODULE_LOAD_KEYS = [
    "httplib2", "pubsub_v1", "gax_exceptions", "service_account",
    "AuthorizedHttp", "build_service", "HttpError", "MediaFileUpload",
]
STATE_KEYS = DICT_KEYS + SCALAR_KEYS + MODULE_LOAD_KEYS


class _SilentLogger:
    def __getattr__(self, level):
        return lambda *a, **k: None


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "os": os, "json": json, "enum": None,
        "Any": object, "Optional": object, "Callable": object,
        "Dict": dict, "List": list, "Tuple": tuple,
        "dataclass": dataclass, "field": field, "asdict": asdict,
        "contextlib": __import__("contextlib"),
        "threading": __import__("threading"),
        "asyncio": _asyncio, "logging": __import__("logging"),
        "__package__": PKG, "__name__": MODNAME,
    })
    src = open(A_SRC, encoding="utf-8").read()
    seed_constants(src, ns, A_SRC)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, A_SRC)
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
    ns["logger"] = _SilentLogger()
    required = ALL_PORTED + ["SendResult", "_GOOGLE_IMPORTS", "_TYPING_CONSUMED_SENTINEL"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_google_chat_flow2: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
RUNTIME_NS = {}
REAL = {nm: NS[nm] for nm in ALL_PORTED}
SendResult = NS["SendResult"]


# ── canonical projection ──
class CallableDouble:
    def __init__(self, marker, args=None):
        self.__gc2_marker__ = marker
        self.__gc2_args__ = args or {}

    def __call__(self, *a, **k):
        return None


def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, CallableDouble):
            return {"__callable__": x.__gc2_marker__, "args": x.__gc2_args__}
        if isinstance(x, ModuleDouble):
            return conv(x._m)
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items() if not k.startswith("__")})
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exceptions ──
HttpError = type("HttpError", (Exception,), {})
EXC_CLASSES = {
    "OSError": OSError, "IOError": OSError, "TimeoutError": TimeoutError,
    "PermissionError": PermissionError, "FileNotFoundError": FileNotFoundError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "JSONDecodeError": json.JSONDecodeError,
    "HttpError": HttpError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and isinstance(cls, type) and issubclass(cls, OSError):
        return cls(en, msg)
    if kind == "JSONDecodeError":
        return cls(msg, spec.get("doc", ""), spec.get("pos", 0))
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


def proj(x):
    if isinstance(x, BaseException):
        return {"cls": type(x).__name__, "msg": str(x), "attrs": {}}
    if isinstance(x, CallableDouble):
        return {"__callable__": x.__gc2_marker__, "args": x.__gc2_args__}
    return x


# ── universal seam backend ──
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


class SeamCoro:
    def __init__(self, ctx, name, args, build=None):
        self._ctx, self._name, self._args, self._build = ctx, name, args, build

    def __await__(self):
        ctx, name, args, build = self._ctx, self._name, self._args, self._build

        async def _run():
            raw = ctx.call(name, args)
            return build(raw) if build else raw
        return _run().__await__()


def rec_async(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        return SeamCoro(ctx, name, args_fn(*a, **k) if args_fn else {}, build)
    return f


def sr_build(raw):
    if raw is None:
        return None
    if isinstance(raw, SendResult):
        return raw
    if isinstance(raw, dict):
        return SendResult(**raw)
    return raw


class RecLogger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log


class ModuleDouble(types.SimpleNamespace):
    """__getattr__ fires the 'getattr' seam (obj=raw ret of import seam)."""

    def __init__(self, marker, ctx):
        super().__init__()
        object.__setattr__(self, "_m", marker)
        object.__setattr__(self, "_ctx", ctx)

    def __getattr__(self, attr):
        if attr.startswith("__"):
            raise AttributeError(attr)
        return self._ctx.call("getattr", {"obj": self._m, "attr": attr})


class CtxCopyDouble:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "copy", rec(ctx, "scope_ctx.copy", lambda: {}, build=lambda r: CtxRunDouble(ctx)))
        object.__setattr__(self, "run", None)  # set by build below


class FutureDouble(types.SimpleNamespace):
    pass


class CtxRunDouble:
    """ctx.run(fn, ...) recorder; fn marker decides the arg projection."""

    def __init__(self, ctx):
        def run(fn, *a, **k):
            marker = getattr(fn, "__gc2_marker__", getattr(fn, "__name__", "fn"))
            if marker == "_handle_pubsub_message":
                args = {"fn": marker, "message": proj(a[0]) if a else None}
            else:
                args = {"fn": "safe_schedule_threadsafe",
                        "coro": proj(a[0]) if a else None,
                        "loop": proj(a[1]) if len(a) > 1 else None,
                        "log_message": k.get("log_message"),
                        "log_level": k.get("log_level")}
            raw = ctx.call("context.run", args)
            if raw is None:
                return None
            return FutureDouble(add_done_callback=rec(
                ctx, "future.add_done_callback", lambda cb: {"callback": getattr(cb, "__gc2_marker__", "_log_background_failure")}))
        self.run = run


# ── import registry ──
IMPORT_SEAMS = {
    "google.oauth2": "import.google.oauth2",
    "google.auth.transport": "import.google.auth.transport",
    PKG + ".oauth": "import.oauth",
    PKG + ".setup_files": "import.setup_files",
    "agent.async_utils": "import.agent.async_utils",
}


def import_module_double(full, ctx):
    if full == "google.oauth2":
        return types.SimpleNamespace(
            id_token=types.SimpleNamespace(verify_oauth2_token=rec(
                ctx, "id_token.verify_oauth2_token",
                lambda token, request, audience: {"token": token, "request": proj(request), "audience": audience})))
    if full == "google.auth.transport":
        return types.SimpleNamespace(
            requests=types.SimpleNamespace(Request=rec(ctx, "google_requests.Request", lambda: {})))
    if full == PKG + ".oauth":
        return types.SimpleNamespace(
            refresh_or_none=CallableDouble("refresh_or_none"),
            load_user_credentials=CallableDouble("load_user_credentials"),
            build_user_chat_service=CallableDouble("build_user_chat_service"))
    if full == PKG + ".setup_files":
        return types.SimpleNamespace(
            handle_setup_files_command=rec_async(
                ctx, "handle_setup_files_command",
                lambda _self, chat_id, thread_id, raw_text, sender_email=None:
                    {"chat_id": chat_id, "thread_id": thread_id, "raw_text": raw_text, "sender_email": sender_email}))
    if full == "agent.async_utils":
        return types.SimpleNamespace(safe_schedule_threadsafe=CallableDouble("safe_schedule_threadsafe"))
    return types.SimpleNamespace()


def identify_fn(fn):
    if isinstance(fn, CallableDouble):
        return fn.__gc2_marker__, fn.__gc2_args__
    code = getattr(fn, "__code__", None)
    if code and code.co_freevars and fn.__closure__:
        free = dict(zip(code.co_freevars, (c.cell_contents for c in fn.__closure__)))
        if "creds" in free:
            return "build_user_chat_service", {"creds": free["creds"]}
    return getattr(fn, "__name__", "fn"), {}


def setup_case(func, inp, state):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    for nm in ALL_PORTED:
        NS[nm] = REAL[nm]
    # per-case global reset from state
    for k in SCALAR_KEYS + MODULE_LOAD_KEYS:
        NS[k] = state.get(k)

    NS["logger"] = RecLogger(ctx)
    NS["HttpError"] = HttpError
    NS["logging"] = types.SimpleNamespace(WARNING=30)
    NS["t"] = rec(ctx, "t", lambda key, **kw: {"key": key, "kwargs": dict(kw)})
    NS["_thread_body"] = rec(ctx, "_thread_body", lambda body, thread_id=None:
                             {"body": body, "thread_id": thread_id})
    NS["_proxy_error"] = rec(ctx, "_proxy_error", lambda exc: {"exc": proj(exc)})
    NS["_http_status"] = rec(ctx, "_http_status", lambda exc: {"exc": proj(exc)})
    NS["_format_message"] = rec(ctx, "_format_message", lambda content: {"content": content})
    NS["asyncio"] = types.SimpleNamespace(to_thread=_make_to_thread(ctx))
    NS["contextvars"] = types.SimpleNamespace(
        copy_context=rec(ctx, "contextvars.copy_context", lambda: {}, build=lambda r: CtxRunDouble(ctx)))
    NS["importlib"] = types.SimpleNamespace(import_module=rec(
        ctx, "importlib.import_module", lambda module: {"module": module}, build=lambda r: ModuleDouble(r, ctx)))
    NS["httplib2"] = types.SimpleNamespace(Http=rec(ctx, "httplib2.Http", lambda timeout: {"timeout": timeout}))
    NS["AuthorizedHttp"] = rec(ctx, "AuthorizedHttp", lambda credentials, http=None:
                               {"credentials": proj(credentials), "http": proj(http)})
    NS["_CachedGoogleAuthRequest"] = rec(ctx, "_CachedGoogleAuthRequest", lambda request: {"request": proj(request)})
    # module-level ported collaborator seams (composed paths)
    if func != "_get_google_id_token_request":
        NS["_get_google_id_token_request"] = rec(ctx, "_get_google_id_token_request", lambda: {})

    slf = types.SimpleNamespace()
    slf._LEGACY_USER_IDENTITY = "__legacy__"
    slf._credentials = state.get("_credentials")
    slf._user_chat_api = state.get("_user_chat_api")
    slf._user_credentials = state.get("_user_credentials")
    slf._loop = state.get("_loop")
    for k in DICT_KEYS:
        setattr(slf, k, state.setdefault(k, {}))
    sc = state.get("_scope_ctx")
    if sc is not None:
        cd = CtxCopyDouble(ctx)
        cd.run = CtxRunDouble(ctx).run
        slf._scope_ctx = cd
    else:
        slf._scope_ctx = None
    # seamed instance siblings
    slf._load_per_user_chat_api = rec_async(ctx, "_load_per_user_chat_api", lambda email: {"email": email})
    slf._refresh_or_none = rec_async(ctx, "_refresh_or_none",
        lambda creds, email, log_msg: {"creds": creds, "email": email, "log_msg": log_msg})
    slf._invalidate_user_creds = rec(ctx, "_invalidate_user_creds", lambda identity: {"identity": identity})
    slf._patch_message = rec_async(ctx, "_patch_message",
        lambda message_name, body: {"message_name": message_name, "body": body}, build=sr_build)
    slf._create_message = rec_async(ctx, "_create_message",
        lambda chat_id, body: {"chat_id": chat_id, "body": body}, build=sr_build)
    slf._resolve_thread_id = rec(ctx, "_resolve_thread_id",
        lambda reply_to, metadata, chat_id=None: {"reply_to": reply_to, "metadata": metadata, "chat_id": chat_id})
    slf._send_file = rec_async(ctx, "_send_file",
        lambda chat_id, path, caption, mime_hint=None, thread_id=None, override_filename=None:
            {"chat_id": chat_id, "path": path, "caption": caption, "mime_hint": mime_hint,
             "thread_id": thread_id, "override_filename": override_filename})
    slf.warning_text = rec(ctx, "warning_text", lambda text, caption="": {"text": text, "caption": caption})
    slf._handle_pubsub_message = CallableDouble("_handle_pubsub_message")
    slf._log_background_failure = CallableDouble("_log_background_failure")
    slf.send_image = rec_async(ctx, "send_image",
        lambda chat_id, image_url, caption=None, reply_to=None, metadata=None:
            {"chat_id": chat_id, "image_url": image_url, "caption": caption,
             "reply_to": reply_to, "metadata": metadata})
    slf._send_file_reply = rec_async(ctx, "_send_file_reply",
        lambda chat_id, path, caption, reply_to, kwargs, mime_hint=None, override_filename=None:
            {"chat_id": chat_id, "path": path, "caption": caption, "reply_to": reply_to,
             "kwargs": kwargs, "mime_hint": mime_hint, "override_filename": override_filename})
    slf._loop_accepts_callbacks = rec(ctx, "_loop_accepts_callbacks", lambda loop: {"loop": proj(loop)})
    # live target binding
    if func in METHOD_FUNCS:
        if func in PLAIN_FUNCS:
            setattr(slf, func, REAL[func])
        else:
            setattr(slf, func, types.MethodType(REAL[func], slf))
    RUNTIME_NS.clear()
    RUNTIME_NS.update({k: NS.get(k) for k in ("httplib2", "AuthorizedHttp", "HttpError")})
    return ctx, slf


def _make_to_thread(ctx):
    async def _to_thread(fn, *a, **k):
        marker, siteargs = identify_fn(fn)
        if marker == "refresh_or_none":
            siteargs = {"creds": a[0] if a else None, "email": a[1] if len(a) > 1 else None}
        elif marker == "load_user_credentials":
            siteargs = {"email": a[0] if a else None}
        raw = ctx.call("asyncio.to_thread", {"fn": marker, "args": siteargs})
        return raw
    return _to_thread


# ── drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    for k in DICT_KEYS:
        state.setdefault(k, {})
    for k in SCALAR_KEYS + MODULE_LOAD_KEYS:
        state.setdefault(k, None)
    ctx, slf = setup_case(func, inp, state)
    real_import = builtins.__import__

    def case_import(name, globals=None, locals=None, fromlist=(), level=0):
        if level:
            pkg = (globals or {}).get("__package__") or PKG
            full = pkg + "." + name
        else:
            full = name
        if full in IMPORT_SEAMS:
            ctx.call(IMPORT_SEAMS[full], {"fromlist": list(fromlist or ())})
            return import_module_double(full, ctx)
        return real_import(name, globals, locals, fromlist, level)

    builtins.__import__ = case_import
    try:
        ret = dispatch(func, args, slf, ctx)
    except BaseException as exc:
        ret = proj_exc(exc)
    finally:
        builtins.__import__ = real_import
    # sync state
    for k in DICT_KEYS:
        state[k] = getattr(slf, k, state.get(k))
    for k in ("_credentials", "_user_chat_api", "_user_credentials", "_loop"):
        state[k] = getattr(slf, k, state.get(k))
    for k in ("_google_id_token_request", "GOOGLE_CHAT_AVAILABLE", "_google_modules_loaded") + tuple(MODULE_LOAD_KEYS):
        v = NS.get(k, state.get(k))
        state[k] = None if v is RUNTIME_NS.get(k) else v
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


def dispatch(func, args, slf, ctx):
    g = args.get
    if func == "format_message":
        return NS[func](None, g("content"))
    if func == "_refresh_or_none":
        return _asyncio.run(slf._refresh_or_none(g("creds"), g("email"), g("log_msg")))
    if func == "_verify_google_id_token":
        return NS[func](g("token"), g("audience"))
    if func == "_get_google_id_token_request":
        return NS[func]()
    if func == "_load_google_modules":
        return NS[func]()
    if func == "send_animation":
        return _asyncio.run(slf.send_animation(g("chat_id"), g("animation_url"), caption=g("caption"),
                                               reply_to=g("reply_to"), metadata=g("metadata")))
    if func in ("send_document", "send_image_file", "send_video", "send_voice"):
        return _asyncio.run(getattr(slf, func)(g("chat_id"), g("path"), caption=g("caption"),
                                               reply_to=g("reply_to"), **g("kwargs") or {}))
    if func == "_send_file_reply":
        return _asyncio.run(slf._send_file_reply(g("chat_id"), g("path"), g("caption"), g("reply_to"),
                                                  g("kwargs") or {}, g("mime_hint"),
                                                  override_filename=g("override_filename")))
    if func == "_new_authed_http":
        return slf._new_authed_http()
    if func == "_handle_setup_files_command":
        return _asyncio.run(slf._handle_setup_files_command(g("chat_id"), g("thread_id"), g("raw_text"),
                                                             g("sender_email")))
    if func == "_patch_quietly":
        return _asyncio.run(slf._patch_quietly(g("message_name"), g("text"), g("log_msg"), *(g("log_args") or [])))
    if func == "_on_pubsub_message":
        return slf._on_pubsub_message(g("message"))
    if func == "_post_attachment_fallback":
        return _asyncio.run(slf._post_attachment_fallback(g("chat_id"), g("path"), g("filename"),
                                                           g("caption"), g("thread_id")))
    if func == "_consume_typing_card_with_text":
        return _asyncio.run(slf._consume_typing_card_with_text(g("chat_id"), g("text")))
    if func == "_acquire_user_chat_api":
        return _asyncio.run(slf._acquire_user_chat_api(g("sender_email")))
    if func == "_load_per_user_chat_api":
        return _asyncio.run(slf._load_per_user_chat_api(g("email")))
    if func == "_submit_on_loop":
        return slf._submit_on_loop(g("coro"))
    raise RuntimeError(f"unknown func {func}")


# ── seam-name set equality ──
SEAM_NAMES = [
    "AuthorizedHttp", "_CachedGoogleAuthRequest", "_create_message",
    "_exc_text", "_format_message", "_get_google_id_token_request",
    "_http_status", "_invalidate_user_creds", "_load_per_user_chat_api",
    "_loop_accepts_callbacks", "_patch_message", "_proxy_error",
    "_refresh_or_none", "_resolve_thread_id", "_send_file", "_send_file_reply",
    "_thread_body", "asyncio.to_thread", "context.run", "contextvars.copy_context",
    "future.add_done_callback", "getattr", "google_requests.Request",
    "handle_setup_files_command", "httplib2.Http", "id_token.verify_oauth2_token",
    "import.agent.async_utils", "import.google.auth.transport", "import.google.oauth2",
    "import.oauth", "import.setup_files", "importlib.import_module",
    "logger.debug", "logger.error", "logger.exception", "logger.info",
    "logger.warning", "scope_ctx.copy", "send_image", "t",
    "warning_text",
]
PORTED_SEAMS = {
    "format_message": ["_format_message"],
    "_verify_google_id_token": ["import.google.oauth2", "_get_google_id_token_request", "id_token.verify_oauth2_token"],
    "_get_google_id_token_request": ["import.google.auth.transport", "google_requests.Request", "_CachedGoogleAuthRequest"],
    "_load_google_modules": ["importlib.import_module", "getattr"],
    "send_animation": ["send_image"],
    "send_document": ["_send_file_reply"], "send_image_file": ["_send_file_reply"],
    "send_video": ["_send_file_reply"], "send_voice": ["_send_file_reply"],
    "_send_file_reply": ["_resolve_thread_id", "_send_file"],
    "_new_authed_http": ["httplib2.Http", "AuthorizedHttp"],
    "_handle_setup_files_command": ["import.setup_files", "handle_setup_files_command"],
    "_refresh_or_none": ["import.oauth", "asyncio.to_thread", "_proxy_error", "logger.debug"],
    "_patch_quietly": ["_patch_message", "_proxy_error", "logger.debug"],
    "_on_pubsub_message": ["scope_ctx.copy", "contextvars.copy_context", "context.run"],
    "_post_attachment_fallback": ["t", "warning_text", "_thread_body", "_create_message", "_proxy_error", "logger.debug"],
    "_consume_typing_card_with_text": ["_patch_message", "_http_status"],
    "_acquire_user_chat_api": ["_load_per_user_chat_api", "_refresh_or_none", "logger.warning", "_invalidate_user_creds"],
    "_load_per_user_chat_api": ["import.oauth", "asyncio.to_thread", "_refresh_or_none", "_invalidate_user_creds", "logger.debug", "_proxy_error"],
    "_submit_on_loop": ["_loop_accepts_callbacks", "logger.warning", "import.agent.async_utils", "scope_ctx.copy", "context.run", "future.add_done_callback"],
}


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "google_chat_flow2.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'gc2_call\(\s*ctx,\s*"([^"]+)"', csrc))
    c_names |= {f"logger.{lv}" for lv in ("debug", "info", "warning", "error", "exception")}
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_google_chat_flow2: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec(?:_async)?\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= set(IMPORT_SEAMS.values())
    used |= {f"logger.{lv}" for lv in ("debug", "warning", "error", "exception", "info")}
    used |= set(SEAM_NAMES)
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_google_chat_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_google_chat_flow2: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "google_chat_flow2.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                case = json.loads(line)
                fix_keys |= set((case.get("in") or {}).get("io") or {})
                fix_funcs.add(case["func"])
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_google_chat_flow2: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(ALL_PORTED)
        if unbound:
            print(f"FATAL oracle_google_chat_flow2: fixture funcs not ported: {sorted(unbound)}")
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
    if shown >= 14:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:420]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:420]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:420]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:420]}")
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
    print(f"ORACLE google_chat_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    fix = _REPO / "tests" / "fixtures" / "google_chat_flow2.jsonl"
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
            if any(f"'{nm}'" in msg for nm in ALL_PORTED):
                bad += 1
                print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST google_chat_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = [f for f in ALL_PORTED if f not in counts]
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {missing}")
        bad += 1
    few = [f for f in ALL_PORTED if 0 < counts.get(f, 0) < 6]
    if few:
        print(f"SELFTEST FUNCS WITH <6 CASES: {few}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
