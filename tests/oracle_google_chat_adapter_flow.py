#!/usr/bin/env python3
"""Oracle for the Google Chat adapter FLOW port
(src/gateway/platforms/google_chat_adapter_flow.c).

Drives the LIVE plugins/platforms/google_chat/adapter.py leaves and flow
methods with SCRIPTED seams: every sibling method, module helper, injected
callable, logger call, scoped-secret / env read and foreign import is a
recording stub fed the same scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the mutated adapter state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; logger re-set to the recorder
AFTER seeding). Every exec namespace is a registered sys.modules module
(py3.14). The heavy quarry import chain is never imported; MessageType is a
real enum seeded with the live values; foreign helpers are recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE google_chat_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds errno when set, message "[Errno N] msg"). BaseException kinds
  (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) pass through
  every except-Exception site.
- Exception doubles (args.exc): {"cls": name, "msg": str(exc), "attrs": {...}};
  attrs values are attribute carriers (resp -> SimpleNamespace). The spec is
  projected verbatim into seam args.
- Attribute carriers (config / loop / parsed) travel as {"__obj__": {attrs}};
  a plain JSON object IS a Python dict. loop.is_closed == "seam" marks a
  scripted callable; absent -> the default lambda (no seam event).
- MessageType returns project to .value; tuple returns project to arrays.
- logger seams record {"fmt": <literal>, "args": [<raw args>]}; exc_info drops.
  seed_extra_from_env's constant keys arg elides; get_scoped_secret args carry
  {name, default} (default null for 1-arg call sites).
- Path modeling POSIX; read_text(utf-8-sig) strips ONE leading BOM after the
  seam call. ASCII scope: strip/lower/upper fold ASCII; int() parses ASCII
  digits with the int-strip set (White_Space minus 0x1c-0x1f).
- urlparse is SEAMED (result scripted as {scheme, hostname}); _http_status
  scripts return int/bool/null/str (no floats); json.loads corpus is
  clearly-valid / clearly-invalid JSON.
"""
import ast
import asyncio
import copy
import enum
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "google_chat", "adapter.py")

_FLAGS = _fut.annotations.compiler_flag
MODNAME = "google_chat_adapter_flow_oracle"


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


MODULE_FUNCS = [
    "_thread_body", "_create_kwargs", "_mime_for_message_type", "_http_status",
    "_is_retryable_error", "_redact_sensitive", "_proxy_error", "_exc_text",
    "_is_google_owned_host", "_adc_would_borrow_foreign_credentials",
    "_validate_config", "_is_connected", "_env_inbound_settings",
    "_env_inbound_configured", "check_google_chat_requirements",
    "_check_for_registry", "_env_enablement", "_standalone_error",
]
METHOD_FUNCS = [
    "_int_setting", "_str_setting", "_extract_message_payload",
    "_prepare_inbound", "_resolve_thread_id", "_note_rate_limit",
    "_invalidate_user_creds", "stop_typing", "_track_outbound_thread",
    "_release_unadmitted", "_connect_failed", "_chunk_text",
    "verify_http_event_request", "_load_cached_bot_id",
    "_log_background_failure", "_loop_accepts_callbacks",
]
STATIC_METHODS = {
    "_int_setting", "_str_setting", "_extract_message_payload",
    "_log_background_failure", "_loop_accepts_callbacks",
}
# ported module siblings that are SEAMED when composed into another flow
MODULE_SEAMED = {
    "_http_status", "_proxy_error", "_redact_sensitive", "_exc_text",
    "_validate_config", "_env_inbound_settings", "_env_inbound_configured",
    "check_google_chat_requirements",
}
ALL_PORTED = MODULE_FUNCS + METHOD_FUNCS


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "asyncio": asyncio, "os": os, "re": re, "json": json,
        "MessageType": MessageType, "enum": enum,
        "logging": __import__("logging"), "threading": __import__("threading"),
        "Any": object, "Optional": object, "Callable": object,
        "Dict": dict, "List": list, "Tuple": tuple,
    })
    src = open(A_SRC, encoding="utf-8").read()
    seed_constants(src, ns, A_SRC)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, A_SRC)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "GoogleChatAdapter")
    for sub in cls.body:
        if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHOD_FUNCS:
            sub = copy.copy(sub)
            sub.decorator_list = []
            exec_node(src, sub, ns, A_SRC)
    # the logger Assign ran during seeding; re-set the recorder AFTER
    ns["logger"] = _SilentLogger()
    required = ALL_PORTED + [
        "_RETRYABLE_HTTP_STATUSES", "_TRANSPORT_ERROR_PHRASES",
        "_TRUSTED_ATTACHMENT_HOSTS", "_REDACTIONS", "_MIME_MESSAGE_TYPES",
        "_MAX_TEXT_LENGTH", "_TYPING_CONSUMED_SENTINEL", "_ENV_SEED_KEYS",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_google_chat_adapter_flow: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in ALL_PORTED}


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items() if k != "__spec__"})
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
    "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and isinstance(cls, type) and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


