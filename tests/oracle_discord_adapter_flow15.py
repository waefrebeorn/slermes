#!/usr/bin/env python3
"""Oracle for the Discord adapter FLOW part 15 port
(src/gateway/platforms/discord_adapter_flow15.c).

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

# method funcs hosted on the synthetic class; module-level funcs stay in NS.
METHODS = [
    "_reset_voice_timeout", "_voice_timeout_limit", "_playback_timeout_limit",
    "_resolve_channel", "_resolve_interaction_channel", "_resolve_channel_skills",
    "_resolve_credentials", "_should_backfill_discord_message",
    "_self_contained_prompt_content", "_run_simple_slash",
    "_safe_sync_slash_commands", "_run_post_connect_initialization",
    "_run_missed_message_backfill", "_resolve_allowed_usernames",
]
MODULE_FUNCS = ["_standalone_read_json_limited", "_standalone_is_forum"]
KINDS = METHODS + MODULE_FUNCS
ORDER = list(METHODS)

# EXACTLY the C da15_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "cancel_voice_timeout", "voice_timeout_limit", "logger_debug",
    "voice_timeout_handler", "ensure_future",
    "channel_id_from_link", "get_channel", "fetch_channel",
    "resolve_channel_skills",
    "bytes", "monotonic", "note_dave_passthrough_window",
    "read_bytes_limited", "resp_json", "response_encoding", "json_loads",
    "lookup_channel_type", "probe_is_forum_cached", "rest_url", "client_timeout",
    "client_session", "session_get", "remember_channel_is_forum",
    "persistently_complete", "active_claim", "attempts_exhausted",
    "has_non_down_bot_response",
    "t", "logger_info", "check_slash_authorization", "defer_unless_expired",
    "build_slash_event", "handle_message", "edit_original_response",
    "delete_original_response",
    "tree_get_commands", "tree_fetch_commands", "delete_global_command",
    "upsert_global_command", "edit_global_command", "sleep_between_mutations",
    "existing_command_to_payload", "canonicalize_payload", "patchable_payload",
    "command_sync_policy", "tree_sync", "desired_sync_fingerprint",
    "command_sync_skip_reason", "record_command_sync_attempt", "wait_for",
    "safe_sync", "is_discord_rate_limit", "extract_retry_after",
    "record_command_sync_rate_limit", "record_command_sync_success",
    "backfill_channels", "ledger_db", "record_recovery_scan_start",
    "finish_recovery_scan", "iter_backfill_candidates", "dedup_contains",
    "dedup_discard", "should_backfill", "record_discord_message_seen",
    "record_recovery_attempt", "dispatch_recovered", "max_dispatches",
    "logger_error", "logger_warning",
    "au_print", "au_fetch_members", "multiplex_active", "replace_bridged_env",
    "forum_read_json",
]

DISCORD_TEXT_LIMIT = 100
MAX_APP_COMMANDS = 100
MAX_MESSAGE_LENGTH = 2000
RATE_LIMIT_SLEEP = 30.0
JSON_BODY_LIMIT = 1024 * 1024


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


_MOD = types.ModuleType("da15_oracle_adapter")
NS = _MOD.__dict__
NS["__builtins__"] = builtins
sys.modules["da15_oracle_adapter"] = _MOD


def extract():
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    classes = {c.name: c for c in tree.body if isinstance(c, ast.ClassDef)}
    method_nodes = {}
    for cname in ("DiscordAdapter", "VoiceReceiver"):
        cls = classes.get(cname)
        if cls:
            for n in cls.body:
                if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in METHODS:
                    method_nodes[n.name] = n
    top = {n.name: n for n in tree.body
           if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in MODULE_FUNCS}
    nodes = {**top, **method_nodes}
    missing = [k for k in KINDS if k not in nodes]
    if missing:
        print(f"FATAL oracle_discord_adapter_flow15: missing defs: {missing}")
        sys.exit(2)
    return nodes, src, top


_NODES, _SRC, _TOP = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        tn = type(x).__name__
        if tn in ("_Tree", "_Cmd", "_CmdDef", "_Interaction", "_Session", "_Resp",
                  "_ACM", "_Member", "_Existing"):
            return conv(getattr(x, "_proj", lambda: {})())
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


# ── per-case script backend (mirrors the C da15_call exactly) ──
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
                if kind == "CancelledError":
                    cls = asyncio.CancelledError
                else:
                    cls = getattr(builtins, kind, None)
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
class _ACM:
    def __init__(self, ret):
        self.ret = ret

    async def __aenter__(self):
        return self.ret

    async def __aexit__(self, *a):
        return False

    def _proj(self):
        return "<acm>"


class _Cmd:
    def __init__(self, payload):
        self.payload = payload

    def to_dict(self, tree):
        return self.payload

    def _proj(self):
        return {"name": self.payload.get("name", "") if isinstance(self.payload, dict) else ""}


class _Existing:
    def __init__(self, d):
        self.id = d.get("id")
        self.name = d.get("name", "")
        self.type = d.get("type", 1)

    def _proj(self):
        return {"id": self.id, "name": self.name, "type": self.type}


class _Tree:
    def __init__(self, st):
        self.get_commands = st["tree_get_commands"]
        self.fetch_commands = st["tree_fetch_commands"]
        self.sync = st["tree_sync"]

    def _proj(self):
        return "<tree>"


class _Resp:
    def __init__(self, st, status):
        self.status = status
        self.json = arec("resp_json", lambda: {})

    def _proj(self):
        return {"status": self.status}


class _Session:
    def __init__(self, st, resp_status):
        self._st = st
        self._resp_status = resp_status

    def get(self, url, headers=None, **kw):
        _CUR.call("session_get", {"url": url, "headers": _proj(headers),
                                  **{k: _proj(v) for k, v in kw.items()}})
        return _ACM(_Resp(self._st, self._resp_status))

    def _proj(self):
        return "<session>"


class _Interaction:
    def __init__(self, st, d):
        u = d.get("user")
        self.user = SimpleNamespace(id=u.get("id", "?"), name=u.get("name", "?")) if isinstance(u, dict) else None
        self.channel_id = d.get("channel_id")
        ch = d.get("channel")
        self.channel = SimpleNamespace(id=ch.get("id")) if isinstance(ch, dict) else None
        self.guild_id = d.get("guild_id")
        self.response = SimpleNamespace(send_message=arec("response_send_message", lambda message, ephemeral=False: {}))
        self.edit_original_response = arec("edit_original_response", lambda content=None: {"content": content})
        self.delete_original_response = arec("delete_original_response", lambda: {})

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
    d = {k: ns_double(x) for k, x in d.items()}
    return SimpleNamespace(**d)


def identity_deco(f):
    return f


# ── seam stubs ──
def _seam_name_coverage():
    # literal names for the strict oracle/C SEAM-NAME set equality check
    rec("logger_debug", lambda: {})
    rec("logger_info", lambda: {})
    rec("logger_warning", lambda: {})
    rec("logger_error", lambda: {})


def _log_seam(name):
    return rec(name, lambda *a, **k: {"args": [_proj(x) for x in a],
                                      "kwargs": {kk: _proj(vv) for kk, vv in k.items()}})


def build_seams():
    st = {}
    st["cancel_voice_timeout"] = rec("cancel_voice_timeout", lambda guild_id: {"guild_id": guild_id})
    st["voice_timeout_limit"] = rec("voice_timeout_limit", lambda: {})
    st["logger_debug"] = _log_seam("logger_debug")
    st["logger_info"] = _log_seam("logger_info")
    st["logger_warning"] = _log_seam("logger_warning")
    st["logger_error"] = _log_seam("logger_error")
    st["voice_timeout_handler"] = rec("voice_timeout_handler",
        lambda gid, to: {"guild_id": gid, "timeout": to})
    st["ensure_future"] = rec("ensure_future", lambda coro: {"coro": _proj(coro)})

    st["channel_id_from_link"] = rec("channel_id_from_link", lambda value: {"value": value})
    st["get_channel"] = rec("get_channel", lambda channel_id: {"channel_id": channel_id})
    st["fetch_channel"] = arec("fetch_channel", lambda channel_id: {"channel_id": channel_id})

    st["resolve_channel_skills"] = rec("resolve_channel_skills",
        lambda extra, channel_id, parent_id: {"extra": _proj(extra), "channel_id": channel_id,
                                              "parent_id": parent_id})

    st["bytes"] = rec("bytes", lambda value: {"value": _proj(value)})
    st["monotonic"] = rec("monotonic", lambda: {})
    st["note_dave_passthrough_window"] = rec("note_dave_passthrough_window",
        lambda seconds: {"seconds": seconds})

    st["read_bytes_limited"] = arec("read_bytes_limited", lambda limit_bytes: {"limit_bytes": limit_bytes})
    st["resp_json"] = arec("resp_json", lambda: {})
    st["response_encoding"] = rec("response_encoding", lambda resp: {})
    st["json_loads"] = rec("json_loads", lambda s: {"s": s})

    st["lookup_channel_type"] = rec("lookup_channel_type",
        lambda platform, chat_id: {"platform": platform, "chat_id": chat_id})
    st["probe_is_forum_cached"] = rec("probe_is_forum_cached", lambda chat_id: {"chat_id": chat_id})
    st["rest_url"] = rec("rest_url", lambda path: {"path": path})
    st["client_timeout"] = rec("client_timeout", lambda total: {"total": total})
    st["client_session"] = rec("client_session",
        lambda timeout, **kw: {"timeout": _proj(timeout), **{k: _proj(v) for k, v in kw.items()}})
    st["session_get"] = rec("session_get", lambda: {})
    st["remember_channel_is_forum"] = rec("remember_channel_is_forum",
        lambda chat_id, is_forum: {"chat_id": chat_id, "is_forum": is_forum})
    st["forum_read_json"] = arec("forum_read_json", lambda resp, limit_bytes: {"limit_bytes": limit_bytes})

    st["persistently_complete"] = rec("persistently_complete", lambda message_id: {"id": message_id})
    st["active_claim"] = rec("active_claim", lambda message_id: {"id": message_id})
    st["attempts_exhausted"] = rec("attempts_exhausted", lambda message_id: {"id": message_id})
    st["has_non_down_bot_response"] = arec("has_non_down_bot_response",
        lambda message: {"message": _proj(message)})

    st["t"] = rec("t", lambda key: {"key": key})
    st["check_slash_authorization"] = arec("check_slash_authorization",
        lambda interaction, command_text: {"interaction": _proj(interaction), "command_text": command_text})
    st["defer_unless_expired"] = arec("defer_unless_expired",
        lambda interaction, warn_fmt, *warn_args: {"interaction": _proj(interaction),
            "warn_fmt": warn_fmt, "warn_args": [_proj(a) for a in warn_args]})
    st["build_slash_event"] = rec("build_slash_event",
        lambda interaction, command_text: {"interaction": _proj(interaction), "command_text": command_text})
    st["handle_message"] = arec("handle_message", lambda event: {"event": _proj(event)})
    st["edit_original_response"] = arec("edit_original_response", lambda content=None: {"content": content})
    st["delete_original_response"] = arec("delete_original_response", lambda: {})

    st["tree_get_commands"] = rec("tree_get_commands", lambda: {},
        build=lambda raw: [_Cmd(x.get("payload", x) if isinstance(x, dict) else x)
                           for x in (raw or [])])
    st["tree_fetch_commands"] = arec("tree_fetch_commands", lambda: {},
        build=lambda raw: [_Existing(x) for x in (raw or [])])
    st["delete_global_command"] = arec("delete_global_command",
        lambda app_id, command_id: {"app_id": app_id, "command_id": command_id})
    st["upsert_global_command"] = arec("upsert_global_command",
        lambda app_id, desired: {"app_id": app_id, "desired": _proj(desired)})
    st["edit_global_command"] = arec("edit_global_command",
        lambda app_id, command_id, desired: {"app_id": app_id, "command_id": command_id,
                                             "desired": _proj(desired)})
    st["sleep_between_mutations"] = arec("sleep_between_mutations", lambda: {})
    st["existing_command_to_payload"] = rec("existing_command_to_payload",
        lambda command: {"command": _proj(command)})
    st["canonicalize_payload"] = rec("canonicalize_payload", lambda payload: {"payload": _proj(payload)})
    st["patchable_payload"] = rec("patchable_payload", lambda payload: {"payload": _proj(payload)})

    st["command_sync_policy"] = rec("command_sync_policy", lambda: {})
    st["tree_sync"] = arec("tree_sync", lambda: {})
    st["desired_sync_fingerprint"] = rec("desired_sync_fingerprint", lambda: {})
    st["command_sync_skip_reason"] = rec("command_sync_skip_reason",
        lambda app_id, fingerprint: {"app_id": _proj(app_id), "fingerprint": fingerprint})
    st["record_command_sync_attempt"] = rec("record_command_sync_attempt",
        lambda app_id, fingerprint: {"app_id": _proj(app_id), "fingerprint": fingerprint})
    async def _wait_for(coro, timeout):
        raw = _CUR.call("wait_for", {"timeout": timeout})
        return await coro
    st["wait_for"] = _wait_for
    st["safe_sync"] = arec("safe_sync", lambda: {})
    st["is_discord_rate_limit"] = rec("is_discord_rate_limit", lambda error: {"error": _proj(error)})
    st["extract_retry_after"] = rec("extract_retry_after", lambda error: {"error": _proj(error)})
    st["record_command_sync_rate_limit"] = rec("record_command_sync_rate_limit",
        lambda app_id, fingerprint, retry_after: {"app_id": _proj(app_id), "fingerprint": fingerprint,
                                                  "retry_after": retry_after})
    st["record_command_sync_success"] = rec("record_command_sync_success",
        lambda app_id, fingerprint, summary: {"app_id": _proj(app_id), "fingerprint": fingerprint,
                                              "summary": _proj(summary)})

    st["backfill_channels"] = rec("backfill_channels", lambda: {})
    st["ledger_db"] = arec("ledger_db", lambda fn, default=None: {})
    st["record_recovery_scan_start"] = rec("record_recovery_scan_start", lambda channels: {"channels": _proj(channels)})
    st["finish_recovery_scan"] = arec("finish_recovery_scan",
        lambda scan_id, status, counts, error=None: {"scan_id": scan_id, "status": status,
            "counts": _proj(counts), "error": _proj(error)})
    def _mk_agen(raw):
        async def _a():
            for m in (raw or []):
                yield msg_double(m)
        return _a()
    st["iter_backfill_candidates"] = rec("iter_backfill_candidates",
        lambda channels: {"channels": _proj(channels)}, build=lambda raw: _mk_agen(raw))
    st["dedup_contains"] = rec("dedup_contains", lambda message_id: {"message_id": message_id})
    st["dedup_discard"] = rec("dedup_discard", lambda message_id: {"message_id": message_id})
    st["should_backfill"] = arec("should_backfill", lambda message: {"message": _proj(message)})
    st["record_discord_message_seen"] = rec("record_discord_message_seen",
        lambda message, status: {"message": _proj(message), "status": status})
    st["record_recovery_attempt"] = rec("record_recovery_attempt",
        lambda message, status, error=None: {"message": _proj(message), "status": status,
                                             "error": _proj(error)})
    st["dispatch_recovered"] = arec("dispatch_recovered", lambda message: {"message": _proj(message)})
    st["max_dispatches"] = rec("max_dispatches", lambda: {})

    st["au_print"] = rec("au_print", lambda *a: {"text": " ".join(str(x) for x in a)})
    st["au_fetch_members"] = arec("au_fetch_members", lambda guild: {"guild": _proj(guild)})
    st["multiplex_active"] = rec("multiplex_active", lambda: {})
    st["replace_bridged_env"] = rec("replace_bridged_env",
        lambda key, value: {"key": key, "value": value})
    return st


# ── synthetic class hosting the ported instance methods ──
class BaseDouble:
    VOICE_TIMEOUT = 300
    PLAYBACK_TIMEOUT = 120
    MAX_MESSAGE_LENGTH = MAX_MESSAGE_LENGTH


NS["BaseDouble"] = BaseDouble
_syn = ast.ClassDef(name="DA15Double", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
                    keywords=[], body=[_NODES[n] for n in ORDER], decorator_list=[])
ast.fix_missing_locations(_syn)
_mod_node = ast.Module(body=[_syn], type_ignores=[])
ast.fix_missing_locations(_mod_node)
exec(compile(_mod_node, A_SRC, "exec", flags=_FLAGS, dont_inherit=True), NS)
DA15Double = NS["DA15Double"]
PORTED = {n: DA15Double.__dict__[n] for n in ORDER}
for n in MODULE_FUNCS:
    exec(compile(ast.Module(body=[_TOP[n]], type_ignores=[]), A_SRC, "exec",
                 flags=_FLAGS, dont_inherit=True), NS)
PORTED.update({n: NS[n] for n in MODULE_FUNCS})
ORIG = dict(PORTED)

# Methods that are themselves ported but act as nested collaborators elsewhere.
NESTED_STUBS = {
    "_voice_timeout_limit": "voice_timeout_limit",
    "_should_backfill_discord_message": "should_backfill",
    "_record_discord_message_seen": "record_discord_message_seen",
    "_record_recovery_attempt": "record_recovery_attempt",
    "_record_recovery_scan_start": "record_recovery_scan_start",
    "_safe_sync_slash_commands": "safe_sync",
}
MODULE_NESTED = {"_standalone_read_json_limited": "forum_read_json"}


def seed(st, cur_func):
    NS["logger"] = SimpleNamespace(info=st["logger_info"], debug=st["logger_debug"],
                                   warning=st["logger_warning"], error=st["logger_error"])
    NS["time"] = SimpleNamespace(time=rec("time_time", lambda: {}), monotonic=st["monotonic"])
    NS["json"] = SimpleNamespace(dumps=rec("json_dumps", lambda value: {"value": _proj(value)}),
                                 loads=st["json_loads"])
    NS["asyncio"] = SimpleNamespace(to_thread=_passthrough_to_thread,
                                    create_task=rec("create_task", lambda coro: {}),
                                    wait_for=st["wait_for"], sleep=arec("async_sleep", lambda interval: {}),
                                    ensure_future=st["ensure_future"], CancelledError=asyncio.CancelledError)
    NS["math"] = math
    NS["contextlib"] = contextlib
    NS["enum"] = enum
    NS["builtins"] = builtins
    NS["bytes"] = st["bytes"]
    NS["Optional"] = object
    NS["Any"] = object
    NS["discord"] = SimpleNamespace(
        Interaction=object,
        app_commands=SimpleNamespace(Command=rec("app_command_ctor", lambda **kw: {"kw": _proj(kw)}),
            describe=lambda **kw: identity_deco, autocomplete=lambda **kw: identity_deco,
            Choice=lambda **kw: SimpleNamespace(**kw)))

    for m in NESTED_STUBS:
        if m in ORIG:
            setattr(DA15Double, m, ORIG[m])
    for m, seam in NESTED_STUBS.items():
        if m != cur_func:
            setattr(DA15Double, m, _NoBind(st[seam]))
    for m, seam in MODULE_NESTED.items():
        if m in ORIG:
            NS[m] = ORIG[m]
    for m, seam in MODULE_NESTED.items():
        if m != cur_func:
            NS[m] = st[seam]

    DA15Double._cancel_voice_timeout = _NoBind(st["cancel_voice_timeout"])
    DA15Double._voice_timeout_handler = _NoBind(st["voice_timeout_handler"])
    DA15Double.note_dave_passthrough_window = _NoBind(st["note_dave_passthrough_window"])

    DA15Double._check_slash_authorization = _NoBind(st["check_slash_authorization"])
    DA15Double._defer_unless_expired = _NoBind(st["defer_unless_expired"])
    DA15Double._build_slash_event = _NoBind(st["build_slash_event"])
    DA15Double.handle_message = _NoBind(st["handle_message"])
    DA15Double._sleep_between_command_sync_mutations = _NoBind(st["sleep_between_mutations"])
    DA15Double._existing_command_to_payload = _NoBind(st["existing_command_to_payload"])
    DA15Double._canonicalize_app_command_payload = _NoBind(st["canonicalize_payload"])
    DA15Double._patchable_app_command_payload = _NoBind(st["patchable_payload"])
    DA15Double._get_discord_command_sync_policy = _NoBind(st["command_sync_policy"])
    DA15Double._desired_command_sync_fingerprint = _NoBind(st["desired_sync_fingerprint"])
    DA15Double._command_sync_skip_reason = _NoBind(st["command_sync_skip_reason"])
    DA15Double._record_command_sync_attempt = _NoBind(st["record_command_sync_attempt"])
    DA15Double._is_discord_rate_limit = _NoBind(st["is_discord_rate_limit"])
    DA15Double._extract_discord_retry_after = _NoBind(st["extract_retry_after"])
    DA15Double._record_command_sync_rate_limit = _NoBind(st["record_command_sync_rate_limit"])
    DA15Double._record_command_sync_success = _NoBind(st["record_command_sync_success"])
    DA15Double._missed_message_backfill_channels = _NoBind(st["backfill_channels"])
    DA15Double._with_discord_recovery_db_async = _NoBind(st["ledger_db"])
    DA15Double._finish_recovery_scan = _NoBind(st["finish_recovery_scan"])
    DA15Double._iter_missed_message_backfill_candidates = _NoBind(st["iter_backfill_candidates"])
    DA15Double._dispatch_recovered_message = _NoBind(st["dispatch_recovered"])
    DA15Double._missed_message_backfill_max_dispatches = _NoBind(st["max_dispatches"])

    DA15Double._discord_message_is_persistently_complete = _NoBind(st["persistently_complete"])
    DA15Double._discord_message_has_active_claim = _NoBind(st["active_claim"])
    DA15Double._discord_message_attempts_exhausted = _NoBind(st["attempts_exhausted"])
    DA15Double._message_has_non_down_bot_response = _NoBind(st["has_non_down_bot_response"])

    NS["_multiplex_active"] = st["multiplex_active"]
    NS["_replace_bridged_env"] = st["replace_bridged_env"]
    NS["_DISCORD_COMMAND_SYNC_MAX_RATE_LIMIT_SLEEP_SECONDS"] = RATE_LIMIT_SLEEP
    NS["_DISCORD_STANDALONE_JSON_BODY_LIMIT_BYTES"] = JSON_BODY_LIMIT
    NS["discord_channel_id_from_link"] = st["channel_id_from_link"]

    # module funcs used by other module funcs
    NS["_standalone_read_response_bytes_limited"] = _read_bytes_shim
    NS["_standalone_response_encoding"] = st["response_encoding"]
    NS["_probe_is_forum_cached"] = st["probe_is_forum_cached"]
    NS["_rest_url"] = st["rest_url"]
    NS["_remember_channel_is_forum"] = st["remember_channel_is_forum"]
    NS["t"] = st["t"]
    NS["print"] = st["au_print"]

    # in-function imports
    gwp = types.ModuleType("gateway"); gwp.__path__ = []
    gwpl = types.ModuleType("gateway.platforms"); gwpl.__path__ = []
    gwpb = types.ModuleType("gateway.platforms.base")
    gwpb.resolve_channel_skills = st["resolve_channel_skills"]
    gwpb.resolve_channel_prompt = st["resolve_channel_skills"]
    gwcd = types.ModuleType("gateway.channel_directory")
    gwcd.lookup_channel_type = st["lookup_channel_type"]
    for nm, mod in [("gateway", gwp), ("gateway.platforms", gwpl),
                    ("gateway.platforms.base", gwpb), ("gateway.channel_directory", gwcd)]:
        sys.modules[nm] = mod
    NS["aiohttp_Double"] = st


async def _passthrough_to_thread(func, *a, **k):
    return func(*a, **k)


async def _read_bytes_shim(resp, limit):
    raw = _CUR.call("read_bytes_limited", {"limit_bytes": limit})
    body, truncated = raw
    if isinstance(body, str):
        body = body.encode("utf-8")
    return body, bool(truncated)


def _build_guilds(guilds, st, resp_status):
    out = []
    for g in guilds or []:
        members = [ns_double(m) for m in (g.get("members") or [])]
        gl = SimpleNamespace(name=g.get("name", ""), member_count=g.get("member_count", len(members)),
                             members=members, fetch_members=_make_fetch(g, st))
        out.append(gl)
    return out


def _make_fetch(g, st):
    async def _fetch(limit=None):
        raw = _CUR.call("au_fetch_members", {"guild": g.get("name", "")})
        for m in (raw or []):
            yield ns_double(m)
    return _fetch


def make_self(state, st):
    inst = DA15Double()
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
        if func == "_reset_voice_timeout":
            inst._voice_timeout_tasks = {}
            ret = fn(inst, args.get("guild_id", 0))

        elif func in ("_voice_timeout_limit", "_playback_timeout_limit"):
            ret = fn(inst)

        elif func == "_resolve_channel":
            inst._client = SimpleNamespace(
                get_channel=st["get_channel"], fetch_channel=st["fetch_channel"])
            cid = args.get("channel_id")
            ret = run_call(fn, inst, cid)

        elif func == "_resolve_interaction_channel":
            if args.get("no_client"):
                inst._client = None
            else:
                inst._client = SimpleNamespace(get_channel=st["get_channel"],
                                                fetch_channel=st["fetch_channel"])
            d = args.get("interaction") or {}
            ix = SimpleNamespace(channel=ns_double(d["channel"]) if d.get("channel") else None,
                                 channel_id=d.get("channel_id"))
            ret = run_call(fn, inst, ix)

        elif func == "_resolve_channel_skills":
            inst.config = SimpleNamespace(extra=args.get("extra") or {})
            ret = fn(inst, args.get("channel_id", ""), args.get("parent_id"))

        elif func == "_resolve_credentials":
            conn_d = args.get("conn") or {}
            conn = SimpleNamespace(
                secret_key=conn_d.get("secret_key", []),
                dave_session=conn_d.get("dave_session"),
                dave_protocol_version=conn_d.get("dave_protocol_version", 0),
                dave_downgraded=conn_d.get("dave_downgraded", False),
                ssrc=conn_d.get("ssrc"),
                dave_pending_transitions=conn_d.get("dave_pending_transitions") or {})
            ret = fn(inst, conn)

        elif func == "_standalone_read_json_limited":
            resp = SimpleNamespace(json=st["resp_json"])
            ret = run_call(fn, resp, args.get("limit_bytes", 1000))

        elif func == "_standalone_is_forum":
            aiohttp = SimpleNamespace(
                ClientSession=lambda **kw: _client_session(st, kw, args),
                ClientTimeout=st["client_timeout"])
            ret = run_call(fn, aiohttp, args.get("chat_id", ""), args.get("json_headers") or {},
                           args.get("sess_kw") or {}, args.get("req_kw") or {})

        elif func == "_should_backfill_discord_message":
            if args.get("no_client"):
                inst._client = None
            else:
                inst._client = SimpleNamespace(user=SimpleNamespace(id=args.get("user_id")))
            ret = run_call(fn, inst, msg_double(args.get("message")))

        elif func == "_self_contained_prompt_content":
            ret = fn(inst, args.get("header", ""), args.get("body", ""),
                     code_block=bool(args.get("code_block", False)), tail=args.get("tail", ""))

        elif func == "_run_simple_slash":
            ret = run_call(fn, inst, _Interaction(st, args.get("interaction") or {}),
                           args.get("command_text", ""), args.get("followup_msg"))

        elif func == "_safe_sync_slash_commands":
            cl = args.get("_client", state.get("_client"))
            if cl is None:
                inst._client = None
            elif isinstance(cl, dict):
                inst._client = _client_obj(st, cl)
            ret = run_call(fn, inst)

        elif func == "_run_post_connect_initialization":
            cl = args.get("_client", state.get("_client"))
            inst.name = args.get("name", "bot")
            if cl is None:
                inst._client = None
            elif isinstance(cl, dict):
                inst._client = _client_obj(st, cl)
            ret = run_call(fn, inst)

        elif func == "_run_missed_message_backfill":
            cl = state.get("_client", "__missing__")
            inst._client = None if cl is None else SimpleNamespace()
            inst.name = args.get("name", "bot")
            inst._dedup = SimpleNamespace(contains=st["dedup_contains"], discard=st["dedup_discard"])
            ret = run_call(fn, inst)

        elif func == "_resolve_allowed_usernames":
            if args.get("guilds") is None:
                inst._client = None
            else:
                inst._client = SimpleNamespace(guilds=_build_guilds(args.get("guilds"), st, 200))
            inst._allowed_user_ids = set(args.get("allowed_user_ids") or [])
            inst.name = args.get("name", "bot")
            if "gate_env_snapshot" in args:
                inst._gate_env_snapshot = copy.deepcopy(args["gate_env_snapshot"])
            ret = run_call(fn, inst)

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


def _client_session(st, kw, args):
    _CUR.call("client_session", {"timeout": _proj(kw.get("timeout")),
                 **{k: _proj(v) for k, v in kw.items() if k != "timeout"}})
    return _ACM(_Session(st, args.get("resp_status", 200)))


def _client_obj(st, cl):
    tree = _Tree(st)
    http = SimpleNamespace(delete_global_command=st["delete_global_command"],
                           upsert_global_command=st["upsert_global_command"],
                           edit_global_command=st["edit_global_command"],
                           max_ratelimit_timeout=cl.get("max_ratelimit_timeout"))
    user = SimpleNamespace(id=cl.get("user", {}).get("id")) if isinstance(cl.get("user"), dict) else None
    return SimpleNamespace(tree=tree, http=http, application_id=cl.get("application_id"), user=user)


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "discord_adapter_flow15.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'da15_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_discord_adapter_flow15: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'(?:rec|arec)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_discord_adapter_flow15: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(
        r'PoP:\s*da15_\w+\s*@\s*plugins/platforms/discord/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_discord_adapter_flow15: PoP/KINDS drift: "
              f"c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow15.jsonl"
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
            print(f"FATAL oracle_discord_adapter_flow15: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_discord_adapter_flow15: fixture func set != KINDS: "
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
    print(f"ORACLE discord_adapter_flow15: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow15.jsonl"
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
    print(f"SELFTEST discord_adapter_flow15: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "discord_adapter_flow15.jsonl"
    out = _REPO / "tests" / "fixtures" / "discord_adapter_flow15.wants.jsonl"
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
