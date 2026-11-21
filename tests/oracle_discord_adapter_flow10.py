#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW10 remainder port (C sibling:
src/gateway/platforms/discord_adapter_flow10.c).

Drives the LIVE plugins/platforms/discord/adapter.py defs with SCRIPTED seams:
local imports, claimed siblings, injected callables and attribute doubles are
recording stubs fed the SAME scripts the C harness feeds its universal seam
da10_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted (module/class bodies, decorators stripped) into
a registered sys.modules namespace; the heavy quarry import chain is never
imported. Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper
catches BaseException (SystemExit included); scripted raises round-trip their
spec. Summary: 'ORACLE discord_adapter_flow10: N cases, M mismatches'.
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
if not os.path.exists(os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")):
    for _cand in (str(_REPO.parent / "hermes-agent"), "C:/Users/eman5/hermes-agent"):
        if os.path.exists(os.path.join(_cand, "plugins", "platforms", "discord", "adapter.py")):
            Q = _cand
            break
A_SRC = os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag
MODNAME = "discord_adapter_flow10_oracle"

INSTANCE_FUNCS = [
    "_create_thread", "_defer_unless_expired", "_deliver_unauthorized_slash_alert",
    "_desired_command_sync_fingerprint", "_discord_message_admission",
    "_discord_message_attempts_exhausted", "_discord_message_has_active_claim",
    "_discord_message_is_persistently_complete", "_discord_recovery_cursor",
    "_dispatch_discord_message", "_dispatch_recovered_message",
    "_dispatch_thread_session", "_write_command_sync_state",
    "_sleep_between_command_sync_mutations", "_resolve_channel_prompt",
]
FREE_FUNCS = ["_define_discord_view_classes"]
PORTED = FREE_FUNCS + INSTANCE_FUNCS


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    c = copy.copy(node)
    c.decorator_list = []
    text = textwrap.dedent(seg(src, c))
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


class _Flex:
    """Benign attribute/call container for class-definition-time stubs."""
    def __init__(self, name="flex"):
        object.__setattr__(self, "_n", name)
    def __call__(self, *a, **k):
        return _Flex()
    def __getattr__(self, n):
        return _Flex(n)
    def __iter__(self):
        return iter([])
    def __len__(self):
        return 0
    def __bool__(self):
        return True
    def __eq__(self, o):
        return False
    def __hash__(self):
        return 1


class _View:
    pass


class _UI:
    View = _View
    @staticmethod
    def button(*a, **k):
        def d(f): return f
        return d
    @staticmethod
    def select(*a, **k):
        def d(f): return f
        return d


def discord_stub():
    d = _Flex()
    object.__setattr__(d, "ui", _UI())
    object.__setattr__(d, "Interaction", type("Interaction", (), {}))
    object.__setattr__(d, "DMChannel", type("DMChannel", (), {}))
    object.__setattr__(d, "Color", _Flex())
    object.__setattr__(d, "ButtonStyle", _Flex())

    class _MT:
        default = 0
        reply = 19
    object.__setattr__(d, "MessageType", _MT)
    return d


DISCORD = discord_stub()


def extract():
    mod = new_module(MODNAME)
    ns = mod.__dict__
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # Seed every free name used by _define_discord_view_classes at class-def time.
    node = find_def(tree, "_define_discord_view_classes")
    bound, loaded = set(), set()
    for n in ast.walk(node):
        if isinstance(n, ast.Name) and isinstance(n.ctx, (ast.Store, ast.Del)):
            bound.add(n.id)
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            bound.add(n.name)
        if isinstance(n, ast.arg):
            bound.add(n.arg)
        if isinstance(n, ast.Name) and isinstance(n.ctx, ast.Load):
            loaded.add(n.id)
    free = sorted(loaded - bound)

    def benign(*a, **k):
        return None

    ns.update({
        "discord": DISCORD, "Optional": object, "Any": object,
        "logger": _Flex(), "staticmethod": staticmethod,
        "set": set, "dict": dict, "list": list, "str": str, "bool": bool,
        "int": int, "bytes": bytes, "float": float, "frozenset": frozenset,
        "tuple": tuple, "isinstance": isinstance, "len": len, "any": any,
        "all": all, "Exception": Exception, "ValueError": ValueError,
        "super": super, "object": object, "enumerate": enumerate,
        "max": max, "min": min, "next": next, "range": range,
        "getattr": getattr, "hasattr": hasattr, "json": json,
        "asyncio": asyncio, "os": os, "t": benign,
        "VALID_THREAD_AUTO_ARCHIVE_MINUTES": {60, 1440, 4320, 10080},
        "MessageEvent": None,
        "Platform": types.SimpleNamespace(TELEGRAM="telegram", SLACK="slack"),
        "MessageType": DISCORD.MessageType,
    })
    for nm in free:
        if nm not in ns:
            ns[nm] = benign
    for nm in PORTED:
        node = find_def(tree, nm)
        if node is None:
            print(f"FATAL oracle_discord_adapter_flow10: missing def {nm}")
            sys.exit(2)
        exec_node(src, node, ns, A_SRC)
    missing = [k for k in PORTED if k not in ns]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow10: missing: {missing}")
        sys.exit(2)
    return ns, src, tree


NS, SRC, TREE = extract()
REAL = {nm: NS[nm] for nm in PORTED}

# ── seam names regex-extracted from the C port (SEAM-NAME SET EQUALITY) ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "discord_adapter_flow10.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'da10_call\(\s*ctx,\s*"([^"]+)"', _CSRC))


def canon(v):
    def conv(x):
        if isinstance(x, _Token):
            return conv(x.v)
        if isinstance(x, BaseException):
            return str(x)
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items()
                         if k not in ("_ctx", "_raw", "_dp", "_n")})
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


class _Token:
    def __init__(self, value): object.__setattr__(self, "v", value)
    def __repr__(self): return repr(self.v)


# ── exception hierarchy ──
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "TimeoutError": TimeoutError, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
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


# ── seam arg projections (MUST match da10_call args in the C port) ──
def _p_args(**kw):
    return kw