def attr_double(spec):
    """plain dict -> SimpleNamespace attribute carrier (exc attrs)."""
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            setattr(d, k, attr_double(v))
        d.__spec__ = spec
        return d
    return spec


def obj_double(spec):
    """{"__obj__": {attrs}} -> SimpleNamespace; nested __obj__ recurse."""
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


# ── per-case script backend (mirrors the C gca_call exactly) ──
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


class RecLogger:
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log


class EnvDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def get(self, key, default=None):
        return self._ctx.call("os.environ.get", {"key": key})


class PathDouble:
    """_bot_id_cache_path() returns a Path; live calls .read_text on it."""
    def __init__(self, path, ctx):
        self._path = path
        self._ctx = ctx

    def read_text(self, encoding=None, errors=None):
        raw = self._ctx.call("bot_id_cache.read_text",
                             {"path": self._path, "encoding": encoding})
        # utf-8-sig strips ONE leading BOM after the seam (trace ret stays raw)
        if encoding == "utf-8-sig" and isinstance(raw, str) and raw.startswith("\ufeff"):
            return raw[1:]
        return raw

    def __str__(self):
        return self._path if isinstance(self._path, str) else ""


MODULE_ARGMAP = {
    "_http_status": lambda exc: {"exc": proj(exc)},
    "_proxy_error": lambda exc, *targets: {"exc": proj(exc), "targets": list(targets)},
    "_redact_sensitive": lambda text: {"text": text},
    "_exc_text": lambda exc, *targets: {"exc": proj(exc), "targets": list(targets)},
    "_validate_config": lambda config: {"config": proj(config)},
    "_env_inbound_settings": lambda: {},
    "_env_inbound_configured": lambda: {},
    "check_google_chat_requirements": lambda: {},
}


def loop_double(spec, ctx):
    if spec is None:
        return None
    d = obj_double(spec)
    inner = spec.get("__obj__", {}) if isinstance(spec, dict) else {}
    if inner.get("is_closed") == "seam":
        d.is_closed = rec(ctx, "loop.is_closed", lambda: {})
    elif hasattr(d, "is_closed"):
        del d.is_closed
    return d


