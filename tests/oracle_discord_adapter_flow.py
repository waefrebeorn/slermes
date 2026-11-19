#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW port
(src/gateway/platforms/discord_adapter_flow.c).

Drives the LIVE plugins/platforms/discord/adapter.py leaves and flow methods
with SCRIPTED seams: every sibling method, module helper, injected callback,
logger call, os.getenv / time.time read and foreign-module import is a
recording stub fed the same scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the state echo ("state" — no
ported function mutates adapter state).

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; logger re-set to the recorder
AFTER seeding). Every exec namespace is a registered sys.modules module
(py3.14). The heavy quarry import chain is never imported; discord /
agent.secret_scope / hermes_cli.lifecycle are stub modules installed per case.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE discord_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds errno when set, message "[Errno N] msg"). BaseException kinds
  (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) pass through
  every except-Exception site.
- Exception doubles (args.exc / args.error): {"cls": name, "msg": str(exc),
  "discord_base": null|<discord class name>, "attrs": {...}}. cls names a
  builtin (or synthetic Exception subclass); discord_base builds a synthetic
  subclass of the stub discord class for the isinstance paths.
- Attribute doubles (message/channel/thread/author/guild): JSON objects ->
  SimpleNamespace (always truthy, even empty); scalars keep value truthiness.
  config.extra stays a raw dict (isinstance dict gate).
- cast params travel as "int"/"float"; _discord_snowflake_time projects its
  datetime return to .isoformat().
- Seam args keep explicit nulls; logger exc_info kwargs drop on both sides.
- The seamed _source_for_platform_event stub carries the REAL signature, so
  CPython itself produces the binding TypeErrors; its recorded args always
  carry all six kwargs (message_id defaults to null), matching the C port.
- state flags: discord_available, multiplex_import_ok, lifecycle_import_ok
  (absent import flags default true); state.name is the adapter log name.
  _platform_event_handler: null -> absent, "seam" -> async recording stub,
  any other RAW value -> non-callable attribute (TypeError at the call).
- Fixture corpus: str() positions carry strings/integral numbers/bools/None;
  float()/int() string parsing folds ASCII digits + Unicode White_Space
  strip; lower()/isdigit() fold ASCII; thread_id seam results are str/null;
  shown is str/null; app_id is str/integral/bool/null.