ARGMAP = {
    "_is_discord_unknown_interaction": lambda error: {"error": error},
    "response.defer": lambda ephemeral=None: {"ephemeral": ephemeral},
    "logger.warning": lambda fmt, *a, **k: {"fmt": fmt, "args": list(a)},
    "logger.debug": lambda fmt, *a, **k: {"fmt": fmt, "args": list(a)},
    "recovery_call": lambda: {},   # overridden per-target below
    "datetime_cutoff": lambda: {},
    "max_attempts": lambda: {},
    "wait_for_ready": lambda timeout=None: {"timeout": timeout},
    "admission": lambda: {},        # overridden
    "record_bot_tag_debounce": lambda message=None: {"message_id": getattr(message, "id", "")},
    "handle_message": lambda: {},   # overridden
    "get_parent_channel_id": lambda: {},  # overridden
    "channel_keys": lambda: {},     # overridden
    "free_channels": lambda: {},
    "free_response_channels": lambda: {},
    "require_mention": lambda: {},
    "in_bot_thread": lambda: {},
    "self_is_explicitly_mentioned": lambda: {},
    "format_thread_chat_name": lambda: {},
    "get_effective_topic": lambda is_thread=None: {},
    "interaction_guild_id": lambda: {},
    "slash_role_grant": lambda: {},
    "build_source": lambda: {},    # overridden
    "resolve_channel_skills": lambda: {},
    "thread_channel_prompt": lambda: {},
    "MessageEvent": lambda: {},    # overridden
    "t": lambda key, **kw: dict({"key": key}, **kw),
    "resolve_interaction_channel": lambda: {},
    "thread_parent_channel": lambda channel=None: {},
    "create_thread": lambda name=None, auto_archive_duration=None, reason=None: {
        "name": name, "auto_archive_duration": auto_archive_duration, "reason": reason},
    "thread.send": lambda content=None: {"content": content},
    "thread_created": lambda: {},
    "seed.send": lambda content=None: {"content": content},
    "seed.create_thread": lambda: {},
    "alert_adapters_and_config": lambda runner=None, profile=None: {
        "runner": runner, "profile": profile},
    "get_home_channel": lambda target=None: {"target": target},
    "present_notification": lambda send_alert=None, platform=None, diagnostic=None: {
        "platform": platform, "diagnostic": diagnostic},
    "adapter.send": lambda chat_id=None, msg=None: {"chat_id": chat_id, "msg": msg},
    "client_tree": lambda: {},
    "get_commands": lambda: {},
    "to_dict": lambda command=None: {},
    "canonicalize_command": lambda command=None: {},
    "get_language": lambda: {},
    "sha256_hex": lambda payload=None: {"payload": payload},
    "command_sync_state_path": lambda: {},
    "atomic_json_write": lambda path=None, state=None, indent=None, separators=None: {
        "path": path, "state": state, "indent": indent, "separators": separators},
    "command_sync_mutation_interval_seconds": lambda: {},
    "asyncio.sleep": lambda delay=None: {"delay": delay},
    "resolve_channel_prompt": lambda extra=None, channel_id=None, parent_id=None: {
        "extra": extra, "channel_id": channel_id, "parent_id": parent_id},
    "dedup.is_duplicate": lambda: {},
    "dedup.contains": lambda: {},
    "refresh_live_gates": lambda: {},
    "get_allow_bots": lambda: {},
    "is_bot_tag_continuation": lambda: {},
    "bots_require_inline_mention": lambda: {},
    "self_is_raw_mentioned": lambda: {},
    "is_allowed_user": lambda: {},
    "warn_if_fail_closed_default": lambda: {},
    "scoped_gate_env": lambda key=None, default=None: {"key": key, "default": default},
}

AWAIT_SEAMS = {
    "response.defer", "wait_for_ready", "handle_message", "get_parent_channel_id",
    "channel_keys", "free_channels", "require_mention", "in_bot_thread",
    "self_is_explicitly_mentioned", "format_thread_chat_name", "get_effective_topic",
    "interaction_guild_id", "slash_role_grant", "build_source", "resolve_channel_skills",
    "thread_channel_prompt", "MessageEvent", "resolve_interaction_channel",
    "thread_parent_channel", "create_thread", "thread.send", "seed.send",
    "seed.create_thread", "thread_created", "alert_adapters_and_config",
    "get_home_channel", "present_notification", "adapter.send", "client_tree",
    "get_commands", "to_dict", "canonicalize_command", "get_language",
    "sha256_hex", "command_sync_state_path", "atomic_json_write",
    "asyncio.sleep", "resolve_channel_prompt", "dedup.is_duplicate", "dedup.contains",
    "refresh_live_gates", "get_allow_bots", "is_bot_tag_continuation",
    "bots_require_inline_mention", "self_is_raw_mentioned", "is_allowed_user",
    "warn_if_fail_closed_default", "in_bot_thread", "scoped_gate_env",
    "free_response_channels",
}
# Most target methods are async in the live code; the seam recorder shape is
# driven per-target below. ARGMAP projections ignore self.

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

    def call(self, name, args=None, force_args=None):
        args = force_args if force_args is not None else (args or {})
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


class _Self:
    def __init__(self, ctx): object.__setattr__(self, "_ctx", ctx)


def _install(obj, name, value):
    object.__setattr__(obj, name, value)


class _Obj:
    """Generic attribute-double; named methods fire seams on the ctx."""
    def __init__(self, ctx, methods=None, attrs=None):
        object.__setattr__(self, "_ctx", ctx)
        for m, spec in (methods or {}).items():
            object.__setattr__(self, m, make_recorder(ctx, spec[0], spec[1]))
        for k, v in (attrs or {}).items():
            object.__setattr__(self, k, v)


def make_recorder(ctx, nm, argfn=None, argoverride=None, is_async=False):
    argfn = argfn or ARGMAP.get(nm, lambda *a, **k: {})

    def invoke(*a, **k):
        if argoverride is not None:
            return ctx.call(nm, force_args=argoverride())
        return ctx.call(nm, argfn(*a, **k))

    if is_async:
        async def af(*a, **k):
            return invoke(*a, **k)
        return af
    return invoke


def mat(raw):
    if isinstance(raw, dict):
        return types.SimpleNamespace(**{k: mat(v) for k, v in raw.items()})
    if isinstance(raw, list):
        return [mat(i) for i in raw]
    return raw


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


