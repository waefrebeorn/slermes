#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 11.

Independent Python reference MODEL of the 16 LIVE
plugins/platforms/slack/adapter.py remainders ported by the C file. Each model
is a faithful transcription of the live function's DECISION logic with every
injected/unported callable replaced by a recording stub fed the SAME io
scripts the C harness feeds sa11_call. No quarry import chain is imported.

Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and echoed state ("state"). Summary:
'ORACLE slack_adapter_flow11: N cases, M mismatches'.

Fail-closed: missing/exhausted seam scripts and missing funcs => exit 2.
BaseException-only kinds pass through every except-Exception site; Exception-
family raises are caught exactly where the live body catches them.

Seam-name SET EQUALITY across C source / this file / fixture io keys.
"""
import asyncio
import re
import copy
import json
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Seam names MUST match the C port and the fixture io-key union exactly.
SEAM_NAMES = [
    "append_feedback_block",
    "apply_slack_proxy",
    "async_web_client",
    "auth_fn_call",
    "call_with_block_fallback",
    "chat_postEphemeral",
    "collect_thread_root_images",
    "create_task",
    "date_sub",
    "ensure_socket_watchdog",
    "entity_label_sub",
    "event_declares_bot_sender",
    "evict_oldest_by_ts",
    "extra_flag",
    "extract_assistant_thread_metadata",
    "fenced_code_sub",
    "fetch_thread_context",
    "find_user_mentions",
    "format_chunks",
    "get_client",
    "get_running_loop",
    "get_thread_watermark",
    "has_active_session_for_thread",
    "hermes_user_agent",
    "inline_code_sub",
    "inline_style_collapse",
    "is_sender_authorized",
    "logger.debug",
    "logger.warning",
    "mark_thread_rehydration_checked",
    "markdown_block_payload",
    "maybe_blocks",
    "mrkdwn_link_sub",
    "permalink_sub",
    "render_blocks",
    "resolve_user_is_bot",
    "resolve_user_name",
    "restart_socket_mode",
    "runner_auth_fn",
    "sanitize_blocks",
    "set_thread_watermark",
    "slack_allow_bots",
    "slack_response_payload",
    "slack_unfurl_kwargs",
    "sub_user_mention",
    "task_cancelled",
    "task_exception",
    "thread_rehydration_key",
    "trim_mentioned_threads",
    "truncate_message",
    "unescape_slack_entities",
    "workspace_message_marker",
    "workspace_thread_key",
]

TARGETS = [
    "_humanize_user_mentions",
    "_hydrate_thread_context",
    "_is_interactive_user_authorized",
    "_lookup_assistant_thread_metadata",
    "_mark_thread_rehydration_checked",
    "_maybe_blocks",
    "_new_web_client",
    "_normalize_slack_text_for_dedupe",
    "_note_attachment_failure",
    "_on_socket_mode_task_done",
    "_on_socket_watchdog_done",
    "_peer_bot_drop",
    "_post_chunks",
    "_post_ephemeral_fallback",
    "_register_mentioned_thread",
    "_track_reacting_message",
]

EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "OSError": OSError, "TimeoutError": TimeoutError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "LookupError": LookupError, "IndexError": IndexError, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "UnicodeError": UnicodeError, "BufferError": BufferError, "EOFError": EOFError,
    "MemoryError": MemoryError, "NameError": NameError, "ReferenceError": ReferenceError,
    "SyntaxError": SyntaxError, "SystemError": SystemError, "AssertionError": AssertionError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ConnectionError": ConnectionError, "ConnectionResetError": ConnectionResetError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "FileNotFoundError": FileNotFoundError,
    "FileExistsError": FileExistsError, "PermissionError": PermissionError,
    "NotADirectoryError": NotADirectoryError, "IsADirectoryError": IsADirectoryError,
    "InterruptedError": InterruptedError, "ProcessLookupError": ProcessLookupError,
    "ChildProcessError": ChildProcessError, "BlockingIOError": BlockingIOError,
    "RecursionError": RecursionError, "NotImplementedError": NotImplementedError,
    "UnboundLocalError": UnboundLocalError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "StopAsyncIteration": StopAsyncIteration,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "ResourceWarning": ResourceWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "BaseException": BaseException, "BaseExceptionGroup": BaseExceptionGroup,
    "ExceptionGroup": ExceptionGroup,
    "TimeoutException": Exception, "HTTPStatusError": Exception,
}


def canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False, default=str))


def T(v):
    """Mirror sa11_truthy."""
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, list):
        return len(v) > 0
    if isinstance(v, dict):
        if "__obj__" in v:
            return True
        m = v.get("__set__", v.get("__tuple__"))
        if m is not None:
            return isinstance(m, list) and len(m) > 0
        return len(v) > 0
    return False


def S(v, d=""):
    return v if isinstance(v, str) else d


def num(v, d=0):
    if isinstance(v, bool):
        return 1 if v else 0
    if isinstance(v, (int, float)):
        return v
    return d


def pystr(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return v
    if isinstance(v, (int, float)):
        if float(v).is_integer():
            return str(int(v))
        return str(v)
    return ""


def strval(v):
    return "" if v is None else pystr(v)


def strip(v):
    return strval(v).strip(" \t\n\r\x0b\x0c\x1c\x1d\x1e\x1f")


def lstrip_char(v, ch):
    return S(v, "").lstrip(ch)


def replace(s, frm, to):
    return S(s, "").replace(S(frm, ""), to)


def norm_raise(spec):
    d = {"__raise__": spec.get("type", "RuntimeError"), "message": spec.get("message", "")}
    for k, val in spec.items():
        if k not in ("type", "message"):
            d[k] = canon(val)
    return d


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    e = cls(spec.get("message", ""))
    e.spec = norm_raise(spec)
    return e


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def base_only(r):
    """True if the raise is NOT in the Exception family."""
    kind = r.get("__raise__", "Exception")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return not issubclass(cls, Exception)


_MISSING = object()


class Ctx:
    def __init__(self, io, state):
        self.io = io or {}
        self.state = state
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        args = canon(args if args is not None else {})
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            spec = {"type": "RuntimeError", "message": "no script for " + name}
            self.trace.append({"name": name, "args": args, "ret": norm_raise(spec)})
            raise make_exc(spec)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                spec = {"type": "RuntimeError", "message": "script exhausted for " + name}
                self.trace.append({"name": name, "args": args, "ret": norm_raise(spec)})
                raise make_exc(spec)
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                spec = used["raise"]
                self.trace.append({"name": name, "args": args, "ret": norm_raise(spec)})
                raise make_exc(spec)
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def spec_of(exc):
    return getattr(exc, "spec", {"__raise__": type(exc).__name__, "message": str(exc)})


def exc_msg(e):
    sp = getattr(e, "spec", None)
    return sp.get("message", str(e)) if isinstance(sp, dict) else str(e)


def sget(state, name, key, default=None):
    d = state.get(name)
    return canon(d.get(key, default)) if isinstance(d, dict) else canon(default)


def shas(state, name, key):
    d = state.get(name)
    return isinstance(d, dict) and key in d


def spop(state, name, key, default=None):
    d = state.get(name)
    if not isinstance(d, dict):
        d = {}
        state[name] = d
    return canon(d.pop(key, default))


def sset(state, name, key, val):
    d = state.get(name)
    if not isinstance(d, dict):
        d = {}
        state[name] = d
    d[key] = canon(val)


def sdel(state, name, key):
    d = state.get(name)
    if isinstance(d, dict):
        d.pop(key, None)


def t(key, args=None):
    return key


def collection_has_str(coll, s):
    if not s or coll is None:
        return False
    if isinstance(coll, dict) and "__set__" in coll:
        coll = coll["__set__"]
    if isinstance(coll, list):
        return any(isinstance(x, str) and x == s for x in coll)
    if isinstance(coll, dict):
        return s in coll
    return False


def parse_int(s):
    try:
        return int(s), True
    except (ValueError, TypeError):
        return 0, False



# ── local helpers mirroring the C port ──────────────────────────────────────
def set_add(state, name, val):
    d = state.get(name)
    if not isinstance(d, list):
        d = []
        state[name] = d
    if isinstance(val, str):
        if val in d:
            return
    d.append(canon(val))


def set_has(state, name, key):
    d = state.get(name)
    if not isinstance(d, list) or not isinstance(key, str):
        return False
    return key in d


def env_yes(s):
    return S(s) in ("true", "1", "yes")


def collapse_ws(v):
    return re.sub(r"[ \t\n\r\x0b\x0c\x1c-\x1f]+", " ", S(v)).strip(" \t\n\r\x0b\x0c\x1c\x1d\x1e\x1f")


def sr(success, err):
    return {"success": success, "error": canon(err), "message_id": None}


def handle_raise(ctx, callfn, try_handler):
    """Run callfn; on Exception-family raise run try_handler(exc-spec)."""
    try:
        return callfn()
    except BaseException as exc:
        sp = spec_of(exc)
        if base_only(sp):
            raise
        return try_handler(sp)


# ── models (faithful transcripts of the C flow) ─────────────────────────────
def m_new_web_client(ctx, a, state):
    ua = ctx.call("hermes_user_agent", {"label": "HermesAgent"})
    client = ctx.call("async_web_client", {"token": a.get("token"), "user_agent_prefix": ua})
    ctx.call("apply_slack_proxy", {"proxy_url": a.get("proxy_url"), "client": client})
    return client


def m_note_attachment_failure(ctx, a, state):
    detail = a.get("detail")
    if T(detail):
        set_add(state, "notices", detail)
        ctx.call("logger.warning", {"msg": "[Slack] %s", "args": [canon(detail)], "exc_info": False})
    else:
        ei = a.get("exc_info")
        ctx.call("logger.warning", {"msg": a.get("fallback_msg"),
                                    "args": canon(a.get("fallback_args") or []),
                                    "exc_info": bool(ei) if isinstance(ei, bool) else False})
    return None


def m_mark_thread_rehydration_checked(ctx, a, state):
    key = ctx.call("thread_rehydration_key", {"channel_id": a.get("channel_id"),
        "thread_ts": a.get("thread_ts"), "user_id": a.get("user_id"), "team_id": a.get("team_id")})
    set_add(state, "_thread_rehydration_checked", key)
    ctx.call("evict_oldest_by_ts", {"items": canon(state.get("_thread_rehydration_checked", [])),
                                    "max_size": num(a.get("max_size"), 100)})
    return None


def m_register_mentioned_thread(ctx, a, state):
    if not T(a.get("thread_ts")):
        return None
    marker = ctx.call("workspace_message_marker", {"team_id": a.get("team_id"), "ts": a.get("thread_ts")})
    set_add(state, "_mentioned_threads", marker)
    ctx.call("trim_mentioned_threads", {})
    return None


def m_track_reacting_message(ctx, a, state):
    marker = ctx.call("workspace_message_marker", {"team_id": a.get("team_id"), "ts": a.get("ts")})
    set_add(state, "_reacting_message_ids", marker)
    ctx.call("evict_oldest_by_ts", {"items": canon(state.get("_reacting_message_ids", [])),
                                    "max_size": num(a.get("max_size"), 100)})
    return None


def m_on_socket_watchdog_done(ctx, a, state):
    task = a.get("task") if isinstance(a.get("task"), dict) else {}
    tid = S(task.get("id"))
    wid = S(state.get("_socket_watchdog_task"))
    if not tid or not wid or tid != wid:
        return None
    cancelled = ctx.call("task_cancelled", {"task_id": tid})
    running = state.get("_running")
    run = isinstance(running, bool) and running
    if T(cancelled) or not run:
        return None
    exc = ctx.call("task_exception", {"task_id": tid})
    em = S(exc)
    if em:
        ctx.call("logger.warning", {"msg": "[Slack] Socket Mode watchdog exited with error; restarting: %s",
                                    "args": [canon(exc)], "exc_info": True})
    else:
        ctx.call("logger.warning", {"msg": "[Slack] Socket Mode watchdog exited; restarting",
                                    "args": [], "exc_info": False})
    state["_socket_watchdog_task"] = None
    ctx.call("ensure_socket_watchdog", {})
    return None


def m_on_socket_mode_task_done(ctx, a, state):
    task = a.get("task") if isinstance(a.get("task"), dict) else {}
    tid = S(task.get("id"))
    mid = S(state.get("_socket_mode_task"))
    running = state.get("_running")
    run = isinstance(running, bool) and running
    if not tid or not mid or tid != mid or not run:
        return None
    cancelled = ctx.call("task_cancelled", {"task_id": tid})
    if T(cancelled):
        return None
    em = None
    try:
        exc = ctx.call("task_exception", {"task_id": tid})
        em = S(exc)
    except BaseException as exc0:
        sp = spec_of(exc0)
        if sp.get("__raise__") == "CancelledError":
            return None
        if base_only(sp):
            raise
        ctx.call("logger.debug", {})
    if em:
        ctx.call("logger.warning", {"msg": "[Slack] Socket Mode task exited with error: %s",
                                    "args": [em], "exc_info": True})
    else:
        ctx.call("logger.warning", {"msg": "[Slack] Socket Mode task exited unexpectedly",
                                    "args": [], "exc_info": False})
    try:
        ctx.call("get_running_loop", {})
    except BaseException as exc1:
        sp = spec_of(exc1)
        cls = EXC_CLASSES.get(sp.get("__raise__"), Exception)
        if issubclass(cls, RuntimeError):
            return None
        raise
    ctx.call("restart_socket_mode", {})
    ctx.call("create_task", {"target": "restart_socket_mode"})
    return None


def m_peer_bot_drop(ctx, a, state):
    uid = S(a.get("user_id"))
    bot = S(a.get("bot_uid"))
    if not T(a.get("user_id")) or (uid and bot and uid == bot):
        return False
    declared = ctx.call("event_declares_bot_sender", {"event": canon(a.get("event"))})
    is_bot = T(declared)
    if not is_bot:
        is_bot = T(ctx.call("resolve_user_is_bot", {"user_id": uid,
                        "chat_id": a.get("channel_id"), "team_id": a.get("team_id")}))
    if not is_bot:
        return False
    allow = S(ctx.call("slack_allow_bots", {}))
    mentioned = bool(a.get("is_mentioned")) if isinstance(a.get("is_mentioned"), bool) else False
    return allow == "none" or (allow == "mentions" and not mentioned)


def m_maybe_blocks(ctx, a, state):
    on = ctx.call("extra_flag", {"key": "markdown_blocks"})
    if T(on):
        md = ctx.call("markdown_block_payload", {"content": a.get("content")})
        if T(md):
            fb = ctx.call("append_feedback_block", {"blocks": canon(md)})
            return ctx.call("sanitize_blocks", {"blocks": canon(fb)})
    rich = ctx.call("extra_flag", {"key": "rich_blocks"})
    if not T(rich):
        return None
    try:
        blocks = ctx.call("render_blocks", {"content": a.get("content"),
                                            "mrkdwn_fn": "format_message"})
    except BaseException as exc:
        sp = spec_of(exc)
        if base_only(sp):
            raise
        ctx.call("logger.debug", {})
        return None
    fb = ctx.call("append_feedback_block", {"blocks": canon(blocks)})
    return ctx.call("sanitize_blocks", {"blocks": canon(fb)})


def m_post_ephemeral_fallback(ctx, a, state):
    cx = a.get("ctx") if isinstance(a.get("ctx"), dict) else {}
    uid = S(cx.get("user_id"))
    if not T(cx.get("user_id")) or not uid:
        return sr(False, "no user_id in slash context for postEphemeral")
    chunks = ctx.call("format_chunks", {"content": a.get("content")})
    client = ctx.call("get_client", {"chat_id": a.get("chat_id")})
    for chunk in (chunks if isinstance(chunks, list) else []):
        try:
            res = ctx.call("chat_postEphemeral", {"client": canon(client),
                "channel": a.get("chat_id"), "user": uid, "text": canon(chunk)})
        except BaseException as exc:
            sp = spec_of(exc)
            if base_only(sp):
                raise
            return sr(False, sp.get("message", ""))
        try:
            payload = ctx.call("slack_response_payload", {"response": canon(res)})
        except BaseException as exc:
            sp = spec_of(exc)
            if base_only(sp):
                raise
            return sr(False, sp.get("message", ""))
        if not (isinstance(payload, dict) and T(payload.get("ok"))):
            if isinstance(payload, dict) and len(payload) > 0:
                err = S(payload.get("error")) or "unknown_error"
                return sr(False, "chat.postEphemeral failed: " + err)
            return sr(False, "chat.postEphemeral failed: unexpected_response")
    return sr(True, None)


def m_post_chunks(ctx, a, state):
    chunks = ctx.call("truncate_message", {"text": a.get("formatted"),
                                          "max": num(a.get("max_length"), 3900)})
    extra = a.get("extra")
    broadcast = bool(extra.get("reply_broadcast")) if isinstance(extra, dict) else False
    blocks = None
    cl = chunks if isinstance(chunks, list) else []
    if len(cl) == 1:
        blocks = ctx.call("maybe_blocks", {"content": a.get("content")})
    thread = S(a.get("thread_ts"))
    has_thread = T(a.get("thread_ts")) and bool(thread)
    last = None
    for i, chunk in enumerate(cl):
        unf = ctx.call("slack_unfurl_kwargs", {"extra": canon(extra)})
        kw = {"channel": a.get("chat_id"), "text": canon(chunk), "mrkdwn": True}
        if isinstance(unf, dict):
            for k, v in unf.items():
                kw[k] = v
        if T(blocks) and i == 0:
            kw["blocks"] = canon(blocks)
        if has_thread:
            kw["thread_ts"] = thread
            if broadcast and i == 0:
                kw["reply_broadcast"] = True
        last = ctx.call("call_with_block_fallback", {"client_fn": "client_fn",
            "method": "chat_postMessage", "kwargs": canon(kw), "purpose": "send"})
    return last


def m_lookup_assistant_thread_metadata(ctx, a, state):
    md = ctx.call("extract_assistant_thread_metadata", {"event": canon(a.get("event")),
                                                        "body": canon(a.get("body"))})
    if not isinstance(md, dict):
        md = {}
    if T(a.get("channel_id")) and not T(md.get("channel_id")):
        md["channel_id"] = canon(a.get("channel_id"))
    if T(a.get("thread_ts")) and not T(md.get("thread_ts")):
        md["thread_ts"] = canon(a.get("thread_ts"))
    if T(a.get("team_id")) and not T(md.get("team_id")):
        md["team_id"] = pystr(a.get("team_id"))
    key = ctx.call("workspace_thread_key", {"team_id": S(md.get("team_id", "")),
        "channel_id": S(md.get("channel_id", "")), "thread_ts": S(md.get("thread_ts", ""))})
    store = state.get("_assistant_threads")
    cached = None
    if T(key) and isinstance(store, dict):
        cached = store.get(S(key))
    if T(cached):
        out = {}
        if isinstance(cached, dict):
            out.update(canon(cached))
        for k, v in md.items():
            if T(v):
                out[k] = canon(v)
        return out
    return md


def m_humanize_user_mentions(ctx, a, state):
    tv = a.get("text")
    text = S(tv)
    if not T(tv) or "<@" not in text:
        return canon(tv)
    work = text
    uids = ctx.call("find_user_mentions", {"text": text})
    for uidv in (uids if isinstance(uids, list) else []):
        uid = S(uidv)
        name = ctx.call("resolve_user_name", {"user_id": uid, "chat_id": a.get("chat_id"),
                                              "team_id": a.get("team_id")})
        ns = name if isinstance(name, str) else None
        base = ns if (T(name) and ns) else uid
        cl = strip(base)
        disp = cl if cl else uid
        work = S(ctx.call("sub_user_mention", {"text": work, "uid": uid, "replacement": "@" + disp}))
    return work


def m_hydrate_thread_context(ctx, a, state):
    cc = None
    if not T(a.get("is_thread_reply")):
        return [None, [], []]
    chat_type = "dm" if (bool(a.get("is_dm")) if isinstance(a.get("is_dm"), bool) else False) else "group"
    is_ment = bool(a.get("is_mentioned")) if isinstance(a.get("is_mentioned"), bool) else False
    wa_common = {"channel_id": a.get("channel_id"), "thread_ts": a.get("event_thread_ts"),
                 "user_id": a.get("user_id"), "team_id": a.get("team_id")}
    def fetch(after=None, force=False):
        nonlocal cc
        fa = dict(wa_common)
        fa["current_ts"] = a.get("ts")
        fa["chat_type"] = chat_type
        if after is not None:
            fa["after_ts"] = canon(after)
        if force:
            fa["force_refresh"] = True
        r = ctx.call("fetch_thread_context", fa)
        if T(r):
            cc = r
    ha = dict(wa_common); ha["chat_type"] = chat_type
    has = ctx.call("has_active_session_for_thread", ha)
    active = T(has)
    urls, types = [], []
    if not active:
        fetch()
        im = ctx.call("collect_thread_root_images", {"channel_id": a.get("channel_id"),
            "thread_ts": a.get("event_thread_ts"), "team_id": a.get("team_id")})
        if isinstance(im, list):
            urls = canon(im[0]) if len(im) > 0 else []
            types = canon(im[1]) if len(im) > 1 else []
    elif is_ment:
        w = ctx.call("get_thread_watermark", dict(wa_common))
        fetch(after=w, force=True)
    else:
        rk = ctx.call("thread_rehydration_key", dict(wa_common))
        if set_has(state, "_thread_rehydration_checked", S(rk)):
            sw = dict(wa_common); sw["watermark_ts"] = canon(a.get("ts"))
            ctx.call("set_thread_watermark", sw)
            return [None, [], []]
        wts = ctx.call("get_thread_watermark", dict(wa_common))
        if T(wts):
            fetch(after=wts, force=True)
    sw = dict(wa_common); sw["watermark_ts"] = canon(a.get("ts"))
    ctx.call("set_thread_watermark", sw)
    ctx.call("mark_thread_rehydration_checked", dict(wa_common))
    return [canon(cc), urls, types]


def m_normalize_slack_text_for_dedupe(ctx, a, state):
    work = S(a.get("text"))
    for name in ("unescape_slack_entities", "mrkdwn_link_sub", "date_sub", "permalink_sub",
                 "entity_label_sub"):
        work = S(ctx.call(name, {"text": work}))
    bot = S(a.get("bot_uid"))
    if bot:
        work = work.replace("<@" + bot + ">", "")
    for name in ("fenced_code_sub", "inline_code_sub", "inline_style_collapse"):
        work = S(ctx.call(name, {"text": work}))
    return collapse_ws(work)


def m_is_interactive_user_authorized(ctx, a, state):
    nid = strip(strval(a.get("user_id")))
    if not nid:
        return False
    cid = S(a.get("channel_id"))
    chat_type = "dm" if cid.startswith("D") else "group"
    marker = state.get("_authorization_check")
    if marker is not None:
        injected = ctx.call("is_sender_authorized", {"user_id": nid, "chat_type": chat_type,
                                                      "channel_id": cid})
        if injected is not None:
            return canon(injected)
    authfn = ctx.call("runner_auth_fn", {})
    if T(authfn):
        src = {"platform": "slack", "chat_id": cid if cid else nid, "chat_type": chat_type,
               "user_id": nid}
        un = a.get("user_name")
        src["user_name"] = strip(strval(un)) if T(un) else None
        ti = a.get("team_id")
        src["scope_id"] = pystr(ti) if T(ti) else None
        try:
            r = ctx.call("auth_fn_call", {"source": canon(src)})
            return bool(T(r))
        except BaseException as exc:
            sp = spec_of(exc)
            if base_only(sp):
                raise
            ctx.call("logger.debug", {})
    env = state.get("_gate_env") if isinstance(state.get("_gate_env"), dict) else {}
    if env_yes(env.get("SLACK_ALLOW_ALL_USERS")):
        return True
    allowed = set()
    for var in ("SLACK_ALLOWED_USERS", "GATEWAY_ALLOWED_USERS"):
        for part in S(env.get(var)).split(","):
            cl = part.strip()
            if cl:
                allowed.add(cl)
    if allowed:
        return "*" in allowed or nid in allowed
    return env_yes(env.get("GATEWAY_ALLOW_ALL_USERS"))


MODELS = {
    "_humanize_user_mentions": m_humanize_user_mentions,
    "_hydrate_thread_context": m_hydrate_thread_context,
    "_is_interactive_user_authorized": m_is_interactive_user_authorized,
    "_lookup_assistant_thread_metadata": m_lookup_assistant_thread_metadata,
    "_mark_thread_rehydration_checked": m_mark_thread_rehydration_checked,
    "_maybe_blocks": m_maybe_blocks,
    "_new_web_client": m_new_web_client,
    "_normalize_slack_text_for_dedupe": m_normalize_slack_text_for_dedupe,
    "_note_attachment_failure": m_note_attachment_failure,
    "_on_socket_mode_task_done": m_on_socket_mode_task_done,
    "_on_socket_watchdog_done": m_on_socket_watchdog_done,
    "_peer_bot_drop": m_peer_bot_drop,
    "_post_chunks": m_post_chunks,
    "_post_ephemeral_fallback": m_post_ephemeral_fallback,
    "_register_mentioned_thread": m_register_mentioned_thread,
    "_track_reacting_message": m_track_reacting_message,
}


def drive(func, inp):
    args = inp.get("args") or {}
    state0 = inp.get("state") or {}
    state = copy.deepcopy(state0)
    ctx = Ctx(inp.get("io"), state)
    try:
        ret = MODELS[func](ctx, args, state)
    except BaseException as exc:
        ret = spec_of(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


def cross_check():
    csrc = open(os.path.join(_REPO, "src", "gateway", "platforms", "slack_adapter_flow11.c"),
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow11: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if '"' + n + '"' not in csrc]
    if missing_c:
        print("FATAL oracle_slack_adapter_flow11: seams absent from C port: " + str(missing_c))
        sys.exit(2)
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow11.jsonl")
    if os.path.exists(fix):
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print("FATAL oracle_slack_adapter_flow11: fixture io keys != SEAM_NAMES; "
                  "missing=" + str(sorted(ss - fix_keys)) + " extra=" + str(sorted(fix_keys - ss)))
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
    print("MISMATCH " + func + ":")
    if want.get("ret") != got.get("ret"):
        print("  ret want=" + json.dumps(want.get("ret"), ensure_ascii=False)[:500])
        print("  ret got = " + json.dumps(got.get("ret"), ensure_ascii=False)[:500])
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print("  trace[" + str(i) + "] want=" + json.dumps(w, ensure_ascii=False)[:500])
        print("  trace[" + str(i) + "] got = " + json.dumps(g, ensure_ascii=False)[:500])
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        print("  state want=" + json.dumps(ws, ensure_ascii=False)[:300])
        print("  state got = " + json.dumps(gs, ensure_ascii=False)[:300])


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
            print("MISMATCH <unparsable harness line>: " + str(exc))
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print("MISMATCH " + func + ": harness emitted no result: " + str(got))
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print("MISMATCH " + func + ": oracle drive raised " + type(exc).__name__ + ": " + str(exc))
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print("ORACLE slack_adapter_flow11: " + str(n) + " cases, " + str(m) + " mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow11.jsonl")
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
            print("SELFTEST DRIVE RAISED " + func + " case " + str(n) + ": " +
                  type(exc).__name__ + ": " + str(exc))
            continue
        r = out["ret"]
        expect = case.get("expect_raise")
        if expect:
            if not (isinstance(r, dict) and r.get("__raise__") == expect):
                bad += 1
                print("SELFTEST EXPECT " + func + " case " + str(n) + ": wanted raise " +
                      str(expect) + ", got " + json.dumps(r)[:200])
        elif isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print("SELFTEST SCRIPT GAP " + func + " case " + str(n) + ": " + str(r["message"]))
    missing = [f for f in TARGETS if f not in counts]
    if missing:
        bad += 1
        print("SELFTEST MISSING FUNCS: " + str(missing))
    thin = [f for f, c in counts.items() if c < 6]
    if thin:
        bad += 1
        print("SELFTEST THIN FUNCS (<6 cases): " + str(thin))
    print("SELFTEST slack_adapter_flow11: " + str(n) + " cases, " + str(bad) + " drive/script failures")
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))
    return 1 if bad or n == 0 else 0


def dump():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow11.jsonl")
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        out = drive(case["func"], case.get("in") or {})
        print(json.dumps({"func": case["func"], "in": case.get("in"), "want": out},
                         ensure_ascii=False))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--dump":
        dump()
    else:
        main()

