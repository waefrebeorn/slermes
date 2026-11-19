#!/usr/bin/env python3
"""Oracle for base_flow.c — the platform-adapter base FLOW cores of
gateway/platforms/base.py (LIVE authority).

Drives the LIVE functions with SCRIPTED I/O: defs are AST-extracted from the
quarry (no heavy gateway import), module constants + class attributes seeded,
and every send/sleep/task/config/i18n/log/ledger dependency replaced by a
recording monkeypatch stub returning the SAME scripted results the C harness
feeds its universal seam. Compares the ordered DECISION SEQUENCE (trace of
seam calls) plus the return value and mutated adapter/event state.

Input: stdin JSON lines from t_port_base_flow:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...], "st":..., "ev":...}}
Summary: ORACLE base_flow: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
"""
import ast
import asyncio
import copy
import enum
import json
import math
import os
import re
import sys
import types
import weakref
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "platforms", "base.py")

# C harness func -> live python name. Module functions and class methods are
# both extracted; methods are called with a fake self as first argument.
PY = {
    "or_default": "_or_default",
    "thread_metadata_for_source": "_thread_metadata_for_source",
    "thread_metadata_for_event": "_thread_metadata_for_event",
    "mark_notify_metadata": "_mark_notify_metadata",
    "media_failure_text": "_media_failure_text",
    "should_send_media_as_audio": "should_send_media_as_audio",
    "custom_unit_to_cp": "_custom_unit_to_cp",
    "fit_prefix": "fit_prefix",
    "prefix_within_utf16_limit": "_prefix_within_utf16_limit",
    "has_media_directives": "_has_media_directives",
    "terminal_sentinel_start": "_terminal_sentinel_start",
    "code_spans": "_code_spans",
    "blank_spans": "_blank_spans",
    "delete_spans": "_delete_spans",
    "append_text": "_append_text",
    "any_in": "_any_in",
    "classify_send_error": "classify_send_error",
    "split_post_delivery_entry": "_split_post_delivery_entry",
    "extract_images": "extract_images",
    "coerce_float_extra": "_coerce_float_extra",
    "configure_text_batch_delays": "_configure_text_batch_delays",
    "text_batch_delay_for": "_text_batch_delay_for",
    "owner_transport_profile": "_owner_transport_profile",
    "is_rate_limited_error": "_is_rate_limited_error",
    "is_timeout_error": "_is_timeout_error",
    "send_retry_is_final": "_send_retry_is_final",
    "is_partial_delivery": "_is_partial_delivery",
    "merge_caption": "_merge_caption",
    "requeue_backoff_delay": "_requeue_backoff_delay",
    "get_human_delay": "_get_human_delay",
    "wants_auto_tts": "_wants_auto_tts",
    "shared_runtime_owns_input": "_shared_runtime_owns_input",
    "clear_session_guard": "_clear_session_guard",
    "warning_notifications_enabled": "warning_notifications_enabled",
    "warning_text": "warning_text",
    "exec_approval_actions": "_exec_approval_actions",
    "exec_approval_prompt_choices": "choices",
    "text_debounce_cancel_timer": "cancel_timer",
    "unauthorized_action_notice": "unauthorized_action_notice",
    "get_inbound_media_max_bytes": "get_inbound_media_max_bytes",
    "validate_inbound_media_size": "validate_inbound_media_size",
    "read_httpx_body_with_limit": "read_httpx_body_with_limit",
    "is_network_accessible": "is_network_accessible",
    "detect_macos_system_proxy": "_detect_macos_system_proxy",
    "reset_macos_proxy_cache": "reset_macos_proxy_cache",
    "resolve_proxy_url": "resolve_proxy_url",
    "config_section": "_config_section",
    "proxy_kwargs_for_aiohttp": "proxy_kwargs_for_aiohttp",
    "aiohttp_socks_connector": "_aiohttp_socks_connector",
    "should_bypass_proxy": "should_bypass_proxy",
    "is_host_excluded_by_no_proxy": "is_host_excluded_by_no_proxy",
    "format_message": "format_message",
    "message_len_fn": "message_len_fn",
    "supports_draft_streaming": "supports_draft_streaming",
    "send_draft": "send_draft",
    "mark_degraded": "_mark_degraded",
    "lazy_attr": "_lazy_attr",
    "accepts_kwarg": "_accepts_kwarg",
    "prepare_tts_text": "prepare_tts_text",
    "media_delivery_scope": "_media_delivery_scope",
    "pending_recovery_owners": "_pending_recovery_owners",
    "track_session_task": "_track_session_task",
    "stop_typing_quietly": "_stop_typing_quietly",
    "start_typing_refresh": "_start_typing_refresh",
    "chain_callbacks": "_chain_callbacks",
    "send_plain_fallback": "_send_plain_fallback",
    "resume_partial_send": "_resume_partial_send",
    "play_tts_file": "_play_tts_file",
    "record_delivery_obligation": "_record_delivery_obligation",
    "finalize_delivery_obligation": "_finalize_delivery_obligation",
    "fire_post_delivery_callback": "_fire_post_delivery_callback",
    "release_turn_marker": "_release_turn_marker",
    "ledger_attempted_final": "ledger_attempted_final",
    "session_key_profile": "_session_key_profile",
    "canonicalize": "_canonicalize",
    "drop_unresolved": "_drop_unresolved",
    "ea_fit": "_ea_fit",
    "ea_deadline_line": "_ea_deadline_line",
    "exec_approval_cmd_budget": "_exec_approval_cmd_budget",
    "supports_exec_approval_buttons": "supports_exec_approval_buttons",
    "event_session_key": "_event_session_key",
    "source_session_key": "_source_session_key",
    "text_batch_key": "_text_batch_key",
    "pop_text_batch": "_pop_text_batch",
    "dispatch_text_batch": "_dispatch_text_batch",
    "handle_message_while_active": "_handle_message_while_active",
    "deliver_media_attachments": "_deliver_media_attachments",
    "send_image_batch": "_send_image_batch",
    "acquire_platform_lock": "_acquire_platform_lock",
    "release_platform_lock": "_release_platform_lock",
    "wire_plugin_handlers": "_wire_plugin_handlers",
    "rewire_plugin_handlers": "rewire_plugin_handlers",
    "enqueue_text_event": "_enqueue_text_event",
    "flush_text_batch": "_flush_text_batch",
    "flush_text_batch_now": "_flush_text_batch_now",
    "finish_session_task": "_finish_session_task",
    "spawn_drain_task": "_spawn_drain_task",
    "drain_after": "_drain_after",
    "send_final_ledgered": "send_final_ledgered",
    "send_final_text": "_send_final_text",
    "notify_turn_error": "_notify_turn_error",
    "deliver_attachments": "_deliver_attachments",
    "emit_warning": "emit_warning",
    "emit_media_warning": "emit_media_warning",
    "send_media_fallback_notice": "_send_media_fallback_notice",
    "send_exec_approval": "send_exec_approval",
    "send_clarify": "send_clarify",
    "build_source": "build_source",
    "dispatch_inline_reply": "_dispatch_inline_reply",
    "send_image": "send_image",
    "send_animation": "send_animation",
    "send_voice": "send_voice",
    "send_video": "send_video",
    "send_document": "send_document",
    "send_image_file": "send_image_file",
    "play_tts": "play_tts",
    "send_slash_confirm": "send_slash_confirm",
    "send_exec_approval_prompt_default": "_send_exec_approval_prompt",
}
WANTED = set(PY.values())
# Live sibling defs the targets call that must also be extracted.
SIBLINGS = {
    "_platform_name", "_reply_anchor_for_event", "_error_blob", "utf16_len",
    "_should_auto_tts_for_chat", "_streaming_tts_turn_key",
    "_streaming_tts_turn_completed", "_get_ephemeral_system_ttl_default",
    "_unwrap_ephemeral", "_is_retryable_error", "_send_retry_is_final",
    "_is_partial_delivery", "_resume_partial_send", "_send_plain_fallback",
    "_merge_caption", "_text_debounce_store", "_is_queue_text_debounce_candidate",
    "_can_merge_text_debounce_events", "_text_debounce_delay",
    "_flush_text_debounce_now", "_release_session_guard", "_session_task_is_stale",
    "_start_session_processing", "_track_session_task", "_clear_session_guard",
    "_cleanup_finished_session_task", "_final_delivery_adapter",
    "_notify_media_delivery_failure", "_schedule_ephemeral_delete",
    "_stop_typing_with_metadata", "_accepts_kwarg", "_chain_callbacks",
    "pop_post_delivery_callback", "register_post_delivery_callback",
    "_split_post_delivery_entry", "_lazy_attr", "get_pending_message",
    "_event_session_key", "_source_session_key", "_session_key_profile",
    "_canonicalize", "_owner_transport_profile", "_drop_unresolved",
    "_text_batch_key", "_text_batch_delay_for", "_pop_text_batch",
    "_dispatch_text_batch", "_send_image_batch", "_get_human_delay",
    "_deliver_media_attachments", "_mark_degraded", "_write_runtime_status_safe",
    "_set_fatal_error", "warning_notifications_enabled", "warning_text",
    "emit_warning", "emit_media_warning", "_send_media_fallback_notice",
    "_media_failure_text", "should_send_media_as_audio", "_exec_approval_actions",
    "_ea_escape", "_ea_fit", "send_final_ledgered", "_release_turn_marker",
    "_send_final_text", "handle_message", "_shared_runtime_owns_input",
    "_pending_recovery_owners", "_dispatch_inline_reply", "_send_with_retry",
    "send", "send_image", "send_animation", "send_voice", "send_video",
    "send_document", "send_image_file", "send_multiple_images", "play_tts",
    "delete_message", "_keep_typing", "stop_typing", "send_typing",
    "_process_message_background", "_queue_text_debounce",
    "_dispatch_active_session_command", "_run_processing_hook",
    "pause_typing_for_chat", "resume_typing_for_chat",
    "_fire_post_delivery_callback", "_finish_session_task", "_spawn_drain_task",
    "_drain_after", "_requeue_backoff_delay", "_heal_stale_session_lock",
    "_discard_text_debounce", "extract_media", "extract_images",
    "extract_local_files", "filter_media_delivery_paths",
    "filter_local_delivery_paths", "_extract_response_content",
    "_bounded_history_media_paths_for_session", "_wants_auto_tts",
    "_synthesize_auto_tts", "_play_tts_file", "_record_delivery_obligation",
    "_finalize_delivery_obligation", "ledger_attempted_final",
    "_deliver_attachments", "send_exec_approval", "_send_exec_approval_prompt",
    "send_clarify", "send_slash_confirm", "build_source", "format_message",
    "truncate_message", "_mark_connected", "_mark_disconnected",
    "max_message_length_for_chat", "message_len_fn_for_chat",
    "_EA_ACTION_LABELS",
}
ALL_DEFS = WANTED | SIBLINGS
CONSTS = {
    "_AUDIO_MIME_TYPES", "_AUDIO_EXTS", "_VIDEO_EXTS", "_IMAGE_EXTS",
    "_TELEGRAM_AUDIO_ATTACHMENT_EXTS", "_TELEGRAM_VOICE_EXTS", "_MEDIA_KIND_KEYS",
    "_RETRYABLE_ERROR_PATTERNS", "_SEND_RETRY_INLINE_WAIT_CAP_SECS",
    "SEND_ERROR_KINDS", "_CHAT_LEVEL_NOT_FOUND_SUBSTRINGS",
    "_SUBCHAT_NOT_FOUND_SUBSTRINGS", "_SEND_ERROR_CLASSIFIERS",
    "DEFAULT_BUSY_TEXT_DEBOUNCE_SECONDS", "DEFAULT_BUSY_TEXT_HARD_CAP_SECONDS",
    "_TERMINAL_SENTINEL", "_FENCED_CODE_RE", "_INLINE_CODE_RE",
    "MD_IMAGE_TAG_PATTERN", "HTML_IMAGE_TAG_PATTERN", "_IMAGE_URL_MARKERS",
    "SUPPORTED_DOCUMENT_TYPES", "SUPPORTED_VIDEO_TYPES",
    "SUPPORTED_IMAGE_DOCUMENT_TYPES", "_MIME_TO_EXT",
    "DEFAULT_INBOUND_MEDIA_MAX_BYTES", "UNAUTHORIZED_ACTION_NOTICE",
    "_POST_DELIVERY_CALLBACK_TIMEOUT_SECONDS", "MEDIA_DELIVERY_EXTS",
}
# Class attributes of BasePlatformAdapter the methods read via self.
CLASS_ATTRS = {
    "supports_code_blocks", "supports_status_text", "supports_async_delivery",
    "splits_long_messages", "typed_command_prefix", "supports_inchannel_continuable",
    "interactive_resume", "serves_profile_prefix", "gateway_runner",
    "DEGRADED_STATUS_MESSAGE", "_SPLIT_THRESHOLD", "_text_batch_delay_seconds",
    "_text_batch_split_delay_seconds", "_TEXT_BATCH_DEFAULT_DELAY_S",
    "_TEXT_BATCH_MAX_DELAY_S", "_TEXT_BATCH_DEFAULT_SPLIT_DELAY_S",
    "_TEXT_BATCH_MAX_SPLIT_DELAY_S", "_EA_CODE_OPEN", "_EA_CODE_CLOSE",
    "_EA_DEADLINE_PREFIX", "_EA_CMD_BUDGET", "_EA_CARD_CAP", "_EA_REASON_BUDGET",
    "_EA_ACTION_STYLES", "_ACK_EMOJI", "_OK_EMOJI", "_FAIL_EMOJI",
    "_REQUEUE_BACKOFF_INITIAL_SECONDS", "_REQUEUE_BACKOFF_MAX_SECONDS",
    "_plugin_handler_native", "_plugin_handlers_wired", "REQUIRES_EDIT_FINALIZE",
}


