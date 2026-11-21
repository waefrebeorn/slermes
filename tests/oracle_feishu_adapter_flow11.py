#!/usr/bin/env python3
"""Oracle for the Feishu adapter remainders FLOW port (C sibling:
src/gateway/platforms/feishu_adapter_flow11.c).

Drives the LIVE plugins/platforms/feishu/adapter.py defs with SCRIPTED seams:
every collaborator is a recording stub fed the same scripts the C harness feeds
its universal seam fa11_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py; the heavy quarry import chain
is never imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case
wrapper catches BaseException (SystemExit included); scripted raises round-trip
their spec. Summary: 'ORACLE feishu_adapter_flow11: N cases, M mismatches'.
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
from pathlib import PurePosixPath, PurePath
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = __import__("pathlib").Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_hydrate_bot_identity", "_install_lark_ws_isolation", "_load_lark_oapi",
    "_load_seen_message_ids", "_load_settings", "_maybe_extract_text_document",
    "_on_drive_comment_event", "_on_meeting_invited_event", "_on_message_event",
    "_persist_seen_message_ids",
    "_id_set", "_secret", "_extra_or_secret", "_dispatch_connect",
]

SETTINGS_KEYS = [
    "app_id", "app_secret", "domain_name", "connection_mode", "encrypt_key",
    "verification_token", "group_policy", "allowed_group_users", "bot_open_id",
    "bot_user_id", "bot_name", "dedup_cache_size", "text_batch_delay_seconds",
    "text_batch_split_delay_seconds", "text_batch_max_messages", "text_batch_max_chars",
    "media_batch_delay_seconds", "webhook_host", "webhook_port", "webhook_path",
    "ws_reconnect_nonce", "ws_reconnect_interval", "ws_ping_interval",
    "ws_ping_timeout", "admins", "default_group_policy", "group_rules",
    "allow_bots", "require_mention", "allow_all_dm",
]

# import-module names for _load_lark_oapi (must match the C MODS table)
LARK_MODS = [
    "lark_oapi", "lark_oapi.api.application.v6", "lark_oapi.api.im.v1",
    "lark_oapi.core", "lark_oapi.core.const", "lark_oapi.core.model",
    "lark_oapi.event.callback.model.p2_card_action_trigger",
    "lark_oapi.event.dispatcher_handler", "lark_oapi.ws",
]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa11_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Optional, Sequence
    ns.update({
        "Path": PurePosixPath,
        "PurePath": PurePath,
        "json": json,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List,
        "Optional": Optional, "Sequence": Sequence,
        "FeishuAdapter": None,
        "FEISHU_AVAILABLE": False,
        "_FEISHU_DEDUP_TTL_SECONDS": 86400,
        "_MAX_TEXT_INJECT_BYTES": 100 * 1024,
        "_DEFAULT_DEDUP_CACHE_SIZE": 2048,
        "_DEFAULT_TEXT_BATCH_DELAY_SECONDS": 0.6,
        "_DEFAULT_TEXT_BATCH_MAX_MESSAGES": 8,
        "_DEFAULT_TEXT_BATCH_MAX_CHARS": 4000,
        "_DEFAULT_MEDIA_BATCH_DELAY_SECONDS": 0.8,
        "_DEFAULT_WEBHOOK_HOST": "127.0.0.1",
        "_DEFAULT_WEBHOOK_PORT": 8765,
        "_DEFAULT_WEBHOOK_PATH": "/feishu/webhook",
        "_ONBOARD_OPEN_URLS": {"feishu": "https://open.feishu.cn", "lark": "https://open.larksuite.com"},
        "_WS_ISOLATION_INSTALLED": False,
        "_WEBHOOK_EVENT_HANDLERS": {},
        "time": types.SimpleNamespace(time=lambda: 0.0),
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_LARK_SDK_IMPORTS" for t in node.targets):
            ns["_LARK_SDK_IMPORTS"] = ast.literal_eval(node.value)
    done = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC); done.add(node.name)
        elif isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch); c.decorator_list = []
                    exec_node(src, c, ns, SRC); done.add(ch.name)
    # nested closures (portable FLOW targets)
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS and node.name not in done:
            c = copy.copy(node); c.decorator_list = []
            exec_node(src, c, ns, SRC); done.add(node.name)
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow11: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow11.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa11_call\(\s*ctx,\s*"([^"]+)"', _CSRC))

# ── canonical projection ──
class _Token:
    def __init__(self, value): object.__setattr__(self, "v", value)
    def __repr__(self): return repr(self.v)


def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, PurePath):
            return str(x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception hierarchy ──
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "FileNotFoundError": FileNotFoundError, "IsADirectoryError": IsADirectoryError,
    "PermissionError": PermissionError, "TimeoutError": TimeoutError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeError": UnicodeError,
    "JSONDecodeError": json.JSONDecodeError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "Warning": Warning, "ArithmeticError": ArithmeticError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
}
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = (BaseException,) if kind in BASE_EXC else (Exception,)
        cls = type(kind, cls, {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections (MUST match fa11_call args in the C port) ──
ARGMAP = {
    "tenant_get_bot_info": lambda path: {"path": path},
    "parse_bot_response": lambda data: {"data": canon(data)},
    "build_app_request": lambda app_id, lang: {"app_id": app_id, "lang": lang},
    "run_blocking_app": lambda func, request: {},
    "response_success": lambda: {},
    "wrap_loop": lambda loop: {"loop": canon(loop)},
    "install_connect": lambda connect_name: {"connect_name": connect_name},
    "install_receive": lambda: {},
    "import_module": lambda name: {"name": name},
    "read_text": lambda path: {"path": path},
    "scoped_secret": lambda name, default="": {"name": name, "default": default},
    "extra_or_secret": lambda extra, key, env, default="": {"key": key, "env": env, "default": default},
    "env_int": lambda name, default: {"name": name, "default": default},
    "env_float": lambda name, default: {"name": name, "default": default},
    "coerce_required_int": lambda value, default, min_value: {
        "value": canon(value), "default": default, "min_value": min_value},
    "coerce_int": lambda value, default, min_value: {
        "value": canon(value), "default": default, "min_value": min_value},
    "to_boolean": lambda value: {"value": canon(value)},
    "group_rule": lambda policy, allowlist, blacklist, require_mention: {
        "policy": policy, "allowlist": canon(allowlist), "blacklist": canon(blacklist),
        "require_mention": canon(require_mention)},
    "settings": lambda **kw: {k: canon(kw.get(k)) for k in SETTINGS_KEYS},
    "getsize": lambda path: {"path": path},
    "display_name": lambda cached_path: {"cached_path": cached_path},
    "submit_if_ready": lambda label, callback: {"label": label},
    "loop_accepts": lambda loop: {"loop": canon(loop)},
    "enqueue_pending": lambda data: {"data": canon(data)},
    "thread_start": lambda name: {"name": name},
    "handle_event_data": lambda data: {"data": canon(data)},
    "submit_on_loop": lambda loop, coro: {"loop": canon(loop)},
    "mkdir": lambda parent: {"parent": canon(parent)},
    "atomic_write": lambda path, payload, indent=None: {
        "path": canon(getattr(path, "_path", path)), "payload": canon(payload), "indent": canon(indent)},
    "connect_kwargs": lambda: {},
    "real_connect": lambda *a, **k: {"args": canon(list(a)), "kwargs": canon(dict(k))},
}

AWAIT_SEAMS = {"tenant_get_bot_info", "run_blocking_app"}

# ── per-case script backend (mirrors C fa11_call exactly) ──
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


def _install(obj, name, value):
    object.__setattr__(obj, name, value)


class _Self:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)


class _DummyLock:
    def __enter__(self): return self
    def __exit__(self, *a): return False


class _Resp:
    def __init__(self, ctx, raw):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "_raw", mat_event(raw) if raw is not None else None)
    def __bool__(self):
        raw = object.__getattribute__(self, "_raw")
        return False if raw is None else bool(raw)
    def success(self):
        return self._ctx.call("response_success", {})
    def __getattr__(self, name):
        raw = object.__getattribute__(self, "_raw")
        return getattr(raw, name) if raw is not None else None


class _DedupPath:
    def __init__(self, ctx, path):
        object.__setattr__(self, "_ctx", ctx); object.__setattr__(self, "_path", path)
    def read_text(self, encoding=None):
        return self._ctx.call("read_text", {"path": object.__getattribute__(self, "_path")})
    @property
    def parent(self):
        p = object.__getattribute__(self, "_path")
        return PurePosixPath(p).parent


class _FakePath:
    def __init__(self, p): object.__setattr__(self, "_p", p)
    @property
    def suffix(self):
        return PurePosixPath(object.__getattribute__(self, "_p")).suffix
    def read_text(self, encoding=None):
        return _CUR_CTX.call("read_text", {"path": object.__getattribute__(self, "_p")})


class _WsDouble:
    def __init__(self, ctx, connect_name):
        object.__setattr__(self, "_ctx", ctx)
        def real_connect(*a, **k): return None
        real_connect.__name__ = connect_name
        object.__setattr__(self, "connect", real_connect)
    def __setattr__(self, name, value):
        cur = object.__getattribute__(self, "connect")
        self._ctx.call("install_connect", {"connect_name": getattr(cur, "__name__", "connect")})
        object.__setattr__(self, name, value)


class _ClientDouble:
    def __init__(self, ctx):
        object.__setattr__(self, "_ctx", ctx)
        async def orig(self): return None
        object.__setattr__(self, "_receive_message_loop", orig)
    def __setattr__(self, name, value):
        self._ctx.call("install_receive", {})
        object.__setattr__(self, name, value)


class _IsoState:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __getattr__(self, name):
        if name == "connect_kwargs":
            return self._ctx.call("connect_kwargs", {})
        raise AttributeError(name)


class _ThreadStub:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)
    def __call__(self, target=None, args=None, name=None, daemon=False):
        self._name = name
        return self
    def start(self):
        self._ctx.call("thread_start", {"name": getattr(self, "_name", "")})


def mat_event(raw):
    if isinstance(raw, dict):
        return types.SimpleNamespace(**{k: mat_event(v) for k, v in raw.items()})
    if isinstance(raw, list):
        return [mat_event(i) for i in raw]
    return raw


SELF = None
_CUR_CTX = None
_ORIG_IMPORTLIB = sys.modules.get("importlib")


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    if nm in AWAIT_SEAMS:
        async def af(*a, **k):
            return ctx.call(nm, argfn(*a, **k))
        return af
    def sf(*a, **k):
        return ctx.call(nm, argfn(*a, **k))
    return sf


def _stub_pkg(name):
    m = types.ModuleType(name); m.__path__ = []
    sys.modules[name] = m
    return m


def install_recorders(ctx, target):
    global SELF, _CUR_CTX
    _CUR_CTX = ctx
    for nm in SEAMED:
        NS[nm] = make_recorder(ctx, nm)
    NS["FeishuAdapter"] = None
    NS["logger"] = types.SimpleNamespace(
        warning=lambda *a, **k: None, error=lambda *a, **k: None,
        info=lambda *a, **k: None, debug=lambda *a, **k: None,
        exception=lambda *a, **k: None)
    # importlib stub only for the _load_lark_oapi target
    if target == "_load_lark_oapi":
        m = types.ModuleType("importlib")
        def import_rec(name):
            return mat_event(ctx.call("import_module", {"name": name}))
        m.import_module = import_rec
        sys.modules["importlib"] = m
    else:
        if _ORIG_IMPORTLIB is not None:
            sys.modules["importlib"] = _ORIG_IMPORTLIB
    SELF = _Self(ctx)

    if target == "_hydrate_bot_identity":
        NS["_parse_bot_response"] = make_recorder(ctx, "parse_bot_response")
        NS["describe_proxy_error"] = lambda e, p: ""
        NS["env_proxy_for_redaction"] = lambda u: ""
        NS["_onboard_open_base_url"] = lambda d: "https://open.feishu.cn"
        _install(SELF, "_tenant_get_raw", make_recorder(ctx, "tenant_get_bot_info"))
        _install(SELF, "_build_get_application_request", make_recorder(ctx, "build_app_request"))
        async def rb(func, request):
            return _Resp(ctx, ctx.call("run_blocking_app", {}))
        _install(SELF, "_run_blocking", rb)
    elif target == "_install_lark_ws_isolation":
        NS["_WS_ISOLATION_LOCK"] = _DummyLock()
        NS["_ThreadLocalLoopProxy"] = make_recorder(ctx, "wrap_loop")
    elif target == "_load_lark_oapi":
        NS["_lark_import_lock"] = _DummyLock()
    elif target == "_load_seen_message_ids":
        pass
    elif target == "_load_settings":
        NS["_get_scoped_secret"] = make_recorder(ctx, "scoped_secret")
        NS["_shared_extra_or_secret"] = make_recorder(ctx, "extra_or_secret")
        NS["env_int"] = make_recorder(ctx, "env_int")
        NS["env_float"] = make_recorder(ctx, "env_float")
        NS["_coerce_required_int"] = make_recorder(ctx, "coerce_required_int")
        NS["_coerce_int"] = make_recorder(ctx, "coerce_int")
        NS["_to_boolean"] = make_recorder(ctx, "to_boolean")
        NS["FeishuGroupRule"] = make_recorder(ctx, "group_rule")
        NS["FeishuAdapterSettings"] = make_recorder(ctx, "settings")
    elif target == "_maybe_extract_text_document":
        NS["os"] = types.SimpleNamespace(path=types.SimpleNamespace(getsize=make_recorder(ctx, "getsize")))
        NS["Path"] = _FakePath
        _install(SELF, "_display_name_from_cached_path", make_recorder(ctx, "display_name"))
    elif target in ("_on_drive_comment_event", "_on_meeting_invited_event"):
        for pkg in ("plugins", "plugins.platforms", "plugins.platforms.feishu"):
            if pkg not in sys.modules:
                _stub_pkg(pkg)
        fc = sys.modules["plugins.platforms.feishu.feishu_comment"] = types.ModuleType(
            "plugins.platforms.feishu.feishu_comment")
        fc.handle_drive_comment_event = lambda *a, **k: None
        mi = sys.modules["plugins.platforms.feishu.feishu_meeting_invite"] = types.ModuleType(
            "plugins.platforms.feishu.feishu_meeting_invite")
        mi.handle_meeting_invited_event = lambda *a, **k: None
        _install(SELF, "_submit_if_ready", make_recorder(ctx, "submit_if_ready"))
    elif target == "_on_message_event":
        _install(SELF, "_loop_accepts_callbacks", make_recorder(ctx, "loop_accepts"))
        _install(SELF, "_enqueue_pending_inbound_event", make_recorder(ctx, "enqueue_pending"))
        _install(SELF, "_drain_pending_inbound_events", lambda: None)
        _install(SELF, "_submit_on_loop", make_recorder(ctx, "submit_on_loop"))
        _install(SELF, "_handle_message_event_data", make_recorder(ctx, "handle_event_data"))
        NS["threading"] = types.SimpleNamespace(Thread=_ThreadStub(ctx))
        NS["contextvars"] = types.SimpleNamespace(copy_context=lambda: types.SimpleNamespace(run=lambda fn: None))
    elif target == "_persist_seen_message_ids":
        hc = sys.modules["hermes_constants"] = types.ModuleType("hermes_constants")
        hc.mkdir_under_hermes_home = make_recorder(ctx, "mkdir")
        NS["atomic_json_write"] = make_recorder(ctx, "atomic_write")
        SELF._dedup_lock = _DummyLock()
    elif target == "_secret":
        NS["_get_scoped_secret"] = make_recorder(ctx, "scoped_secret")
    elif target == "_extra_or_secret":
        NS["_shared_extra_or_secret"] = make_recorder(ctx, "extra_or_secret")
    elif target == "_dispatch_connect":
        NS["_ws_isolation_state"] = _IsoState(ctx)
        NS["real_connect"] = make_recorder(ctx, "real_connect")


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def _run(x):
    if inspect_awaitable(x):
        return asyncio.run(x)
    return x


def inspect_awaitable(x):
    import inspect
    return inspect.isawaitable(x)


def _app_client_double():
    return types.SimpleNamespace(application=types.SimpleNamespace(
        v6=types.SimpleNamespace(application=types.SimpleNamespace(get=lambda: None))))


def live_call(func, args, ctx):
    g = args.get
    if func == "_hydrate_bot_identity":
        _install(SELF, "_client", _app_client_double() if g("client", False) else None)
        _install(SELF, "_bot_open_id", g("bot_open_id", ""))
        _install(SELF, "_bot_name", g("bot_name", ""))
        _install(SELF, "_app_id", g("app_id", ""))
        _install(SELF, "_domain_name", g("domain_name", "feishu"))
        return NS[func](SELF)
    if func == "_install_lark_ws_isolation":
        ws = types.SimpleNamespace(loop=mat_event(g("loop")))
        ws.websockets = _WsDouble(ctx, g("connect_name", "connect"))
        ws.Client = _ClientDouble(ctx)
        NS["_WS_ISOLATION_INSTALLED"] = bool(g("installed", False))
        return NS[func](ws)
    if func == "_load_lark_oapi":
        NS["FEISHU_AVAILABLE"] = bool(g("available", False))
        return NS[func]()
    if func == "_load_seen_message_ids":
        _install(SELF, "_dedup_state_path", _DedupPath(ctx, g("path", "")))
        _install(SELF, "_dedup_cache_size", g("cache_size", 2048))
        return NS[func](SELF)
    if func == "_load_settings":
        NS["extra"] = args
        return NS[func](args)
    if func == "_maybe_extract_text_document":
        return NS[func](SELF, g("cached_path", ""), g("media_type", ""))
    if func in ("_on_drive_comment_event", "_on_meeting_invited_event"):
        _install(SELF, "_client", "C")
        _install(SELF, "_bot_open_id", "ou_bot")
        _install(SELF, "_owner_profile", g("owner", None))
        return NS[func](SELF, mat_event(g("data")))
    if func == "_on_message_event":
        _install(SELF, "_loop", mat_event(g("loop")))
        return NS[func](SELF, mat_event(g("data")))
    if func == "_persist_seen_message_ids":
        _install(SELF, "_dedup_state_path", _DedupPath(ctx, g("path", "")))
        _install(SELF, "_seen_message_order", list(g("seen_order") or []))
        _install(SELF, "_seen_message_ids", dict(g("seen_ids") or {}))
        _install(SELF, "_dedup_cache_size", g("cache_size", 2))
        return NS[func](SELF)
    if func == "_id_set":
        return NS[func](list(g("values") or []))
    if func == "_secret":
        return NS[func](g("name", ""), g("default", ""))
    if func == "_extra_or_secret":
        NS["extra"] = g("extra") or {}
        return NS[func](g("key", ""), g("env", ""), g("default", ""))
    if func == "_dispatch_connect":
        return NS[func](*(g("pos_args") or []), **dict(g("kwargs") or {}))
    raise RuntimeError(f"unknown func {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    try:
        ret = _run(live_call(func, args, ctx))
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── cross-checks ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow11: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow11: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow11.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow11: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow11: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow11: fixture funcs not in TARGETS: {unknown_funcs}")
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
    if shown >= 200:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:600]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:600]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:600]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:600]}")


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
    print(f"ORACLE feishu_adapter_flow11: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow11.jsonl"
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
        if case.get("_note") == "fail-closed":
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in TARGETS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST feishu_adapter_flow11: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