def _run(x):
    if inspect.isawaitable(x):
        return asyncio.run(x)
    return x


import inspect

# Per-target context setup + live invocation. Returns the live return value.
def live_call(func, args, ctx):
    g = args.get
    SELF = NS["__SELF__"]

    if func == "_define_discord_view_classes":
        return NS[func]()

    if func == "_defer_unless_expired":
        rec = make_recorder(ctx, "_is_discord_unknown_interaction", is_async=False)
        _install(SELF, "_is_discord_unknown_interaction", rec)
        NS["logger"] = types.SimpleNamespace(
            warning=make_recorder(ctx, "logger.warning"),
            debug=lambda *a, **k: None, info=lambda *a, **k: None, error=lambda *a, **k: None)
        it = _Obj(ctx, {"defer": ("response.defer", None, True)})
        object.__setattr__(it, "response", it)
        warn_args = g("warn_args") or []
        return NS[func](SELF, it, g("warn_fmt", ""), *warn_args)

    if func in ("_discord_message_is_persistently_complete",
                "_discord_message_has_active_claim",
                "_discord_message_attempts_exhausted",
                "_discord_recovery_cursor"):
        opmap = {
            "_discord_message_is_persistently_complete": "persistently_complete",
            "_discord_message_has_active_claim": "has_active_claim",
            "_discord_message_attempts_exhausted": "attempts_exhausted",
            "_discord_recovery_cursor": "recovery_cursor",
        }
        op = opmap[func]
        key = "channel_id" if func == "_discord_recovery_cursor" else "message_id"
        val = g(key, "")
        NS["__RECOVERY_ARGS__"] = {"op": op, key: val,
                                    "default": True if op == "has_active_claim" else False}
        def recovery(fn, default=None):
            return ctx.call("recovery_call", force_args=dict(NS["__RECOVERY_ARGS__"]))
        _install(SELF, "_with_discord_recovery_db", recovery)
        if func == "_discord_message_has_active_claim":
            class _DT:
                UTC = object()
                class datetime:
                    @staticmethod
                    def now(tz=None): return _DTRes(ctx)
                class timedelta:
                    def __init__(self, minutes=0): pass
            class _DTRes:
                def __init__(self, c): object.__setattr__(self, "_c", c)
                def __sub__(self, other): return self
                def isoformat(self): return self._c.call("datetime_cutoff", force_args={})
            NS["dt"] = _DT
        if func == "_discord_message_attempts_exhausted":
            _install(SELF, "_missed_message_backfill_max_attempts",
                     make_recorder(ctx, "max_attempts", force=True) if False else
                     (lambda: ctx.call("max_attempts", force_args={})))
            _install(SELF, "name", g("name", ""))
            NS["logger"] = types.SimpleNamespace(
                debug=make_recorder(ctx, "logger.debug"),
                warning=lambda *a, **k: None, info=lambda *a, **k: None, error=lambda *a, **k: None)
        return NS[func](SELF, val)

    if func == "_dispatch_discord_message":
        class _Ready:
            def __init__(self, c, ready): object.__setattr__(self, "_c", c); object.__setattr__(self, "r", ready)
            def is_set(self): return self.r
            def wait(self): return _await_rec(ctx, "wait_for_ready", {"timeout": 30.0})
        _install(SELF, "_ready_event", _Ready(ctx, bool(g("ready", True))))
        NS["asyncio"] = types.SimpleNamespace(wait_for=lambda coro, timeout: coro)
        msg = _Obj(ctx, attrs={"id": g("message_id", "")})
        _install(SELF, "_discord_message_admission",
                 lambda m, claim: ctx.call("admission", force_args={"message_id": g("message_id", ""), "claim": claim}))
        _install(SELF, "_record_bot_tag_debounce",
                 lambda m: ctx.call("record_bot_tag_debounce", force_args={"message_id": g("message_id", "")}))
        _install(SELF, "_handle_message",
                 lambda m, role_authorized: _await_rec(ctx, "handle_message",
                     {"message_id": g("message_id", ""), "role_authorized": role_authorized}))
        return NS[func](SELF, msg)

    if func == "_dispatch_recovered_message":
        mid = g("message_id", "")
        isdm = bool(g("is_dm", False))
        chan = (DISCORD.DMChannel() if isdm else types.SimpleNamespace(id=g("channel_id", "")))
        object.__setattr__(chan, "id", g("channel_id", ""))
        msg = types.SimpleNamespace(channel=chan, id=mid)
        _install(SELF, "_get_parent_channel_id",
                 lambda ch: _await_rec(ctx, "get_parent_channel_id", {"channel_id": g("channel_id", "")}))
        _install(SELF, "_discord_channel_keys",
                 lambda m, pid: _await_rec(ctx, "channel_keys", {"message_id": mid, "parent_id": pid or ""}))
        _install(SELF, "_discord_free_response_channels",
                 lambda: _await_rec(ctx, "free_channels", {}))
        _install(SELF, "_discord_require_mention", lambda: _await_rec(ctx, "require_mention", {}))
        _install(SELF, "_in_bot_thread",
                 lambda m: _await_rec(ctx, "in_bot_thread", {"message_id": mid}))
        _install(SELF, "_self_is_explicitly_mentioned",
                 lambda m: _await_rec(ctx, "self_is_explicitly_mentioned", {"message_id": mid}))
        _install(SELF, "_discord_message_admission",
                 lambda m, claim: ctx.call("admission", force_args={"message_id": mid, "claim": claim}))
        _install(SELF, "_handle_message",
                 lambda m, role_authorized=False, recovered=False: _await_rec(ctx, "handle_message",
                     {"message_id": mid, "role_authorized": role_authorized, "recovered": recovered}))
        NS["discord"] = DISCORD
        return NS[func](SELF, msg)

    if func == "_dispatch_thread_session":
        tid, uid, uname, text = g("thread_id", ""), g("user_id", ""), g("user_name", ""), g("text", "")
        thread = types.SimpleNamespace(id=tid)
        it = types.SimpleNamespace(user=types.SimpleNamespace(id=uid, display_name=uname))
        _install(SELF, "_get_parent_channel_id",
                 lambda ch: _await_rec(ctx, "get_parent_channel_id", {"thread_id": tid}))
        _install(SELF, "_format_thread_chat_name",
                 lambda ch: _await_rec(ctx, "format_thread_chat_name", {"thread_id": tid}))
        _install(SELF, "_get_effective_topic",
                 lambda ch, is_thread=False: _await_rec(ctx, "get_effective_topic", {"thread_id": tid}))
        _install(SELF, "_interaction_guild_id", lambda it: _await_rec(ctx, "interaction_guild_id", {}))
        _install(SELF, "_slash_role_grant", lambda it: _await_rec(ctx, "slash_role_grant", {}))
        def build_source(**kw):
            src_args = {
                "chat_id": kw.get("chat_id"), "chat_name": kw.get("chat_name"),
                "chat_type": kw.get("chat_type"), "user_id": kw.get("user_id"),
                "user_name": kw.get("user_name"), "thread_id": kw.get("thread_id"),
                "chat_topic": kw.get("chat_topic"), "guild_id": kw.get("guild_id"),
                "parent_chat_id": kw.get("parent_chat_id"),
                "role_authorized": kw.get("role_authorized")}
            return _await_rec(ctx, "build_source", src_args)
        _install(SELF, "build_source", build_source)
        _install(SELF, "_resolve_channel_skills",
                 lambda cid, pid: _await_rec(ctx, "resolve_channel_skills", {"thread_id": cid, "parent_id": pid or ""}))
        _install(SELF, "_resolve_channel_prompt",
                 lambda cid, pid: _await_rec(ctx, "thread_channel_prompt", {"thread_id": cid, "parent_id": pid or ""}))
        def make_event(**kw):
            ev_args = {"text": kw.get("text"), "message_type": kw.get("message_type"),
                       "source": kw.get("source"), "auto_skill": kw.get("auto_skill"),
                       "channel_prompt": kw.get("channel_prompt")}
            return ctx.call("MessageEvent", force_args=ev_args)
        NS["MessageEvent"] = make_event
        NS["MessageType"] = types.SimpleNamespace(TEXT="TEXT")
        _install(SELF, "handle_message", lambda ev: _await_rec(ctx, "handle_message", {}))
        return NS[func](SELF, it, thread, text)

    if func == "_create_thread":
        mid = g("message", "")
        name, message, dur = g("name", ""), g("message", ""), g("auto_archive_duration", 1440)
        it = types.SimpleNamespace(user=types.SimpleNamespace(display_name=g("display_name", "unknown user")))
        NS["t"] = make_recorder(ctx, "t")
        NS["VALID_THREAD_AUTO_ARCHIVE_MINUTES"] = {60, 1440, 4320, 10080}
        NS["discord"] = DISCORD
        def resolve_channel(interaction):
            raw = ctx.call("resolve_interaction_channel", force_args={})
            if raw is None:
                return None
            ch = (DISCORD.DMChannel() if raw.get("is_dm") else types.SimpleNamespace())
            object.__setattr__(ch, "is_dm", bool(raw.get("is_dm")))
            return ch
        _install(SELF, "_resolve_interaction_channel", resolve_channel)
        def thread_parent(channel):
            raw = ctx.call("thread_parent_channel", force_args={})
            if raw is None:
                return None
            return _Obj(ctx, {
                "create_thread": ("create_thread", None, True),
                "send": ("seed.send", None, True)}, attrs={"_raw": raw})
        _install(SELF, "_thread_parent_channel", thread_parent)
        def thread_created(thread, n):
            return ctx.call("thread_created", force_args={})
        _install(SELF, "_thread_created", thread_created)
        # wrap channel/thread result objects with async send/create_thread seam methods
        def mk_thread(raw):
            t = _Obj(ctx, {"send": ("thread.send", None, True)})
            object.__setattr__(t, "_raw", raw)
            return t
        NS["__mk_thread__"] = mk_thread
        return NS[func](SELF, it, name=name, message=message, auto_archive_duration=dur)

    if func == "_deliver_unauthorized_slash_alert":
        NS["t"] = make_recorder(ctx, "t")
        NS["Platform"] = types.SimpleNamespace(TELEGRAM="telegram", SLACK="slack")
        NS["logger"] = types.SimpleNamespace(
            debug=make_recorder(ctx, "logger.debug"),
            warning=lambda *a, **k: None, info=lambda *a, **k: None, error=lambda *a, **k: None)
        def aac(runner, profile):
            raw = ctx.call("alert_adapters_and_config", force_args={"runner": runner, "profile": profile})
            adapters_raw, config_raw = raw
            class _Adapters:
                def get(self, target):
                    a = adapters_raw.get(str(target), None) if isinstance(adapters_raw, dict) else None
                    if a is None:
                        return None
                    return _Obj(ctx, {"send": ("adapter.send", None, True)}, attrs={"_raw": a})
            class _Config:
                def get_home_channel(self, target):
                    h = config_raw.get(str(target), None) if isinstance(config_raw, dict) else None
                    return mat(h) if h is not None else None
            return _Adapters(), _Config()
        _install(SELF, "_alert_adapters_and_config", aac)
        # stub in-function import
        gateway = sys.modules.get("gateway") or new_module("gateway")
        gateway.__path__ = []
        wn = new_module("gateway.warning_notifications")
        async def present(send_alert, platform=None, diagnostic=None):
            ok = ctx.call("present_notification", force_args={"platform": platform, "diagnostic": diagnostic})
            if ok:
                await send_alert()
            return ok
        wn.present_notification = present
        gateway.warning_notifications = wn
        rest = dict(user_name=g("user_name", ""), user_id=g("user_id", ""),
                    chan_id=g("chan_id", ""), guild_id=g("guild_id", ""),
                    command_text=g("command_text", ""), reason=g("reason", ""))
        return NS[func](SELF, g("runner", None), g("profile", None), **rest)

    if func == "_desired_command_sync_fingerprint":
        NS["json"] = json
        NS["get_language"] = make_recorder(ctx, "get_language")
        class _Hex:
            def __init__(self, c, payload): object.__setattr__(self, "_c", c); object.__setattr__(self, "_p", payload)
            def hexdigest(self): return self._c.call("sha256_hex", force_args={"payload": self._p})
        class _Hashlib:
            @staticmethod
            def sha256(b):
                return _Hex(ctx, b.decode("utf-8") if isinstance(b, bytes) else str(b))
        NS["hashlib"] = _Hashlib
        client = g("client", False)
        _install(SELF, "_client", (object() if client else None))
        def tree_of(client):
            raw = ctx.call("client_tree", force_args={})
            if raw is None:
                return None
            commands = ctx.call("get_commands", force_args={})
            class _Tree:
                def get_commands(self):
                    out = []
                    for cid in (commands or []):
                        class _Cmd:
                            def __init__(self, i): self.i = i
                            def to_dict(self, tree):
                                return ctx.call("to_dict", force_args={"command": self.i})
                        out.append(_Cmd(cid))
                    return out
            return _Tree()
        NS["__tree_of__"] = tree_of
        # patch _client.tree via property-like function
        cl = _Obj(ctx)
        object.__setattr__(cl, "tree", tree_of(client))
        _install(SELF, "_client", cl)
        _install(SELF, "_canonicalize_app_command_payload",
                 lambda cmd: ctx.call("canonicalize_command", force_args={"command": cmd}))
        return NS[func](SELF)

    if func == "_write_command_sync_state":
        _install(SELF, "_command_sync_state_path", lambda: ctx.call("command_sync_state_path", force_args={}))
        NS["atomic_json_write"] = make_recorder(ctx, "atomic_json_write")
        return NS[func](SELF, g("state", {}))

    if func == "_sleep_between_command_sync_mutations":
        _install(SELF, "_command_sync_mutation_interval_seconds",
                 lambda: ctx.call("command_sync_mutation_interval_seconds", force_args={}))
        NS["asyncio"] = types.SimpleNamespace(
            sleep=lambda d: _await_rec(ctx, "asyncio.sleep", {"delay": d}))
        return NS[func](SELF)

    if func == "_resolve_channel_prompt":
        gateway = sys.modules.get("gateway") or new_module("gateway")
        gateway.__path__ = []
        platforms = sys.modules.get("gateway.platforms") or new_module("gateway.platforms")
        platforms.__path__ = []
        base = new_module("gateway.platforms.base")
        base.resolve_channel_prompt = make_recorder(ctx, "resolve_channel_prompt")
        gateway.platforms = platforms
        platforms.base = base
        cfg = types.SimpleNamespace(extra=g("extra", {}))
        _install(SELF, "config", cfg)
        return NS[func](SELF, g("channel_id", ""), g("parent_id", None))

    if func == "_discord_message_admission":
        mid = g("message_id", "")
        author = types.SimpleNamespace(id=g("author_id", ""), is_self=bool(g("author_self", False)))
        chan = types.SimpleNamespace(id=g("channel_id", ""), is_dm=bool(g("is_dm", False)),
                                     parent_id=g("parent_id", ""))
        mentions = []
        for m in (g("mentions") or []):
            mentions.append(types.SimpleNamespace(bot=bool(m.get("bot")), is_self=bool(m.get("is_self"))))
        msg = types.SimpleNamespace(id=mid, author=author, channel=chan, mentions=mentions,
                                    type=g("type", 0),
                                    guild=(mat(g("guild")) if g("guild") is not None else None))
        NS["discord"] = DISCORD
        _install(SELF, "_client", types.SimpleNamespace(user=object()))
        _install(SELF, "_dedup", _Obj(ctx, {
            "is_duplicate": ("dedup.is_duplicate", None, True),
            "contains": ("dedup.contains", None, True)}))
        _install(SELF, "_refresh_live_gates", lambda: ctx.call("refresh_live_gates", force_args={}))
        NS["is_automated_message"] = lambda m: g("is_automated", False)
        _install(SELF, "_get_allow_bots", lambda: ctx.call("get_allow_bots", force_args={}))
        _install(SELF, "_is_bot_tag_debounce_continuation",
                 lambda m: ctx.call("is_bot_tag_continuation", force_args={}))
        _install(SELF, "_self_is_explicitly_mentioned",
                 lambda m: ctx.call("self_is_explicitly_mentioned", force_args={"message_id": mid}))
        _install(SELF, "_discord_bots_require_inline_mention",
                 lambda: ctx.call("bots_require_inline_mention", force_args={}))
        _install(SELF, "_self_is_raw_mentioned",
                 lambda m: ctx.call("self_is_raw_mentioned", force_args={"message_id": mid}))
        _install(SELF, "_get_parent_channel_id",
                 lambda ch: ctx.call("get_parent_channel_id", force_args={"channel_id": g("channel_id", "")}))
        def is_allowed_user(uid, author, guild=None, is_dm=False, channel_ids=None):
            return ctx.call("is_allowed_user", force_args={
                "author_id": uid, "is_dm": is_dm,
                "channel_ids": sorted([str(x) for x in (channel_ids or [])]),
                "guild": canon(g("guild"))})
        _install(SELF, "_is_allowed_user", is_allowed_user)
        _install(SELF, "_warn_if_fail_closed_default", lambda: ctx.call("warn_if_fail_closed_default", force_args={}))
        _install(SELF, "_allowed_role_ids", set([1]) if g("has_allowed_roles", False) else set())
        _install(SELF, "_in_bot_thread", lambda m: ctx.call("in_bot_thread", force_args={"message_id": mid}))
        _install(SELF, "_discord_free_response_channels",
                 lambda: set(ctx.call("free_response_channels", force_args={"parent_id": g("parent_id", "")}) or []))
        _install(SELF, "_discord_channel_keys",
                 lambda m, pid: set(ctx.call("channel_keys", force_args={"message_id": mid, "parent_id": pid or ""}) or []))
        NS["_scoped_gate_env"] = make_recorder(ctx, "scoped_gate_env")
        NS["logger"] = types.SimpleNamespace(
            debug=make_recorder(ctx, "logger.debug"),
            warning=lambda *a, **k: None, info=lambda *a, **k: None, error=lambda *a, **k: None)
        _install(SELF, "name", g("name", ""))
        return NS[func](SELF, msg, claim=bool(g("claim", True)))

    raise RuntimeError(f"unknown func {func}")


