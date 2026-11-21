#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 15 port
(src/gateway/platforms/telegram_adapter_flow15.c).

Drives the LIVE plugins/platforms/telegram/adapter.py defs with SCRIPTED seams.
Every non-pure collaborator is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the ordered seam trace + final
return + state echo.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE telegram_adapter_flow15: N cases, M mismatches'.
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
A_SRC = os.path.join(Q, "plugins", "platforms", "telegram", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

# kind: module | instance | static
KINDS = {
    "_handle_location_message": "instance",
    "_handle_media_message": "instance",
    "_handle_model_picker_callback": "instance",
    "_handle_polling_conflict": "instance",
    "_handle_polling_network_error": "instance",
    "_handle_slash_confirm_callback": "instance",
    "_handle_sticker": "instance",
    "_handle_text_message": "instance",
    "_handle_update_prompt_callback": "instance",
    "_handoff_polling_fatal_error": "instance",
    "_hold_chat_outbound_slot": "instance",
    "_initialize_app_with_retries": "instance",
    "_instrument_polling_request": "instance",
    "_is_callback_user_authorized": "instance",
    "_warn_then": "instance",
    "_set_status_indicator": "instance",
}
ORDER = list(KINDS)

# EXACTLY the C ta15_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "absorb_claimed_text", "accept_update", "admit_media", "answer_callback", "answer_or_note", "apply_observe_attribution",
    "auth_fn_call", "auth_source", "await_with_thread_deadline", "build_animated_sticker_injection", "build_message_event", "build_sticker_injection",
    "build_triggered_event", "builder_build", "cache_image_from_bytes", "cache_observed_media", "cache_sticker_description", "callback_authorized",
    "claim_callback_state", "claim_pending_text", "consume_vault_code_reply", "current_task", "drain_general_connections", "drain_polling_connections",
    "edit_keeping_prompt", "edit_md_quiet", "effective_update_message", "enqueue_text_event", "ensure_forum_commands", "env_allowlist_decision",
    "env_float", "expand_link_entities", "fallback", "fetch_telegram_file", "gate_or_observe", "get_cached_description",
    "get_hermes_home", "go_fatal_network", "group_trigger_text", "handle_message", "handoff_polling_fatal_error", "hash_for_log",
    "is_sender_authorized", "is_user_authorized_from_message", "json_loads", "legacy_runner_auth_fn", "log_blocked_user", "logger_debug",
    "logger_error", "logger_info", "logger_warning", "looks_like_network_error", "looks_like_pool_timeout", "loop_time",
    "media_message_type", "normalize_chat_id", "normalize_chat_type", "notify_fatal_error", "parse_page", "picker_show_models",
    "picker_show_providers", "provider_get_label", "queue_media_group_event", "redact_telegram_error_text", "redundant_live_refresh", "refresh_gates",
    "refusal_text", "refuse_tap", "register_handlers", "replace", "restart_polling_in_task", "rich_message_text",
    "scoped_gate_env", "set_fatal_error", "set_status_indicator", "should_observe_unmentioned", "shutdown_abandoned_app", "slash_confirm_refusal",
    "slash_confirm_resolve", "sleep", "start_polling_once", "sticker_local_root", "stop_updater_or_go_fatal", "toast",
    "translate", "unauthorized", "update_prompt_refusal", "vision_analyze_tool", "wire_plugin_handlers", "write_text",
]
SEAM_NAMES = list(dict.fromkeys(SEAM_NAMES))

_TOAST_LIMIT = 200


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


_MOD = new_module("ta15_oracle_adapter")
NS = _MOD.__dict__
NS["__builtins__"] = builtins


def extract():
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    nodes = {}
    for n in cls.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in KINDS:
            nodes[n.name] = n
    missing = [k for k in KINDS if k not in nodes]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow15: missing defs: {missing}")
        sys.exit(2)
    return nodes


_NODES = extract()


# ── canonical projection ──
class SeamFn:
    def __init__(self, name, args_fn=None):
        self.name = name
        self.args_fn = args_fn or (lambda *a, **k: {})

    def __call__(self, *a, **k):
        return _CUR.call(self.name, self.args_fn(*a, **k))


def canon(v):
    def conv(x):
        tn = type(x).__name__
        if isinstance(x, _TaskRef):
            return x.name
        if isinstance(x, _Bytes):
            return x.s
        if isinstance(x, _Path):
            return x._raw
        if isinstance(x, _Proc):
            return conv(x._raw)
        if isinstance(x, _Sentinel):
            return x.name
        if asyncio.iscoroutine(x):
            return "<awaitable>"
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items() if not callable(val)}
        if isinstance(x, dict):
            return {str(k): conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = asyncio.CancelledError if kind == "CancelledError" else getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ta15_call exactly) ──