"""
import ast
import asyncio
import copy
import datetime as dt
import json
import math
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path
from typing import Any, Optional
from urllib.parse import quote

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")

_FLAGS = _fut.annotations.compiler_flag
MODNAME = "discord_adapter_flow_oracle"


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


MODULE_FUNCS = [
    "_image_ext_from_content_type", "_format_discord_markdown_link",
    "_needs_server_members_intent", "_format_privileged_intents_guidance",
    "_discord_snowflake_time", "_metadata_marks_nonconversational",
    "_prompt_target_id", "_clean_discord_id",
    "_truncate_discord_component_text", "_discord_ready_timeout_seconds",
    "_multiplex_active",
    "discord_deps_present", "_is_discord_transport_error",
]
METHOD_FUNCS = [
    "_config_value", "_warn_liveness_config_disabled", "_liveness_knob",
    "_finite_positive_config_float", "_config_int",
    "_classify_connect_exception", "_thread_id_and_chat_for_channel",
    "_source_for_platform_event", "_platform_events_subscribed",
    "_fire_platform_event", "_emit_platform_event",
    "_message_event_parts", "_thread_event_parts",
    "_command_sync_state_key", "_command_sync_mutation_interval_seconds",
    "_command_sync_skip_reason", "_extract_discord_retry_after",
    "_is_discord_rate_limit", "_is_discord_unknown_interaction",
    "_text_batch_flush_deadline_seconds",
]
STATIC_METHODS = {
    "_platform_events_subscribed", "_thread_event_parts",
    "_extract_discord_retry_after", "_is_discord_rate_limit",
    "_is_discord_unknown_interaction",
}
# Seamed-only siblings (not ported): called by ported flows, must be bound
# on the self double as recorders or every such flow dies AttributeError.
SEAMED_ONLY_METHODS = ["_read_command_sync_state"]


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "asyncio": asyncio, "dt": dt, "math": math, "os": os, "re": re,
        "json": json, "quote": quote, "Any": Any, "Optional": Optional,
        "logging": __import__("logging"),
    })
    src = open(A_SRC, encoding="utf-8").read()
    seed_constants(src, ns, A_SRC)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, A_SRC)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "DiscordAdapter")
    for sub in cls.body:
        if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHOD_FUNCS:
            sub = copy.copy(sub)
            sub.decorator_list = []
            exec_node(src, sub, ns, A_SRC)
    # the logger Assign ran during seeding; re-set the recorder AFTER
    ns["logger"] = _SilentLogger()
    required = MODULE_FUNCS + METHOD_FUNCS + [
        "_DISCORD_MARKDOWN_LINK_LABEL_RE", "_DISCORD_URL_LABEL_SCHEME_RE",
        "_DISCORD_NONCONVERSATIONAL_METADATA_KEYS",
        "_DISCORD_COMMAND_SYNC_MUTATION_INTERVAL_SECONDS",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow: missing: {missing}")
        sys.exit(2)
    return ns


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


NS = extract()
REAL = {nm: NS[nm] for nm in MODULE_FUNCS + METHOD_FUNCS}


# ── discord stub module (isinstance doubles) ──
DISCORD_EXC_NAMES = [
    "ConnectionClosed", "GatewayNotFound", "DiscordServerError",
    "LoginFailure", "PrivilegedIntentsRequired", "RateLimited", "HTTPException",
]
DISCORD_STUB = types.ModuleType("discord")
for _n in DISCORD_EXC_NAMES:
    setattr(DISCORD_STUB, _n, type(_n, (Exception,), {}))
DISCORD_STUB.Thread = type("Thread", (), {})


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items() if k != "__spec__"})
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
    """JSON object -> SimpleNamespace attribute carrier (always truthy);
    nested objects recurse; scalars/null stay raw. __spec__ carries the
    original fixture spec for seam-arg projection."""
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            setattr(d, k, attr_double(v))
        d.__spec__ = spec
        return d
    return spec


def proj(x):
    """attribute double -> its fixture spec; anything else verbatim."""
    if hasattr(x, "__spec__"):
        return x.__spec__
    return x


def channel_double(spec, avail):
    """Channel carrier for _thread_id_and_chat_for_channel: when the discord
    stub is live and the spec carries is_thread, the double must be a real
    instance of (a subclass of) stub Thread so the live isinstance agrees
    with the C flag projection."""
    d = attr_double(spec)
    if avail and isinstance(spec, dict) and spec.get("is_thread"):
        cls = type("ChannelCase", (DISCORD_STUB.Thread,), {})
        inst = cls()
        for k, v in vars(d).items():
            setattr(inst, k, v)
        return inst
    return d


def make_exc_obj(spec):
    spec = spec or {}
    cls_name = spec.get("cls", "Exception")
    dbase = spec.get("discord_base")
    msg = spec.get("msg", "")
    if dbase:
        base = getattr(DISCORD_STUB, dbase)
        cls = type(cls_name, (base,), {})
    else:
        cls = EXC_CLASSES.get(cls_name)
        if cls is None:
            cls = type(cls_name, (Exception,), {})
    e = cls(msg)
    for k, v in (spec.get("attrs") or {}).items():
        if k == "response" and isinstance(v, dict):
            r = types.SimpleNamespace()
            for rk, rv in v.items():
                setattr(r, rk, rv)  # headers stays a raw dict
            setattr(e, k, r)
        else:
            setattr(e, k, v)
    return e


# ── per-case script backend (mirrors the C da_call exactly) ──
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
    """self-method recorder: strips the leading self."""
    def f(self, *a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def async_method_rec(ctx, name, args_fn=None):
    async def f(self, *a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        return ctx.call(name, args)
    return f


class RecLogger:
    """logger.<level>(fmt, *args, **kw) -> scripted seam event; exc_info
    kwargs are dropped (C never passes them)."""
    def __init__(self, ctx):
        self._ctx = ctx

    def __getattr__(self, level):
        def log(fmt, *a, **k):
            return self._ctx.call(f"logger.{level}", {"fmt": fmt, "args": list(a)})
        return log
# ── per-case namespace installation ──
def install_import_stub(pkg_name, sub_name, attr, fn, ok):
    """Register pkg + pkg.sub with attr=fn; when ok is False REMOVE the
    submodule so the live `from pkg.sub import attr` raises ImportError."""
    pkg = types.ModuleType(pkg_name)
    pkg.__path__ = []
    sys.modules[pkg_name] = pkg
    full = f"{pkg_name}.{sub_name}"
    if ok:
        sub = types.ModuleType(full)
        setattr(sub, attr, fn)
        sys.modules[full] = sub
        setattr(pkg, sub_name, sub)
    else:
        # None entry HALTS the import (ImportError) — popping is not enough:
        # the quarry venv's editable-install meta-path finder resolves the
        # real submodule regardless of the stub parent's empty __path__.
        sys.modules[full] = None


def source_recorder(ctx):
    """Seamed _source_for_platform_event with the REAL signature: CPython
    itself raises the binding TypeErrors (module-qualified ** message,
    missing/unexpected keyword messages). Recorded args carry all six
    kwargs, message_id defaulting to None — matching the C port."""
    def __rec__(**kw):
        return ctx.call("_source_for_platform_event", kw)
    stub_ns = {"__name__": MODNAME, "__rec__": __rec__}
    exec(
        "def _source_for_platform_event(self, *, chat_id, user_id, user_name, "
        "thread_id, guild_id, message_id=None):\n"
        "    return __rec__(chat_id=chat_id, user_id=user_id, user_name=user_name,\n"
        "                   thread_id=thread_id, guild_id=guild_id, message_id=message_id)\n",
        stub_ns,
    )
    return stub_ns["_source_for_platform_event"]


METHOD_ARGMAP = {
    "_config_value": lambda key, default, env_key=None: {
        "key": key, "default": default, "env_key": env_key},
    "_warn_liveness_config_disabled": lambda key, raw: {"key": key, "raw": raw},
    "_liveness_knob": lambda key, default, cast, env_key=None: {
        "key": key, "default": default,
        "cast": "int" if cast is int else "float", "env_key": env_key},
    "_read_command_sync_state": lambda: {},
    "_command_sync_state_key": lambda app_id: {"app_id": app_id},
    "_thread_id_and_chat_for_channel": lambda channel: {"channel": proj(channel)},
    "_fire_platform_event": lambda event, source: {"event": event, "source": source},
    "_platform_events_subscribed": lambda: {},
}


def setup_case(func, inp):
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    # restore every target def to REAL before installing this case's stubs
    for nm, fn in REAL.items():
        NS[nm] = fn
    avail = bool(state.get("discord_available", False))
    NS["DISCORD_AVAILABLE"] = avail
    NS["discord"] = DISCORD_STUB if avail else None
    sys.modules["discord"] = DISCORD_STUB if avail else None  # None blocks import
    NS["logger"] = RecLogger(ctx)
    NS["os"] = types.SimpleNamespace(
        getenv=rec(ctx, "os.getenv", lambda key, default=None: {"key": key, "default": default}))
    NS["time"] = types.SimpleNamespace(time=rec(ctx, "time.time", lambda: {}))
    NS["_scoped_gate_env"] = rec(ctx, "_scoped_gate_env", lambda key: {"key": key})
    NS["is_automated_message"] = rec(
        ctx, "is_automated_message", lambda message: {"message": proj(message)})
    NS["parse_retry_after_seconds"] = rec(
        ctx, "parse_retry_after_seconds", lambda value: {"value": value})
    NS["_prefix_within_utf16_limit"] = rec(
        ctx, "_prefix_within_utf16_limit", lambda text, limit: {"text": text, "limit": limit})
    if func != "_needs_server_members_intent":
        NS["_needs_server_members_intent"] = rec(
            ctx, "_needs_server_members_intent",
            lambda u, r: {"allowed_user_ids": u, "allowed_role_ids": r})
    if func != "_format_privileged_intents_guidance":
        NS["_format_privileged_intents_guidance"] = rec(
            ctx, "_format_privileged_intents_guidance",
            lambda *, needs_members: {"needs_members": needs_members})
    install_import_stub(
        "agent", "secret_scope", "is_multiplex_active",
        rec(ctx, "is_multiplex_active", lambda: {}),
        bool(state.get("multiplex_import_ok", True)))
    install_import_stub(
        "hermes_cli", "lifecycle", "has_hook",
        rec(ctx, "has_hook", lambda name: {"name": name}),
        bool(state.get("lifecycle_import_ok", True)))
    # adapter self double
    slf = types.SimpleNamespace()
    slf.name = state.get("name", "Discord")
    cfg = state.get("config") or {}
    slf.config = types.SimpleNamespace(extra=cfg.get("extra"))
    slf._allowed_user_ids = state.get("_allowed_user_ids")
    slf._allowed_role_ids = state.get("_allowed_role_ids")
    h = state.get("_platform_event_handler")
    if h == "seam":
        async def handler(event, source):
            return ctx.call("platform_event_handler", {"event": event, "source": source})
        slf._platform_event_handler = handler
    else:
        slf._platform_event_handler = h  # None -> absent; raw value -> non-callable
    slf.build_source = rec(ctx, "build_source", lambda **kw: kw)
    for nm in METHOD_FUNCS:
        if nm == func:
            fn = REAL[nm]
        elif nm == "_source_for_platform_event":
            fn = source_recorder(ctx)
        elif nm == "_fire_platform_event":
            fn = async_method_rec(ctx, nm, METHOD_ARGMAP[nm])
        elif nm in STATIC_METHODS:
            fn = rec(ctx, nm, METHOD_ARGMAP.get(nm))
        else:
            fn = method_rec(ctx, nm, METHOD_ARGMAP.get(nm))
        if nm in STATIC_METHODS:
            setattr(slf, nm, fn)  # plain: staticmethods take no self
        else:
            setattr(slf, nm, types.MethodType(fn, slf))
    for nm in SEAMED_ONLY_METHODS:
        setattr(slf, nm, types.MethodType(method_rec(ctx, nm, METHOD_ARGMAP.get(nm)), slf))
    return ctx, slf


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx, slf = setup_case(func, inp)
    try:
        if func == "_image_ext_from_content_type":
            ret = NS[func](args["content_type"])
        elif func == "_format_discord_markdown_link":
            ret = NS[func](args["label"], args["url"])
        elif func == "_needs_server_members_intent":
            ret = NS[func](args.get("allowed_user_ids"), args.get("allowed_role_ids"))
        elif func == "_format_privileged_intents_guidance":
            ret = NS[func](needs_members=args.get("needs_members"))
        elif func == "_discord_snowflake_time":
            ret = NS[func](args["snowflake"]).isoformat()
        elif func == "_metadata_marks_nonconversational":
            ret = NS[func](args.get("metadata"))
        elif func == "_prompt_target_id":
            ret = NS[func](args["chat_id"], args.get("metadata"))
        elif func == "_clean_discord_id":
            ret = NS[func](args["entry"])
        elif func == "_truncate_discord_component_text":
            ret = NS[func](args["text"], args["limit"])
        elif func in ("_discord_ready_timeout_seconds",
                      "_multiplex_active", "discord_deps_present"):
            ret = NS[func]()
        elif func == "_text_batch_flush_deadline_seconds":
            ret = getattr(slf, func)()
        elif func == "_is_discord_transport_error":
            ret = NS[func](make_exc_obj(args.get("exc")))
        elif func == "_config_value":
            ret = slf._config_value(args["key"], args["default"], env_key=args.get("env_key"))
        elif func == "_warn_liveness_config_disabled":
            ret = slf._warn_liveness_config_disabled(args["key"], args["raw"])
        elif func == "_liveness_knob":
            cast = int if args["cast"] == "int" else float
            ret = slf._liveness_knob(args["key"], args["default"], cast,
                                     env_key=args.get("env_key"))
        elif func in ("_finite_positive_config_float", "_config_int"):
            ret = getattr(slf, func)(args["key"], args["default"], env_key=args.get("env_key"))
        elif func == "_classify_connect_exception":
            ret = slf._classify_connect_exception(make_exc_obj(args.get("error")),
                                                  shown=args.get("shown"))
        elif func == "_thread_id_and_chat_for_channel":
            avail = bool((inp.get("state") or {}).get("discord_available", False))
            ret = slf._thread_id_and_chat_for_channel(channel_double(args.get("channel"), avail))
        elif func == "_source_for_platform_event":
            ret = slf._source_for_platform_event(**args)
        elif func == "_platform_events_subscribed":
            ret = NS[func]()
        elif func == "_fire_platform_event":
            ret = asyncio.run(slf._fire_platform_event(args.get("event"), args.get("source")))
        elif func == "_emit_platform_event":
            build = rec(ctx, "build", lambda: {})
            ret = asyncio.run(slf._emit_platform_event(args["event_type"], build))
        elif func == "_message_event_parts":
            extra = rec(ctx, "extra_payload",
                        lambda message, author: {"message": proj(message), "author": proj(author)})
            ret = slf._message_event_parts(attr_double(args.get("message")), extra)
        elif func == "_thread_event_parts":
            extra = rec(ctx, "extra_payload",
                        lambda thread, owner_id: {"thread": proj(thread), "owner_id": owner_id})
            ret = NS[func](attr_double(args.get("thread")), extra)
        elif func == "_command_sync_state_key":
            ret = slf._command_sync_state_key(args.get("app_id"))
        elif func == "_command_sync_mutation_interval_seconds":
            ret = slf._command_sync_mutation_interval_seconds()
        elif func == "_command_sync_skip_reason":
            ret = slf._command_sync_skip_reason(args.get("app_id"), args.get("fingerprint"))
        elif func == "_extract_discord_retry_after":
            ret = NS[func](make_exc_obj(args.get("exc")))
        elif func in ("_is_discord_rate_limit", "_is_discord_unknown_interaction"):
            ret = NS[func](make_exc_obj(args.get("exc")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "os.getenv", "logger.warning", "logger.debug", "time.time",
    "is_multiplex_active", "has_hook", "_prefix_within_utf16_limit",
    "_scoped_gate_env", "parse_retry_after_seconds", "is_automated_message",
    "_config_value", "_liveness_knob", "_warn_liveness_config_disabled",
    "_read_command_sync_state", "_command_sync_state_key",
    "_platform_events_subscribed", "_source_for_platform_event",
    "_fire_platform_event", "_thread_id_and_chat_for_channel",
    "_needs_server_members_intent", "_format_privileged_intents_guidance",
    "build_source", "build", "extra_payload", "platform_event_handler",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'da_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = registry - c_names
    if missing_c:
        print(f"FATAL oracle_discord_adapter_flow: seams absent from C port: {sorted(missing_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'(?:rec|method_rec|async_method_rec)\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= {"logger.warning", "logger.debug"}  # RecLogger dynamic levels
    # recorders installed via the generic method loop (variable seam names)
    used |= set(METHOD_ARGMAP)
    used |= {"_source_for_platform_event", "_fire_platform_event",
             "build_source", "platform_event_handler"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_discord_adapter_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    unstubbed = c_names - used
    if unstubbed:
        print(f"FATAL oracle_discord_adapter_flow: C seams without oracle stub: {sorted(unstubbed)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE discord_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow.jsonl"
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
    print(f"SELFTEST discord_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = [f for f in MODULE_FUNCS + METHOD_FUNCS if f not in counts]
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {missing}")
        bad += 1
    # Hand-computed expectations: catch uniform wrong-result families that a
    # drive-exception/script-gap check alone cannot see.
    HAND = [
        ("_image_ext_from_content_type", {"args": {"content_type": "image/jpeg"}}, "jpg"),
        ("_discord_snowflake_time", {"args": {"snowflake": 0}}, "2015-01-01T00:00:00+00:00"),
        ("_clean_discord_id", {"args": {"entry": "<@!123>"}}, "123"),
        ("_command_sync_state_key", {"args": {"app_id": 0}}, "unknown"),
        ("_config_value",
         {"args": {"key": "k", "default": "d", "env_key": None},
          "state": {"config": {"extra": {"k": "v"}}}}, "v"),
        ("_liveness_knob",
         {"args": {"key": "k", "default": 1, "cast": "int", "env_key": None},
          "io": {"_config_value": "15"}}, 15),
        ("_liveness_knob",
         {"args": {"key": "k", "default": 1, "cast": "int", "env_key": None},
          "io": {"_config_value": "15.5", "_warn_liveness_config_disabled": None}}, 0),
        ("_command_sync_skip_reason",
         {"args": {"app_id": 123, "fingerprint": "fp"},
          "io": {"_read_command_sync_state": {"app1": {"retry_after_until": 1010}},
                 "_command_sync_state_key": "app1", "time.time": 1000}},
         "Discord asked us to wait before syncing slash commands; retry in 10s"),
        ("_classify_connect_exception",
         {"args": {"error": {"cls": "LoginFailure", "msg": "bad", "attrs": {}}, "shown": None}},
         ["discord_auth_error",
          "Discord bot token rejected: bad. The token is invalid or was revoked "
          "\u2014 regenerate it in the Discord Developer Portal and update DISCORD_BOT_TOKEN.",
          False]),
        ("_source_for_platform_event",
         {"args": {"chat_id": "c"}, "io": {"build_source": None}},
         {"__raise__": "TypeError",
          "message": "_source_for_platform_event() missing 4 required keyword-only "
                     "arguments: 'user_id', 'user_name', 'thread_id', and 'guild_id'"}),
        ("_needs_server_members_intent",
         {"args": {"allowed_user_ids": ["alice"], "allowed_role_ids": None}}, True),
        ("_text_batch_flush_deadline_seconds", {"io": {"os.getenv": "10"}}, 8.0),
    ]
    for func, inp, want in HAND:
        out = drive(func, inp)
        if out["ret"] != canon(want):
            bad += 1
            print(f"SELFTEST HAND CHECK FAILED {func}: want={want!r} got={out['ret']!r}")
    # one happy-path _emit trace shape check (4 ordered seam events)
    out = drive("_emit_platform_event", {
        "args": {"event_type": "message_edited"},
        "io": {"_platform_events_subscribed": True,
               "build": [[{"chat_id": "c"}, {"chat_id": "c", "user_id": "u",
                                             "user_name": "n", "thread_id": None,
                                             "guild_id": "g"}]],
               "_source_for_platform_event": {"s": 1}, "_fire_platform_event": None}})
    names = [e["name"] for e in out["trace"]]
    if names != ["_platform_events_subscribed", "build",
                 "_source_for_platform_event", "_fire_platform_event"]:
        bad += 1
        print(f"SELFTEST HAND CHECK FAILED _emit trace: {names}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