class RecLogger:
    def _log(self, level, fmt, *args):
        try:
            msg = fmt % args if args else fmt
        except Exception:
            msg = fmt
        CUR["trace"].append({"ev": "log", "args": {"level": level, "msg": msg}, "ret": None})

    def debug(self, fmt, *args, **kw): self._log("debug", fmt, *args)
    def info(self, fmt, *args, **kw): self._log("info", fmt, *args)
    def warning(self, fmt, *args, **kw): self._log("warning", fmt, *args)
    def error(self, fmt, *args, **kw): self._log("error", fmt, *args)


CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "tasks": {}, "guards": {}}


def cin(key, default=None):
    return CUR["in"].get(key, default)


def script(name):
    sc = CUR["scripts"].get(name)
    if sc is None:
        return None
    if isinstance(sc, list):
        i = CUR["idx"].get(name, 0)
        CUR["idx"][name] = i + 1
        return sc[min(i, len(sc) - 1)]
    return sc


def rec(name, args, ret):
    CUR["trace"].append({"ev": name, "args": args, "ret": ret})


class ScopedLockDirUnusable(Exception):
    def __init__(self, filename):
        super().__init__(filename)
        self.filename = filename


class ProfileRouteRejected(Exception):
    def __init__(self, profile):
        super().__init__(profile)
        self.profile = profile


class _GaiError(Exception):
    pass