_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.st = {}
        self.inst = None
        self.task_obj = None

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
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


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


def _marker(x):
    return "<lambda>" if callable(x) else x


class _TaskRef:
    def __init__(self, name):
        self.name = name


class _Bytes:
    def __init__(self, s):
        self.s = "" if s is None else (s.decode("utf-8", "replace") if isinstance(s, bytes) else str(s))

    def decode(self, *a, **k):
        return self.s


class _Path:
    def __init__(self, raw):
        self._raw = "" if raw is None else str(raw)

    def with_suffix(self, suf):
        return _Path(self._raw + suf)

    def write_text(self, data, *a, **k):
        return _CUR.call("write_text", {"path": self._raw, "data": data})

    def replace(self, target):
        return _CUR.call("replace", {"src": self._raw, "dst": target._raw if isinstance(target, _Path) else target})

    def __truediv__(self, part):
        return _Path(self._raw + ("" if self._raw.endswith("/") else "/") + str(part))

    def __str__(self):
        return self._raw


class _Proc:
    def __init__(self, raw):
        object.__setattr__(self, "_raw", raw if isinstance(raw, dict) else {})

    @property
    def returncode(self):
        return self._raw.get("returncode")


class _Sentinel:
    def __init__(self, name):
        self.name = name


async def _noop_init():
    return None


class _Builder:
    def build(self):
        _CUR.call("builder_build", {})
        return SimpleNamespace(bot=_Sentinel("bot"), initialize=_noop_init)


class _Keyboard:
    def __init__(self, rows):
        self.rows = rows


