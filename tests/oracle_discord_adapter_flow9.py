#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW9 remainder port
(src/gateway/platforms/discord_adapter_flow9.c).

Drives the LIVE plugins/platforms/discord/adapter.py defs with SCRIPTED seams:
local imports, claimed siblings, injected callables and attribute doubles are
recording stubs fed the SAME scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and the state echo.

The live defs are AST-extracted (module/class bodies, decorators stripped) into
a registered sys.modules namespace; the heavy quarry import chain is never
imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper
catches BaseException (SystemExit included). Summary:
'ORACLE discord_adapter_flow9: N cases, M mismatches'.

Representation contract (shared with the C port): see the port header. None
== JSON null; a raise is {"__raise__": cls, "message": str(exc)}. Real builtin
exception classes are used so except clause matching is exact. Attribute
doubles are SimpleNamespace (object always truthy); getattr-default reads by
KEY PRESENCE; a truthy slot at a callable key fires the recorded seam.
"""
import ast
import asyncio
import contextlib
import copy
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
A_SRC = os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag
MODNAME = "discord_adapter_flow9_oracle"

FREE_FUNCS = ["_clean_discord_id", "_apply_yaml_config", "_alert_adapters_and_config"]
INSTANCE_FUNCS = [
    "_command_sync_state_path", "_cancel_bot_task", "_cancel_liveness_task",
    "_advance_discord_recovery_cursor", "_apply_owner_only_visibility",
    "_build_slash_event", "_component_live_auth", "_auto_create_thread",
    "_cache_discord_image", "_cache_discord_audio", "_cache_discord_document",
    "_cache_simple_media", "_collect_attachment_media",
]
PORTED = FREE_FUNCS + INSTANCE_FUNCS
# EXACTLY the C da9_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "MessageEvent", "_adapters_for_profile", "_derive_auto_thread_name",
    "_format_thread_chat_name", "_get_effective_topic", "_get_parent_channel_id",
    "_interaction_guild_id", "_is_sender_authorized", "_read_attachment_bytes",
    "_resolve_channel_prompt", "_resolve_channel_skills", "_slash_role_grant",
    "_utc_now_iso", "asyncio.current_task", "bot_task.await", "bot_task.cancel",
    "bot_task.done", "build_source", "cache_bytes", "cache_doc_bytes",
    "cache_document", "cache_media", "cache_simple", "cache_url",
    "channel.send", "component_chat_context", "conn.execute", "create_thread",
    "discord.Permissions", "env.set", "get_hermes_home", "get_profile_dir",
    "is_safe_url", "liveness.await", "liveness.cancel", "liveness.done",
    "load_gateway_config", "logger.debug", "logger.info", "logger.warning",
    "max_doc_bytes", "mkdir", "print", "proxy_kwargs", "re.sub",
    "resolve_proxy_url", "resp.read", "seed.create_thread", "session.get",
    "set.default_permissions", "sleep", "stamp", "t", "tree.get_commands",
]
# Seam names wired through custom classes / dynamic names (not the rec helper).
EXPLICIT_SEAMS = {
    "bot_task.done", "bot_task.cancel", "bot_task.await",
    "liveness.done", "liveness.cancel", "liveness.await",
    "set.default_permissions", "mkdir", "resp.read", "session.get",
}

# Fixed constants shared with the C port.
DOC_TYPES = {".txt": "text/plain", ".pdf": "application/pdf", ".csv": "text/csv",
             ".md": "text/markdown", ".json": "application/json", ".log": "text/plain"}
TEXT_EXTS = {".txt", ".csv", ".md", ".log"}


class _DM:
    pass


class _TH:
    pass


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    node = copy.copy(node)
    node.decorator_list = []
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def find_def(tree, name):
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            return n
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            return n
    return None


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    ns.update({
        "asyncio": asyncio, "contextlib": contextlib, "suppress": contextlib.suppress,
        "os": os, "json": json,
        "Any": object, "Optional": object, "dict": dict, "set": set, "list": list,
        "str": str, "bool": bool, "int": int, "bytes": bytes, "float": float,
        "logging": __import__("logging"), "logger": None,
        "MessageType": types.SimpleNamespace(COMMAND="COMMAND", TEXT="TEXT"),
        "discord": types.SimpleNamespace(DMChannel=_DM, Thread=_TH,
                                         Permissions=None, Interaction=object),
        "MessageEvent": None,
        "SUPPORTED_DOCUMENT_TYPES": DOC_TYPES, "_TEXT_INJECT_EXTENSIONS": TEXT_EXTS,
        "_DISCORD_COMMAND_SYNC_STATE_SUBDIR": "gateway",
        "_DISCORD_COMMAND_SYNC_STATE_FILENAME": "discord_command_sync_state.json",
        "_YAML_BOOL_ENV_KEYS": (
            ("require_mention", "DISCORD_REQUIRE_MENTION"),
            ("thread_require_mention", "DISCORD_THREAD_REQUIRE_MENTION"),
            ("bots_require_inline_mention", "DISCORD_BOTS_REQUIRE_INLINE_MENTION")),
        "_YAML_WEBSOCKET_LIVENESS_KEYS": (
            ("websocket_liveness_interval_seconds", "liveness_interval_seconds", "HERMES_DISCORD_LIVENESS_INTERVAL_SECONDS"),
            ("websocket_liveness_failure_threshold", "liveness_failure_threshold", "HERMES_DISCORD_LIVENESS_FAILURE_THRESHOLD"),
            ("websocket_heartbeat_ack_max_age_seconds", None, None),
            ("websocket_max_latency_seconds", None, None),
            ("websocket_event_max_silence_seconds", None, None)),
        "t": None, "print": print,
    })
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    for nm in PORTED:
        node = find_def(tree, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow9: missing def {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    missing = [k for k in PORTED if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow9: missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}


# --- canonical projection (compare parsed JSON, never raw strings) ---
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items() if k not in ("__spec__", "_ctx", "_dp")})
        if isinstance(x, (set, frozenset)):
            return [conv(i) for i in x]
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# --- exception materialization / projection (REAL builtin classes) ---
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
    "ModuleNotFoundError": ModuleNotFoundError, "NameError": NameError,
    "IndexError": IndexError, "LookupError": LookupError, "ArithmeticError": ArithmeticError,
    "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
    "FloatingPointError": FloatingPointError, "AssertionError": AssertionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "ReferenceError": ReferenceError,
    "MemoryError": MemoryError, "BufferError": BufferError, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "UnboundLocalError": UnboundLocalError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "FutureWarning": FutureWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
    "EncodingWarning": EncodingWarning, "Exception": Exception,
    "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
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


# --- attribute doubles ---
def attr_double(spec):
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            setattr(d, k, attr_double(v))
        d.__spec__ = spec
        return d
    if isinstance(spec, (list, tuple)):
        return [attr_double(i) for i in spec]
    return spec


def seam_double(spec, ctx, seam_map):
    if isinstance(spec, dict):
        d = types.SimpleNamespace()
        for k, v in spec.items():
            if k in seam_map and bool(v):
                setattr(d, k, rec(ctx, seam_map[k]))
            else:
                setattr(d, k, seam_double(v, ctx, seam_map))
        d.__spec__ = spec
        return d
    if isinstance(spec, (list, tuple)):
        return [seam_double(i, ctx, seam_map) for i in spec]
    return spec


def proj(x):
    return x.__spec__ if hasattr(x, "__spec__") else x


# --- per-case script backend (mirrors C da9_call exactly) ---
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


def rec(ctx, name, args_fn=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        return ctx.call(name, args)
    return f


# --- custom doubles ---
class PathDouble:
    def __init__(self, ctx, p):
        self.p = p
        self._ctx = ctx

    def __truediv__(self, other):
        base = self.p
        if len(base) > 1 and base.endswith("/"):
            base = base.rstrip("/")
        return PathDouble(self._ctx, base + "/" + str(other))

    def mkdir(self, **kw):
        return self._ctx.call("mkdir", {"parents": kw.get("parents"),
                                        "exist_ok": kw.get("exist_ok")})

    def __str__(self):
        return self.p


class LiveTask:
    def __init__(self, ctx, spec, prefix):
        self._ctx = ctx
        self._prefix = prefix
        self.ident = spec.get("id") if isinstance(spec, dict) else None
        self.__spec__ = spec

    def done(self):
        return self._ctx.call(self._prefix + ".done", {})

    def cancel(self):
        return self._ctx.call(self._prefix + ".cancel", {})

    async def _await(self):
        return self._ctx.call(self._prefix + ".await", {})

    def __await__(self):
        return self._await().__await__()


class CmdDouble:
    def __init__(self, ctx, spec):
        object.__setattr__(self, "_ctx", ctx)
        object.__setattr__(self, "__spec__", spec)
        object.__setattr__(self, "_dp", None)
        for k, v in (spec or {}).items():
            object.__setattr__(self, k, attr_double(v))

    @property
    def default_permissions(self):
        return object.__getattribute__(self, "_dp")

    @default_permissions.setter
    def default_permissions(self, val):
        ctx = object.__getattribute__(self, "_ctx")
        ctx.call("set.default_permissions", {"cmd": proj(self), "value": proj(val)})
        object.__setattr__(self, "_dp", val)


class Resp:
    def __init__(self, ctx, spec):
        self._ctx = ctx
        self.status = (spec or {}).get("status", 200)

    async def read(self):
        return self._ctx.call("resp.read", {})


class Scope:
    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False


def make_logger(ctx):
    def shape(fmt, *a, **kw):
        if fmt.startswith("[Discord] _apply_owner_only_visibility"):
            return {"fmt": fmt, "e": proj(a[0]) if a else None}
        if fmt.startswith("[Discord] Could not set"):
            return {"fmt": fmt, "name": proj(a[0]) if a else None,
                    "e": proj(a[1]) if len(a) > 1 else None}
        if fmt.startswith("[Discord] Hid"):
            return {"fmt": fmt, "applied": proj(a[0]) if a else None}
        if "rejected att.read()" in fmt or "cache_audio_from_bytes failed" in fmt:
            return {"fmt": fmt, "e": proj(a[0]) if a else None}
        if "Auto-thread creation failed" in fmt:
            return {"fmt": fmt, "name": proj(a[0]) if a else None,
                    "direct": proj(a[1]) if len(a) > 1 else None,
                    "fallback": proj(a[2]) if len(a) > 2 else None}
        if "Document too large" in fmt:
            return {"fmt": fmt, "size": proj(a[0]) if a else None,
                    "cap": proj(a[1]) if len(a) > 1 else None,
                    "filename": proj(a[2]) if len(a) > 2 else None}
        if fmt.startswith("[Discord] Cached user"):
            return {"fmt": fmt, "kind": proj(a[0]) if a else None,
                    "path": proj(a[1]) if len(a) > 1 else None}
        if "Failed to cache document" in fmt:
            return {"fmt": fmt, "filename": proj(a[0]) if a else None,
                    "e": proj(a[1]) if len(a) > 1 else None}
        if "Liveness task shutdown failed" in fmt:
            return {"fmt": fmt, "name": proj(a[0]) if a else None,
                    "exc_info": kw.get("exc_info", None)}
        return {"fmt": fmt, "args": [proj(x) for x in a],
                "kw": {k: proj(v) for k, v in kw.items()}}
    return types.SimpleNamespace(debug=rec(ctx, "logger.debug", shape),
                                 info=rec(ctx, "logger.info", shape),
                                 warning=rec(ctx, "logger.warning", shape))


def _reg_mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class SessionCM:
    def __init__(self, ctx, kw):
        self._ctx = ctx

    async def __aenter__(self):
        return self

    async def __aexit__(self, *a):
        return False

    def get(self, url, timeout=None, **kw):
        return GetCM(self._ctx, url)


class GetCM:
    def __init__(self, ctx, url):
        self._ctx = ctx
        self._url = url

    async def __aenter__(self):
        v = self._ctx.call("session.get", {"url": self._url, "timeout": 30})
        return Resp(self._ctx, v)

    async def __aexit__(self, *a):
        return False


def make_interaction(spec, ctx):
    spec = spec or {}
    chspec = spec.get("channel") or {}
    kind = chspec.get("__kind__", "group")
    base = _DM if kind == "dm" else (_TH if kind == "thread" else object)
    ch = type("Ch", (base,), {})()
    for k, v in chspec.items():
        if k != "__kind__":
            setattr(ch, k, attr_double(v))
    ch.__spec__ = chspec
    uspec = spec.get("user") or {}
    user = types.SimpleNamespace(**{k: attr_double(v) for k, v in uspec.items()})
    user.__spec__ = uspec
    d = types.SimpleNamespace(channel=ch, user=user, channel_id=spec.get("channel_id"))
    d.__spec__ = spec
    return d


def setup_case(func, inp, state):
    ctx = Ctx(inp.get("io"))
    for nm in PORTED:
        NS[nm] = REAL[nm]
    NS["logger"] = make_logger(ctx)
    NS["print"] = rec(ctx, "print", lambda *a, **k: {"text": str(a[0]) if a else ""})
    NS["t"] = rec(ctx, "t", lambda key, name: {"key": key, "name": name})
    NS["re"] = types.SimpleNamespace(
        sub=rec(ctx, "re.sub", lambda pattern, repl, string: {"pattern": pattern, "repl": repl, "string": string}))
    NS["MessageType"] = types.SimpleNamespace(COMMAND="COMMAND", TEXT="TEXT")
    NS["discord"] = types.SimpleNamespace(DMChannel=_DM, Thread=_TH, Permissions=None, Interaction=object)
    NS["MessageEvent"] = rec(
        ctx, "MessageEvent",
        lambda text, message_type, source, raw_message, channel_prompt, auto_skill: {
            "text": text, "message_type": message_type, "source": proj(source),
            "raw_message": proj(raw_message), "channel_prompt": proj(channel_prompt),
            "auto_skill": proj(auto_skill)})
    NS["SUPPORTED_DOCUMENT_TYPES"] = DOC_TYPES
    NS["_TEXT_INJECT_EXTENSIONS"] = TEXT_EXTS
    # local-import modules
    def gh():
        v = ctx.call("get_hermes_home", {})
        return PathDouble(ctx, str(v))
    _reg_mod("hermes_constants", get_hermes_home=gh)
    load_cfg = rec(ctx, "load_gateway_config")
    get_pdir = rec(ctx, "get_profile_dir")
    _reg_mod("gateway")
    _reg_mod("gateway.config", load_gateway_config=load_cfg)
    _reg_mod("gateway.run", _async_profile_runtime_scope=lambda path: Scope())
    _reg_mod("hermes_cli")
    _reg_mod("hermes_cli.profiles", get_profile_dir=get_pdir)
    _reg_mod("plugins")
    _reg_mod("plugins.platforms")
    _reg_mod("plugins.platforms.discord")
    comp_ctx = rec(ctx, "component_chat_context",
                   lambda interaction, thread: {"interaction": proj(interaction), "thread": "Thread"})
    _reg_mod("plugins.platforms.discord.adapter_component_auth", component_chat_context=comp_ctx)
    _reg_mod("gateway.platforms")
    _reg_mod("gateway.platforms.base",
             resolve_proxy_url=rec(ctx, "resolve_proxy_url",
                                   lambda platform_env_var: {"platform_env_var": platform_env_var}),
             proxy_kwargs_for_aiohttp=rec(ctx, "proxy_kwargs", lambda proxy: {"proxy": proxy}))
    _reg_mod("aiohttp", ClientTimeout=lambda total: types.SimpleNamespace(total=total),
             ClientSession=lambda **kw: SessionCM(ctx, kw))
    # asyncio namespace
    async def _sleep(seconds):
        return ctx.call("sleep", {"seconds": seconds})
    cur_default_inner = rec(ctx, "asyncio.current_task")
    def cur_default():
        return cur_default_inner()
    NS["asyncio"] = types.SimpleNamespace(current_task=cur_default, sleep=_sleep,
                                          CancelledError=asyncio.CancelledError)

    # env.setter for _apply_yaml_config
    env_rec = rec(ctx, "env.set", lambda key, value: {"key": key, "value": value})
    NS["_yaml_env_setter"] = lambda: env_rec

    slf = types.SimpleNamespace()
    slf.name = state.get("name", "")
    extras = {}

    # --- per-func stubs ---
    if func == "_command_sync_state_path":
        slf._command_sync_state_path = REAL[func]

    if func == "_cancel_bot_task":
        slf._bot_task = LiveTask(ctx, state["_bot_task"], "bot_task") if state.get("_bot_task") else None

    if func == "_cancel_liveness_task":
        tasks = {}
        for nm in ("_liveness_task", "_liveness_notification_task"):
            v = state.get(nm)
            tasks[nm] = LiveTask(ctx, v, "liveness") if v else None
            setattr(slf, nm, tasks[nm])
        cur_inner = rec(ctx, "asyncio.current_task")
        OTHER = object()

        def cur_task():
            marker = cur_inner()
            for t in tasks.values():
                if t is not None and t.ident == marker:
                    return t
            return OTHER
        NS["asyncio"] = types.SimpleNamespace(current_task=cur_task, sleep=_sleep,
                                              CancelledError=asyncio.CancelledError)

    if func == "_advance_discord_recovery_cursor":
        slf._utc_now_iso = rec(ctx, "_utc_now_iso")
        conn = types.SimpleNamespace(execute=rec(
            ctx, "conn.execute",
            lambda sql, params: {"channel_id": params[0], "message_id": params[1], "now": params[2]}))
        slf._with_discord_recovery_db = lambda op: op(conn)

    if func == "_alert_adapters_and_config":
        def adapters_for(profile):
            v = ctx.call("_adapters_for_profile", {"profile": profile})
            return runner.adapters if v == "SAME" else v
        runner = types.SimpleNamespace(adapters=state.get("adapters"),
                                       config=state.get("config"),
                                       _adapters_for_profile=adapters_for)
        extras["runner"] = runner

    if func == "_apply_owner_only_visibility":
        NS["discord"].Permissions = rec(ctx, "discord.Permissions", lambda value: {"value": value})
        def get_cmds():
            v = ctx.call("tree.get_commands", {})
            return [CmdDouble(ctx, s) for s in (v or [])]
        extras["tree"] = types.SimpleNamespace(get_commands=get_cmds)

    if func == "_build_slash_event":
        slf._format_thread_chat_name = rec(ctx, "_format_thread_chat_name", lambda channel: {"channel": proj(channel)})
        slf._get_effective_topic = rec(ctx, "_get_effective_topic",
                                       lambda channel, is_thread: {"channel": proj(channel), "is_thread": is_thread})
        slf._get_parent_channel_id = rec(ctx, "_get_parent_channel_id", lambda channel: {"channel": proj(channel)})
        slf._interaction_guild_id = rec(ctx, "_interaction_guild_id",
                                        lambda interaction: {"interaction": proj(interaction)})
        slf._slash_role_grant = rec(ctx, "_slash_role_grant", lambda interaction: {"interaction": proj(interaction)})
        slf.build_source = rec(ctx, "build_source", lambda **kw: {k: proj(v) for k, v in kw.items()})
        slf._resolve_channel_prompt = rec(ctx, "_resolve_channel_prompt",
                                          lambda channel_id, parent_id: {"channel_id": channel_id, "parent_id": parent_id})
        slf._resolve_channel_skills = rec(ctx, "_resolve_channel_skills",
                                          lambda channel_id, parent_id: {"channel_id": channel_id, "parent_id": parent_id})

    if func == "_component_live_auth":
        slf._is_sender_authorized = rec(
            ctx, "_is_sender_authorized",
            lambda user_id, chat_type, chat_id, thread_id=None: {
                "user_id": user_id, "chat_type": proj(chat_type), "chat_id": proj(chat_id),
                "thread_id": proj(thread_id)})

    if func == "_auto_create_thread":
        slf._derive_auto_thread_name = rec(ctx, "_derive_auto_thread_name", lambda content: {"content": content})
        stamp = rec(ctx, "stamp", lambda thread, name: {"thread": proj(thread), "name": name})
        slf._stamp_auto_thread_name = stamp
        create_rec = rec(ctx, "create_thread",
                         lambda name, auto_archive_duration: {"name": name, "auto_archive_duration": auto_archive_duration})
        async def create(name, auto_archive_duration):
            return create_rec(name, auto_archive_duration)
        seed_create = rec(ctx, "seed.create_thread",
                          lambda name, auto_archive_duration, reason: {
                              "name": name, "auto_archive_duration": auto_archive_duration, "reason": reason})
        async def seed_ct(name, auto_archive_duration, reason):
            return seed_create(name, auto_archive_duration, reason)
        async def channel_send(text):
            v = ctx.call("channel.send", {"text": text})
            return types.SimpleNamespace(create_thread=seed_ct, __spec__=v)
        message = types.SimpleNamespace(
            content=args_content(inp), author=types.SimpleNamespace(display_name=args_author(inp)))
        message.create_thread = create
        message.channel = types.SimpleNamespace(send=channel_send)
        message.__spec__ = inp.get("args", {}).get("message", {})
        extras["message"] = message

    if func in ("_cache_discord_image", "_cache_discord_audio"):
        async def read_bytes(att, media_type):
            return ctx.call("_read_attachment_bytes", {"att": proj(att), "media_type": media_type})
        slf._read_attachment_bytes = read_bytes
        async def cbytes(data, ext):
            return ctx.call("cache_bytes", {"data": proj(data), "ext": ext})
        async def curl(url, ext):
            return ctx.call("cache_url", {"url": url, "ext": ext})
        NS["cache_image_from_bytes_async"] = cbytes
        NS["cache_audio_from_bytes_async"] = cbytes
        NS["cache_image_from_url"] = curl
        NS["cache_audio_from_url"] = curl

    if func == "_cache_discord_document":
        async def read_bytes(att, media_type):
            return ctx.call("_read_attachment_bytes", {"att": proj(att), "media_type": media_type})
        slf._read_attachment_bytes = read_bytes
        NS["is_safe_url"] = rec(ctx, "is_safe_url", lambda url: {"url": url})

    if func == "_cache_simple_media":
        a = inp.get("args") or {}
        ct = a.get("content_type", "")
        kind = a.get("kind", "")
        exts_list = a.get("exts", [])
        de = a.get("default_ext", "")
        async def cacher(att, ext):
            return ctx.call("cache_media", {"att": proj(att), "content_type": ct, "kind": kind,
                                            "exts": exts_list, "default_ext": de, "ext": ext})
        slf._cache_discord_image = cacher
        slf._cache_discord_audio = cacher

    if func == "_collect_attachment_media":
        async def csimple(att, content_type, kind, exts, default_ext):
            return ctx.call("cache_simple", {"att": proj(att), "content_type": content_type,
                                             "kind": kind, "default_ext": default_ext})
        slf._cache_simple_media = csimple
        slf._discord_max_attachment_bytes = rec(ctx, "max_doc_bytes")
        async def cdoc(att, ext):
            v = ctx.call("cache_document", {"att": proj(att), "ext": ext})
            return v.encode("utf-8") if isinstance(v, str) else v
        slf._cache_discord_document = cdoc
        async def cdb(data, filename):
            return ctx.call("cache_doc_bytes", {
                "data": data.decode("utf-8", "replace") if isinstance(data, (bytes, bytearray)) else str(data),
                "filename": filename})
        NS["cache_document_from_bytes_async"] = cdb

    for nm in INSTANCE_FUNCS:
        if func == nm:
            setattr(slf, nm, types.MethodType(REAL[nm], slf))
    return ctx, slf, extras


def args_content(inp):
    m = (inp.get("args") or {}).get("message") or {}
    return m.get("content", "")


def args_author(inp):
    m = (inp.get("args") or {}).get("message") or {}
    a = m.get("author") or {}
    return a.get("display_name", "")


def drive(func, inp):
    args = inp.get("args") or {}
    state = copy.deepcopy(inp.get("state") or {})
    ctx, slf, extras = setup_case(func, inp, state)
    try:
        if func == "_clean_discord_id":
            ret = NS[func](args.get("entry", ""))
        elif func == "_apply_yaml_config":
            ret = NS[func](args.get("yaml_cfg", {}), args.get("discord_cfg", {}))
        elif func == "_alert_adapters_and_config":
            ret = NS[func](extras["runner"], args.get("profile"))
        elif func == "_command_sync_state_path":
            ret = slf._command_sync_state_path()
        elif func == "_cancel_bot_task":
            ret = slf._cancel_bot_task()
        elif func == "_cancel_liveness_task":
            ret = slf._cancel_liveness_task()
        elif func == "_advance_discord_recovery_cursor":
            ret = slf._advance_discord_recovery_cursor(args.get("channel_id", ""), args.get("message_id", ""))
        elif func == "_apply_owner_only_visibility":
            ret = slf._apply_owner_only_visibility(extras["tree"])
        elif func == "_build_slash_event":
            ret = slf._build_slash_event(make_interaction(args.get("interaction"), ctx), args.get("text", ""))
        elif func == "_component_live_auth":
            ret = slf._component_live_auth(attr_double(args.get("interaction")))
        elif func == "_auto_create_thread":
            ret = slf._auto_create_thread(extras["message"])
        elif func in ("_cache_discord_image", "_cache_discord_audio", "_cache_discord_document"):
            ret = getattr(slf, func)(attr_double(args.get("att")), args.get("ext", ""))
        elif func == "_cache_simple_media":
            ret = slf._cache_simple_media(attr_double(args.get("att")), args.get("content_type", ""),
                                          args.get("kind", ""), set(args.get("exts", [])),
                                          args.get("default_ext", ""))
        elif func == "_collect_attachment_media":
            ret = slf._collect_attachment_media([attr_double(a) for a in (args.get("all_attachments") or [])])
        else:
            raise RuntimeError(f"unknown func {func}")
        if asyncio.iscoroutine(ret):
            ret = asyncio.run(ret)
    except BaseException as exc:
        ret = proj_exc(exc)
    finally:
        if func == "_cancel_bot_task" and "_bot_task" in state and getattr(slf, "_bot_task", 1) is None:
            state["_bot_task"] = None
        elif func == "_cancel_liveness_task":
            for nm in ("_liveness_task", "_liveness_notification_task"):
                if nm in state and getattr(slf, nm, 1) is None:
                    state[nm] = None
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# --- three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ---
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow9.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    c_names = set(re.findall(r'da9_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_names != registry:
        print(f"FATAL oracle_discord_adapter_flow9: seam-set mismatch C<->registry")
        print(f"  in C only: {sorted(c_names - registry)}")
        print(f"  in registry only: {sorted(registry - c_names)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= EXPLICIT_SEAMS
    if used != registry:
        print(f"FATAL oracle_discord_adapter_flow9: seam-set mismatch oracle<->registry")
        print(f"  in oracle only: {sorted(used - registry)}")
        print(f"  not stubbed: {sorted(registry - used)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow9.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow9: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE discord_adapter_flow9: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow9.jsonl"
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
    print(f"SELFTEST discord_adapter_flow9: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = [f for f in PORTED if f not in counts]
    if missing:
        print(f"SELFTEST FUNCS WITHOUT CASES: {missing}")
        bad += 1
    low = [f for f in PORTED if counts.get(f, 0) < 6]
    if low:
        print(f"SELFTEST FUNCS WITH <6 CASES: {low}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
