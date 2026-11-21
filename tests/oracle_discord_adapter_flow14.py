#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW part 14 port
(src/gateway/platforms/discord_adapter_flow14.c).

Drives the LIVE plugins/platforms/discord/adapter.py defs with scripted seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo. Fail-closed: missing defs or 0 cases => exit 2.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import enum
import json
import math
import os
import re
import subprocess
import sys
import textwrap
import types
from pathlib import Path
from types import SimpleNamespace
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "discord", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

# Every target below is an instance method of DiscordAdapter.
KINDS = {
    "_record_command_sync_attempt": "instance",
    "_record_command_sync_success": "instance",
    "_missed_message_backfill_enabled": "instance",
    "_record_discord_message_seen": "instance",
    "_record_recovery_attempt": "instance",
    "_record_discord_processing_start": "instance",
    "_record_discord_processing_complete": "instance",
    "_record_response_async": "instance",
    "_record_discord_response": "instance",
    "_record_recovery_scan_start": "instance",
    "_record_recovery_scan_complete": "instance",
    "_refresh_skill_catalog_state": "instance",
    "_register_skill_group": "instance",
    "_register_slash_commands": "instance",
    "_register_thread_slash": "instance",
    "_reject_slash": "instance",
}
ORDER = list(KINDS)

# EXACTLY the C da14_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "backfill_enabled", "utc_now_iso", "message_channel_ids", "record_discord_message_seen",
    "with_recovery_db", "conn_execute", "conn_fetchone",
    "advance_recovery_cursor", "time_time", "os_getpid", "json_dumps",
    "backfill_window", "backfill_limit",
    "update_command_sync_entry",
    "native_slash_commands", "tree_get_commands", "tree_command",
    "tree_add_command", "app_command_ctor", "skill_commands_by_category",
    "slash_proxy", "resolve_config_gates", "command_registry",
    "is_gateway_available", "iter_plugin_command_entries",
    "scoped_gate_env", "apply_owner_only_visibility",
    "register_thread_slash", "register_skill_group",
    "refresh_skill_catalog_state",
    "logger_info", "logger_debug", "logger_warning",
    "t_discord", "truncate_text", "t", "unauthorized",
    "response_send_message", "notify_unauthorized_slash", "create_task",
    "to_thread", "record_discord_response",
]

DISCORD_TEXT_LIMIT = 100
MAX_APP_COMMANDS = 100


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


_MOD = types.ModuleType("da14_oracle_adapter")
NS = _MOD.__dict__
NS["__builtins__"] = builtins
sys.modules["da14_oracle_adapter"] = _MOD


def extract():
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    classes = {c.name: c for c in tree.body if isinstance(c, ast.ClassDef)}
    cls = classes.get("DiscordAdapter")
    nodes = {}
    if cls:
        for n in cls.body:
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in KINDS:
                nodes[n.name] = n
    missing = [k for k in KINDS if k not in nodes]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow14: missing defs: {missing}")
        sys.exit(2)
    return nodes, src


_NODES, _SRC = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        tn = type(x).__name__
        if tn in ("_Tree", "_Cmd", "_CmdDef", "_Choice", "_Interaction", "_Client",
                  "_StubTree", "_Resp"):
            return conv(getattr(x, "_proj", lambda: {})())
        if tn in ("_Stamp",):
            return {"__stamp__": x._iso}
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items() if not callable(val)}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, (bytes, bytearray)):
            return bytes(x).hex()
        if callable(x):
            return "<callable>"
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def _proj(v):
    return canon(v)


# ── per-case script backend (mirrors the C da14_call exactly) ──
_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io, state):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.st = state if isinstance(state, dict) else {}
        self.inst = None

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
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
                spec = used["raise"]
                kind = spec.get("type", "RuntimeError")
                cls = asyncio.CancelledError if kind == "CancelledError" else getattr(builtins, kind, None)
                if not (isinstance(cls, type) and issubclass(cls, BaseException)):
                    cls = type(kind, (Exception,), {})
                self._record_raise(name, args, cls(spec.get("message", "")))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


class _NoBind:
    def __init__(self, fn):
        self._fn = fn

    def __call__(self, *a, **k):
        return self._fn(*a, **k)