# ── seam stubs ──
def build_seams():
    st = {}
    st["normalize_chat_id"] = rec("normalize_chat_id", lambda chat_id: {"chat_id": chat_id})
    st["loop_time"] = rec("loop_time", lambda: {})
    st["current_task"] = rec("current_task", lambda: {})
    st["notify_fatal_error"] = arec("notify_fatal_error", lambda self_: {})
    st["redact_error"] = rec("redact_error", lambda err: {"error": err})
    st["logger_warning"] = rec("logger_warning", lambda *a, **k: {"args": list(a)})
    st["logger_info"] = rec("logger_info", lambda *a, **k: {"args": list(a)})
    st["logger_error"] = rec("logger_error", lambda *a, **k: {"args": list(a)})
    st["logger_debug"] = rec("logger_debug", lambda *a, **k: {"args": list(a)})
    st["fallback"] = arec("fallback", lambda: {})
    st["set_status_indicator"] = arec("set_status_indicator", lambda self_, online: {"online": online})
    st["refresh_gates"] = rec("refresh_gates", lambda self_: {})
    st["normalize_chat_type"] = rec("normalize_chat_type",
                                   lambda self_, ct, *, is_forum=False: {"chat_type": ct, "is_forum": is_forum})
    st["is_sender_authorized"] = rec("is_sender_authorized",
        lambda self_, uid, *, chat_type, chat_id, thread_id: {
            "user_id": uid, "chat_type": chat_type, "chat_id": chat_id, "thread_id": thread_id})
    st["legacy_runner_auth_fn"] = rec("legacy_runner_auth_fn", lambda self_: {})
    st["auth_source"] = rec("auth_source", lambda **kw: dict(kw))
    st["auth_fn_call"] = rec("auth_fn_call", lambda source: {"source": source})
    st["env_allowlist_decision"] = rec("env_allowlist_decision", lambda self_, uid: {"user_id": uid})
    st["scoped_gate_env"] = rec("scoped_gate_env", lambda key: {"key": key})
    st["effective_update_message"] = rec("effective_update_message", lambda self_, update: {"update": update},
        build=lambda raw: ns_double(raw) if raw is not None else None)
    st["is_user_authorized_from_message"] = rec("is_user_authorized_from_message", lambda self_, msg: {"msg": msg})
    st["log_blocked_user"] = rec("log_blocked_user", lambda self_, msg, **k: {"msg": msg, "kw": {x: _marker(v) for x, v in k.items()}})
    st["rich_message_text"] = rec("rich_message_text", lambda msg: {"msg": msg})
    st["consume_vault_code_reply"] = arec("consume_vault_code_reply", lambda self_, msg: {"msg": msg})
    st["gate_or_observe"] = rec("gate_or_observe", lambda self_, msg, update, mtype: {
        "msg": msg, "update": update, "message_type": mtype})
    st["ensure_forum_commands"] = arec("ensure_forum_commands", lambda self_, msg: {"msg": msg})
    st["build_triggered_event"] = arec("build_triggered_event", lambda self_, msg, update, mtype: {
        "msg": msg, "update": update, "message_type": mtype},
        build=lambda raw: ns_double(raw) if raw is not None else None)
    st["enqueue_text_event"] = rec("enqueue_text_event", lambda self_, event: {"event": event})
    st["build_message_event"] = rec("build_message_event", lambda self_, msg, mtype, *, update_id: {
        "msg": msg, "message_type": mtype, "update_id": update_id},
        build=lambda raw: ns_double(raw) if raw is not None else None)
    st["apply_observe_attribution"] = rec("apply_observe_attribution", lambda self_, event: {"event": event},
        build=lambda raw: raw)
    st["handle_message"] = arec("handle_message", lambda self_, event: {"event": event})
    st["redundant_live_refresh"] = rec("redundant_live_refresh", lambda self, msg, loc, lat, lon: {
        "lat": lat, "lon": lon})
    st["toast"] = rec("toast", lambda key, **kw: {"args": [key], "kwargs": dict(kw)})
    st["translate"] = rec("translate", lambda *a, **kw: {"args": list(a), "kwargs": dict(kw)})
    st["answer_or_note"] = arec("answer_or_note", lambda self_, query, text: {
        "self": "<self>", "query": query, "text": text})
    st["answer_callback"] = arec("answer_callback", lambda query, text=None, **kw: {
        "query": query, "text": text, **({} if not kw else {"kwargs": dict(kw)})})
    st["answer_callback_kw"] = arec("answer_callback_kw", lambda query, **kw: {"query": query, "kwargs": dict(kw)})
    st["edit_md_quiet"] = arec("edit_md_quiet", lambda self_, query, text: {"query": query, "text": text})
    st["build_animated_sticker_injection"] = rec("build_animated_sticker_injection", lambda emoji: {"emoji": emoji})
    st["get_cached_description"] = rec("get_cached_description", lambda fid: {"file_unique_id": fid})
    st["build_sticker_injection"] = rec("build_sticker_injection", lambda desc, emoji, set_name: {
        "description": desc, "emoji": emoji, "set_name": set_name})
    st["sticker_local_root"] = rec("sticker_local_root", lambda self_: {})
    st["fetch_telegram_file"] = arec("fetch_telegram_file", lambda obj, kind, root: {
        "kind": kind, "obj_id": getattr(obj, "file_id", None)})
    st["cache_image_from_bytes"] = arec("cache_image_from_bytes", lambda b, *, ext=None: {"ext": ext})
    st["accept_update"] = rec("accept_update", lambda self_: {})
    st["vision_analyze_tool"] = arec("vision_analyze_tool", lambda *, image_url, user_prompt: {
        "image_url": image_url, "user_prompt": user_prompt})
    st["json_loads"] = rec("json_loads", lambda s: {"s": s})
    st["cache_sticker_description"] = arec("cache_sticker_description", lambda fid, desc, emoji, set_name: {
        "file_unique_id": fid, "description": desc, "emoji": emoji, "set_name": set_name})
    st["callback_authorized"] = arec("callback_authorized", lambda self_, query, cb, unauth: {
        "query": query, "cb": cb, "unauth": unauth})
    st["unauthorized"] = rec("unauthorized", lambda: {})
    st["refuse_tap"] = arec("refuse_tap", lambda query, refusal: {"query": query, "refusal": refusal})
    st["update_prompt_refusal"] = rec("update_prompt_refusal", lambda user, **kw: {"user_id": getattr(user, "id", None), "kw": kw})
    st["get_hermes_home"] = rec("get_hermes_home", lambda: {}, build=lambda raw: _Path(raw))
    st["hash_for_log"] = rec("hash_for_log", lambda uid: {"user_id": uid})
    st["provider_get_label"] = rec("provider_get_label", lambda self_: {})
    st["parse_page"] = arec("parse_page", lambda self_, query, s: {"query": query, "s": s})
    st["picker_show_models"] = arec("picker_show_models", lambda self_, query, state, page: {
        "query": query, "page": page, "state": state})
    st["picker_show_providers"] = arec("picker_show_providers", lambda self_, query, state, page, label: {
        "query": query, "page": page, "state": state, "label": label})
    st["picker_selection"] = arec("picker_selection", lambda self_, query, state, s: {
        "query": query, "s": s, "state": state})
    st["picker_switch"] = arec("picker_switch", lambda self_, query, chat_id, model_id, slug, callback: {
        "query": query, "chat_id": chat_id, "model_id": model_id, "slug": slug, "callback": callback})
    st["to_thread"] = arec("to_thread", lambda fn, model_id, *, provider: {"model_id": model_id, "provider": provider})
    st["provider_groups_get"] = rec("provider_groups_get", lambda gid: {"group_id": gid})
    st["picker_back_cancel_row"] = rec("picker_back_cancel_row", lambda self_: {})
    st["picker_edit"] = arec("picker_edit", lambda self_, query, text, keyboard, **kw: {
        "query": query, "text": text, "keyboard": keyboard.rows if isinstance(keyboard, _Keyboard) else keyboard,
        **({} if not kw else {"kwargs": {k: _marker(v) for k, v in kw.items()}})})
    st["claim_callback_state"] = arec("claim_callback_state", lambda self_, query, cb, state, cid, unauth, resolved, **kw: {
        "query": query, "cb": cb, "state": state, "confirm_id": cid, "unauth": unauth,
        "resolved": resolved, "kw": dict(kw)})
    st["slash_confirm_refusal"] = rec("slash_confirm_refusal", lambda session_key, cid, uid, **kw: {
        "session_key": session_key, "confirm_id": cid, "user_id": uid, "kw": kw})
    st["refusal_text"] = rec("refusal_text", lambda requester: {"requester": requester})
    st["edit_keeping_prompt"] = arec("edit_keeping_prompt", lambda self_, query, text: {"query": query, "text": text})
    st["format_message"] = rec("format_message", lambda self_, text: {"text": text})
    st["link_preview_kwargs"] = rec("link_preview_kwargs", lambda self_: {})
    st["thread_kwargs_for_send"] = rec("thread_kwargs_for_send", lambda self_, chat, thread, meta, **kw: {
        "chat_id": chat, "thread_id": thread, "meta": meta, "kw": dict(kw)})
    st["send_message_with_thread_fallback"] = arec("send_message_with_thread_fallback", lambda **kw: dict(kw))
    st["slash_confirm_resolve"] = arec("slash_confirm_resolve", lambda session_key, cid, choice: {
        "session_key": session_key, "confirm_id": cid, "choice": choice})
    st["go_fatal_network"] = arec("go_fatal_network", lambda self_, message, log_message, *args: {
        "message": message, "log_message": log_message, "args": list(args)})
    st["looks_like_pool_timeout"] = rec("looks_like_pool_timeout", lambda self_, err: {"error": err})
    st["stop_updater_or_go_fatal"] = arec("stop_updater_or_go_fatal", lambda self_, app, reason, **kw: {
        "app": app, "reason": reason, "kw": dict(kw)})
    st["drain_general_connections"] = arec("drain_general_connections", lambda self_: {})
    st["drain_polling_connections"] = arec("drain_polling_connections", lambda self_: {})
    st["start_polling_once"] = arec("start_polling_once", lambda self_, app, **kw: {
        "app": app, "kw": {k: (_marker(v) if callable(v) else v) for k, v in kw.items()}})
    st["polling_error_callback_ref"] = rec("polling_error_callback_ref", lambda self_: {})
    st["sleep"] = arec("sleep", lambda delay: {"delay": delay})
    st["ensure_future"] = rec("ensure_future", lambda coro: {})
    st["background_add"] = rec("background_add", lambda tasks, task: {"task": task})
    st["task_done_callback"] = rec("task_done_callback", lambda task, cb: {})
    st["set_fatal_error"] = rec("set_fatal_error", lambda self_, kind, message, **kw: {
        "kind": kind, "message": message, "kw": dict(kw)})
    st["await_with_thread_deadline"] = arec("await_with_thread_deadline", lambda awaitable, **kw: {
        "kw": {k: (_marker(v) if callable(v) else v) for k, v in kw.items()}})
    st["handoff_polling_fatal_error"] = arec("handoff_polling_fatal_error", lambda self_: {})
    st["restart_polling_in_task"] = rec("restart_polling_in_task", lambda self_, coro: {})
    st["redact_telegram_error_text"] = rec("redact_telegram_error_text", lambda err, proxy=None: {
        "error": err, "proxy": proxy})
    st["looks_like_network_error"] = rec("looks_like_network_error", lambda self_, err: {"error": err})
    st["env_float"] = rec("env_float", lambda key, default: {"key": key, "default": default})
    st["app_initialize"] = arec("app_initialize", lambda self_: {})
    st["shutdown_abandoned_app"] = arec("shutdown_abandoned_app", lambda app: {})
    st["builder_build"] = rec("builder_build", lambda builder: {},
                              build=lambda raw: SimpleNamespace(bot=_Sentinel("bot"), initialize=st["app_initialize"]))
    _lraf = st["legacy_runner_auth_fn"]
    def _legacy_auth(*a, **k):
        raw = _lraf(*a, **k)
        return (lambda source: st["auth_fn_call"](source)) if raw else None
    st["legacy_runner_auth_fn"] = _legacy_auth
    st["wire_plugin_handlers"] = rec("wire_plugin_handlers", lambda self_, app: {"app": app})
    st["register_handlers"] = rec("register_handlers", lambda self_, app: {"app": app})
    st["admit_media"] = arec("admit_media", lambda self_, msg: {"msg": msg})
    st["should_observe_unmentioned"] = rec("should_observe_unmentioned", lambda self_, msg: {"msg": msg})
    st["media_message_type"] = rec("media_message_type", lambda self_, msg: {"msg": msg})
    st["clean_bot_trigger_text"] = rec("clean_bot_trigger_text", lambda self_, text: {"text": text})
    st["expand_link_entities"] = rec("expand_link_entities", lambda msg: {"msg": msg})
    st["cache_observed_media"] = arec("cache_observed_media", lambda self_, msg, event: {"msg": msg, "event": event})
    st["observe_unmentioned"] = rec("observe_unmentioned", lambda self_, msg, mtype, **kw: {
        "msg": msg, "message_type": mtype, "kw": dict(kw)})
    st["group_trigger_text"] = rec("group_trigger_text", lambda self_, msg, text: {"msg": msg, "text": text})
    st["ext_from_path"] = rec("ext_from_path", lambda self_, path, exts, default: {
        "path": path, "exts": exts, "default": default})
    st["set_cached_media"] = rec("set_cached_media", lambda self_, event, path, mime, mtype, msg: {
        "path": path, "mime": mime, "message_type": mtype, "msg": msg})
    st["route_photo_event"] = arec("route_photo_event", lambda self_, msg, event: {"msg": msg, "event": event})
    st["surface_media_cache_failure"] = arec("surface_media_cache_failure", lambda self_, msg, event, kind, err: {
        "kind": kind, "error": err})
    st["cache_inbound_voice"] = arec("cache_inbound_voice", lambda self_, msg, event, clips: {"clips": clips})
    st["cache_inbound_av"] = arec("cache_inbound_av", lambda self_, msg, event, obj, desc, kind, ext, mime: {
        "desc": desc, "kind": kind, "ext": ext, "mime": mime})
    st["cache_inbound_document"] = arec("cache_inbound_document", lambda self_, msg, event: {"msg": msg, "event": event})
    st["claim_pending_text"] = rec("claim_pending_text", lambda self_, msg, event: {"msg": msg, "event": event})
    st["absorb_claimed_text"] = rec("absorb_claimed_text", lambda self_, msg, event: {"msg": msg, "event": event})
    st["queue_media_group_event"] = arec("queue_media_group_event", lambda self_, gid, event: {
        "media_group_id": gid, "event": event})
    st["read_update_id"] = rec("read_update_id", lambda update: {"update": update})
    return st