def setup_case(func, inp, state):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    # restore every target def to REAL before installing this case's stubs
    for nm, fn in REAL.items():
        NS[nm] = fn
    NS["logger"] = RecLogger(ctx)
    NS["_get_scoped_secret"] = rec(
        ctx, "get_scoped_secret", lambda name, default=None: {"name": name, "default": default})
    NS["_seed_extra_from_env"] = rec(
        ctx, "seed_extra_from_env", lambda keys, home_env=None: {"home_env": home_env})
    NS["send_error"] = rec(ctx, "send_error", lambda msg: {"msg": msg})
    NS["is_multiplex_active"] = rec(ctx, "is_multiplex_active", lambda: {})
    NS["urlparse"] = rec(ctx, "urlparse", lambda url: {"url": url}, build=obj_double)
    NS["describe_proxy_error"] = rec(
        ctx, "describe_proxy_error", lambda exc, proxies: {"exc": proj(exc), "proxies": proxies})
    NS["env_proxy_for_redaction"] = rec(
        ctx, "env_proxy_for_redaction", lambda *targets: {"targets": list(targets)})
    NS["_load_google_modules"] = rec(ctx, "_load_google_modules", lambda: {})
    NS["_verify_google_id_token"] = rec(
        ctx, "_verify_google_id_token", lambda token, audience: {"token": token, "audience": audience})
    NS["os"] = types.SimpleNamespace(environ=EnvDouble(ctx))
    # seamed ported module siblings (not the target)
    for nm in MODULE_SEAMED:
        if nm != func:
            NS[nm] = rec(ctx, nm, MODULE_ARGMAP[nm])
    # adapter self double
    slf = types.SimpleNamespace()
    slf._dedup = types.SimpleNamespace(
        is_duplicate=rec(ctx, "dedup.is_duplicate", lambda name: {"name": name}),
        discard=rec(ctx, "dedup.discard", lambda name: {"name": name}))
    slf._thread_count_store = types.SimpleNamespace(
        incr=rec(ctx, "thread_count_store.incr",
                 lambda chat_id, thread_name: {"chat_id": chat_id, "thread_name": thread_name}),
        decr=rec(ctx, "thread_count_store.decr",
                 lambda chat_id, thread_name: {"chat_id": chat_id, "thread_name": thread_name}))
    slf._set_fatal_error = rec(ctx, "_set_fatal_error", lambda **kw: kw)
    slf._LEGACY_USER_IDENTITY = "__legacy__"  # class constant read through self
    slf._bot_id_cache_path = rec(ctx, "_bot_id_cache_path", lambda: {},
                                 build=lambda r: PathDouble(r, ctx))
    # per-method state attrs (no creation for read-only methods)
    if func == "_note_rate_limit":
        slf._rate_limit_hits = state.setdefault("_rate_limit_hits", {})
    elif func == "stop_typing":
        slf._typing_messages = state.setdefault("_typing_messages", {})
    elif func == "_resolve_thread_id":
        slf._last_inbound_thread = state.get("_last_inbound_thread")
    elif func == "verify_http_event_request":
        slf._http_events_audience = state.get("_http_events_audience")
        slf._http_events_service_account_email = state.get("_http_events_service_account_email")
    elif func == "_invalidate_user_creds":
        ident = args.get("identity")
        slf._user_credentials = state.get("_user_credentials")
        slf._user_chat_api = state.get("_user_chat_api")
        if ident and ident != "__legacy__":
            slf._user_creds_by_email = state.setdefault("_user_creds_by_email", {})
            slf._user_chat_api_by_email = state.setdefault("_user_chat_api_by_email", {})
        else:
            slf._user_creds_by_email = state.get("_user_creds_by_email")
            slf._user_chat_api_by_email = state.get("_user_chat_api_by_email")
    # bind the target method + the seamed static sibling _extract_message_payload
    for nm in METHOD_FUNCS:
        if nm == func:
            fn = REAL[nm]
        elif nm == "_extract_message_payload":
            fn = rec(ctx, "_extract_message_payload",
                     lambda envelope, ce_type="": {"envelope": envelope, "ce_type": ce_type})
        else:
            continue  # not composed by any ported target
        if nm in STATIC_METHODS:
            setattr(slf, nm, fn)
        else:
            setattr(slf, nm, types.MethodType(fn, slf))
    return ctx, slf# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf = setup_case(func, inp, state)
    try:
        if func == "_thread_body":
            ret = NS[func](args.get("text"), args.get("thread_id"))
        elif func == "_create_kwargs":
            ret = NS[func](args.get("chat_id"), args.get("body"))
        elif func == "_mime_for_message_type":
            ret = NS[func](args.get("mime")).value
        elif func == "_http_status":
            ret = NS[func](make_exc_obj(args.get("exc")))
        elif func == "_is_retryable_error":
            ret = NS[func](make_exc_obj(args.get("exc")))
        elif func == "_redact_sensitive":
            ret = NS[func](args.get("text"))
        elif func == "_proxy_error":
            ret = NS[func](make_exc_obj(args.get("exc")), *args.get("targets", []))
        elif func == "_exc_text":
            ret = NS[func](make_exc_obj(args.get("exc")), *args.get("targets", []))
        elif func == "_is_google_owned_host":
            ret = NS[func](args.get("url"))
        elif func == "_adc_would_borrow_foreign_credentials":
            ret = NS[func]()
        elif func == "_validate_config":
            ret = NS[func](obj_double(args.get("config")))
        elif func == "_is_connected":
            ret = NS[func](obj_double(args.get("config")))
        elif func in ("_env_inbound_settings", "_env_inbound_configured",
                      "check_google_chat_requirements", "_check_for_registry",
                      "_env_enablement"):
            ret = NS[func]()
        elif func == "_standalone_error":
            ret = NS[func](args.get("detail"))
        elif func == "_int_setting":
            ret = slf._int_setting(args.get("extra"), args.get("key"),
                                   args.get("env_name"), args.get("default"))
        elif func == "_str_setting":
            ret = slf._str_setting(args.get("extra"), args.get("key"),
                                   args.get("env_name"), args.get("fallback", ""))
        elif func == "_extract_message_payload":
            ret = NS[func](args.get("envelope"), args.get("ce_type", ""))
        elif func == "_prepare_inbound":
            ret = slf._prepare_inbound(args.get("envelope"), args.get("ce_type"))
        elif func == "_resolve_thread_id":
            ret = slf._resolve_thread_id(args.get("reply_to"), args.get("metadata"),
                                         chat_id=args.get("chat_id"))
        elif func == "_note_rate_limit":
            ret = slf._note_rate_limit(args.get("chat_id"))
        elif func == "_invalidate_user_creds":
            ret = slf._invalidate_user_creds(args.get("identity"))
        elif func == "stop_typing":
            ret = asyncio.run(slf.stop_typing(args.get("chat_id")))
        elif func == "_track_outbound_thread":
            ret = slf._track_outbound_thread(args.get("chat_id"), args.get("resp"))
        elif func == "_release_unadmitted":
            ret = slf._release_unadmitted(args.get("msg"), args.get("envelope"))
        elif func == "_connect_failed":
            ret = slf._connect_failed(args.get("log_fmt"), make_exc_obj(args.get("exc")),
                                      args.get("code"), retryable=args.get("retryable"))
        elif func == "_chunk_text":
            ret = slf._chunk_text(args.get("text"))
        elif func == "verify_http_event_request":
            ret = slf.verify_http_event_request(args.get("auth_header"))
        elif func == "_load_cached_bot_id":
            ret = slf._load_cached_bot_id()
        elif func == "_log_background_failure":
            fut = types.SimpleNamespace(result=rec(ctx, "future.result", lambda: {}))
            ret = NS[func](fut)
        elif func == "_loop_accepts_callbacks":
            ret = NS[func](loop_double(args.get("loop"), ctx))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    # sync mutated state attrs back from the self double
    if func == "_note_rate_limit":
        state["_rate_limit_hits"] = slf._rate_limit_hits
    elif func == "stop_typing":
        state["_typing_messages"] = slf._typing_messages
    elif func == "_invalidate_user_creds":
        ident = args.get("identity")
        if ident and ident == "__legacy__":
            state["_user_credentials"] = slf._user_credentials
            state["_user_chat_api"] = slf._user_chat_api
        elif ident:
            state["_user_creds_by_email"] = slf._user_creds_by_email
            state["_user_chat_api_by_email"] = slf._user_chat_api_by_email
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "logger.debug", "logger.warning", "logger.error", "logger.exception",
    "get_scoped_secret", "seed_extra_from_env", "send_error", "is_multiplex_active",
    "os.environ.get", "urlparse", "describe_proxy_error", "env_proxy_for_redaction",
    "_load_google_modules", "_verify_google_id_token",
    "_http_status", "_proxy_error", "_redact_sensitive", "_exc_text",
    "_validate_config", "_env_inbound_settings", "_env_inbound_configured",
    "check_google_chat_requirements", "_extract_message_payload",
    "dedup.is_duplicate", "dedup.discard",
    "thread_count_store.incr", "thread_count_store.decr",
    "_set_fatal_error", "_bot_id_cache_path", "bot_id_cache.read_text",
    "future.result", "loop.is_closed",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "google_chat_adapter_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'gca_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_google_chat_adapter_flow: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= {f"logger.{lv}" for lv in ("debug", "warning", "error", "exception")}
    used |= set(MODULE_ARGMAP)
    used |= {"_extract_message_payload", "bot_id_cache.read_text", "os.environ.get"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_google_chat_adapter_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_google_chat_adapter_flow: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "google_chat_adapter_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_google_chat_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
    # fixture func set must match the ported set
    if fix.exists():
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_funcs.add(json.loads(line)["func"])
        unbound = fix_funcs - set(ALL_PORTED)
        if unbound:
            print(f"FATAL oracle_google_chat_adapter_flow: fixture funcs not ported: {sorted(unbound)}")
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
    print(f"ORACLE google_chat_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "google_chat_adapter_flow.jsonl"
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
        # uniform AttributeError naming a ported method/seam = binding bug
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            if any(f"'{nm}'" in msg for nm in ALL_PORTED + ["_dedup", "_thread_count_store"]):
                bad += 1
                print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
    print(f"SELFTEST google_chat_adapter_flow: {n} cases, {bad} drive/script failures")
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
    # hand-computed expectations
    HAND = [
        ("_thread_body", {"args": {"text": "hi", "thread_id": None}}, {"text": "hi"}),
        ("_thread_body", {"args": {"text": "hi", "thread_id": "t"}},
         {"text": "hi", "thread": {"name": "t"}}),
        ("_create_kwargs", {"args": {"chat_id": "spaces/A", "body": {"text": "x"}}},
         {"parent": "spaces/A", "body": {"text": "x"}}),
        ("_create_kwargs", {"args": {"chat_id": "spaces/A", "body": {"thread": {"name": "t"}}}},
         {"parent": "spaces/A", "body": {"thread": {"name": "t"}},
          "messageReplyOption": "REPLY_MESSAGE_FALLBACK_TO_NEW_THREAD"}),
        ("_mime_for_message_type", {"args": {"mime": "image/png"}}, "photo"),
        ("_mime_for_message_type", {"args": {"mime": "application/pdf"}}, "document"),
        ("_http_status", {"args": {"exc": {"cls": "HttpError", "msg": "x",
                                            "attrs": {"resp": {"status": 429}}}}}, 429),
        ("_http_status", {"args": {"exc": {"cls": "E", "msg": "x", "attrs": {}}}}, None),
        ("_is_retryable_error", {"args": {"exc": {"cls": "E", "msg": "x", "attrs": {}}},
                                 "io": {"_http_status": 503}}, True),
        ("_is_retryable_error", {"args": {"exc": {"cls": "E", "msg": "Connection reset", "attrs": {}}},
                                 "io": {"_http_status": None}}, True),
        ("_is_retryable_error", {"args": {"exc": {"cls": "E", "msg": "bad request", "attrs": {}}},
                                 "io": {"_http_status": 400}}, False),
        ("_redact_sensitive", {"args": {"text": "projects/p1/subscriptions/s1"}},
         "projects/<redacted>/subscriptions/<redacted>"),
        ("_redact_sensitive", {"args": {"text": "a.b@proj.iam.gserviceaccount.com"}},
         "<sa>@<project>.iam.gserviceaccount.com"),
        ("_redact_sensitive", {"args": {"text": ""}}, ""),
        ("_is_google_owned_host", {"args": {"url": "u"},
                                   "io": {"urlparse": {"__obj__": {"scheme": "https", "hostname": "chat.googleapis.com"}}}}, True),
        ("_is_google_owned_host", {"args": {"url": "u"},
                                   "io": {"urlparse": {"__obj__": {"scheme": "http", "hostname": "chat.googleapis.com"}}}}, False),
        ("_is_google_owned_host", {"args": {"url": "u"},
                                   "io": {"urlparse": {"__obj__": {"scheme": "https", "hostname": "evil.com"}}}}, False),
        ("_validate_config", {"args": {"config": {"__obj__": {"extra": {"http_events_url": "u"}}}}}, True),
        ("_validate_config", {"args": {"config": {"__obj__": {"extra": {}}}}}, False),
        ("_env_inbound_settings", {"io": {"get_scoped_secret": ["p", "s", "u"]}}, ["p", "s", "u"]),
        ("_env_inbound_configured", {"io": {"_env_inbound_settings": [["p", "s", "u"]]}}, True),
        ("_env_inbound_configured", {"io": {"_env_inbound_settings": [[None, None, None]]}}, False),
        ("_standalone_error", {"args": {"detail": "boom"}, "io": {"send_error": {"success": False}}},
         {"success": False}),
        ("_int_setting", {"args": {"extra": {"k": "15"}, "key": "k", "env_name": "E", "default": 1}}, 15),
        ("_int_setting", {"args": {"extra": {}, "key": "k", "env_name": "E", "default": 1},
                          "io": {"get_scoped_secret": "abc"}}, 1),
        ("_str_setting", {"args": {"extra": {"k": "  v  "}, "key": "k", "env_name": "E"}}, "v"),
        ("_resolve_thread_id", {"args": {"reply_to": None, "metadata": {"thread_id": "t"}, "chat_id": "c"},
                                "state": {"_last_inbound_thread": {}}}, "t"),
        ("_note_rate_limit", {"args": {"chat_id": "c"}, "state": {"_rate_limit_hits": {"c": 2}}}, 3),
        ("_chunk_text", {"args": {"text": "short"}}, ["short"]),
        ("_loop_accepts_callbacks", {"args": {"loop": None}}, False),
        ("_loop_accepts_callbacks", {"args": {"loop": {"__obj__": {}}}}, True),
        ("_loop_accepts_callbacks", {"args": {"loop": {"__obj__": {"is_closed": "seam"}}},
                                     "io": {"loop.is_closed": True}}, False),
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