EXC_MAP = {
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "RuntimeError": RuntimeError, "ImportError": ImportError, "OSError": OSError,
    "Exception": Exception, "gaierror": _GaiError,
    "ScopedLockDirUnusable": ScopedLockDirUnusable,
    "ProfileRouteRejected": ProfileRouteRejected,
    "CancelledError": asyncio.CancelledError,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        cls = EXC_MAP.get(r["__raise__"], RuntimeError)
        if cls is ScopedLockDirUnusable:
            raise cls(r.get("filename", ""))
        if cls is ProfileRouteRejected:
            raise cls(r.get("profile"))
        raise cls(r.get("msg", ""))
    return r


def norm(o):
    """Normalize a live result/state value to the JSON model the C side uses."""
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    sr_cls = ns_g.get("SendResult")
    if sr_cls is not None and isinstance(o, sr_cls):
        keys = getattr(o, "_script_keys", None)
        if keys is not None:
            return {k: norm(getattr(o, k)) for k in keys}
        # internally constructed result: the C port builds only the fields it
        # assigns — project onto non-default fields (success always present)
        out = {"success": norm(o.success)}
        for k, dflt in (("message_id", None), ("error", None), ("raw_response", None),
                        ("retryable", False), ("retry_after", None),
                        ("continuation_message_ids", ()), ("error_kind", None)):
            v = getattr(o, k)
            if v != dflt:
                out[k] = norm(v)
        return out
    if isinstance(o, enum.Enum):
        if type(o).__name__ == "Platform":
            return {"value": o.value, "name": o.name}
        return o.value
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, dict):
        return {k: norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        if all(isinstance(v, TaskDouble) for v in o):
            return sorted(v.id for v in o)
        return sorted((norm(v) for v in o), key=lambda x: json.dumps(x, sort_keys=True, default=str))
    if isinstance(o, weakref.ReferenceType):
        return "self"
    if isinstance(o, EventDouble):
        return {"id": o.id, "set": o.is_set()}
    if isinstance(o, TaskDouble):
        d = {"id": o.id, "done": o.done()}
        if o.cancelled:
            d["cancelled"] = True
        if o.unhashable:
            d["unhashable"] = True
        if o.done_callbacks:
            d["done_callbacks"] = o.done_callbacks
        return d
    if hasattr(o, "__dict__"):
        return {k: norm(v) for k, v in vars(o).items()
                if not callable(v) or isinstance(v, weakref.ReferenceType)}
    return str(o)


class EventDouble:
    _next_id = 1

    def __init__(self, id=None, set=False):
        self.id = id if id is not None else EventDouble._next_id
        EventDouble._next_id += 1
        self._set = set

    def set(self): self._set = True
    def clear(self): self._set = False
    def is_set(self): return self._set


class TaskDouble:
    def __init__(self, id, done=False, cancelled=False, unhashable=False):
        self.id = id
        self._done = done
        self.cancelled = cancelled
        self.unhashable = unhashable
        self.done_callbacks = 0

    def done(self): return self._done
    def cancel(self): self.cancelled = True
    def add_done_callback(self, cb): self.done_callbacks += 1
    def __hash__(self):
        if self.unhashable:
            raise TypeError("unhashable")
        return hash(self.id)


def task_double(spec):
    if spec is None:
        return None
    if isinstance(spec, TaskDouble):
        return spec
    tid = spec.get("id")
    if tid in CUR["tasks"]:
        return CUR["tasks"][tid]
    t = TaskDouble(tid, spec.get("done", False), spec.get("cancelled", False),
                   spec.get("unhashable", False))
    CUR["tasks"][tid] = t
    return t


def guard_double(spec):
    if spec is None:
        return None
    if isinstance(spec, EventDouble):
        return spec
    gid = spec.get("id")
    if gid in CUR["guards"]:
        g = CUR["guards"][gid]
        g._set = spec.get("set", False)
        return g
    g = EventDouble(gid, spec.get("set", False))
    CUR["guards"][gid] = g
    return g

# ── extraction ──

def extract():
    import abc
    import contextlib
    import inspect
    import logging
    import threading
    import time
    import uuid
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "gateway.platforms.base_extracted",
        "re": re, "os": os, "math": math, "sys": sys, "json": json,
        "asyncio": asyncio, "contextlib": contextlib, "inspect": inspect,
        "threading": threading, "time": time, "uuid": uuid, "weakref": weakref,
        "Path": Path, "dataclass": dataclass, "field": field,
        "Any": Any, "Callable": Callable, "Optional": Optional,
        "logger": RecLogger(),
        "Enum": enum.Enum, "ipaddress": __import__("ipaddress"),
        "contextlib": __import__("contextlib"),
        "ABC": abc.ABC, "abstractmethod": abc.abstractmethod,
        # Upstream observability decorator on _send_with_retry: awaits the
        # wrapped send and returns its result unchanged (metrics side-effects
        # only, no seam in the port's table) -> transparent for the flow.
        "records_delivery": lambda f: f,
    }
    tree = ast.parse(src)
    # Module constants (plain-Name Assign/AnnAssign only; skip I/O assigns).
    const_nodes = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            const_nodes.append(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            const_nodes.append(node)

    def seed_consts():
        for node in const_nodes:
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    seed_consts()
    # Dataclasses + enums + EphemeralReply defined at module level.
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in (
                "SendResult", "EphemeralReply", "TextDebounceState",
                "ExecApprovalPrompt", "_ExtractedResponse", "CachedMedia"):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # Module-level functions.
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in ALL_DEFS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # Class methods + class attributes.
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "BasePlatformAdapter")
    class_attrs = {}
    for sub in cls.body:
        if isinstance(sub, ast.Assign):
            for t in sub.targets:
                if isinstance(t, ast.Name) and t.id in CLASS_ATTRS:
                    try:
                        val = eval(compile(ast.Expression(sub.value), SRC, "eval"), ns)
                        class_attrs[t.id] = val
                    except Exception:
                        pass
        elif isinstance(sub, ast.AnnAssign) and isinstance(sub.target, ast.Name) and sub.value:
            if sub.target.id in CLASS_ATTRS:
                try:
                    class_attrs[sub.target.id] = eval(compile(ast.Expression(sub.value), SRC, "eval"), ns)
                except Exception:
                    pass
        elif isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in ALL_DEFS:
            exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
    ns["_CLASS_ATTRS"] = class_attrs
    # agent.retry_utils.jittered_backoff — live base.py imports it for the
    # requeue backoff; extract the real function with its module globals.
    ru_src = open(os.path.join(QUARRY, "agent", "retry_utils.py"), encoding="utf-8").read()
    ru_ns = {"time": time, "random": __import__("random"), "threading": threading,
             "_jitter_counter": 0, "_jitter_lock": threading.Lock()}
    for node in ast.parse(ru_src).body:
        if isinstance(node, ast.FunctionDef) and node.name == "jittered_backoff":
            exec(compile(ast.Module(body=[node], type_ignores=[]), "agent/retry_utils.py", "exec"), ru_ns)
    ns["jittered_backoff"] = ru_ns["jittered_backoff"]
    # Re-seed constants that depend on extracted functions (e.g.
    # _SEND_ERROR_CLASSIFIERS references _any_in / _error_blob).
    seed_consts()
    # Enums from sibling modules (live member values).
    ev = ast.parse(open(os.path.join(QUARRY, "gateway", "platforms", "event.py"), encoding="utf-8").read())
    for cn in ("MessageType", "ProcessingOutcome"):
        c = next(n for n in ev.body if isinstance(n, ast.ClassDef) and n.name == cn)
        members = {n.targets[0].id: n.value.value for n in c.body
                   if isinstance(n, ast.Assign) and isinstance(n.value, ast.Constant)}
        ns[cn] = enum.Enum(cn, members)
    cfg = ast.parse(open(os.path.join(QUARRY, "gateway", "config.py"), encoding="utf-8").read())
    pc = next(n for n in cfg.body if isinstance(n, ast.ClassDef) and n.name == "Platform")
    pm = {n.targets[0].id: n.value.value for n in pc.body
          if isinstance(n, ast.Assign) and isinstance(n.value, ast.Constant)}
    ns["Platform"] = enum.Enum("Platform", pm)
    # SessionSource double (build_source constructs it; live reads attrs).
    class SessionSource:
        def __init__(self, **kw):
            self.__dict__.update(kw)
    ns["SessionSource"] = SessionSource
    missing = (WANTED | CONSTS) - set(ns)
    # choices/cancel_timer are dataclass members, present on the classes.
    missing -= {"choices", "cancel_timer"}
    if missing:
        print("FATAL base_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    if "choices" not in WANTED or not hasattr(ns.get("ExecApprovalPrompt"), "choices"):
        print("FATAL base_flow: ExecApprovalPrompt.choices missing")
        sys.exit(2)
    return ns


# ── scripted-stub machinery ──

def ga(a, k, idx, names, default=None):
    for nm in names:
        if nm in k:
            return k[nm]
    if idx < len(a):
        return a[idx]
    return default


def na(d):
    """Normalize args dict, dropping None-valued keys (absent == None)."""
    return {k: norm(v) for k, v in d.items() if v is not None}


def materialize(kind, r):
    if r is None:
        return None
    if kind == "sendresult":
        if isinstance(r, dict):
            sr = ns_g["SendResult"](**r)
            # C passes the scripted dict through verbatim; norm() must project
            # the live dataclass back onto exactly the scripted keys. Store on
            # the object (id() keys would collide after GC reuse).
            sr._script_keys = list(r.keys())
            return sr
        return r
    if kind == "lockresult":
        # live unpacks: acquired, existing = acquire_scoped_lock(...)
        if isinstance(r, dict):
            return (r.get("acquired", False), r.get("existing"))
        return (bool(r), None)
    if kind == "task":
        return task_double(r)
    if kind == "guard":
        return guard_double(r)
    if kind == "ns":
        return types.SimpleNamespace(**r) if isinstance(r, dict) else r
    if kind == "event":
        return DictEvent(r) if isinstance(r, dict) else r
    if kind == "bool":
        return bool(r)
    return r


ns_g = {}


def mk_stub(seam, argspec, kind="json", is_async=False, strip_self=False):
    def body(a, k):
        if strip_self:
            a = a[1:]
        args = na(argspec(a, k))
        r = script(seam)
        rec(seam, args, r)
        return materialize(kind, maybe_raise(r))

    if is_async:
        async def stub(*a, **k):
            return body(a, k)
        return stub

    def stub(*a, **k):
        return body(a, k)
    return stub


class DictEvent:
    # live MessageEvent dataclass field defaults for fields the flows read but
    # fixtures omit; returned WITHOUT storing so snapshots stay fixture-shaped
    _FIELD_DEFAULTS = {"media_urls": [], "media_types": [], "media_text_inlined": []}

    def __init__(self, d):
        self.__dict__.update(copy.deepcopy(d or {}))

    def __getattr__(self, name):
        if name in DictEvent._FIELD_DEFAULTS:
            return copy.deepcopy(DictEvent._FIELD_DEFAULTS[name])
        raise AttributeError(name)


def _event_stubs(e):
    """Seam stubs for the live MessageEvent methods the flows call. The
    receiver is the event itself (C records it as event/existing)."""
    e.get_command = mk_stub("get_command", lambda a, k: {"event": e}, "json", False, False)
    e.is_command = mk_stub("is_command", lambda a, k: {"event": e}, "bool", False, False)
    e.extend_media = mk_stub("extend_media",
                             lambda a, k: {"existing": e, "event": ga(a, k, 0, ["other", "event"])},
                             "json", False, False)
    e.absorb_turn_input = mk_stub("absorb_turn_input",
                                  lambda a, k: {"existing": e, "event": ga(a, k, 0, ["other", "event"])},
                                  "json", False, False)
    return e


def ev(d):
    e = DictEvent(d)
    mt = e.__dict__.get("message_type")
    if isinstance(mt, str) and "MessageType" in ns_g:
        try:
            e.message_type = ns_g["MessageType"](mt)
        except ValueError:
            pass
    if isinstance(e.__dict__.get("source"), dict):
        e.source = src_double(e.source)
    # asyncio.Event-typed fields materialize to guard doubles (.set() etc.)
    if isinstance(e.__dict__.get("_gateway_marker_released"), dict):
        e._gateway_marker_released = guard_double(e._gateway_marker_released)
    _event_stubs(e)
    CUR["ev"] = e
    return e


def src_double(d):
    """dict -> attribute double (live reads getattr(source, ...)); nested dicts
    become attribute doubles too (e.g. platform {value})."""
    if d is None:
        return None
    if not isinstance(d, dict):
        return d
    return types.SimpleNamespace(**{k: src_double(v) for k, v in d.items()})


# argspec builders (seam name -> f(positional-after-self, kwargs)).
def _ev(a, k): return {"event": ga(a, k, 0, ["event"])}
def _src(a, k): return {"source": ga(a, k, 0, ["source"])}


ARGSPEC = {
    "reply_anchor": _ev,
    "t": lambda a, k: {"key": ga(a, k, 0, ["key"]), "kwargs": ga(a, k, 1, ["kwargs"], {})},
    "measure": lambda a, k: {"s": ga(a, k, 0, ["s"])},
    "uniform": lambda a, k: {"lo": ga(a, k, 0, ["lo"]), "hi": ga(a, k, 1, ["hi"])},
    "should_auto_tts": lambda a, k: {"chat_id": ga(a, k, 0, ["chat_id"])},
    "tts_turn_completed": lambda a, k: {"session_key": ga(a, k, 0, ["session_key"]),
                                        "generation": ga(a, k, 1, ["generation"]),
                                        "event": k.get("event")},
    "get_command": _ev,
    "is_command": _ev,
    "warning_policy": lambda a, k: {"platform": ga(a, k, 0, ["platform"])},
    "ea_labels": lambda a, k: {},
    "config_section": lambda a, k: {"name": ga(a, k, 0, ["name"])},
    "load_config": lambda a, k: {},
    "scoped_secret": lambda a, k: {"name": ga(a, k, 0, ["name"]), "default": ga(a, k, 1, ["default"])},
    "trust_env": lambda a, k: {},
    "first_proxy_env": lambda a, k: {},
    "normalize_proxy": lambda a, k: {"value": ga(a, k, 0, ["value"])},
    "macos_proxy": lambda a, k: {},
    "bypass": lambda a, k: {"target_hosts": ga(a, k, 0, ["target_hosts"])},
    "socks_connector": lambda a, k: {"proxy_url": ga(a, k, 0, ["proxy_url"])},
    "import_connector": lambda a, k: {"proxy_url": ga(a, k, 0, ["proxy_url"]), "rdns": k.get("rdns")},
    "safe_proxy_url": lambda a, k: {"url": ga(a, k, 0, ["url"])},
    "bypass_matcher": lambda a, k: {"target_hosts": ga(a, k, 0, ["target_hosts"]),
                                    "no_proxy_value": k.get("no_proxy_value")},
    "is_darwin": lambda a, k: {},
    "monotonic": lambda a, k: {},
    "scutil": lambda a, k: {},
    "write_status": lambda a, k: {"platform": k.get("platform"),
                                  "platform_state": k.get("platform_state"),
                                  "error_code": k.get("error_code"),
                                  "error_message": k.get("error_message")},
    "factory": lambda a, k: {"name": ga(a, k, 0, ["name"])},
    "spoken": lambda a, k: {"text": ga(a, k, 0, ["text"]), "max_chars": k.get("max_chars")},
    "scope_resolve": _src,
    "delivery_adapter": _src,
    "stop_typing_meta": lambda a, k: {"chat_id": ga(a, k, 0, ["chat_id"]), "metadata": ga(a, k, 1, ["metadata"])},
    "typing_wanted": lambda a, k: {"event": ga(a, k, 1, ["event"])},
    "accepts_stop_event": lambda a, k: {"name": ga(a, k, 1, ["name"]), "var_kw": k.get("var_kw"),
                                        "unknown": k.get("unknown")},
    "send": lambda a, k: {"chat_id": ga(a, k, 0, ["chat_id"]), "content": ga(a, k, 1, ["content"]),
                          "reply_to": ga(a, k, 2, ["reply_to"]), "metadata": ga(a, k, 3, ["metadata"])},
    "record_delivery": lambda a, k: {"result": ga(a, k, 0, ["result"])},
    "play_tts": lambda a, k: {"chat_id": k.get("chat_id"), "audio_path": k.get("audio_path"),
                              "caption": k.get("caption"), "metadata": k.get("metadata")},
    "record_final_obligation": lambda a, k: {"event": ga(a, k, 1, ["event"]),
                                             "session_key": ga(a, k, 2, ["session_key"]),
                                             "text_content": ga(a, k, 3, ["text_content"]),
                                             "delivery_adapter": ga(a, k, 4, ["delivery_adapter"]),
                                             "is_ephemeral_response": ga(a, k, 5, ["is_ephemeral_response"])},
    "finalize_final_obligation": lambda a, k: {"obligation_id": ga(a, k, 1, ["obligation_id"]),
                                               "result": ga(a, k, 2, ["result"]),
                                               "event": ga(a, k, 3, ["event"]),
                                               "delivery_adapter": ga(a, k, 4, ["delivery_adapter"])},
    "pop_post_delivery": lambda a, k: {"session_key": ga(a, k, 0, ["session_key"]),
                                       "generation": k.get("generation")},
    "run_post_delivery": lambda a, k: {"callback": ga(a, k, 0, ["callback"]), "timeout": ga(a, k, 1, ["timeout"])},
    "clear_durable_turn": _ev,
    "extract_text": lambda a, k: {"response": ga(a, k, 0, ["response"]), "event": ga(a, k, 1, ["event"]),
                                  "session_key": ga(a, k, 2, ["session_key"]),
                                  "is_ephemeral_response": k.get("is_ephemeral_response")},
    "ledger_attempted": lambda a, k: {"event": ga(a, k, 1, ["event"]), "session_key": ga(a, k, 2, ["session_key"]),
                                      "text": ga(a, k, 3, ["text"]), "error": ga(a, k, 4, ["error"])},
    "identity_of": _src,
    "store_resolve": _src,
    "canonical_identity": lambda a, k: {"source": ga(a, k, 0, ["source"]),
                                        "transport_profile": k.get("transport_profile")},
    "drop_rejected_route": lambda a, k: {"source": ga(a, k, 1, ["source"])},
    "ea_escape": lambda a, k: {"text": ga(a, k, 0, ["text"])},
    "approval_timeout": lambda a, k: {},
    "deadline_line": lambda a, k: {"timeout": ga(a, k, 0, ["timeout"])},
    "cmd_budget": lambda a, k: {"description": ga(a, k, 1, ["description"]),
                                "smart_denied": ga(a, k, 2, ["smart_denied"])},
    "dm_thread_key_source": lambda a, k: {"source": ga(a, k, 0, ["source"]), "extra": ga(a, k, 1, ["extra"]),
                                          "store": ga(a, k, 2, ["store"])},
    "build_session_key": lambda a, k: {"source": ga(a, k, 0, ["source"]),
                                       "group_sessions_per_user": k.get("group_sessions_per_user"),
                                       "thread_sessions_per_user": k.get("thread_sessions_per_user"),
                                       "profile": k.get("profile")},
    "dm_thread_batch_suffix": lambda a, k: {"source": ga(a, k, 0, ["source"]), "extra": ga(a, k, 1, ["extra"])},
    "handle_message": _ev,
    "start_session": lambda a, k: {"owner": "self", "event": ga(a, k, 0, ["event"]),
                                   "session_key": ga(a, k, 1, ["session_key"])},
    "get_pending": lambda a, k: {"owner": "self", "session_key": ga(a, k, 0, ["session_key"])},
    "dispatch_busy_burst": lambda a, k: {"event": ga(a, k, 1, ["event"]), "session_key": ga(a, k, 2, ["session_key"])},
    "should_bypass": lambda a, k: {"cmd": ga(a, k, 0, ["cmd", "command_name"])},
    "is_interrupt": lambda a, k: {"cmd": ga(a, k, 0, ["cmd", "command_name"])},
    "dispatch_active_session_command": lambda a, k: {"event": ga(a, k, 1, ["event"]),
                                                     "session_key": ga(a, k, 2, ["session_key"]),
                                                     "cmd": ga(a, k, 3, ["cmd"])},
    "dispatch_inline_reply": _ev,
    "clarify_pending": lambda a, k: {"session_key": ga(a, k, 0, ["session_key"]),
                                     "include_choice_prompts": k.get("include_choice_prompts")},
    "busy_handler": lambda a, k: {"event": ga(a, k, 0, ["event"]), "session_key": ga(a, k, 1, ["session_key"])},
    "merge_pending": lambda a, k: {"session_key": ga(a, k, 1, ["session_key"]), "event": ga(a, k, 2, ["event"]),
                                   "merge_text": k.get("merge_text")},
    "queue_debounce": lambda a, k: {"session_key": ga(a, k, 0, ["session_key"]), "event": ga(a, k, 1, ["event"])},
    "pad_flags": _ev,
    "extend_media": lambda a, k: {"existing": ga(a, k, 0, ["existing"]), "event": ga(a, k, 1, ["event"])},
    "absorb_turn_input": lambda a, k: {"existing": ga(a, k, 0, ["existing"]), "event": ga(a, k, 1, ["event"])},
    "sleep": lambda a, k: {"delay": ga(a, k, 0, ["delay"])},
    "dispatch": _ev,
    "flush_debounce": lambda a, k: {"session_key": ga(a, k, 0, ["session_key"])},
    "cleanup_finished": lambda a, k: {"session_key": ga(a, k, 0, ["session_key"]), "guard": ga(a, k, 1, ["guard", "interrupt_event"])},
    "process_background": lambda a, k: {"event": ga(a, k, 0, ["event"]), "session_key": ga(a, k, 1, ["session_key"])},
    "ledger_bracket": lambda a, k: {"event": ga(a, k, 1, ["event"]), "session_key": ga(a, k, 2, ["session_key"]),
                                    "text_content": ga(a, k, 3, ["text_content"]),
                                    "is_ephemeral_response": k.get("is_ephemeral_response")},
    "send_with_retry": lambda a, k: {"chat_id": k.get("chat_id"), "content": k.get("content"),
                                     "reply_to": k.get("reply_to"), "metadata": k.get("metadata")},
    "stop_clock": lambda a, k: {"adapter": ga(a, k, 0, ["adapter"]), "chat_id": ga(a, k, 1, ["chat_id"]),
                                "result": ga(a, k, 2, ["result"])},
    "ledger_finalize": lambda a, k: {"event": ga(a, k, 1, ["event"]), "session_key": ga(a, k, 2, ["session_key"]),
                                     "obligation_id": ga(a, k, 3, ["obligation_id"]), "result": ga(a, k, 4, ["result"]),
                                     "delivery_adapter": ga(a, k, 5, ["delivery_adapter"])},
    "schedule_delete": lambda a, k: {"chat_id": ga(a, k, 0, ["chat_id"]), "message_id": ga(a, k, 1, ["message_id"]),
                                     "ttl": ga(a, k, 2, ["ttl_seconds", "ttl"])},
    "wake_muted": _ev,
    "final_adapter": _src,
    "send_multiple_images": lambda a, k: {"chat_id": k.get("chat_id"), "images": k.get("images"),
                                          "metadata": k.get("metadata"), "human_delay": k.get("human_delay")},
    "send_footer": lambda a, k: {"chat_id": ga(a, k, 1, ["chat_id"]), "images": ga(a, k, 2, ["images"]),
                                 "result": ga(a, k, 3, ["result"]), "metadata": ga(a, k, 4, ["metadata"])},
    "send_voice": lambda a, k: dict({"chat_id": k.get("chat_id"), "audio_path": k.get("audio_path")},
                                    **{kk: vv for kk, vv in k.items() if kk not in ("chat_id", "audio_path")}),
    "send_video": lambda a, k: {"chat_id": k.get("chat_id"), "video_path": k.get("video_path"),
                                "metadata": k.get("metadata")},
    "send_document": lambda a, k: {"chat_id": k.get("chat_id"), "file_path": k.get("file_path"),
                                   "metadata": k.get("metadata")},
    "notify_failure": lambda a, k: {"chat_id": ga(a, k, 0, ["chat_id"]), "path": ga(a, k, 1, ["media_path", "path"]),
                                    "is_voice": k.get("is_voice"), "metadata": k.get("metadata")},
    "acquire_lock": lambda a, k: {"scope": ga(a, k, 0, ["scope"]), "identity": ga(a, k, 1, ["identity"]),
                                  "metadata": k.get("metadata")},
    "dir_remedy": lambda a, k: {"filename": ga(a, k, 0, ["filename"])},
    "takeover": lambda a, k: {"existing": ga(a, k, 0, ["existing"])},
    "owner_label": lambda a, k: {"existing": ga(a, k, 0, ["existing"])},
    "set_fatal_error": lambda a, k: {"code": ga(a, k, 0, ["code"]), "message": ga(a, k, 1, ["message"]),
                                     "retryable": k.get("retryable")},
    "release_lock": lambda a, k: {"scope": ga(a, k, 0, ["scope"]), "identity": ga(a, k, 1, ["identity"])},
    "get_factories": lambda a, k: {"platform": ga(a, k, 0, ["platform"])},
    "factory": lambda a, k: {"native": ga(a, k, 0, ["native"]), "factory_id": ga(a, k, 1, ["factory_id"])},
    "ea_default_reason": lambda a, k: {},
    "render": lambda a, k: {"command": ga(a, k, 1, ["command"]), "description": ga(a, k, 2, ["description"]),
                            "smart_denied": ga(a, k, 3, ["smart_denied"]),
                            "full_command_shown": ga(a, k, 4, ["full_command_shown"])},
    "card_whole": lambda a, k: {"chat_id": ga(a, k, 1, ["chat_id"]), "session_key": ga(a, k, 2, ["session_key"]),
                                "request_id": ga(a, k, 3, ["request_id"]), "command": ga(a, k, 4, ["command"]),
                                "metadata": ga(a, k, 5, ["metadata"]), "truncated": ga(a, k, 6, ["truncated"])},
    "send_prompt": lambda a, k: {"prompt": ga(a, k, 0, ["prompt"])},
    "clarify_get_pending": lambda a, k: {"clarify_id": ga(a, k, 0, ["clarify_id"])},
    "mark_awaiting": lambda a, k: {"clarify_id": ga(a, k, 0, ["clarify_id"])},
    "configured_bot": lambda a, k: {"config": ga(a, k, 0, ["config"]), "user_id": ga(a, k, 1, ["user_id"])},
    "profile_for_source": lambda a, k: {"source": ga(a, k, 0, ["source"]),
                                        "adapter_profile": k.get("adapter_profile")},
    "unwrap_ephemeral": lambda a, k: {"response": ga(a, k, 0, ["response"])},
    "message_handler": _ev,
    "fallback_notice": lambda a, k: {"method": ga(a, k, 0, ["method"]), "kind": ga(a, k, 1, ["kind"]),
                                     "path": ga(a, k, 2, ["path"]), "chat_id": ga(a, k, 3, ["chat_id"]),
                                     "caption": ga(a, k, 4, ["caption"]), "reply_to": ga(a, k, 5, ["reply_to"]),
                                     "metadata": ga(a, k, 6, ["metadata"]), "file_name": k.get("file_name")},
    "send_image": lambda a, k: {"chat_id": k.get("chat_id"), "image_url": k.get("image_url"),
                                "caption": k.get("caption"), "reply_to": k.get("reply_to"),
                                "metadata": k.get("metadata")},
}

# ── stub installation ──

def modstub(seam, argspec, kind="json", is_async=False):
    return mk_stub(seam, argspec, kind, is_async, strip_self=False)


def install_base_stubs(ns):
    def _t_stub(key, **kw):
        r = script("t")   # ONE consume: rec and return must see the same value
        rec("t", na({"key": key, "kwargs": kw}), r)
        return maybe_raise(r)
    ns["t"] = _t_stub
    ns["logger"] = RecLogger()
    ns["_reply_anchor_for_event"] = modstub("reply_anchor", _ev)
    ns["merge_pending_message_event"] = modstub(
        "merge_pending", lambda a, k: {"session_key": ga(a, k, 1, ["session_key"]),
                                       "event": ga(a, k, 2, ["event"]), "merge_text": k.get("merge_text")})
    ns["pad_inline_flags"] = modstub("pad_flags", _ev)
    ns["diagnostic_wake_muted"] = modstub("wake_muted", _ev, "bool")
    ns["should_bypass_active_session"] = modstub("should_bypass", lambda a, k: {"cmd": ga(a, k, 0, ["cmd"])}, "bool")
    ns["is_interrupt_then_dispatch"] = modstub("is_interrupt", lambda a, k: {"cmd": ga(a, k, 0, ["cmd"])}, "bool")
    ns["build_session_key"] = modstub(
        "build_session_key", lambda a, k: {"source": ga(a, k, 0, ["source"]),
                                           "group_sessions_per_user": k.get("group_sessions_per_user"),
                                           "thread_sessions_per_user": k.get("thread_sessions_per_user"),
                                           "profile": k.get("profile")})
    ns["dm_thread_key_source"] = modstub(
        "dm_thread_key_source", lambda a, k: {"source": ga(a, k, 0, ["source"]), "extra": ga(a, k, 1, ["extra"])})
    ns["dm_thread_batch_suffix"] = modstub(
        "dm_thread_batch_suffix", lambda a, k: {"source": ga(a, k, 0, ["source"]), "extra": ga(a, k, 1, ["extra"])})
    ns["normalize_proxy_url"] = modstub("normalize_proxy", lambda a, k: {"value": ga(a, k, 0, ["value"])})
    ns["first_proxy_env_value"] = modstub("first_proxy_env", lambda a, k: {})
    ns["safe_proxy_url_for_log"] = modstub("safe_proxy_url", lambda a, k: {"url": ga(a, k, 0, ["url"])})
    ns["unusable_dir_remedy"] = modstub("dir_remedy", lambda a, k: {"filename": ga(a, k, 0, ["filename"])})
    ns["typing_refresh_wanted"] = modstub("typing_wanted", lambda a, k: {"event": ga(a, k, 1, ["event"])}, "bool")
    ns["send_footer_unless_captioned"] = modstub(
        "send_footer", lambda a, k: {"chat_id": ga(a, k, 1, ["chat_id"]), "images": ga(a, k, 2, ["images"]),
                                     "result": ga(a, k, 3, ["result"]), "metadata": ga(a, k, 4, ["metadata"])},
        is_async=True)
    ns["run_post_delivery_callback"] = modstub(
        "run_post_delivery", lambda a, k: {"callback": ga(a, k, 0, ["callback"]), "timeout": ga(a, k, 1, ["timeout"])},
        is_async=True)
    ns["record_final_obligation"] = modstub(
        "record_final_obligation", ARGSPEC["record_final_obligation"], is_async=True)
    ns["finalize_final_obligation"] = modstub(
        "finalize_final_obligation", ARGSPEC["finalize_final_obligation"], is_async=True)
    ns["ledger_attempted"] = modstub("ledger_attempted", ARGSPEC["ledger_attempted"], "bool", is_async=True)
    ns["ea_action_labels"] = modstub("ea_labels", lambda a, k: {})
    ns["ea_default_reason_text"] = modstub("ea_default_reason", lambda a, k: {})
    def _render(adapter, command, description, smart_denied, full_command_shown):
        r = script("render")
        rec("render", na({"command": command, "description": description,
                          "smart_denied": smart_denied, "full_command_shown": full_command_shown}), r)
        r = maybe_raise(r) or {}
        return (r.get("text", ""), r.get("truncated", False))
    ns["render_exec_approval"] = _render
    ns["card_shows_whole_command"] = modstub("card_whole", ARGSPEC["card_whole"], "bool", is_async=True)
    ns["exec_approval_cmd_budget"] = modstub(
        "cmd_budget", lambda a, k: {"description": ga(a, k, 1, ["description"]),
                                    "smart_denied": ga(a, k, 2, ["smart_denied"])})
    ns["approval_timeout_seconds"] = modstub("approval_timeout", lambda a, k: {})
    ns["format_approval_deadline_line"] = modstub("deadline_line", lambda a, k: {"timeout": ga(a, k, 0, ["timeout"])})
    ns["stop_reply_clock"] = modstub("stop_clock", ARGSPEC["stop_clock"])
    ns["ledger_bracket"] = _ledger_bracket_stub   # live module-level import
    ns["configured_bot_user"] = modstub("configured_bot", ARGSPEC["configured_bot"], "bool")
    # foreign modules reached by function-local imports
    _mkmod("gateway.session_identity", {
        "identity_of": modstub("identity_of", _ev if False else _src, "ns"),
        "canonical_identity": modstub("canonical_identity",
                                      lambda a, k: {"source": ga(a, k, 0, ["source"]),
                                                    "transport_profile": k.get("transport_profile")}, "ns"),
        "replace_source": lambda src, **kw: src,
    })
    _mkmod("gateway.parked_route_refusal", {
        "drop_rejected_route": modstub("drop_rejected_route", lambda a, k: {"source": ga(a, k, 1, ["source"])}),
    })
    _mkmod("gateway.warning_notifications", {
        "warning_notifications_enabled": modstub("warning_policy", lambda a, k: {"platform": ga(a, k, 0, ["platform"])}, "bool"),
        "diagnostic_wake_muted": ns["diagnostic_wake_muted"],
    })
    _mkmod("gateway.status", {
        "acquire_scoped_lock": modstub("acquire_lock", ARGSPEC["acquire_lock"], "lockresult"),
        "take_over_scoped_lock_holder": modstub("takeover", ARGSPEC["takeover"]),
        "scoped_lock_owner_label": modstub("owner_label", ARGSPEC["owner_label"]),
        "publish_runtime_status": modstub("write_status", ARGSPEC["write_status"]),
    })
    _mkmod("gateway.status_scoped_locks", {
        "release_scoped_lock": modstub("release_lock", ARGSPEC["release_lock"]),
    })
    _mkmod("gateway.status_lock_dir", {"ScopedLockDirUnusable": ScopedLockDirUnusable})
    _mkmod("hermes_dir_acl", {"unusable_dir_remedy": ns["unusable_dir_remedy"],
                              "make_user_dir": lambda *a, **k: None})
    _mkmod("gateway.profile_routing", {"ProfileRouteRejected": ProfileRouteRejected})
    _mkmod("gateway.bot_loop_guard", {"configured_bot_user": ns["configured_bot_user"]})
    _mkmod("gateway.platforms._shared", {
        "get_scoped_secret": modstub("scoped_secret", ARGSPEC["scoped_secret"]),
    })
    _mkmod("agent.proxy_bypass", {
        "first_proxy_env_value": ns["first_proxy_env_value"],
        "should_bypass_proxy": modstub("bypass_matcher", ARGSPEC["bypass_matcher"], "bool"),
    })
    _mkmod("utils", {"normalize_proxy_url": ns["normalize_proxy_url"],
                     "unlink_files_older_than": lambda *a, **k: 0})
    _mkmod("gateway.platforms.base_log_safety", {
        "safe_proxy_url_for_log": ns["safe_proxy_url_for_log"],
        "safe_url_for_log": lambda u: u, "_log_safe_path": lambda p: p,
        "describe_proxy_error": lambda e: str(e), "env_proxy_for_redaction": lambda: "",
    })
    _mkmod("gateway.platforms.base_media_flags", {"pad_inline_flags": ns["pad_inline_flags"]})
    _mkmod("gateway.platforms.helpers", {
        "typing_refresh_wanted": ns["typing_refresh_wanted"],
        "typing_metadata_for_turn": lambda *a, **k: {},
        "sender_may_see_activity": lambda *a, **k: True,
        "typing_pause_key": lambda *a, **k: "",
        "fence_state_after": lambda *a, **k: None, "reply_file_off": lambda: _nullctx(),
        "try_reply_file": lambda *a, **k: (False, None),
    })
    _mkmod("gateway.runtime_footer", {
        "send_footer_unless_captioned": ns["send_footer_unless_captioned"],
        "footer_onto_photo_caption": lambda adapter, extracted, exts: extracted,
    })
    _mkmod("gateway.platforms.base_post_delivery", {"run_post_delivery_callback": ns["run_post_delivery_callback"]})
    _mkmod("gateway.platforms.base_delivery_ledger", {
        "record_final_obligation": ns["record_final_obligation"],
        "finalize_final_obligation": ns["finalize_final_obligation"],
        "ledger_attempted": ns["ledger_attempted"],
        "ledger_bracket": _ledger_bracket_stub,
    })
    _mkmod("gateway.platforms.base_exec_approval", {
        "ea_action_labels": ns["ea_action_labels"], "ea_default_reason_text": ns["ea_default_reason_text"],
        "render_exec_approval": ns["render_exec_approval"], "card_shows_whole_command": ns["card_shows_whole_command"],
        "exec_approval_cmd_budget": ns["exec_approval_cmd_budget"],
        "approval_timeout_seconds": ns["approval_timeout_seconds"],
        "format_approval_deadline_line": ns["format_approval_deadline_line"],
        "ea_header_text": lambda: "header", "ea_reason_label_text": lambda: "reason",
        "ea_smart_deny_line_text": lambda: "deny",
    })
    _mkmod("hermes_cli.plugins", {"get_plugin_manager": _plugin_manager_stub})
    _mkmod("hermes_cli.commands", {
        "should_bypass_active_session": ns["should_bypass_active_session"],
        "is_interrupt_then_dispatch": ns["is_interrupt_then_dispatch"],
    })
    _mkmod("tools.clarify_gateway", {
        "get_pending": modstub("clarify_get_pending", ARGSPEC["clarify_get_pending"], "ns"),
        "mark_awaiting_text": modstub("mark_awaiting", ARGSPEC["mark_awaiting"]),
        "get_pending_for_session": modstub("clarify_pending", ARGSPEC["clarify_pending"], "ns"),
    })
    _mkmod("gateway.session_ingress_burst", {
        "dispatch_busy_burst": modstub("dispatch_busy_burst", ARGSPEC["dispatch_busy_burst"], is_async=True),
        "burst_gate": lambda *a, **k: False, "discard_busy_burst": lambda *a, **k: None,
    })
    _mkmod("gateway.platforms.base_session_command", {
        "command_in_flight": lambda guard: False,
        "guard_under_commands": lambda g: g,
        "dispatch_active_session_command": modstub("dispatch_active_session_command",
                                                   ARGSPEC["dispatch_active_session_command"], is_async=True),
    })
    _mkmod("aiohttp_socks", {"ProxyConnector": types.SimpleNamespace(
        from_url=modstub("import_connector", ARGSPEC["import_connector"], "ns"))})
    _mkmod("tools.tts_text_normalize", {"prepare_spoken_text": modstub("spoken", ARGSPEC["spoken"])})
    _mkmod("gateway.platforms.docker_media_paths", {"hand_back_unreadable_media": lambda paths: _async_none()})
    # asyncio stub module
    ns["asyncio"] = _asyncio_stub()
    ns["random"] = types.SimpleNamespace(uniform=modstub("uniform", ARGSPEC["uniform"]))
    base = type("BasePlatformAdapter", (), {})
    base._send_exec_approval_prompt = ns.get("_send_exec_approval_prompt")
    base.delete_message = ns.get("delete_message")
    ns["BasePlatformAdapter"] = base


def _mkmod(name, attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent not in sys.modules:
            sys.modules[parent] = types.ModuleType(parent)
        setattr(sys.modules[parent], parts[i], m)
    return m


class _nullctx:
    def __enter__(self): return self
    def __exit__(self, *a): return False


async def _async_none():
    return None


class FactoryDouble:
    def __init__(self, spec):
        self.spec = spec
        self.__qualname__ = spec.get("qualname") or None

    def __repr__(self):
        return self.spec.get("repr", "")

    def __call__(self, native, adapter):
        r = script("factory")
        rec("factory", na({"native": native, "factory_id": self.spec.get("id")}), r)
        return maybe_raise(r)


def _plugin_manager_stub():
    def get_factories(platform):
        r = script("get_factories")
        rec("get_factories", na({"platform": platform}), r)
        r = maybe_raise(r) or []
        if isinstance(r, dict):
            r = [r]   # a single factory spec models a one-element list
        return [(FactoryDouble(spec), spec.get("plugin")) for spec in r]
    return types.SimpleNamespace(get_platform_handler_factories=get_factories)


async def _ledger_bracket_stub(adapter, event, session_key, text_content, send_fn, *, is_ephemeral_response=False):
    r = script("ledger_bracket")
    rec("ledger_bracket", na({"event": event, "session_key": session_key,
                              "text_content": text_content, "is_ephemeral_response": is_ephemeral_response}), r)
    r = maybe_raise(r) or {}
    ob = r.get("obligation_id")
    ad = r.get("delivery_adapter") or {}
    adapter_double = types.SimpleNamespace(id=ad.get("id"), name=ad.get("name"))
    adapter_double._send_with_retry = types.MethodType(
        mk_stub("send_with_retry", ARGSPEC["send_with_retry"], "sendresult", True, True), adapter_double)
    adapter_double._schedule_ephemeral_delete = types.MethodType(
        mk_stub("schedule_delete", ARGSPEC["schedule_delete"], "json", False, True), adapter_double)
    result = await send_fn(adapter_double, ob)
    rec("ledger_finalize", na({"event": event, "session_key": session_key, "obligation_id": ob,
                               "result": result, "delivery_adapter": adapter_double}), script("ledger_finalize"))
    return result, adapter_double


def _asyncio_stub():
    m = types.ModuleType("asyncio")
    m.CancelledError = asyncio.CancelledError
    m.Event = EventDouble
    m.Task = TaskDouble

    async def sleep(delay):
        r = script("sleep")
        rec("sleep", na({"delay": delay}), r)
        return maybe_raise(r)

    def create_task(coro):
        loc = coro.cr_frame.f_locals
        cname = coro.cr_code.co_name
        if cname == "_flush_text_batch":
            args = {"key": loc.get("key")}
        elif cname == "_keep_typing":
            args = {"chat_id": loc.get("chat_id"), "metadata": loc.get("metadata")}
            if "stop_event" in loc:
                args["stop_event"] = loc.get("stop_event")
        elif cname == "_drain_after":
            args = {"pending_event": loc.get("pending_event"), "session_key": loc.get("session_key"),
                    "delay": loc.get("delay"), "guard": loc.get("guard")}
        elif cname == "_flush_text_debounce":
            args = {"session_key": loc.get("session_key"), "delay": loc.get("delay")}
        else:
            args = {}
        coro.close()
        r = script("create_task")
        rec("create_task", na(args), r)
        return task_double(maybe_raise(r))

    def current_task():
        tid = cin("current_task_id")
        return task_double({"id": tid}) if tid is not None else None

    async def shield(x):
        return await x

    async def to_thread(fn, *a, **k):
        return fn(*a, **k)

    m.sleep = sleep
    m.create_task = create_task
    m.current_task = current_task
    m.shield = shield
    m.to_thread = to_thread
    m.iscoroutine = asyncio.iscoroutine
    m.ensure_future = asyncio.ensure_future
    return m


# ── fake adapter construction ──

def convert_state(k, v):
    if k == "_active_sessions" and isinstance(v, dict):
        return {kk: guard_double(vv) for kk, vv in v.items()}
    if k in ("_session_tasks", "_pending_text_batch_tasks") and isinstance(v, dict):
        return {kk: task_double(vv) for kk, vv in v.items()}
    if k in ("_pending_messages", "_pending_text_batches") and isinstance(v, dict):
        return {kk: _event_stubs(DictEvent(vv)) for kk, vv in v.items()}
    if k == "_text_debounce" and isinstance(v, dict):
        out = {}
        for kk, vv in v.items():
            out[kk] = types.SimpleNamespace(
                event=DictEvent(vv.get("event")), task=task_double(vv.get("task")),
                first_ts=vv.get("first_ts", 0.0), last_ts=vv.get("last_ts", 0.0))
        return out
    if k in ("_background_tasks", "_expected_cancelled_tasks") and isinstance(v, list):
        return set(task_double({"id": x} if isinstance(x, int) else x) for x in v)
    if k == "_plugin_handlers_wired" and isinstance(v, list):
        return set(tuple(x) if isinstance(x, list) else x for x in v)
    if k == "gateway_runner" and isinstance(v, dict):
        return runner_double(v)
    if k == "_session_store" and isinstance(v, dict):
        store = types.SimpleNamespace(**v)
        if v.get("has_resolver"):
            store._resolve_profile_for_key = modstub("store_resolve", _src)
        return store
    return copy.deepcopy(v)


def runner_double(d):
    r = types.SimpleNamespace()
    r._transport_owner = (lambda: None) if d.get("has_transport_owner") else None
    _da_stub = modstub("scope_resolve", _src, "ns") if d.get("scope_resolve") else None
    r._media_delivery_scope_for_source = _da_stub
    _delivery_stub = modstub("delivery_adapter", _src, "ns")

    def _delivery_adapter_for(source):
        ad = _delivery_stub(source)
        if ad is not None:
            # a recovery owner: equip the seams the busy-admission walk calls
            if not hasattr(ad, "_active_sessions"):
                ad._active_sessions = {}
            aid = getattr(ad, "id", "self")
            ad.get_pending_message = _owner_get_pending(aid)
            ad._start_session_processing = _owner_start_session(aid)
        return ad

    r._delivery_adapter_for = _delivery_adapter_for
    r._profile_name_for_source = modstub("profile_for_source", ARGSPEC["profile_for_source"])
    r._clear_durable_active_turn = modstub("clear_durable_turn", _ev, is_async=True)
    r.session_authority = d.get("session_authority")
    return r


def _overridable_property(name, fget):
    """A property whose value can be shadowed by an instance attribute (the
    drivers override e.g. message_len_fn); keeps the live getter lazy."""
    def getter(self):
        if name in self.__dict__:
            return self.__dict__[name]
        return fget(self)
    def setter(self, value):
        self.__dict__[name] = value
    return property(getter, setter)


# Instance defaults from the live BasePlatformAdapter.__init__ (state the
# flows mutate; fixtures only carry the keys a case cares about).
_FS_DEFAULTS = {
    "_message_handler": None, "_no_message_handler_logged": False,
    "_reaction_handler": None, "_platform_event_handler": None,
    "_topic_recovery_fn": None, "_running": False, "_fatal_error_retryable": True,
    "_fatal_error_code": None, "_fatal_error_message": None,
    "_fatal_error_handler": None, "_detached_fatal_tasks": set(),
    "_platform_lock_takeover_allowed": False, "_platform_lock_takeover_attempted": False,
    "_active_sessions": {}, "_pending_messages": {}, "_requeue_counts": {},
    "_pending_text_batches": {}, "_pending_text_batch_tasks": {},
    "_session_tasks": {},    "_busy_text_mode": "interrupt",
    "_human_delay_range_ms": None, "_text_debounce": {},
    "_background_tasks": set(), "_post_delivery_callbacks": {},
    "_expected_cancelled_tasks": set(), "_busy_session_handler": None,
    "_owner_profile": None, "_shared_listener_profile": None,
    "_authorization_check": None, "_auto_tts_default": False,
    "_auto_tts_mode": "all", "_auto_tts_enabled_chats": set(),
    "_auto_tts_disabled_chats": set(), "_typing_paused": set(),
}


def _title_name(s):
    """C adapter_name(): title-case the platform value (first letter of each
    cased run upper, rest lower)."""
    out = []
    prev_cased = False
    for ch in s:
        cased = ("a" <= ch <= "z") or ("A" <= ch <= "Z")
        if cased:
            if not prev_cased and "a" <= ch <= "z":
                ch = ch.upper()
            elif prev_cased and "A" <= ch <= "Z":
                ch = ch.lower()
        out.append(ch)
        prev_cased = cased
    return "".join(out)


def build_fs(inp):
    st = inp.get("st") or {}
    # A real (per-case) class so live properties/classmethods/staticmethods
    # keep their descriptor semantics; methods bind self naturally.
    body = {}
    for name in ALL_DEFS:
        f = ns_g.get(name)
        if f is None or name in ("choices", "cancel_timer"):
            continue
        if isinstance(f, property):
            f = _overridable_property(name, f.fget)
        body[name] = f
    for k, v in ns_g["_CLASS_ATTRS"].items():
        body[k] = copy.deepcopy(v)
    cls = type(inp.get("class_name") or "AdapterDouble", (), body)
    fs = cls()
    pm = st.get("platform", "telegram")
    pv = pm["value"] if isinstance(pm, dict) else pm
    fs.platform = ns_g["Platform"](pv)
    fs.name = _title_name(pv)
    cfg = st.get("config") or {}
    fs.config = types.SimpleNamespace(extra=copy.deepcopy(cfg.get("extra", {})))
    for k, v in _FS_DEFAULTS.items():
        if not hasattr(fs, k):
            setattr(fs, k, copy.deepcopy(v))
    if not hasattr(fs, "_busy_text_debounce_seconds"):
        fs._busy_text_debounce_seconds = ns_g.get("DEFAULT_BUSY_TEXT_DEBOUNCE_SECONDS", 0.35)
    if not hasattr(fs, "_busy_text_hard_cap_seconds"):
        fs._busy_text_hard_cap_seconds = ns_g.get("DEFAULT_BUSY_TEXT_HARD_CAP_SECONDS", 1.0)
    for k, v in st.items():
        if k in ("platform", "config"):
            continue
        setattr(fs, k, convert_state(k, v))
    # default handler presence flags
    if not hasattr(fs, "_message_handler"):
        fs._message_handler = None
    CUR["fs"] = fs
    CUR["st_before"] = set(vars(fs))
    return fs


def attach(fs, method, seam, kind="json", is_async=False):
    # Bind the stub so self IS passed and strip_self removes it: the ARGSPEC
    # indices count arguments AFTER self. _message_handler stays a plain
    # function because drivers assign its __self__ (handler_self cases).
    if method == "_message_handler":
        setattr(fs, method, mk_stub(seam, ARGSPEC[seam], kind, is_async, strip_self=False))
    else:
        setattr(fs, method, types.MethodType(mk_stub(seam, ARGSPEC[seam], kind, is_async, True), fs))


def state_snapshot(fs, inp):
    st = inp.get("st") or {}
    out = {}
    for k in st:
        if k in ("platform", "config", "gateway_runner", "_busy_session_handler"):
            # opaque to the port: echoed verbatim (runner/handler doubles are
            # presence flags, not state the C side mutates)
            out[k] = copy.deepcopy(st[k])
            continue
        out[k] = norm(getattr(fs, k, None))
    # keys the live call ADDED to the adapter (C json_set's them into st);
    # attached seam stubs are callables, not state
    before = CUR.get("st_before") or set()
    for k in vars(fs):
        if k in before or k in out:
            continue
        v = getattr(fs, k)
        if callable(v):
            continue
        out[k] = norm(v)
    return out


def event_snapshot(e):
    return {k: norm(v) for k, v in vars(e).items() if not callable(v)}


def run_live(fn, *a, **k):
    if asyncio.iscoroutinefunction(fn):
        return asyncio.run(fn(*a, **k))
    return fn(*a, **k)

# ── per-function drivers ──

def callseam(name, args):
    r = script(name)
    rec(name, na(args), r)
    return maybe_raise(r)


def drive(ns, func, inp):
    py = PY[func]
    st = inp.get("st", {})
    saved = {}

    def override(name, val):
        saved[name] = ns.get(name)
        ns[name] = val

    def restore():
        for k, v in saved.items():
            if v is None:
                ns.pop(k, None)
            else:
                ns[k] = v

    try:
        return _drive(ns, func, py, inp, override)
    finally:
        restore()


def _drive(ns, func, py, inp, override):
    st = inp.get("st", {})
    f = ns.get(py)

    # ---- pure string / text helpers ----
    if func == "or_default":
        th = inp.get("thunk", {})
        exc = th.get("exc", 0)
        exc_classes = {1: TypeError, 2: ValueError, 3: KeyError, 4: RuntimeError}

        def thunk():
            if exc:
                raise exc_classes[exc](th.get("msg", "x"))
            return th.get("value")
        mask = inp.get("exc_mask", 3)
        cls = tuple(c for b, c in ((1, TypeError), (2, ValueError), (4, KeyError)) if mask & b)
        try:
            val = f(thunk, inp.get("default"), cls)
            return {"ret": {"value": norm(val), "exc": 0}}
        except Exception as e:
            kind = {TypeError: 1, ValueError: 2, KeyError: 3}.get(type(e), 4)
            return {"ret": {"value": None, "exc": kind}}
    if func == "thread_metadata_for_source":
        return {"ret": norm(f(src_double(inp.get("source")), inp.get("reply_to")))}
    if func == "thread_metadata_for_event":
        return {"ret": norm(f(ev(inp.get("event"))))}
    if func == "mark_notify_metadata":
        return {"ret": norm(f(inp.get("metadata")))}
    if func == "media_failure_text":
        return {"ret": f(inp.get("kind"), inp.get("file_name"))}
    if func == "should_send_media_as_audio":
        return {"ret": f(inp.get("platform"), inp.get("ext", ""), inp.get("is_voice", False))}
    if func == "custom_unit_to_cp":
        return {"ret": f(inp.get("s", ""), inp.get("budget", 0), lambda s: callseam("measure", {"s": s}))}
    if func == "fit_prefix":
        return {"ret": f(inp.get("text", ""), inp.get("budget", 0),
                         lambda s: callseam("measure", {"s": s}), inp.get("suffix", "..."))}
    if func == "prefix_within_utf16_limit":
        return {"ret": f(inp.get("s", ""), inp.get("limit", 0))}
    if func == "has_media_directives":
        return {"ret": f(inp.get("text", ""))}
    if func == "terminal_sentinel_start":
        return {"ret": f(inp.get("text", ""))}
    if func == "code_spans":
        return {"ret": [list(s) for s in f(inp.get("content", ""))]}
    if func == "blank_spans":
        return {"ret": f(inp.get("text", ""), [tuple(s) for s in inp.get("spans", [])])}
    if func == "delete_spans":
        return {"ret": f(inp.get("text", ""), [tuple(s) for s in inp.get("spans", [])])}
    if func == "append_text":
        return {"ret": f(inp.get("existing"), inp.get("new"))}
    if func == "any_in":
        return {"ret": f(inp.get("blob", ""), *inp.get("needles", []))}
    if func == "classify_send_error":
        exc = inp.get("exc")
        e = None
        if exc:
            cname = exc.get("class", "Exception")
            cls = EXC_MAP.get(cname, Exception)
            if cls.__name__ != cname:
                cls = type(cname, (cls,), {})   # live classifiers match type(exc).__name__
            e = cls(exc.get("str", ""))
        return {"ret": f(e, inp.get("error_text", ""))}
    if func == "split_post_delivery_entry":
        entry = inp.get("entry")
        if isinstance(entry, list) and len(entry) == 2:
            entry = tuple(entry)
        r = f(entry)
        return {"ret": [norm(r[0]), norm(r[1])]}
    if func == "extract_images":
        imgs, cleaned = f(inp.get("content", ""))
        return {"ret": {"images": [[u, a] for u, a in imgs], "cleaned": cleaned}}
    if func == "coerce_float_extra":
        fs = build_fs(inp)
        fs.config.extra[inp.get("key", "k")] = inp.get("value")
        mx = inp.get("max_value")
        return {"ret": f(fs, inp.get("key", "k"), inp.get("default", 0.0),
                         min_value=inp.get("min_value", 0.0),
                         max_value=mx)}
    if func == "configure_text_batch_delays":
        fs = build_fs(inp)
        f(fs)
        return {"ret": None, "st": state_snapshot(fs, inp)}
    if func == "text_batch_delay_for":
        fs = build_fs(inp)
        pend = ev(inp["pending"]) if inp.get("pending") is not None else None
        return {"ret": f(fs, pend)}
    if func == "owner_transport_profile":
        fs = build_fs(inp)
        return {"ret": f(fs)}
    if func == "is_rate_limited_error":
        return {"ret": f(inp.get("error"))}
    if func == "is_timeout_error":
        return {"ret": f(inp.get("error"))}
    if func == "send_retry_is_final":
        fs = build_fs(inp)
        return {"ret": f(fs, ns["SendResult"](**inp.get("result", {"success": False})))}
    if func == "is_partial_delivery":
        return {"ret": f(ns["SendResult"](**inp.get("result", {"success": False})))}
    if func == "merge_caption":
        return {"ret": f(inp.get("existing"), inp.get("new", ""))}
    if func == "requeue_backoff_delay":
        fs = build_fs(inp)
        return {"ret": f(fs, inp.get("session_key", "k"), ev(inp.get("pending")), ev(inp.get("dispatched"))),
                "st": state_snapshot(fs, inp)}
    if func == "get_human_delay":
        fs = build_fs(inp)
        return {"ret": f(fs)}
    if func == "wants_auto_tts":
        fs = build_fs(inp)
        attach(fs, "_should_auto_tts_for_chat", "should_auto_tts", "bool")
        attach(fs, "_streaming_tts_turn_completed", "tts_turn_completed", "bool")
        ie = guard_double(inp.get("interrupt_event")) if inp.get("interrupt_event") is not None else EventDouble()
        if inp.get("generation") is not None:
            ie._hermes_run_generation = inp["generation"]
        return {"ret": f(fs, ev(inp.get("event")), inp.get("session_key", "k"), ie,
                         inp.get("text_content", ""), inp.get("media_files", []))}
    if func == "shared_runtime_owns_input":
        fs = build_fs(inp)
        e = ev(inp.get("event"))
        handler = lambda event: None
        handler.__self__ = types.SimpleNamespace(session_authority=inp.get("session_authority")) \
            if inp.get("has_handler_self") else None
        fs._message_handler = handler
        return {"ret": f(fs, e)}
    if func == "clear_session_guard":
        fs = build_fs(inp)
        f(fs, inp.get("session_key", "k"))
        return {"ret": None, "st": state_snapshot(fs, inp)}
    if func == "warning_notifications_enabled":
        fs = build_fs(inp)
        return {"ret": f(fs, inp.get("logical_platform"))}
    if func == "warning_text":
        fs = build_fs(inp)
        return {"ret": f(fs, inp.get("visible"), inp.get("hidden", ""),
                         logical_platform=inp.get("logical_platform"), chat_id=inp.get("chat_id"),
                         metadata=inp.get("metadata"))}
    if func == "exec_approval_actions":
        fs = build_fs(inp)
        rows = f(fs, allow_permanent=inp.get("allow_permanent", True),
                 allow_session=inp.get("allow_session", True), smart_denied=inp.get("smart_denied", False))
        return {"ret": [list(r) for r in rows]}
    if func == "exec_approval_prompt_choices":
        prompt = ns["ExecApprovalPrompt"](chat_id="c", session_key="k", text="t",
                                          actions=[tuple(r) for r in inp.get("actions", [])],
                                          command="cmd", description="d", smart_denied=False)
        return {"ret": prompt.choices}
    if func == "text_debounce_cancel_timer":
        stt = inp.get("state") or {}
        state = ns["TextDebounceState"](event=ev(stt.get("event")), task=task_double(stt.get("task")),
                                        first_ts=0.0, last_ts=0.0)
        unless = task_double({"id": inp.get("unless_id", -1)}) if "unless" in inp else None
        state.cancel_timer(unless=unless)
        return {"ret": None, "state": norm(state.task)}
    if func == "unauthorized_action_notice":
        return {"ret": f(src_double(inp.get("platform")))}
    if func == "get_inbound_media_max_bytes":
        override("_config_section", modstub("config_section", ARGSPEC["config_section"]))
        return {"ret": f()}
    if func == "validate_inbound_media_size":
        override("_config_section", modstub("config_section", ARGSPEC["config_section"]))
        try:
            f(inp.get("size", 0), media_type=inp.get("media_type", "media"), max_bytes=inp.get("max_bytes"))
            return {"ret": None}
        except ValueError as e:
            return {"ret": str(e)}
    if func == "read_httpx_body_with_limit":
        override("_config_section", modstub("config_section", ARGSPEC["config_section"]))

        async def body_iter():
            for c in inp.get("chunks", []):
                yield c.encode() if isinstance(c, str) else c
        resp = types.SimpleNamespace(headers=inp.get("headers", {}), aiter_bytes=body_iter)
        try:
            data = asyncio.run(f(resp, media_type=inp.get("media_type", "media")))
            return {"ret": {"body": data.decode(), "error": None}}
        except ValueError as e:
            return {"ret": {"body": "", "error": str(e)}}
    if func == "is_network_accessible":
        import socket
        def gai(host, *a, **k):
            r = callseam("getaddrinfo", {"host": host})
            if isinstance(r, list):
                return [(socket.AF_INET, socket.SOCK_STREAM, 0, "", (addr, 0)) for addr in r]
            return r
        override("_socket", types.SimpleNamespace(getaddrinfo=gai, gaierror=_GaiError,
                                                  AF_UNSPEC=0, SOCK_STREAM=1))
        return {"ret": f(inp.get("host", ""))}
    if func == "detect_macos_system_proxy":
        darwin = inp.get("darwin", False)
        rec("is_darwin", {}, darwin)
        override("sys", types.SimpleNamespace(platform="darwin" if darwin else "linux"))
        override("time", types.SimpleNamespace(monotonic=lambda: callseam("monotonic", {})))
        import subprocess as sp
        def check_output(*a, **k):
            return callseam("scutil", {})
        override("subprocess", types.SimpleNamespace(check_output=check_output, DEVNULL=sp.DEVNULL))
        cache = inp.get("cache")
        ns["_macos_proxy_cache"] = (cache["ts"], cache.get("value")) if cache else None
        r = f()
        c = ns["_macos_proxy_cache"]
        out_cache = {"ts": c[0], "value": c[1]} if c is not None else None
        return {"ret": r, "cache": out_cache}
    if func == "reset_macos_proxy_cache":
        ns["_macos_proxy_cache"] = (1.0, "x")
        f()
        return {"ret": None, "cache": ns["_macos_proxy_cache"]}
    if func == "resolve_proxy_url":
        override("gateway_trust_env", modstub("trust_env", lambda a, k: {}, "bool"))
        override("should_bypass_proxy", modstub("bypass", ARGSPEC["bypass"], "bool"))
        override("_detect_macos_system_proxy", modstub("macos_proxy", lambda a, k: {}))
        return {"ret": f(inp.get("platform_env_var"), target_hosts=inp.get("target_hosts"),
                         configured=inp.get("configured"))}
    if func == "config_section":
        _mkmod("hermes_cli.config", {"load_config_readonly": modstub("load_config", lambda a, k: {})})
        return {"ret": f(inp.get("name", "gateway"))}
    if func == "proxy_kwargs_for_aiohttp":
        override("_aiohttp_socks_connector", modstub("socks_connector", ARGSPEC["socks_connector"], "ns"))
        sess, req = f(inp.get("proxy_url"))
        return {"ret": {"session": norm(sess), "request": norm(req)}}
    if func == "aiohttp_socks_connector":
        r = f(inp.get("proxy_url", ""))
        return {"ret": norm(r)}
    if func == "should_bypass_proxy":
        override("_should_bypass_proxy", modstub("bypass_matcher", ARGSPEC["bypass_matcher"], "bool"))
        return {"ret": f(inp.get("target_hosts"))}
    if func == "is_host_excluded_by_no_proxy":
        override("_should_bypass_proxy", modstub("bypass_matcher", ARGSPEC["bypass_matcher"], "bool"))
        return {"ret": f(inp.get("hostname", ""), no_proxy_value=inp.get("no_proxy_value"))}
    if func == "format_message":
        fs = build_fs(inp)
        return {"ret": f(fs, inp.get("content", ""))}
    if func == "message_len_fn":
        fs = build_fs(inp)
        return {"ret": fs.message_len_fn(inp.get("s", ""))}
    if func == "supports_draft_streaming":
        fs = build_fs(inp)
        return {"ret": f(fs, inp.get("chat_type"), inp.get("metadata"), inp.get("chat_id"))}
    if func == "send_draft":
        fs = build_fs(inp)
        try:
            asyncio.run(f(fs, "c", 1, "content"))
            return {"ret": None}
        except NotImplementedError as e:
            return {"ret": str(e)}
    if func == "mark_degraded":
        fs = build_fs(inp)
        override("_write_runtime_status_safe", mk_stub("write_status", ARGSPEC["write_status"], "json", False, True))
        f(fs)
        return {"ret": None}
    if func == "lazy_attr":
        fs = build_fs(inp)
        r = f(fs, inp.get("name", "x"), lambda: callseam("factory", {"name": inp.get("name", "x")}))
        return {"ret": norm(r), "st": state_snapshot(fs, inp)}
    if func == "accepts_kwarg":
        params = inp.get("params")
        if params is None:
            fn = type   # builtin whose signature raises ValueError -> unknown branch
        else:
            import inspect as ins
            kinds = {"var_keyword": ins.Parameter.VAR_KEYWORD, "positional": ins.Parameter.POSITIONAL_OR_KEYWORD}
            ps = [ins.Parameter(p["name"], kinds.get("var_keyword" if p.get("var_keyword") else "positional",
                                                     ins.Parameter.POSITIONAL_OR_KEYWORD)) for p in params]
            fn = lambda: None
            fn.__signature__ = ins.Signature(ps)
        return {"ret": f(fn, inp.get("name", "metadata"), var_kw=inp.get("var_kw", False),
                         unknown=inp.get("unknown", False))}
    if func == "prepare_tts_text":
        fs = build_fs(inp)
        return {"ret": f(fs, inp.get("text", ""))}
    if func == "media_delivery_scope":
        import contextlib
        fs = build_fs(inp)
        r = f(fs, src_double(inp["source"]) if inp.get("source") is not None else None)
        if isinstance(r, contextlib.nullcontext):
            return {"ret": {"noop": True}}
        return {"ret": norm(r)}
    if func == "pending_recovery_owners":
        fs = build_fs(inp)
        r = f(fs, ev(inp.get("event")), inp.get("session_key", "k"))
        return {"ret": ["self" if o is fs else getattr(o, "id", "self") for o in r]}
    if func == "track_session_task":
        fs = build_fs(inp)
        t = task_double(inp.get("task"))
        r = f(fs, inp.get("session_key", "k"), t)
        return {"ret": r, "st": state_snapshot(fs, inp)}
    if func == "stop_typing_quietly":
        fs = build_fs(inp)
        attach(fs, "_stop_typing_with_metadata", "stop_typing_meta", "json", True)
        asyncio.run(f(fs, inp.get("chat_id", "c"), inp.get("metadata")))
        return {"ret": None}
    if func == "start_typing_refresh":
        fs = build_fs(inp)
        attach(fs, "_accepts_kwarg", "accepts_stop_event", "bool")
        ie = guard_double(inp.get("interrupt_event")) if inp.get("interrupt_event") is not None else EventDouble()
        r = f(fs, ev(inp.get("event")), ie, inp.get("metadata"))
        return {"ret": norm(r)}
    if func == "chain_callbacks":
        cbs = []
        for i in range(inp.get("n", 0)):
            def cb(i=i):
                r = callseam("cb_%d" % i, {})
                if isinstance(r, dict) and r.get("awaitable"):
                    async def aw():
                        return callseam("await_%d" % i, {})
                    return aw()
                return r
            cbs.append(cb)
        chained = f(*cbs)
        asyncio.run(chained())
        return {"ret": None}
    if func == "send_plain_fallback":
        fs = build_fs(inp)
        attach(fs, "send", "send", "sendresult", True)
        r = asyncio.run(f(fs, inp.get("chat_id", "c"), inp.get("content", ""),
                          reply_to=inp.get("reply_to"), metadata=inp.get("metadata")))
        return {"ret": norm(r)}
    if func == "resume_partial_send":
        fs = build_fs(inp)
        r = asyncio.run(f(fs, "c", ns["SendResult"](**inp.get("result", {"success": False})),
                          reply_to=None, metadata=None))
        return {"ret": norm(r)}
    if func == "play_tts_file":
        fs = build_fs(inp)
        attach(fs, "play_tts", "play_tts", "sendresult", True)
        rd = mk_stub("record_delivery", ARGSPEC["record_delivery"], "json", False, False)
        r = asyncio.run(f(fs, ev(inp.get("event")), inp.get("text_content", ""), inp.get("tts_path", "p"),
                          inp.get("first", False), inp.get("metadata"), rd))
        return {"ret": r}
    if func == "record_delivery_obligation":
        fs = build_fs(inp)
        r = asyncio.run(f(fs, ev(inp.get("event")), inp.get("session_key", "k"), inp.get("text_content", ""),
                          inp.get("delivery_adapter"), inp.get("is_ephemeral_response", False)))
        return {"ret": norm(r)}
    if func == "finalize_delivery_obligation":
        fs = build_fs(inp)
        asyncio.run(f(fs, inp.get("obligation_id"), ns["SendResult"](**inp.get("result", {"success": True})),
                      ev(inp.get("event")), inp.get("delivery_adapter")))
        return {"ret": None}
    if func == "fire_post_delivery_callback":
        fs = build_fs(inp)
        attach(fs, "pop_post_delivery_callback", "pop_post_delivery", "json")
        asyncio.run(f(fs, inp.get("session_key", "k"), inp.get("generation")))
        return {"ret": None}
    if func == "release_turn_marker":
        fs = build_fs(inp)
        e = ev(inp.get("event"))
        asyncio.run(f(fs, e))
        return {"ret": None, "ev": event_snapshot(e)}
    if func == "ledger_attempted_final":
        fs = build_fs(inp)
        attach(fs, "_extract_response_content", "extract_text", "ns", True)
        r = asyncio.run(f(fs, ev(inp.get("event")), inp.get("session_key", "k"), inp.get("response", ""),
                          error=inp.get("error", "")))
        return {"ret": r}
    if func == "session_key_profile":
        fs = build_fs(inp)
        r = f(fs, src_double(inp["source"]) if inp.get("source") is not None else None)
        return {"ret": norm(r)}
    if func == "canonicalize":
        fs = build_fs(inp)
        r = f(fs, src_double(inp["source"]) if inp.get("source") is not None else None)
        return {"ret": norm(r)}
    if func == "drop_unresolved":
        fs = build_fs(inp)
        return {"ret": f(fs, ev(inp.get("event")))}
    if func == "ea_fit":
        fs = build_fs(inp)
        def esc(self, text):
            r = script("ea_escape")
            rec("ea_escape", {"text": text}, r)
            return text
        fs._ea_escape = types.MethodType(esc, fs)
        def mlen(s):
            r = script("measure")
            rec("measure", {"s": s}, r)
            return len(s)
        fs.message_len_fn = mlen   # live property returns the callable itself
        return {"ret": f(fs, inp.get("text", ""), inp.get("budget", 0), inp.get("suffix", "..."))}
    if func == "ea_deadline_line":
        fs = build_fs(inp)
        attach(fs, "_ea_escape", "ea_escape", "json")
        return {"ret": f(fs)}
    if func == "exec_approval_cmd_budget":
        fs = build_fs(inp)
        return {"ret": f(fs, inp.get("description", ""), inp.get("smart_denied", False))}
    if func == "supports_exec_approval_buttons":
        base = ns["BasePlatformAdapter"]
        if inp.get("overrides_prompt"):
            sub = type("Sub", (base,), {"_send_exec_approval_prompt": lambda self, p: None})
        else:
            sub = base
        fn = f.__func__ if isinstance(f, classmethod) else f
        return {"ret": fn(sub)}
    if func in ("event_session_key", "source_session_key", "text_batch_key"):
        fs = build_fs(inp)
        if func == "source_session_key":
            r = f(fs, src_double(inp.get("source")))
        else:
            r = f(fs, ev(inp.get("event")))
        return {"ret": r}
    if func == "pop_text_batch":
        fs = build_fs(inp)
        r = f(fs, inp.get("key", "k"))
        return {"ret": norm(r), "st": state_snapshot(fs, inp)}
    if func == "dispatch_text_batch":
        fs = build_fs(inp)
        attach(fs, "handle_message", "handle_message", "json", True)
        asyncio.run(f(fs, ev(inp.get("event"))))
        return {"ret": None}

    # ---- FLOW cores ----
    fs = build_fs(inp)
    e = ev(inp.get("event")) if inp.get("event") is not None else None
    _wire_flow(fs, e, func, inp)
    ret = _run_flow(ns, fs, e, func, inp)
    out = {"ret": norm(ret)}
    if inp.get("st") is not None:
        out["st"] = state_snapshot(fs, inp)
    if e is not None:
        out["ev"] = event_snapshot(e)
    return out


def _wire_flow(fs, e, func, inp):
    # event method stubs
    if e is not None:
        _event_stubs(e)
    # common claimed self-method seams
    attach(fs, "send", "send", "sendresult", True)
    attach(fs, "_send_with_retry", "send_with_retry", "sendresult", True)
    def _unwrap(self, response):
        r = script("unwrap_ephemeral")
        rec("unwrap_ephemeral", na({"response": response}), r)
        r = maybe_raise(r) or {}
        return (r.get("text"), r.get("ttl", 0))
    fs._unwrap_ephemeral = types.MethodType(_unwrap, fs)
    attach(fs, "_schedule_ephemeral_delete", "schedule_delete", "json")
    attach(fs, "_message_handler", "message_handler", "json", True)
    attach(fs, "handle_message", "handle_message", "json", True)
    # _dispatch_active_session_command stays the REAL method: it delegates to
    # the module seam (adapter first arg), whose argspec counts from idx1.
    attach(fs, "_queue_text_debounce", "queue_debounce", "json", True)
    attach(fs, "_start_session_processing", "start_session", "bool")
    attach(fs, "get_pending_message", "get_pending", "event")
    attach(fs, "_cleanup_finished_session_task", "cleanup_finished", "json")
    attach(fs, "_flush_text_debounce_now", "flush_debounce", "bool", True)
    attach(fs, "_process_message_background", "process_background", "json", True)
    attach(fs, "_notify_media_delivery_failure", "notify_failure", "json", True)
    attach(fs, "_final_delivery_adapter", "final_adapter", "ns")
    attach(fs, "send_multiple_images", "send_multiple_images", "sendresult", True)
    attach(fs, "send_voice", "send_voice", "sendresult", True)
    attach(fs, "send_video", "send_video", "sendresult", True)
    attach(fs, "send_document", "send_document", "sendresult", True)
    attach(fs, "send_image", "send_image", "sendresult", True)
    attach(fs, "play_tts", "play_tts", "sendresult", True)
    attach(fs, "_should_auto_tts_for_chat", "should_auto_tts", "bool")
    attach(fs, "_streaming_tts_turn_completed", "tts_turn_completed", "bool")
    attach(fs, "_stop_typing_with_metadata", "stop_typing_meta", "json", True)
    attach(fs, "_accepts_kwarg", "accepts_stop_event", "bool")
    attach(fs, "pop_post_delivery_callback", "pop_post_delivery", "json")
    attach(fs, "_extract_response_content", "extract_text", "ns", True)
    attach(fs, "_write_runtime_status_safe", "write_status", "json")
    attach(fs, "_set_fatal_error", "set_fatal_error", "json")
    attach(fs, "_dispatch_inline_reply", "dispatch_inline_reply", "json", True)
    attach(fs, "_send_media_fallback_notice", "fallback_notice", "sendresult", True)
    if e is not None and inp.get("has_handler_self"):
        fs._message_handler.__self__ = types.SimpleNamespace(session_authority=inp.get("session_authority"))


def _run_flow(ns, fs, e, func, inp):
    f = ns[PY[func]]
    sk = inp.get("session_key", "k")
    if func == "handle_message_while_active":
        # busy handler presence
        if inp.get("has_busy_handler"):
            fs._busy_session_handler = types.MethodType(
                mk_stub("busy_handler", ARGSPEC["busy_handler"], "bool", True, True), fs)
        else:
            fs._busy_session_handler = None
        # pending recovery owners: self.get_pending_message stub knows owner id
        fs.get_pending_message = _owner_get_pending("self")
        asyncio.run(f(fs, e, sk))
        # C returns {"accepted": event._gateway_accepted}; live returns None and
        # stamps the event — derive the same contract from the live event.
        return {"accepted": getattr(e, "_gateway_accepted", None)}
    if func == "deliver_media_attachments":
        rd = mk_stub("record_delivery", ARGSPEC["record_delivery"], "json", False, False)
        mf = [tuple(x) for x in inp.get("media_files", [])]
        asyncio.run(f(fs, e, mf, inp.get("local_files", []),
                      force_document_attachments=inp.get("force_document_attachments", False),
                      human_delay=inp.get("human_delay", 0.0), metadata=inp.get("metadata"), record_delivery=rd))
        return None
    if func == "send_image_batch":
        rd = mk_stub("record_delivery", ARGSPEC["record_delivery"], "json", False, False)
        imgs = [tuple(x) for x in inp.get("images", [])]
        asyncio.run(f(fs, e, imgs, inp.get("metadata"), inp.get("human_delay", 0.0), rd))
        return None
    if func == "acquire_platform_lock":
        return f(fs, inp.get("scope", "s"), inp.get("identity", "i"), inp.get("resource_desc", "r"))
    if func == "release_platform_lock":
        f(fs)
        return None
    if func == "wire_plugin_handlers":
        native = inp.get("native_id")
        # live compares native by IDENTITY (`is not native`); equal id values
        # in the fixture model the SAME native client object
        if getattr(fs, "_plugin_handler_native", None) == native:
            fs._plugin_handler_native = native
        f(fs, native)
        return None
    if func == "rewire_plugin_handlers":
        f(fs)
        return None
    if func == "enqueue_text_event":
        f(fs, e)
        return None
    if func == "flush_text_batch":
        return asyncio.run(f(fs, inp.get("key", "k")))
    if func == "flush_text_batch_now":
        asyncio.run(f(fs, inp.get("key", "k")))
        return None
    if func == "finish_session_task":
        ie = guard_double(inp.get("interrupt_event")) if inp.get("interrupt_event") is not None else EventDouble()
        f(fs, sk, ie)
        return None
    if func == "spawn_drain_task":
        f(fs, ev(inp.get("pending_event")), sk, inp.get("delay", 0.0))
        return None
    if func == "drain_after":
        g = guard_double(inp.get("guard")) if inp.get("guard") is not None else None
        asyncio.run(f(fs, ev(inp.get("pending_event")), sk, inp.get("delay", 0.0), g))
        return None
    if func == "send_final_ledgered":
        r = asyncio.run(f(fs, e, sk, inp.get("text_content", ""), inp.get("metadata"),
                          reply_to=inp.get("reply_to"), is_ephemeral_response=inp.get("is_ephemeral_response", False)))
        result, adapter = r
        return {"result": norm(result), "adapter": {"id": adapter.id, "name": adapter.name}}
    if func == "send_final_text":
        rd = mk_stub("record_delivery", ARGSPEC["record_delivery"], "json", False, False)
        asyncio.run(f(fs, e, sk, inp.get("text_content", ""), inp.get("metadata"),
                      inp.get("is_ephemeral_response", False), inp.get("ephemeral_ttl", 0), rd))
        return None
    if func == "notify_turn_error":
        return asyncio.run(f(fs, e, Exception("boom")))
    if func == "deliver_attachments":
        # C resolves the final transport then runs the base seams on the SAME
        # state; model the resolved adapter as fs itself (scripted id is only
        # recorded, exactly like the port's freed resolver result).
        def _final_adapter(source):
            r = script("final_adapter")
            rec("final_adapter", na({"source": source}), r)
            maybe_raise(r)
            return fs
        fs._final_delivery_adapter = _final_adapter
        rd = mk_stub("record_delivery", ARGSPEC["record_delivery"], "json", False, False)
        ex = inp.get("extracted") or {}
        extracted = ns["_ExtractedResponse"](
            text_content=ex.get("text_content", ""), images=[tuple(x) for x in ex.get("images", [])],
            media_files=[tuple(x) for x in ex.get("media_files", [])], local_files=ex.get("local_files", []),
            force_document_attachments=ex.get("force_document_attachments", False),
            pre_extract=ex.get("pre_extract", ""))
        asyncio.run(f(fs, e, extracted, inp.get("metadata"), anything_sent=inp.get("anything_sent", False),
                      record_delivery=rd))
        return None
    if func == "emit_warning":
        return asyncio.run(f(fs, inp.get("chat_id", "c"), inp.get("content", ""),
                             reply_to=inp.get("reply_to"), metadata=inp.get("metadata"),
                             logical_platform=inp.get("logical_platform")))
    if func == "emit_media_warning":
        return asyncio.run(f(fs, inp.get("chat_id", "c"), inp.get("notice", ""),
                             caption=inp.get("caption"), reply_to=inp.get("reply_to"),
                             metadata=inp.get("metadata"), shown_metadata=inp.get("shown_metadata")))
    if func == "send_media_fallback_notice":
        return asyncio.run(f(fs, inp.get("method", "send_voice"), inp.get("kind", "audio"),
                             inp.get("path", "p"), inp.get("chat_id", "c"), inp.get("caption"),
                             inp.get("reply_to"), inp.get("metadata"), file_name=inp.get("file_name")))
    if func == "send_exec_approval":
        # the native render hook is the "send_prompt" seam (default returns
        # "Not supported"; the fixture scripts the adapter's native result)
        attach(fs, "_send_exec_approval_prompt", "send_prompt", "sendresult", True)
        return asyncio.run(f(fs, inp.get("chat_id", "c"), inp.get("command", ""), sk,
                             description=inp.get("description"), metadata=inp.get("metadata"),
                             allow_permanent=inp.get("allow_permanent", True),
                             allow_session=inp.get("allow_session", True),
                             smart_denied=inp.get("smart_denied", False), request_id=inp.get("request_id")))
    if func == "send_clarify":
        return asyncio.run(f(fs, inp.get("chat_id", "c"), inp.get("question", ""), inp.get("choices"),
                             inp.get("clarify_id", "cid"), sk, metadata=inp.get("metadata")))
    if func == "build_source":
        fields = inp.get("fields", {})
        if "config" not in (inp.get("st") or {}):
            fs.config = None   # sparse-st model: absent config passes as None
        r = f(fs, **fields)
        return norm(r)
    if func == "dispatch_inline_reply":
        asyncio.run(f(fs, e, log_cmd=inp.get("log_cmd")))
        return None
    if func in ("send_image", "send_animation", "send_voice", "send_video", "send_document", "send_image_file"):
        kw = dict(chat_id=inp.get("chat_id", "c"), caption=inp.get("caption"),
                  reply_to=inp.get("reply_to"), metadata=inp.get("metadata"))
        if func == "send_image":
            kw["image_url"] = inp.get("image_url", "")
        elif func == "send_animation":
            kw["animation_url"] = inp.get("animation_url", "")
        elif func == "send_voice":
            kw["audio_path"] = inp.get("audio_path", "")
        elif func == "send_video":
            kw["video_path"] = inp.get("video_path", "")
        elif func == "send_document":
            kw["file_path"] = inp.get("file_path", "")
            kw["file_name"] = inp.get("file_name")
        elif func == "send_image_file":
            kw["image_path"] = inp.get("image_path", "")
        return asyncio.run(f(fs, **kw))
    if func == "play_tts":
        return asyncio.run(f(fs, chat_id=inp.get("chat_id", "c"), audio_path=inp.get("audio_path", ""),
                             **inp.get("kwargs", {})))
    if func == "send_slash_confirm":
        return asyncio.run(f(fs, "c", "t", "m", sk, "cid"))
    if func == "send_exec_approval_prompt_default":
        return asyncio.run(f(fs, None))
    raise ValueError("unknown func: %s" % func)


def _owner_get_pending(owner_id):
    def stub(session_key):
        r = script("get_pending")
        rec("get_pending", na({"owner": owner_id, "session_key": session_key}), r)
        return materialize("event", maybe_raise(r))
    return stub


def _owner_start_session(owner_id):
    def stub(event, session_key, *, interrupt_event=None):
        r = script("start_session")
        rec("start_session", na({"owner": owner_id, "event": event, "session_key": session_key}), r)
        return bool(maybe_raise(r))
    return stub


# ── main ──

def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o, ordered=False):
    s = json.dumps(_numnorm(o), sort_keys=not ordered, ensure_ascii=False, default=str)
    return json.loads(s)


def main():
    ns = extract()
    ns_g.update(ns)
    install_base_stubs(ns)
    ns_g.update(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["tasks"] = {}
        CUR["guards"] = {}
        CUR["fs"] = None
        CUR["ev"] = None
        CUR["sr_keys"] = {}
        CUR["st_before"] = set()
        EventDouble._next_id = 1000
        try:
            want = drive(ns, func, inp)
        except Exception as ex:
            mism += 1
            import traceback
            print("PYERR %s: %r" % (func, ex))
            traceback.print_exc()
            continue
        # The C harness echoes st/event whenever the input carries them; the
        # early-return drivers don't build the echo themselves.
        if "st" in inp and "st" not in want:
            fs = CUR.get("fs")
            want["st"] = state_snapshot(fs, inp) if fs is not None else copy.deepcopy(inp.get("st") or {})
        if "event" in inp and "ev" not in want:
            e = CUR.get("ev")
            want["ev"] = event_snapshot(e) if e is not None else copy.deepcopy(inp.get("event") or {})
        want["trace"] = CUR["trace"]
        ordered = func in ("thread_metadata_for_source", "thread_metadata_for_event",
                           "mark_notify_metadata", "build_source")
        if canon(want, ordered) != canon(got, ordered):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE base_flow: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        print("FATAL base_flow: zero cases compared")
        return 2
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