class BaseDouble:
    pass


NS["tracking_media_items"] = lambda f: f
NS["BaseDouble"] = BaseDouble
_syn = ast.ClassDef(
    name="TADouble", bases=[ast.Name(id="BaseDouble", ctx=ast.Load())],
    keywords=[], body=[_NODES[n] for n in ORDER], decorator_list=[])
ast.fix_missing_locations(_syn)
_mod_node = ast.Module(body=[_syn], type_ignores=[])
ast.fix_missing_locations(_mod_node)
exec(compile(_mod_node, A_SRC, "exec", flags=_FLAGS, dont_inherit=True), NS)
TADouble = NS["TADouble"]
PORTED = {n: TADouble.__dict__[n] for n in ORDER}
ORIG_METHODS = dict(PORTED)


# exception classes used in control flow
for cname in ("_PollingStallError", "_PollingLifecycleAbort"):
    cls = type(cname, (Exception,), {})
    NS[cname] = cls


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def seed(st):
    NS["logger"] = SimpleNamespace(
        info=st["logger_info"], debug=st["logger_debug"], warning=st["logger_warning"],
        error=st["logger_error"])
    NS["_TOAST_LIMIT"] = _TOAST_LIMIT
    NS["MessageType"] = SimpleNamespace(
        COMMAND="COMMAND", TEXT="TEXT", LOCATION="LOCATION")
    NS["t"] = st["translate"]
    NS["contextlib"] = contextlib
    NS["asyncio"] = SimpleNamespace(
        CancelledError=asyncio.CancelledError, TimeoutError=TimeoutError,
        current_task=lambda: _cur_task(),
        get_running_loop=lambda: SimpleNamespace(time=lambda: _CUR.call("loop_time", {})),
        sleep=st["sleep"], ensure_future=st["ensure_future"], to_thread=st["to_thread"])
    NS["_toast"] = st["toast"]
    NS["_unauthorized"] = st["unauthorized"]
    NS["answer_or_note"] = st["answer_or_note"]
    NS["answer_callback"] = st["answer_callback"]
    NS["refuse_tap"] = st["refuse_tap"]
    NS["_redact_telegram_error_text"] = st["redact_telegram_error_text"]
    NS["is_redundant_live_refresh"] = st["redundant_live_refresh"]
    NS["normalize_telegram_chat_id"] = st["normalize_chat_id"]
    NS["_await_with_thread_deadline"] = st["await_with_thread_deadline"]
    NS["_shutdown_abandoned_app"] = st["shutdown_abandoned_app"]
    NS["env_float"] = st["env_float"]
    NS["update_prompt_refusal"] = st["update_prompt_refusal"]
    NS["admit_media"] = st["admit_media"]
    NS["expand_link_entities"] = st["expand_link_entities"]
    NS["claim_pending_text"] = st["claim_pending_text"]
    NS["absorb_claimed_text"] = st["absorb_claimed_text"]
    NS["fetch_telegram_file"] = st["fetch_telegram_file"]
    NS["cache_image_from_bytes_async"] = st["cache_image_from_bytes"]
    NS["cache_sticker_description_async"] = st["cache_sticker_description_async"] if "cache_sticker_description_async" in st else st["cache_sticker_description"]
    NS["vision_analyze_tool"] = st["vision_analyze_tool"]
    NS["local_root"] = st["sticker_local_root"]
    NS["_UPDATER_STOP_TIMEOUT"] = 10
    NS["json"] = SimpleNamespace(loads=st["json_loads"])
    NS["sys"] = sys
    NS["hash_for_log"] = st["hash_for_log"]
    NS["edit_keeping_prompt"] = st["edit_keeping_prompt"]
    NS["rich_message_text"] = st["rich_message_text"]
    NS["refresh_telegram_gates"] = st["refresh_gates"]
    NS["_scoped_gate_env"] = st["scoped_gate_env"]
    NS["Platform"] = SimpleNamespace(TELEGRAM="telegram")
    NS["logging"] = SimpleNamespace(INFO=20)
    NS["ParseMode"] = SimpleNamespace(MARKDOWN_V2="MARKDOWN_V2")
    NS["ChatType"] = SimpleNamespace(PRIVATE=SimpleNamespace(value="private"))
    NS["STICKER_VISION_PROMPT"] = "sticker-prompt"
    NS["_TELEGRAM_CHAT_OUTBOUND_BUDGET_SECS"] = 30.0
    NS["_TOAST_LIMIT"] = _TOAST_LIMIT

    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.telegram",
                "hermes_cli", "gateway", "tools", "tools.environments"):
        m = sys.modules.get(pkg) or types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m
    _mod("gateway.session", SessionSource=lambda **kw: _CUR.call("auth_source", kw))
    _mod("gateway.approval_actor",
         refusal_text=st["refusal_text"], slash_confirm_refusal=st["slash_confirm_refusal"],
         approval_refusal=rec("approval_refusal", lambda *a, **k: {"args": list(a)}))
    _mod("hermes_constants", get_hermes_home=st["get_hermes_home"],
         hash_for_log=st["hash_for_log"])
    _mod("hermes_cli.model_selection_guards", combined_selection_warning=st["to_thread"])
    _mod("hermes_cli.models_catalog_static", PROVIDER_GROUPS=_Groups(st["provider_groups_get"]))
    _mod("hermes_cli.providers", get_label=rec("provider_get_label", lambda s: {"slug": s}))
    _mod("plugins.platforms.telegram.telegram_context", group_trigger_text=st["group_trigger_text"])
    _mod("plugins.platforms.telegram.telegram_status_indicator", set_status_indicator=st["set_status_indicator"])
    _mod("plugins.platforms.telegram.telegram_batching",
         merge_batched_event=rec("merge_batched_event", lambda *a, **k: {"args": list(a)}),
         resolve_routing=rec("resolve_routing", lambda *a, **k: {"args": list(a)}))
    _mod("gateway.sticker_cache",
         get_cached_description=st["get_cached_description"],
         cache_sticker_description_async=st["cache_sticker_description"],
         build_sticker_injection=st["build_sticker_injection"],
         build_animated_sticker_injection=st["build_animated_sticker_injection"],
         STICKER_VISION_PROMPT="sticker-prompt")
    _mod("tools.vision_tools", vision_analyze_tool=st["vision_analyze_tool"])
    _mod("tools", slash_confirm=SimpleNamespace(resolve=st["slash_confirm_resolve"]))
    _mod("telegram", InlineKeyboardMarkup=lambda rows: _Keyboard(rows),
         InlineKeyboardButton=lambda **kw: _Sentinel("button"),
         InlineQueryResultArticle=lambda **kw: _Sentinel("article"),
         InputTextMessageContent=lambda text: _Sentinel("content"))
    NS["InlineKeyboardMarkup"] = lambda rows: _Keyboard(rows)
    NS["InlineKeyboardButton"] = lambda **kw: _Sentinel("button")
    NS["_cur_task_obj"] = None