def rec(name, args_fn, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def arec(name, args_fn, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


# ── object doubles ──
class _Stamp:
    def __init__(self, iso):
        self._iso = iso

    def isoformat(self):
        return self._iso

    def _proj(self):
        return {"__stamp__": self._iso}


class _Conn:
    def __init__(self, st):
        self._st = st

    def execute(self, sql, params=()):
        _CUR.call("conn_execute", {"sql": " ".join(sql.split()),
                                   "params": [_proj(p) for p in params]})
        return _Cursor(self._st)


class _Cursor:
    def __init__(self, st):
        self._st = st

    def fetchone(self):
        return _CUR.call("conn_fetchone", {})


class _Cmd:
    def __init__(self, name):
        self.name = name

    def _proj(self):
        return {"name": self.name}


class _CmdDef:
    def __init__(self, name, description, args_hint):
        self.name = name
        self.description = description
        self.args_hint = args_hint

    def _proj(self):
        return {"name": self.name, "description": self.description, "args_hint": self.args_hint}


class _Tree:
    def __init__(self, st):
        self.get_commands = st["tree_get_commands"]
        self.command = st["tree_command"]
        self.add_command = st["tree_add_command"]

    def _proj(self):
        return "<tree>"


class _Interaction:
    def __init__(self, st, d):
        u = d.get("user")
        self.user = SimpleNamespace(id=u.get("id", "?"), name=u.get("name", "?")) if isinstance(u, dict) else None
        self.channel_id = d.get("channel_id")
        ch = d.get("channel")
        self.channel = SimpleNamespace(id=ch.get("id")) if isinstance(ch, dict) else None
        self.guild_id = d.get("guild_id")
        self.response = SimpleNamespace(send_message=st["response_send_message"])

    def _proj(self):
        return {"user": _proj(self.user), "channel_id": _proj(self.channel_id),
                "guild_id": _proj(self.guild_id)}


class ProcessingOutcome(enum.Enum):
    SUCCESS = "success"
    CANCELLED = "cancelled"
    FAILED = "failed"


OUTCOME = {"success": ProcessingOutcome.SUCCESS, "cancelled": ProcessingOutcome.CANCELLED,
           "failed": ProcessingOutcome.FAILED, "other": ProcessingOutcome.FAILED}


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def msg_double(v):
    d = copy.deepcopy(v) if isinstance(v, dict) else {}
    ca = d.get("created_at")
    if isinstance(ca, dict) and "__stamp__" in ca:
        d["created_at"] = _Stamp(ca["__stamp__"])
    d = {k: ns_double(x) for k, x in d.items()}
    return SimpleNamespace(**d)


def _func_marker(func):
    return getattr(func, "__name__", "") or "<func>"


def _cb_marker(cb):
    n = getattr(cb, "__name__", "")
    if n in ("_skill_handler", "_autocomplete_name", "slash_thread"):
        return n
    if hasattr(cb, "__self__") and type(cb.__self__).__name__ == "set":
        return "discard"
    return "<callback>"


def identity_deco(f):
    return f


# ── seam stubs ──
def build_seams():
    st = {}

    st["backfill_enabled"] = rec("backfill_enabled", lambda: {})
    st["record_discord_message_seen"] = rec(
        "record_discord_message_seen",
        lambda message, status: {"message": _proj(message), "status": status})
    st["utc_now_iso"] = rec("utc_now_iso", lambda: {})

    def _mci(message):
        return _CUR.call("message_channel_ids", {"message": _proj(message)})
    st["message_channel_ids"] = _mci

    def _with_db(op):
        _CUR.call("with_recovery_db", {})
        return op(_Conn(st))
    st["with_recovery_db"] = _with_db
    st["conn_fetchone"] = rec("conn_fetchone", lambda: {})
    st["advance_recovery_cursor"] = rec(
        "advance_recovery_cursor",
        lambda channel_id, reply_to: {"channel_id": channel_id, "reply_to": reply_to})

    st["time_time"] = rec("time_time", lambda: {})
    st["os_getpid"] = rec("os_getpid", lambda: {})
    st["json_dumps"] = rec("json_dumps", lambda value: {"value": value})
    st["backfill_window"] = rec("backfill_window", lambda: {})
    st["backfill_limit"] = rec("backfill_limit", lambda: {})

    def _update_ces(app_id, fingerprint, *, keep_existing=False, drop=None, fields=None):
        vals = fields(None) if fields is not None else None
        return _CUR.call("update_command_sync_entry", {
            "app_id": _proj(app_id), "fingerprint": fingerprint,
            "keep_existing": keep_existing,
            "drop": list(drop) if drop else None,
            "fields": canon(vals) if vals is not None else None})
    st["update_command_sync_entry"] = _update_ces

    st["native_slash_commands"] = rec(
        "native_slash_commands", lambda: {},
        build=lambda raw: [list(x) for x in (raw or [])])
    st["tree_get_commands"] = rec(
        "tree_get_commands", lambda: {},
        build=lambda raw: [_Cmd(x) for x in (raw or [])])
    st["tree_command"] = rec(
        "tree_command", lambda name, description: {"name": name, "description": description},
        build=lambda raw: identity_deco)
    st["tree_add_command"] = rec(
        "tree_add_command", lambda command: {"command": _proj(command)})

    def _cmd_ctor(*, name, description, callback):
        _CUR.call("app_command_ctor", {"name": name, "description": description,
                                       "callback": _cb_marker(callback)})
        return _Cmd(name)
    st["app_command_ctor"] = _cmd_ctor

    st["skill_commands_by_category"] = rec(
        "skill_commands_by_category", lambda reserved_names: {"reserved_names": sorted(reserved_names)},
        build=lambda raw: raw)
    st["slash_proxy"] = rec(
        "slash_proxy",
        lambda name, args, template, followup, *, strip=True, prefix="": {
            "name": name, "args": _proj(args), "template": template,
            "followup": _proj(followup), "strip": strip, "prefix": prefix})
    st["resolve_config_gates"] = rec("resolve_config_gates", lambda: {})
    st["command_registry"] = rec(
        "command_registry", lambda: {},
        build=lambda raw: [_CmdDef(x[0], x[1], x[2]) for x in (raw or [])])
    st["is_gateway_available"] = rec(
        "is_gateway_available",
        lambda cmd_def, config_overrides: {"cmd_def": _proj(cmd_def),
                                           "config_overrides": _proj(config_overrides)})
    st["iter_plugin_command_entries"] = rec(
        "iter_plugin_command_entries", lambda: {},
        build=lambda raw: [list(x) for x in (raw or [])])
    st["scoped_gate_env"] = rec(
        "scoped_gate_env", lambda key, default: {"key": key, "default": default})
    st["apply_owner_only_visibility"] = rec(
        "apply_owner_only_visibility", lambda tree: {"tree": _proj(tree)})

    st["register_thread_slash"] = rec(
        "register_thread_slash", lambda tree, name, description: {
            "tree": _proj(tree), "name": name, "description": description})
    st["register_skill_group"] = rec(
        "register_skill_group", lambda tree: {"tree": _proj(tree)})

    def _refresh_stub():
        raw = _CUR.call("refresh_skill_catalog_state", {})
        if isinstance(raw, dict) and "entries" in raw:
            _CUR.inst._skill_entries = raw["entries"]
            _CUR.inst._skill_group_hidden_count = raw.get("hidden", 0)
        return None
    st["refresh_skill_catalog_state"] = _refresh_stub

    st["logger_info"] = rec("logger_info", lambda *a: {"args": [_proj(x) for x in a]})
    st["logger_debug"] = rec("logger_debug", lambda *a: {"args": [_proj(x) for x in a]})
    st["logger_warning"] = rec(
        "logger_warning", lambda *a, **k: {"args": [_proj(x) for x in a],
                                           "kwargs": {kk: _proj(vv) for kk, vv in k.items()}})

    def _td(key, limit, **kw):
        raw = _CUR.call("t_discord", {"key": key, "limit": limit, **{k: _proj(v) for k, v in kw.items()}})
        return raw if isinstance(raw, str) else key
    st["t_discord"] = _td

    def _trunc(text, limit):
        raw = _CUR.call("truncate_text", {"text": text, "limit": limit})
        return raw if isinstance(raw, str) else text
    st["truncate_text"] = _trunc

    def _t(key, **kw):
        raw = _CUR.call("t", {"key": key, **{k: _proj(v) for k, v in kw.items()}})
        return raw if isinstance(raw, str) else key
    st["t"] = _t

    st["unauthorized"] = rec("unauthorized", lambda: {}, build=lambda raw: raw if isinstance(raw, str) else "unauthorized")
    st["response_send_message"] = arec(
        "response_send_message", lambda message, ephemeral=False: {"message": message, "ephemeral": ephemeral})

    def _notify(user_name, user_id, chan_id, guild_id, command_text, reason):
        return _CUR.call("notify_unauthorized_slash", {
            "user_name": user_name, "user_id": user_id, "chan_id": _proj(chan_id),
            "guild_id": _proj(guild_id), "command_text": command_text, "reason": reason})
    st["notify_unauthorized_slash"] = _notify

    def _create_task(coro):
        return _CUR.call("create_task", {})
    st["create_task"] = _create_task

    async def _to_thread(func, *a, **k):
        raw = _CUR.call("to_thread", {"func": _func_marker(func),
                                      "args": [_proj(x) for x in a],
                                      "kwargs": {kk: _proj(vv) for kk, vv in k.items()}})
        return func(*a, **k)
    st["to_thread"] = _to_thread

    def _rdr(*a, **k):
        return _CUR.call("record_discord_response", {
            "reply_to": _proj(k.get("reply_to")), "result": _proj(k.get("result")),
            "content": k.get("content"), "final": k.get("final")})
    _rdr.__name__ = "_record_discord_response"
    st["record_discord_response"] = _rdr
    return st


# ── synthetic class hosting the ported instance methods ──
class BaseDouble:
    pass


NS["BaseDouble"] = BaseDouble
_syn = ast.ClassDef(name="DA14Double", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
                    keywords=[], body=[_NODES[n] for n in ORDER], decorator_list=[])
ast.fix_missing_locations(_syn)
_mod_node = ast.Module(body=[_syn], type_ignores=[])
ast.fix_missing_locations(_mod_node)
exec(compile(_mod_node, A_SRC, "exec", flags=_FLAGS, dont_inherit=True), NS)
DA14Double = NS["DA14Double"]
PORTED = {n: DA14Double.__dict__[n] for n in ORDER}
ORIG = dict(PORTED)

# Methods that are themselves ported but act as nested collaborators elsewhere.
NESTED_STUBS = {
    "_missed_message_backfill_enabled": "backfill_enabled",
    "_record_discord_message_seen": "record_discord_message_seen",
    "_record_discord_response": "record_discord_response",
    "_refresh_skill_catalog_state": "refresh_skill_catalog_state",
    "_register_thread_slash": "register_thread_slash",
    "_register_skill_group": "register_skill_group",
}
# extra nested recorder used by _record_recovery_attempt / _record_discord_processing_start
EXTRA_SEAMS = {"record_discord_message_seen"}


def seed(st, cur_func):
    NS["logger"] = SimpleNamespace(info=st["logger_info"], debug=st["logger_debug"],
                                   warning=st["logger_warning"],
                                   error=rec("logger_error", lambda *a: {"args": [_proj(x) for x in a]}))
    NS["time"] = SimpleNamespace(time=st["time_time"], perf_counter=rec("pc", lambda: {}))
    NS["os"] = SimpleNamespace(getpid=st["os_getpid"])
    NS["json"] = SimpleNamespace(dumps=st["json_dumps"], loads=rec("json_loads", lambda s: {"s": s}))
    NS["asyncio"] = SimpleNamespace(to_thread=st["to_thread"], create_task=st["create_task"])
    NS["math"] = math
    NS["contextlib"] = contextlib
    NS["enum"] = enum
    NS["_scoped_gate_env"] = st["scoped_gate_env"]
    NS["_DISCORD_APP_COMMAND_TEXT_LIMIT"] = DISCORD_TEXT_LIMIT
    NS["_DISCORD_MAX_APP_COMMANDS"] = MAX_APP_COMMANDS
    NS["_t_discord"] = st["t_discord"]
    NS["_truncate_discord_component_text"] = st["truncate_text"]
    NS["t"] = st["t"]
    NS["_unauthorized"] = st["unauthorized"]
    NS["_native_slash_commands"] = st["native_slash_commands"]
    NS["ProcessingOutcome"] = ProcessingOutcome
    NS["SendResult"] = object
    NS["MessageEvent"] = object
    NS["Optional"] = object
    NS["Any"] = object

    # nested method stubs (unless this function is the one being driven)
    for m in NESTED_STUBS:
        setattr(DA14Double, m, ORIG[m])  # restore before this case's overrides
    for m, seam in NESTED_STUBS.items():
        if m != cur_func:
            setattr(DA14Double, m, _NoBind(st[seam]))

    # extra stubs not on DA14Double
    DA14Double._update_command_sync_entry = _NoBind(st["update_command_sync_entry"])
    DA14Double._message_channel_ids = _NoBind(st["message_channel_ids"])
    DA14Double._utc_now_iso = _NoBind(st["utc_now_iso"])
    DA14Double._with_discord_recovery_db = _NoBind(st["with_recovery_db"])
    DA14Double._advance_discord_recovery_cursor = _NoBind(st["advance_recovery_cursor"])
    DA14Double._missed_message_backfill_window_seconds = _NoBind(st["backfill_window"])
    DA14Double._missed_message_backfill_limit = _NoBind(st["backfill_limit"])
    DA14Double._slash_proxy = _NoBind(st["slash_proxy"])
    DA14Double._apply_owner_only_visibility = _NoBind(st["apply_owner_only_visibility"])
    DA14Double._notify_unauthorized_slash = _NoBind(st["notify_unauthorized_slash"])

    # discord.app_commands / discord.Interaction doubles
    appc = SimpleNamespace(
        Command=_cmd_ctor_ref(st),
        describe=lambda **kw: identity_deco,
        autocomplete=lambda **kw: identity_deco,
        Choice=lambda **kw: SimpleNamespace(**kw))
    dmod = SimpleNamespace(app_commands=appc, Interaction=object)
    NS["discord"] = dmod
    sys.modules["discord"] = dmod

    # hermes_cli.commands{,_platforms} in-function imports
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    sys.modules["hermes_cli"] = hc
    hcc = types.ModuleType("hermes_cli.commands")
    hcc.COMMAND_REGISTRY = _LazyRegistry()
    hcc._is_gateway_available = st["is_gateway_available"]
    hcc._resolve_config_gates = st["resolve_config_gates"]
    hcc._iter_plugin_command_entries = st["iter_plugin_command_entries"]
    sys.modules["hermes_cli.commands"] = hcc
    hcp = types.ModuleType("hermes_cli.commands_platforms")
    hcp.discord_skill_commands_by_category = st["skill_commands_by_category"]
    sys.modules["hermes_cli.commands_platforms"] = hcp


class _LazyRegistry:
    """Fires the command_registry seam only when _register_slash_commands iterates it."""
    def __iter__(self):
        raw = _CUR.call("command_registry", {})
        return iter([_CmdDef(x[0], x[1], x[2]) for x in (raw or [])])


def _cmd_ctor_ref(st):
    def ctor(**kw):
        return st["app_command_ctor"](**kw)
    return ctor


def make_self(state, st):
    inst = DA14Double()
    for k, v in (state or {}).items():
        vv = copy.deepcopy(v)
        setattr(inst, k, vv)
    return inst


def run_call(fn, *a, **kw):
    r = fn(*a, **kw)
    if asyncio.iscoroutine(r):
        return asyncio.run(r)
    return r


def drive(func, inp):
    global _CUR
    args = copy.deepcopy(inp.get("args") or {})
    state = copy.deepcopy(inp.get("state") or {})
    ctx = Ctx(inp.get("io"), state)
    _CUR = ctx
    st = build_seams()
    seed(st, func)
    inst = make_self(state, st)
    ctx.inst = inst
    fn = PORTED[func]

    try:
        if func == "_record_command_sync_attempt":
            ret = fn(inst, args.get("app_id"), args.get("fingerprint", ""))

        elif func == "_record_command_sync_success":
            ret = fn(inst, args.get("app_id"), args.get("fingerprint", ""),
                     args.get("summary", {}))

        elif func == "_missed_message_backfill_enabled":
            inst.config = SimpleNamespace(extra={"missed_message_backfill": args.get("config_extra")})
            ret = fn(inst)

        elif func in ("_record_discord_message_seen", "_record_recovery_attempt"):
            msg = msg_double(args.get("message"))
            if func == "_record_discord_message_seen":
                ret = fn(inst, msg, status=args.get("status", "discovered"))
            else:
                ret = fn(inst, msg, status=args.get("status", "queued"),
                         error=args.get("error"))

        elif func in ("_record_discord_processing_start", "_record_discord_processing_complete"):
            event = SimpleNamespace(
                raw_message=msg_double(args.get("raw_message")),
                message_id=args.get("message_id", ""))
            if func == "_record_discord_processing_start":
                ret = fn(inst, event, emoji_ack=bool(args.get("emoji_ack", False)))
            else:
                ret = fn(inst, event, OUTCOME.get(args.get("outcome", "success"), ProcessingOutcome.FAILED))

        elif func in ("_record_discord_response", "_record_response_async"):
            result = SimpleNamespace(success=bool(args.get("success", False)),
                                     message_id=args.get("result_message_id"))
            if func == "_record_discord_response":
                ret = fn(inst, reply_to=args.get("reply_to"), result=result,
                         content=args.get("content", ""), final=bool(args.get("final", False)))
            else:
                ret = run_call(fn, inst, args.get("reply_to"), result,
                               args.get("content", ""), bool(args.get("final", False)),
                               args.get("metadata"))

        elif func == "_record_recovery_scan_start":
            ret = fn(inst, set(args.get("channels", [])))

        elif func == "_record_recovery_scan_complete":
            ret = fn(inst, args.get("scan_id", ""), status=args.get("status", "completed"),
                     scanned=int(args.get("scanned", 0)), missed=int(args.get("missed", 0)),
                     dispatched=int(args.get("dispatched", 0)), error=args.get("error"))

        elif func == "_refresh_skill_catalog_state":
            inst._skill_group_reserved_names = set(args.get("reserved_names", []))
            ret = fn(inst)

        elif func == "_register_skill_group":
            tree = _Tree(st)
            inst.name = args.get("name", "bot")
            ret = fn(inst, tree)

        elif func == "_register_slash_commands":
            inst._client = None
            if not args.get("no_client"):
                inst._client = SimpleNamespace(tree=_Tree(st))
            inst.name = args.get("name", "bot")
            ret = fn(inst)

        elif func == "_register_thread_slash":
            tree = _Tree(st)
            ret = fn(inst, tree, args.get("name", "thread"), args.get("description", "desc"))

        elif func == "_reject_slash":
            interaction = _Interaction(st, args.get("interaction") or {})
            ret = run_call(fn, inst, interaction, args.get("command_text", ""),
                           reason=args.get("reason", ""))

        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)

    echo = {}
    for k, v in vars(inst).items():
        if callable(v) or k.startswith("__"):
            continue
        echo[k] = v
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo)}


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "discord_adapter_flow14.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'da14_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_discord_adapter_flow14: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'(?:rec|arec)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_discord_adapter_flow14: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(
        r'PoP:\s*da14_\w+\s*@\s*plugins/platforms/discord/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_discord_adapter_flow14: PoP/KINDS drift: "
              f"c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow14.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        if fix_keys != set(SEAM_NAMES):
            print(f"FATAL oracle_discord_adapter_flow14: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_discord_adapter_flow14: fixture func set != KINDS: "
                  f"missing={sorted(set(KINDS) - fix_funcs)} extra={sorted(fix_funcs - set(KINDS))}")
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
    if shown >= 30:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:800]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:800]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:800]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:800]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:4]:
            print(f"    {k}: want={json.dumps(ws.get(k), ensure_ascii=False)[:300]} got={json.dumps(gs.get(k), ensure_ascii=False)[:300]}")


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
    print(f"ORACLE discord_adapter_flow14: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow14.jsonl"
    n = bad = 0
    counts = {}
    for line in fix.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            w = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = w.get("ret")
        if isinstance(r, dict) and "__raise__" in r:
            msg = r.get("message", "")
            bad_flags = ("takes ", "got multiple values", "missing 1 required",
                         "is not defined", "is not a container", "unexpected keyword",
                         "not callable", "object has no attribute")
            if any(fl in msg for fl in bad_flags):
                bad += 1
                print(f"SELFTEST SUSPICIOUS RAISE {func} case {n}: {r}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST discord_adapter_flow14: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow14.jsonl"
    out = _REPO / "tests" / "fixtures" / "discord_adapter_flow14.wants.jsonl"
    n = 0
    with out.open("w", encoding="utf-8", newline="\n") as f:
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            try:
                w = drive(case["func"], case.get("in") or {})
            except BaseException as exc:
                w = {"ret": proj_exc(exc), "trace": [], "state": {}, "_drive_error": True}
            f.write(json.dumps({"func": case["func"], "in": case.get("in"), "out": w},
                               ensure_ascii=False) + "\n")
            n += 1
    print(f"dumped {n} wants to {out}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--dump":
        dump()
        sys.exit(0)
    main()