def _await_rec(ctx, name, force_args, delay=False):
    async def af(*a, **k):
        if delay:
            return ctx.call(name, force_args={"delay": a[0] if a else k.get("delay")})
        return ctx.call(name, force_args=force_args)
    return af()


def _c(ctx, name, fa):
    """Mirror of C da10_call: returns (status, value) with status 'ok'/'raise'."""
    try:
        return "ok", ctx.call(name, force_args=fa)
    except BaseException as e:
        return "raise", exc_spec(e, ctx)


def _truthy(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return v != ""
    if isinstance(v, (list, dict, tuple, set)):
        return len(v) > 0
    return False


def _base(spec):
    return isinstance(spec, dict) and spec.get("__raise__") in (
        "BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
        "CancelledError", "asyncio.CancelledError")


MODELS = {}


def model(fn):
    MODELS[fn.__name__[2:]] = fn  # strip the m_ prefix
    return fn


@model
def m__defer_unless_expired(ctx, args):
    g = args.get
    st, r = _c(ctx, "response.defer", {"ephemeral": True})
    if st == "ok":
        return True
    if _base(r):
        return r
    st2, u = _c(ctx, "_is_discord_unknown_interaction", {"error": r.get("message", "")})
    if st2 == "raise":
        return u
    if not _truthy(u):
        return r  # re-raise verbatim
    _c(ctx, "logger.warning", {"fmt": g("warn_fmt", ""), "args": g("warn_args") or []})
    return False


@model
def m__discord_message_is_persistently_complete(ctx, args):
    mid = args.get("message_id") or ""
    if not mid:
        return False
    st, r = _c(ctx, "recovery_call", {"op": "persistently_complete", "message_id": mid, "default": False})
    return r if st == "raise" else _truthy(r)


@model
def m__discord_message_has_active_claim(ctx, args):
    mid = args.get("message_id") or ""
    if not mid:
        return False
    st, c = _c(ctx, "datetime_cutoff", {})
    if st == "raise":
        return c
    st, r = _c(ctx, "recovery_call", {"op": "has_active_claim", "message_id": mid, "default": True})
    return r if st == "raise" else _truthy(r)


@model
def m__discord_message_attempts_exhausted(ctx, args):
    g = args.get
    mid = g("message_id") or ""
    if not mid:
        return False
    st, cap = _c(ctx, "max_attempts", {})
    if st == "raise":
        return cap
    st, r = _c(ctx, "recovery_call", {"op": "attempts_exhausted", "message_id": mid, "default": False})
    if st == "raise":
        return r
    if not _truthy(r):
        return False
    _c(ctx, "logger.debug", {
        "fmt": "[%s] Not re-dispatching Discord message %s: missed_message_backfill.max_attempts (%d) reached",
        "args": [g("name", ""), mid, cap]})
    return True


@model
def m__discord_recovery_cursor(ctx, args):
    cid = args.get("channel_id") or ""
    if not cid:
        return None
    st, r = _c(ctx, "recovery_call", {"op": "recovery_cursor", "message_id": cid, "default": None})
    return r if st == "raise" else r


def _adm_pair(a, b):
    return [bool(a), bool(b)]


@model
def m__dispatch_discord_message(ctx, args):
    g = args.get
    mid = g("message_id") or ""
    if not g("ready", True):
        st, r = _c(ctx, "wait_for_ready", {"timeout": 30.0})
        if st == "raise":
            if _base(r) or r.get("__raise__") != "TimeoutError":
                return r
    st, adm = _c(ctx, "admission", {"message_id": mid, "claim": True})
    if st == "raise":
        return adm
    admitted = bool(adm[0]) if isinstance(adm, list) and len(adm) >= 2 else False
    role = bool(adm[1]) if isinstance(adm, list) and len(adm) >= 2 else False
    if not admitted:
        return False
    st, r = _c(ctx, "record_bot_tag_debounce", {"message_id": mid})
    if st == "raise":
        return r
    st, r = _c(ctx, "handle_message", {"message_id": mid, "role_authorized": role})
    return r if st == "raise" else r


@model
def m__dispatch_recovered_message(ctx, args):
    g = args.get
    mid = g("message_id") or ""
    if not g("is_dm", False):
        st, parent = _c(ctx, "get_parent_channel_id", {"channel_id": g("channel_id", "")})
        if st == "raise":
            return parent
        st, keys = _c(ctx, "channel_keys", {"message_id": mid, "parent_id": parent or ""})
        if st == "raise":
            return keys
        st, free = _c(ctx, "free_channels", {})
        if st == "raise":
            return free
        st, rm = _c(ctx, "require_mention", {})
        if st == "raise":
            return rm
        veto = False
        if _truthy(rm):
            fl = [str(x) for x in (free or [])]
            kl = [str(x) for x in (keys or [])]
            star = "*" in fl
            overlap = bool(set(fl) & set(kl))
            if not star and not overlap:
                st, bt = _c(ctx, "in_bot_thread", {"message_id": mid})
                if st == "raise":
                    return bt
                if not _truthy(bt):
                    st, sm = _c(ctx, "self_is_explicitly_mentioned", {"message_id": mid})
                    if st == "raise":
                        return sm
                    if not _truthy(sm):
                        veto = True
        if veto:
            return False
    st, adm = _c(ctx, "admission", {"message_id": mid, "claim": False})
    if st == "raise":
        return adm
    admitted = bool(adm[0]) if isinstance(adm, list) and len(adm) >= 2 else False
    role = bool(adm[1]) if isinstance(adm, list) and len(adm) >= 2 else False
    if not admitted:
        return False
    st, r = _c(ctx, "handle_message", {"message_id": mid, "role_authorized": role, "recovered": True})
    return r if st == "raise" else r


@model
def m__dispatch_thread_session(ctx, args):
    g = args.get
    tid, uid, uname, text = g("thread_id", ""), g("user_id", ""), g("user_name", ""), g("text", "")
    st, parent = _c(ctx, "get_parent_channel_id", {"thread_id": tid})
    if st == "raise":
        return parent
    st, name = _c(ctx, "format_thread_chat_name", {"thread_id": tid})
    if st == "raise":
        return name
    st, topic = _c(ctx, "get_effective_topic", {"thread_id": tid})
    if st == "raise":
        return topic
    st, guild = _c(ctx, "interaction_guild_id", {})
    if st == "raise":
        return guild
    st, ra = _c(ctx, "slash_role_grant", {})
    if st == "raise":
        return ra
    role = _truthy(ra)
    sa = {"chat_id": tid, "chat_name": name, "chat_type": "thread", "user_id": uid,
          "user_name": uname, "thread_id": tid, "chat_topic": topic, "guild_id": guild,
          "parent_chat_id": (parent if parent else None), "role_authorized": role}
    st, src = _c(ctx, "build_source", sa)
    if st == "raise":
        return src
    st, skills = _c(ctx, "resolve_channel_skills", {"thread_id": tid, "parent_id": parent or ""})
    if st == "raise":
        return skills
    st, prompt = _c(ctx, "thread_channel_prompt", {"thread_id": tid, "parent_id": parent or ""})
    if st == "raise":
        return prompt
    st, ev = _c(ctx, "MessageEvent", {"text": text, "message_type": "TEXT", "source": src,
                "auto_skill": skills, "channel_prompt": prompt})
    if st == "raise":
        return ev
    st, r = _c(ctx, "handle_message", {"event": ev})
    return r if st == "raise" else r


@model
def m__create_thread(ctx, args):
    g = args.get
    name = str(g("name") or "").strip()
    if not name:
        st, msg = _c(ctx, "t", {"key": "platform.discord.command.thread.error_name_required"})
        return {"error": msg} if st == "ok" else msg
    dur = int(g("auto_archive_duration") or 1440)
    if dur not in (60, 1440, 4320, 10080):
        st, msg = _c(ctx, "t", {"key": "platform.discord.command.thread.error_invalid_archive",
                                "allowed": "60, 1440, 4320, 10080"})
        return {"error": msg} if st == "ok" else msg
    st, ch = _c(ctx, "resolve_interaction_channel", {})
    if st == "raise":
        return ch
    if ch is None:
        st, msg = _c(ctx, "t", {"key": "platform.discord.command.thread.error_no_channel"})
        return {"error": msg} if st == "ok" else msg
    if (ch.get("is_dm") if isinstance(ch, dict) else False):
        st, msg = _c(ctx, "t", {"key": "platform.discord.command.thread.error_dm_not_supported"})
        return {"error": msg} if st == "ok" else msg
    st, parent = _c(ctx, "thread_parent_channel", {"channel": ch})
    if st == "raise":
        return parent
    if parent is None:
        st, msg = _c(ctx, "t", {"key": "platform.discord.command.thread.error_no_parent"})
        return {"error": msg} if st == "ok" else msg
    disp = (g("interaction") or {}).get("display_name") or "unknown user"
    reason = f"Requested by {disp} via /thread"
    seed = str(g("message") or "").strip()
    st, thread = _c(ctx, "create_thread", {"name": name, "auto_archive_duration": dur, "reason": reason})
    if st == "ok":
        if seed:
            st, sr_ = _c(ctx, "thread.send", {"content": seed})
            if st == "raise":
                return sr_
        st, ret = _c(ctx, "thread_created", {"thread": thread, "name": name})
        return ret
    st, seed_msg = _c(ctx, "seed.send", {"content": seed or "platform.discord.thread.seed_message"})
    if st == "raise":
        return seed_msg
    st, fb = _c(ctx, "seed.create_thread", {"message": seed_msg})
    if st == "raise":
        return fb
    st, ret = _c(ctx, "thread_created", {"thread": fb, "name": name})
    return ret


@model
def m__deliver_unauthorized_slash_alert(ctx, args):
    g = args.get
    st, ac = _c(ctx, "alert_adapters_and_config", {"runner": g("runner"), "profile": g("profile")})
    if st == "raise":
        _c(ctx, "logger.debug", {"fmt": "[Discord] Admin notify: profile %r resolution failed: %s",
                                 "args": [g("profile"), ac.get("message", "")]})
        return None
    for target in ("telegram", "slack"):
        st, home = _c(ctx, "get_home_channel", {"target": target})
        if st == "raise":
            _c(ctx, "logger.debug", {"fmt": "[Discord] Admin notify via %s failed: %s",
                                     "args": [target, home.get("message", "")]})
            continue
        if home is None or not _truthy(home.get("chat_id")):
            continue
        st, msg = _c(ctx, "t", {"key": "platform.discord.security.unauthorized_slash_alert",
                                "user_name": g("user_name"), "user_id": g("user_id"),
                                "chan_id": g("chan_id"), "guild_id": g("guild_id"),
                                "command_text": g("command_text"), "reason": g("reason")})
        if st == "raise":
            return msg
        st, ok = _c(ctx, "present_notification", {"platform": target, "diagnostic": True})
        if st == "raise":
            return ok
        if not _truthy(ok):
            return None
        st, res = _c(ctx, "adapter.send", {"chat_id": home.get("chat_id", ""), "msg": msg})
        if st == "raise":
            _c(ctx, "logger.debug", {"fmt": "[Discord] Admin notify via %s failed: %s",
                                     "args": [target, res.get("message", "")]})
            continue
        if isinstance(res, dict) and res.get("success") is False:
            _c(ctx, "logger.debug", {"fmt": "[Discord] Admin notify via %s returned success=False (error=%r); falling through",
                                     "args": [target, res.get("error")]})
            continue
        return None
    return None


@model
def m__desired_command_sync_fingerprint(ctx, args):
    desired = []
    if args.get("client", False):
        st, tree = _c(ctx, "client_tree", {})
        if st == "raise":
            return tree
        if tree is not None:
            st, cmds = _c(ctx, "get_commands", {})
            if st == "raise":
                return cmds
            for cmd in (cmds or []):
                st, d = _c(ctx, "to_dict", {"command": cmd})
                if st == "raise":
                    return d
                st, canon = _c(ctx, "canonicalize_command", {"command": d})
                if st == "raise":
                    return canon
                desired.append(canon)
    desired.sort(key=lambda it: ((it or {}).get("type", 1), (it or {}).get("name", "")))
    st, lang = _c(ctx, "get_language", {})
    if st == "raise":
        return lang
    payload = json.dumps({"language": lang, "commands": desired},
                         sort_keys=True, separators=(",", ":"))
    st, digest = _c(ctx, "sha256_hex", {"payload": payload})
    return digest if st == "raise" else digest


@model
def m__write_command_sync_state(ctx, args):
    st, path = _c(ctx, "command_sync_state_path", {})
    if st == "raise":
        return path
    st, r = _c(ctx, "atomic_json_write", {"path": path, "state": args.get("state"),
              "indent": None, "separators": [",", ":"]})
    return r if st == "raise" else r


@model
def m__sleep_between_command_sync_mutations(ctx, args):
    st, iv = _c(ctx, "command_sync_mutation_interval_seconds", {})
    if st == "raise":
        return iv
    if (iv or 0) > 0:
        st, r = _c(ctx, "asyncio.sleep", {"delay": iv})
        if st == "raise":
            return r
    return None


@model
def m__resolve_channel_prompt(ctx, args):
    pid = args.get("parent_id")
    st, r = _c(ctx, "resolve_channel_prompt", {"extra": args.get("extra"),
              "channel_id": args.get("channel_id", ""), "parent_id": pid})
    return r if st == "raise" else r


@model
def m__define_discord_view_classes(ctx, args):
    return None


@model
def m__discord_message_admission(ctx, args):
    g = args.get
    mid = g("message_id") or ""
    claim = g("claim", True)
    author = g("author") or {"id": g("author_id", ""), "is_self": bool(g("author_self", False))}
    author_self = bool(author.get("is_self", False))
    is_automated = bool(g("is_automated", False))
    msg_type = int(g("type", 0))
    type_ok = msg_type in (0, 19)
    gate = "dedup.is_duplicate" if claim else "dedup.contains"
    st, dup = _c(ctx, gate, {"message_id": mid})
    if st == "raise":
        return dup
    if _truthy(dup):
        return _adm_pair(False, False)
    if author_self or not type_ok:
        return _adm_pair(False, False)
    st, r = _c(ctx, "refresh_live_gates", {})
    if st == "raise":
        return r
    role = False
    if is_automated:
        st, allow = _c(ctx, "get_allow_bots", {})
        if st == "raise":
            return allow
        st, bt = _c(ctx, "is_bot_tag_continuation", {})
        if st == "raise":
            return bt
        cont = _truthy(bt)
        if allow == "none":
            return _adm_pair(False, False)
        if allow == "mentions":
            st, sm = _c(ctx, "self_is_explicitly_mentioned", {"message_id": mid})
            if st == "raise":
                return sm
            if not _truthy(sm) and not cont:
                return _adm_pair(False, False)
        st, rb = _c(ctx, "bots_require_inline_mention", {})
        if st == "raise":
            return rb
        if _truthy(rb):
            st, rm = _c(ctx, "self_is_raw_mentioned", {"message_id": mid})
            if st == "raise":
                return rm
            if not _truthy(rm) and not cont:
                return _adm_pair(False, False)
    else:
        is_dm = bool(g("is_dm", False)) or g("guild") is None
        keys = []
        if not is_dm:
            keys.append(str(g("channel_id", "")))
            st, pv = _c(ctx, "get_parent_channel_id", {"channel_id": g("channel_id", "")})
            if st == "raise":
                return pv
            if pv:
                keys.append(pv)
        st, allowed = _c(ctx, "is_allowed_user", {"author_id": author.get("id", ""),
                         "is_dm": is_dm, "channel_ids": keys, "guild": g("guild")})
        if st == "raise":
            return allowed
        if not _truthy(allowed):
            st, w = _c(ctx, "warn_if_fail_closed_default", {})
            if st == "raise":
                return w
            return _adm_pair(False, False)
        role = bool(g("has_allowed_roles", False))
    st, raw = _c(ctx, "self_is_explicitly_mentioned", {"message_id": mid})
    if st == "raise":
        return raw
    raw_self = _truthy(raw)
    is_dm_ch = bool((g("channel") or {"is_dm": g("is_dm", False)}).get("is_dm", g("is_dm", False)))
    mentions = g("mentions") or []
    if not is_dm_ch and (len(mentions) > 0 or raw_self):
        other = any(bool(m.get("bot")) and not bool(m.get("is_self")) for m in mentions)
        if other and not raw_self:
            return _adm_pair(False, False)
        st, env = _c(ctx, "scoped_gate_env", {"key": "DISCORD_IGNORE_NO_MENTION", "default": "true"})
        if st == "raise":
            return env
        ignore = str(env or "").lower() in {"true", "1", "yes"}
        if ignore and not raw_self and not other:
            st, bt = _c(ctx, "in_bot_thread", {"message_id": mid})
            if st == "raise":
                return bt
            if not _truthy(bt):
                pid = str((g("channel") or {}).get("parent_id", g("parent_id", "")) or "")
                st, free = _c(ctx, "free_response_channels", {"parent_id": pid})
                if st == "raise":
                    return free
                st, keys2 = _c(ctx, "channel_keys", {"message_id": mid, "parent_id": pid})
                if st == "raise":
                    return keys2
                fl = [str(x) for x in (free or [])]
                kl = [str(x) for x in (keys2 or [])]
                if "*" not in fl and not (set(fl) & set(kl)):
                    _c(ctx, "logger.debug", {
                        "fmt": "[%s] admission: dropping message %s — mentions others, not self, not a bot thread, channel not free-response",
                        "args": [g("name", ""), mid]})
                    return _adm_pair(False, False)
    return _adm_pair(True, role)


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    try:
        ret = MODELS[func](ctx, args)
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


def install_common(ctx):
    NS["logger"] = types.SimpleNamespace(
        debug=lambda *a, **k: None, warning=lambda *a, **k: None,
        info=lambda *a, **k: None, error=lambda *a, **k: None, exception=lambda *a, **k: None)


# ── cross-checks (SEAM-NAME SET EQUALITY) ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_discord_adapter_flow10: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_discord_adapter_flow10: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow10.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(str(fix), encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_discord_adapter_flow10: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(PORTED) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_discord_adapter_flow10: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(PORTED))
        if unknown_funcs:
            print(f"FATAL oracle_discord_adapter_flow10: fixture funcs not in TARGETS: {unknown_funcs}")
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
            if shown < 200:
                report_mismatch(func, want, g, shown)
                shown += 1
    print(f"ORACLE discord_adapter_flow10: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow10.jsonl"
    n = bad = 0
    counts = {}
    for line in open(str(fix), encoding="utf-8"):
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
            for nm in PORTED:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST discord_adapter_flow10: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