class _Groups:
    def __init__(self, fn):
        self.fn = fn

    def get(self, gid, default=None):
        r = self.fn(gid)
        return default if r is None else r


def _cur_task():
    raw = _CUR.call("current_task", {})
    obj = _TaskRef("current")
    _CUR.task_obj = obj
    return obj


def make_self(state, st):
    inst = TADouble()
    for k, v in (state or {}).items():
        setattr(inst, k, ns_double(v))
    TADouble.name = "bot"
    TADouble.config = SimpleNamespace(extra={})
    TADouble._SPLIT_THRESHOLD = 4000
    TADouble._FOREIGN_BOT_HANDLE_RE = None
    for attr in ("_model_picker_state", "_slash_confirm_state", "_update_prompt_id",
                 "_pending_text_batches", "_telegram_chat_outbound_slot_until",
                 "_telegram_chat_outbound_slot_secs", "_telegram_typing_cooldown_seconds",
                 "_polling_error_task", "_polling_heartbeat_task", "_send_path_degraded",
                 "_teardown_started", "has_fatal_error", "fatal_error_code",
                 "_polling_network_error_count", "_polling_conflict_count",
                 "_polling_conflict_quiet_since", "_polling_conflict_quiet_banked",
                 "_app", "_bot", "_ptb_proxy_url", "_authorization_check"):
        if not hasattr(TADouble, attr):
            setattr(TADouble, attr, None)
    binds = {
        "_accept_update": "accept_update", "_notify_fatal_error": "notify_fatal_error",
        "_normalize_chat_type": "normalize_chat_type", "_is_sender_authorized": "is_sender_authorized",
        "_legacy_runner_auth_fn": "legacy_runner_auth_fn", "_env_allowlist_decision": "env_allowlist_decision",
        "_effective_update_message": "effective_update_message",
        "_is_user_authorized_from_message": "is_user_authorized_from_message",
        "_log_blocked_user": "log_blocked_user", "_gate_or_observe": "gate_or_observe",
        "_ensure_forum_commands": "ensure_forum_commands", "_build_triggered_event": "build_triggered_event",
        "_enqueue_text_event": "enqueue_text_event", "_build_message_event": "build_message_event",
        "_apply_telegram_group_observe_attribution": "apply_observe_attribution",
        "handle_message": "handle_message", "consume_vault_code_reply": "consume_vault_code_reply", "_consume_vault_code_reply": "consume_vault_code_reply",
        "_parse_page": "parse_page", "_picker_show_models": "picker_show_models",
        "_picker_show_providers": "picker_show_providers", "_picker_selection": "picker_selection",
        "_picker_switch": "picker_switch", "_picker_back_cancel_row": "picker_back_cancel_row",
        "_picker_edit": "picker_edit", "_provider_get_label": "provider_get_label",
        "_claim_callback_state": "claim_callback_state", "_callback_authorized": "callback_authorized",
        "_edit_md_quiet": "edit_md_quiet", "_edit_result_text": "edit_md_quiet", "_edit_keeping_prompt": "edit_keeping_prompt",
        "_format_message": "format_message", "_link_preview_kwargs": "link_preview_kwargs",
        "_thread_kwargs_for_send": "thread_kwargs_for_send",
        "_send_message_with_thread_fallback": "send_message_with_thread_fallback",
        "_go_fatal_network": "go_fatal_network", "_looks_like_pool_timeout": "looks_like_pool_timeout",
        "_stop_updater_or_go_fatal": "stop_updater_or_go_fatal",
        "_drain_general_connections_after_pool_timeout": "drain_general_connections",
        "_drain_polling_connections": "drain_polling_connections", "_start_polling_once": "start_polling_once",
        "_polling_error_callback_ref": "polling_error_callback_ref", "_set_fatal_error": "set_fatal_error",
        "_handoff_polling_fatal_error": "handoff_polling_fatal_error",
        "_restart_polling_in_task": "restart_polling_in_task", "_looks_like_network_error": "looks_like_network_error",
        "_wire_plugin_handlers": "wire_plugin_handlers", "_register_handlers": "register_handlers",
        "_should_observe_unmentioned_group_message": "should_observe_unmentioned",
        "_media_message_type": "media_message_type", "_clean_bot_trigger_text": "clean_bot_trigger_text",
        "_cache_observed_media": "cache_observed_media",
        "_observe_unmentioned_group_message": "observe_unmentioned",
        "_ext_from_path": "ext_from_path", "_set_cached_media": "set_cached_media",
        "_route_photo_event": "route_photo_event", "_surface_media_cache_failure": "surface_media_cache_failure",
        "_cache_inbound_av": "cache_inbound_av", "_cache_inbound_document": "cache_inbound_document",
        "_claim_pending_text": "claim_pending_text", "_absorb_claimed_text": "absorb_claimed_text",
        "_queue_media_group_event": "queue_media_group_event",
    }
    for m, seam in binds.items():
        setattr(TADouble, m, st[seam])
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
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    for n, m in ORIG_METHODS.items():
        setattr(TADouble, n, m)
    st = build_seams()
    ctx.st = st
    seed(st)
    inst = make_self(state, st)
    ctx.inst = inst
    fn = PORTED[func]

    def A(k):
        return ns_double(args.get(k))
    try:
        if func == "_hold_chat_outbound_slot":
            ret = fn(inst, args.get("chat_id"))

        elif func == "_handoff_polling_fatal_error":
            ret = run_call(fn, inst)

        elif func == "_warn_then":
            ret = run_call(fn, inst, args.get("media_key"), args.get("error"), st["fallback"]())

        elif func == "_set_status_indicator":
            ret = run_call(fn, inst, args.get("online"))

        elif func == "_instrument_polling_request":
            ret = fn(inst, _Sentinel("request"))

        elif func == "_is_callback_user_authorized":
            inst._authorization_check = args.get("has_auth_check", None)
            ret = fn(inst, args.get("user_id"), chat_id=args.get("chat_id"),
                     chat_type=args.get("chat_type"), thread_id=args.get("thread_id"),
                     user_name=args.get("user_name"))

        elif func == "_handle_text_message":
            ret = run_call(fn, inst, A("update"), A("context"))

        elif func == "_handle_location_message":
            ret = run_call(fn, inst, A("update"), A("context"))

        elif func == "_handle_sticker":
            msg = A("msg")
            event = A("event")
            ret = run_call(fn, inst, msg, event)

        elif func == "_handle_update_prompt_callback":
            ret = run_call(fn, inst, A("query"), args.get("data"), A("cb"))

        elif func == "_handle_model_picker_callback":
            inst._model_picker_state = copy.deepcopy(args.get("_model_picker_state", {})) if "_model_picker_state" in args else copy.deepcopy(state.get("_model_picker_state", {}))
            ret = run_call(fn, inst, A("query"), args.get("data"), args.get("chat_id"))

        elif func == "_handle_slash_confirm_callback":
            inst._slash_confirm_state = copy.deepcopy(state.get("_slash_confirm_state", {}))
            ret = run_call(fn, inst, A("query"), args.get("data"), A("cb"))

        elif func == "_initialize_app_with_retries":
            inst._app = SimpleNamespace(initialize=st["app_initialize"], bot=_Sentinel("bot"))
            ret = run_call(fn, inst, _Builder())

        elif func == "_handle_polling_network_error":
            ek = args.get("error_kind")
            err = NS["_PollingStallError"]("stall") if ek == "stall" else (
                TimeoutError("timeout") if ek == "timeout" else _Sentinel("error"))
            ret = run_call(fn, inst, err)

        elif func == "_handle_polling_conflict":
            ek = args.get("error_kind")
            err = TimeoutError("conflict") if ek == "timeout" else _Sentinel("error")
            ret = run_call(fn, inst, err)

        elif func == "_handle_media_message":
            ret = run_call(fn, inst, A("update"), A("context"))

        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)

    echo_state = {k: v for k, v in vars(inst).items() if not callable(v)}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(echo_state)}


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow15.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta15_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow15: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'(?:rec|arec|SeamFn)\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used &= set(SEAM_NAMES)
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow15: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(
        r'PoP:\s*ta15_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow15: PoP/KINDS drift: "
              f"c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow15.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow15: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow15: fixture func set != KINDS: "
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
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:600]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:600]}")
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
    print(f"ORACLE telegram_adapter_flow15: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow15.jsonl"
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
            r = drive(func, case.get("in") or {})
            if isinstance(r.get("ret"), dict) and r["ret"].get("__raise__") == "AttributeError":
                print(f"SELFTEST ATTR RAISE {func} case {n}: {r['ret']}")
                bad += 1
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST telegram_adapter_flow15: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


def dump():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow15.jsonl"
    out = _REPO / "tests" / "fixtures" / "telegram_adapter_flow15.wants.jsonl"
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
