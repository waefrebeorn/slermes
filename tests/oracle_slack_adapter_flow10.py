#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 10.

Independent Python reference MODEL of the 16 LIVE
plugins/platforms/slack/adapter.py remainders ported by the C file. Each model
is a faithful transcription of the live function's DECISION logic with every
injected/unported callable replaced by a recording stub fed the SAME io
scripts the C harness feeds sa10_call. No quarry import chain is imported.

Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and echoed state ("state"). Summary:
'ORACLE slack_adapter_flow10: N cases, M mismatches'.

Fail-closed: missing/exhausted seam scripts and missing funcs => exit 2.
BaseException-only kinds pass through every except-Exception site; Exception-
family raises are caught exactly where the live body catches them.

Seam-name SET EQUALITY across C source / this file / fixture io keys.
"""
import asyncio
import copy
import json
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Seam names MUST match the C port and the fixture io-key union exactly.
SEAM_NAMES = [
    "_handle_slack_message_impl", "ack", "agent_view_context_for_event",
    "append_block_text", "append_forwards", "append_link_unfurls",
    "apply_bot_mention", "begin_interaction", "build_message_event",
    "build_model_picker_model_blocks", "build_model_picker_provider_blocks",
    "build_source", "build_thread_session_key", "cache_assistant_thread_metadata",
    "channel_gate_allows", "channel_prompt_with_identity", "channel_team",
    "chat_postEphemeral", "chat_postMessage", "chat_update", "clarify_entries_get",
    "clarify_mark_awaiting_text", "clarify_resolve", "client",
    "collect_inbound_media", "dedup_is_duplicate", "describe_slack_api_error",
    "ea_deadline_line", "early_reject_unauthorized", "event_command",
    "event_declares_bot_sender", "event_session_key", "event_team_id",
    "extract_assistant_thread_metadata", "files_info", "finalize_interactive_message",
    "first_file_share", "get_client", "get_label", "get_scoped_secret",
    "handle_message", "handle_slack_message", "handle_slash_command",
    "hydrate_thread_context", "is_foreign_author", "is_ignored_channel",
    "is_interactive_user_authorized", "is_mentioned", "logger.debug",
    "logger.error", "logger.info", "logger.warning",
    "lookup_assistant_thread_metadata", "model_picker_expired_notice",
    "monotonic", "msg_event_set_text", "on_model_selected", "peer_bot_drop",
    "picker_refusal", "picker_state_get", "picker_state_pop", "picker_state_set",
    "prefilter_inbound", "reaction_handler_call", "reaction_handler_get",
    "reaction_thread_ts", "reactions_enabled", "refusal_text",
    "refuse_other_requester", "remember_channel_team", "resolve_approval",
    "resolve_author_team", "resolve_channel_name", "resolve_channel_skills",
    "resolve_command", "resolve_slash_confirm", "resolve_user_is_bot",
    "resolve_user_name", "rewrite_known_bang_command", "sanitize_blocks",
    "section_text", "seed_dm_session", "session_ensure_loaded", "session_entry",
    "session_thread_ts", "set_assistant_suggested_prompts", "slack_allow_bots",
    "slack_disable_dms", "slack_mention_detection_text", "slack_reaction_triggers",
    "slack_reply_expected", "slash_channel_gated", "slash_command_text",
    "slash_lanes_release", "slash_lanes_reserve", "slash_lanes_wait",
    "slash_thread_id", "slash_user_id_reset", "slash_user_id_set", "sleep",
    "source_set_fields", "stash_slash_context", "synthetic_reaction_event",
    "synthetic_set", "t_error_prefix", "team_bot_user_id", "team_clients",
    "team_clients_first", "team_clients_get", "team_in_install_set", "time",
    "track_reacting_message", "update_clarify_message", "update_picker_message",
    "workspace_event_id", "workspace_message_marker",
]

TARGETS = [
    "_handle_approval_action", "_handle_assistant_thread_lifecycle_event",
    "_handle_clarify_action", "_handle_feedback_action", "_handle_hermes_command",
    "_handle_model_picker_action", "_handle_slack_file_shared",
    "_handle_slack_message", "_handle_slack_message_impl", "_handle_slack_reaction",
    "_handle_slash_command", "_handle_slash_confirm_action",
    "_has_active_session_for_thread", "_hint_allow_bots",
    "_slack_reaction_trigger_target", "_socket_ping_pong_stale",
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
    """Mirror sa10_truthy."""
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


# ── models ────────────────────────────────────────────────────────────────────
def m_hint_allow_bots(ctx, a, state):
    allow = ctx.call("slack_allow_bots", {})
    if is_raise(allow):
        return allow
    if S(allow) != "none":
        r = ctx.call("logger.info", {"allow": S(allow)})
        if is_raise(r):
            return r
    return None


def m_slack_reaction_trigger_target(ctx, a, state):
    extra = a.get("extra")
    raw = extra.get("reaction_trigger_target") if isinstance(extra, dict) else None
    if raw is None:
        raw = ctx.call("get_scoped_secret", {"name": "SLACK_REACTION_TRIGGER_TARGET", "default": ""})
        if is_raise(raw):
            return raw
    st = strip(raw)
    channel, sep, thread = st.partition(":")
    return [channel.strip(), thread.strip()]


def m_socket_ping_pong_stale(ctx, a, state):
    client = ctx.call("client", {})
    if is_raise(client):
        return client
    if not isinstance(client, dict):
        return False
    piv = client.get("ping_interval")
    last = client.get("last_ping_pong_time")
    if not isinstance(piv, (int, float)) or isinstance(piv, bool) or piv <= 0:
        return False
    ping = piv
    if last is None:
        started = a.get("started")
        if started is None:
            return False
        grace_s = a.get("grace_s", 0)
        grace = max(grace_s, ping * 2)
        now = ctx.call("monotonic", {})
        if is_raise(now):
            return now
        return (num(now) - num(started)) > grace
    if not isinstance(last, (int, float)) or isinstance(last, bool):
        return False
    now = ctx.call("time", {})
    if is_raise(now):
        return now
    return (num(now) - num(last)) > (ping * num(a.get("stale_factor")))


def m_has_active_session_for_thread(ctx, a, state):
    store = a.get("session_store")
    if store is None or not T(store):
        return False
    try:
        key = ctx.call("build_thread_session_key", {
            "channel_id": a.get("channel_id"), "thread_ts": a.get("thread_ts"),
            "user_id": a.get("user_id"), "team_id": a.get("team_id"),
            "chat_type": a.get("chat_type")})
        if not T(key):
            return False
        ctx.call("session_ensure_loaded", {})
        entry = ctx.call("session_entry", {})
        if entry is None:
            return False
        return not T(entry.get("suspended"))
    except BaseException as exc:
        if base_only(spec_of(exc)):
            return spec_of(exc)
        return False


def m_handle_feedback_action(ctx, a, state):
    r = ctx.call("ack", {})
    if is_raise(r):
        return r
    action = a.get("action") or {}
    value = strval(action.get("value"))
    body = a.get("body") or {}
    message = body.get("message")
    if not isinstance(message, dict):
        message = {}
    channel_id = (body.get("channel") or {}).get("id", "")
    user_id = (body.get("user") or {}).get("id", "")
    r = ctx.call("logger.info", {"value": value, "user_id": user_id,
                                 "channel_id": channel_id, "ts": message.get("ts", "")})
    if is_raise(r):
        return r
    return None


def m_handle_hermes_command(ctx, a, state):
    command = a.get("command") or {}
    g = ctx.call("slash_channel_gated", {"channel_id": command.get("channel_id", "")})
    if is_raise(g):
        return g
    if T(g):
        r = ctx.call("ack", {})
        if is_raise(r):
            return r
    else:
        slash = lstrip_char(command.get("command") or "", "/")
        r = ctx.call("ack", {"response_type": "ephemeral",
                             "text": "platform.slack.slash.running"})
        if is_raise(r):
            return r
    r = ctx.call("handle_slash_command", {"command": command})
    return r


def m_handle_assistant_thread_lifecycle_event(ctx, a, state):
    md = ctx.call("extract_assistant_thread_metadata", {"event": a.get("event"), "body": a.get("body")})
    if is_raise(md):
        return md
    r = ctx.call("cache_assistant_thread_metadata", {"metadata": md})
    if is_raise(r):
        return r
    tts = S(md.get("thread_ts"))
    if tts:
        r = ctx.call("seed_dm_session", {
            "metadata": md, "thread_ts": tts,
            "fail_log": ["[Slack] Failed to seed assistant thread session for %s/%s",
                         md.get("channel_id", ""), tts]})
        if is_raise(r):
            return r
    r = ctx.call("set_assistant_suggested_prompts", {
        "channel_id": md.get("channel_id"), "team_id": md.get("team_id"),
        "thread_ts": md.get("thread_ts")})
    if is_raise(r):
        return r
    return None


def _started(started):
    if not isinstance(started, list) or len(started) < 8:
        return None
    return list(canon(started[:8]))


def m_handle_approval_action(ctx, a, state):
    started = _started(ctx.call("begin_interaction", {"kind": "approval"}))
    if started is None:
        return None
    team_id, action_id, session_key, message, msg_ts, channel_id, user_name, user_id = started
    choice = "approve" if action_id == "approve" else "deny"
    if shas(state, "_approval_resolved", msg_ts):
        akey = msg_ts
    else:
        m = ctx.call("workspace_message_marker", {"team_id": team_id, "msg_ts": msg_ts})
        if is_raise(m):
            return m
        akey = S(m)
    req_peek = sget(state, "_approval_request_ids", akey)
    refuse = ctx.call("refuse_other_requester", {
        "session_key": session_key, "user_id": user_id, "request_id": req_peek,
        "choice": choice, "body": a.get("body")})
    if is_raise(refuse):
        return refuse
    if T(refuse):
        return None
    if T(spop(state, "_approval_resolved", akey, True)):
        return None
    req = spop(state, "_approval_request_ids", akey)
    try:
        count = ctx.call("resolve_approval", {
            "session_key": session_key, "choice": choice, "request_id": req,
            "actor_id": user_id, "extra": a.get("extra")})
        ctx.call("logger.info", {"choice": choice, "user_id": user_id, "count": num(count)})
    except BaseException as exc:
        if base_only(spec_of(exc)):
            return spec_of(exc)
        ctx.call("logger.error", {"error": exc_msg(exc)})
        count = 0
    dt = "platform.slack.approval." + choice
    if num(count) == 0:
        dt = "platform.shared.approval_expired"
    st = ctx.call("section_text", {"message": message})
    if is_raise(st):
        return st
    dl = ctx.call("ea_deadline_line", {})
    if is_raise(dl):
        return dl
    orig = replace(S(st), S(dl), "")
    r = ctx.call("finalize_interactive_message", {
        "channel_id": channel_id, "msg_ts": msg_ts, "original_text": orig,
        "decision_text": dt, "placeholder": "Command approval request",
        "label": "approval", "team_id": team_id if team_id else None})
    if is_raise(r):
        return r
    return None


def m_handle_slash_confirm_action(ctx, a, state):
    started = _started(ctx.call("begin_interaction", {"kind": "slash-confirm"}))
    if started is None:
        return None
    team_id, action_id, value, message, msg_ts, channel_id, user_name, user_id = started
    if "|" not in value:
        r = ctx.call("logger.warning", {"value": value})
        return r if is_raise(r) else None
    session_key, confirm_id = value.split("|", 1)
    refuse = ctx.call("refuse_other_requester", {
        "session_key": session_key, "confirm_id": confirm_id,
        "user_id": user_id, "extra": a.get("extra")})
    if is_raise(refuse):
        return refuse
    if T(refuse):
        return None
    choice = "confirm" if action_id == "confirm" else "cancel"
    dt = "platform.slack.slash_confirm." + choice
    st = ctx.call("section_text", {"message": message})
    if is_raise(st):
        return st
    r = ctx.call("finalize_interactive_message", {
        "channel_id": channel_id, "msg_ts": msg_ts, "original_text": S(st),
        "decision_text": dt, "placeholder": "Confirmation prompt",
        "label": "slash-confirm", "team_id": team_id if team_id else None})
    if is_raise(r):
        return r
    try:
        res = ctx.call("resolve_slash_confirm", {
            "session_key": session_key, "confirm_id": confirm_id, "choice": choice})
        if T(res):
            pk = {"channel": channel_id, "text": res}
            thread_ts = (message.get("thread_ts") if isinstance(message, dict) else None) or msg_ts
            if thread_ts:
                pk["thread_ts"] = thread_ts
            ctx.call("get_client", {})
            r = ctx.call("chat_postMessage", pk)
            if is_raise(r):
                return r
    except BaseException as exc:
        if base_only(spec_of(exc)):
            return spec_of(exc)
        ctx.call("logger.error", {"error": exc_msg(exc)})
        return None
    r = ctx.call("logger.info", {"session_key": value, "choice": choice, "user_id": user_id})
    if is_raise(r):
        return r
    return None


def m_handle_clarify_action(ctx, a, state):
    started = _started(ctx.call("begin_interaction", {"kind": "clarify", "team_scoped": False}))
    if started is None:
        return None
    _team_id, action_id, value, message, msg_ts, channel_id, user_name, user_id = started
    if "|" not in value:
        r = ctx.call("logger.warning", {"value": value})
        return r if is_raise(r) else None
    clarify_id, token = value.split("|", 1)
    refuse = ctx.call("refuse_other_requester", {
        "clarify_id": clarify_id, "user_id": user_id, "extra": a.get("extra")})
    if is_raise(refuse):
        return refuse
    if T(refuse):
        return None
    if T(spop(state, "_clarify_resolved", msg_ts, True)):
        return None
    st = ctx.call("section_text", {"message": message, "limit": 0})
    if is_raise(st):
        return st
    orig = S(st)
    exp = "platform.slack.clarify.expired"
    if action_id == "hermes_clarify_other" or token == "other":
        ok = ctx.call("clarify_mark_awaiting_text", {"clarify_id": clarify_id})
        if is_raise(ok):
            return ok
        if not T(ok):
            sdel(state, "_clarify_messages", clarify_id)
            r = ctx.call("update_clarify_message", {
                "channel_id": channel_id, "msg_ts": msg_ts,
                "original_text": orig, "decision_text": exp})
            return r if is_raise(r) else None
        r = ctx.call("update_clarify_message", {
            "channel_id": channel_id, "msg_ts": msg_ts, "original_text": orig,
            "decision_text": "platform.slack.clarify.awaiting"})
        return r if is_raise(r) else None
    idx, ok = parse_int(token)
    if not ok:
        r = ctx.call("logger.warning", {"token": token})
        return r if is_raise(r) else None
    sdel(state, "_clarify_messages", clarify_id)
    resolved = None
    try:
        entry = ctx.call("clarify_entries_get", {"clarify_id": clarify_id})
        if isinstance(entry, dict):
            choices = entry.get("choices")
            if isinstance(choices, list) and 0 <= idx < len(choices):
                resolved = strval(choices[idx])
    except BaseException as exc:
        if base_only(spec_of(exc)):
            return spec_of(exc)
        resolved = None
    if resolved is None:
        resolved = "choice " + str(idx + 1)
    try:
        ok = ctx.call("clarify_resolve", {"clarify_id": clarify_id, "resolved_text": resolved})
    except BaseException as exc:
        if base_only(spec_of(exc)):
            return spec_of(exc)
        ok = None
    if T(ok):
        r = ctx.call("update_clarify_message", {
            "channel_id": channel_id, "msg_ts": msg_ts, "original_text": orig,
            "decision_text": "platform.slack.clarify.resolved"})
        if is_raise(r):
            return r
        r = ctx.call("logger.info", {"clarify_id": clarify_id, "idx": idx, "user_id": user_id})
        if is_raise(r):
            return r
        r = ctx.call("logger.debug", {"resolved_text": resolved})
        if is_raise(r):
            return r
    else:
        r = ctx.call("update_clarify_message", {
            "channel_id": channel_id, "msg_ts": msg_ts, "original_text": orig,
            "decision_text": exp})
        if is_raise(r):
            return r
        r = ctx.call("logger.warning", {"clarify_id": clarify_id})
        if is_raise(r):
            return r
    return None

def _mp_update(ctx, chan, team_v, mts, text):
    return ctx.call("update_picker_message", {
        "channel_id": chan, "team_id": team_v, "msg_ts": mts, "text": text})


def _mp_chat_update(ctx, blocks, chan, mts, text, team_v):
    sb = ctx.call("sanitize_blocks", blocks)
    if is_raise(sb):
        if base_only(sb):
            return sb
        ctx.call("logger.warning", {"error": sb["message"]})
        return None
    cl = ctx.call("get_client", {})
    if is_raise(cl):
        return None
    up = ctx.call("chat_update", {"channel": chan, "ts": mts, "text": text, "blocks": sb})
    if is_raise(up):
        if base_only(up):
            return up
        ctx.call("logger.warning", {"error": up["message"]})
    return None


def m_handle_model_picker_action(ctx, a, state):
    r = ctx.call("ack", {})
    if is_raise(r):
        return r
    team_v = ctx.call("event_team_id", {"event": a.get("event"), "body": a.get("body"),
                                       "install_set": a.get("install_set")})
    if is_raise(team_v):
        return team_v
    action = a.get("action") or {}
    action_id = S(action.get("action_id"))
    body = a.get("body") or {}
    message = body.get("message") or {}
    msg_ts = S(message.get("ts"))
    channel_id = S((body.get("channel") or {}).get("id"))
    user_name = S((body.get("user") or {}).get("name"), "unknown")
    user_id = S((body.get("user") or {}).get("id"))
    auth = ctx.call("is_interactive_user_authorized", {
        "user_id": user_id, "channel_id": channel_id, "user_name": user_name,
        "team_id": team_v})
    if is_raise(auth):
        return auth
    if not T(auth):
        r = ctx.call("logger.warning", {"user_id": user_id})
        return r if is_raise(r) else None
    if msg_ts in (state.get("_model_picker_state") or {}):
        marker = msg_ts
    else:
        m = ctx.call("workspace_message_marker", {"team_id": S(team_v), "msg_ts": msg_ts})
        if is_raise(m):
            return m
        marker = S(m)
    st = ctx.call("picker_state_get", {"marker": marker})
    if is_raise(st):
        return st
    if not T(st):
        notice = ctx.call("model_picker_expired_notice", {})
        if is_raise(notice):
            return notice
        return _mp_update(ctx, channel_id, team_v, msg_ts, notice)
    onms = st.get("on_model_selected")
    requester = ctx.call("picker_refusal", {
        "on_model_selected": onms, "user_id": user_id, "extra": a.get("extra")})
    if is_raise(requester):
        return requester
    if T(requester):
        rt = ctx.call("refusal_text", requester)
        if is_raise(rt):
            return rt
        cl = ctx.call("get_client", {})
        if is_raise(cl):
            return cl
        return ctx.call("chat_postEphemeral", {
            "channel": channel_id, "user": user_id, "text": rt,
            "thread_ts": message.get("thread_ts")})
    providers = st.get("providers") or []
    if action_id == "model_picker_cancel":
        ctx.call("picker_state_pop", {"marker": marker})
        return _mp_update(ctx, channel_id, team_v, msg_ts, t("platform.slack.picker.cancelled"))
    if action_id == "model_picker_provider":
        tok = S((action.get("selected_option") or {}).get("value"))
        idx, iok = parse_int(tok)
        provider = None
        if iok and idx >= 0 and isinstance(providers, list) and idx < len(providers):
            provider = providers[idx]
        if not T(provider):
            r = ctx.call("logger.warning", {"token": tok})
            if is_raise(r):
                return r
            ctx.call("picker_state_pop", {"marker": marker})
            notice = ctx.call("model_picker_expired_notice", {})
            if is_raise(notice):
                return notice
            return _mp_update(ctx, channel_id, team_v, msg_ts, notice)
        slug = S(provider.get("slug"))
        if not T(provider.get("models")):
            r = _mp_update(ctx, channel_id, team_v, msg_ts,
                           t("platform.slack.picker.no_models_for_provider"))
            ctx.call("picker_state_pop", {"marker": marker})
            return r
        ctx.call("picker_state_set", {"key": "stage", "value": "model"})
        ctx.call("picker_state_set", {"key": "selected_provider_slug", "value": slug})
        blocks = ctx.call("build_model_picker_model_blocks", {
            "providers": providers, "provider_slug": slug})
        if is_raise(blocks):
            return blocks
        er = _mp_chat_update(ctx, blocks, channel_id, msg_ts,
                             t("platform.slack.picker.fallback_model"), team_v)
        return er if is_raise(er) else None
    if action_id == "model_picker_back":
        ctx.call("picker_state_set", {"key": "stage", "value": "provider"})
        ctx.call("picker_state_set", {"key": "selected_provider_slug", "value": ""})
        try:
            label = ctx.call("get_label", {"provider": st.get("current_provider")})
        except BaseException as exc:
            if base_only(spec_of(exc)):
                return spec_of(exc)
            label = st.get("current_provider")
        blocks = ctx.call("build_model_picker_provider_blocks", {
            "providers": providers, "current_model": st.get("current_model"),
            "provider_label": label})
        if is_raise(blocks):
            return blocks
        er = _mp_chat_update(ctx, blocks, channel_id, msg_ts,
                             t("platform.slack.picker.fallback_provider"), team_v)
        return er if is_raise(er) else None
    if action_id == "model_picker_model" and S(st.get("stage")) == "model":
        tok = S((action.get("selected_option") or {}).get("value"))
        slug = S(st.get("selected_provider_slug"))
        found = None
        for p in (providers if isinstance(providers, list) else []):
            if S(p.get("slug")) == slug:
                found = p
                break
        models = found.get("models") if isinstance(found, dict) else None
        idx, iok = parse_int(tok)
        model_id = None
        if iok and idx >= 0 and isinstance(models, list) and idx < len(models):
            model_id = models[idx]
        if not T(model_id):
            r = ctx.call("logger.warning", {"token": tok})
            if is_raise(r):
                return r
            ctx.call("picker_state_pop", {"marker": marker})
            notice = ctx.call("model_picker_expired_notice", {})
            if is_raise(notice):
                return notice
            return _mp_update(ctx, channel_id, team_v, msg_ts, notice)
        if not T(onms):
            ctx.call("picker_state_pop", {"marker": marker})
            notice = ctx.call("model_picker_expired_notice", {})
            if is_raise(notice):
                return notice
            return _mp_update(ctx, channel_id, team_v, msg_ts, notice)
        ctx.call("picker_state_pop", {"marker": marker})
        r = _mp_update(ctx, channel_id, team_v, msg_ts, t("platform.slack.picker.switching"))
        if is_raise(r):
            return r
        switch_failed = False
        try:
            confirmation = ctx.call("on_model_selected", {
                "chat_id": st.get("chat_id"), "model_id": model_id, "provider_slug": slug})
            try:
                pf = ctx.call("t_error_prefix", {})
                prefix = S(pf) or "Error:"
            except BaseException as exc:
                if base_only(spec_of(exc)):
                    return spec_of(exc)
                prefix = "Error:"
            if prefix and pystr(confirmation).startswith(prefix):
                switch_failed = True
        except BaseException as exc:
            if base_only(spec_of(exc)):
                return spec_of(exc)
            ctx.call("logger.error", {"error": exc_msg(exc)})
            confirmation = t("platform.slack.picker.switch_failed")
            switch_failed = True
        header = t("platform.slack.picker.switch_failed_header" if switch_failed
                   else "platform.slack.picker.switched_header")
        return _mp_update(ctx, channel_id, team_v, msg_ts,
                          header + "\n\n" + pystr(confirmation))
    return None


def m_handle_slash_command(ctx, a, state):
    command = a.get("command") or {}
    user_id = S(command.get("user_id"))
    channel_id = S(command.get("channel_id"))
    team_id = S(command.get("team_id"))
    if team_id and channel_id:
        ctx.call("remember_channel_team", {"channel_id": channel_id, "team_id": team_id})
    text = ctx.call("slash_command_text", {"command": command})
    if is_raise(text):
        return text
    txt = S(text)
    thread_id = ctx.call("slash_thread_id", {"command": command})
    if is_raise(thread_id):
        return thread_id
    g = ctx.call("slash_channel_gated", {"channel_id": channel_id})
    if is_raise(g):
        return g
    if T(g):
        r = ctx.call("logger.debug", {"channel_id": channel_id})
        return r if is_raise(r) else None
    is_dm = channel_id.startswith("D")
    if is_dm:
        d = ctx.call("slack_disable_dms", {})
        if is_raise(d):
            return d
        if T(d):
            r = ctx.call("logger.info", {"channel_id": channel_id, "user_id": user_id})
            return r if is_raise(r) else None
    rejected = ctx.call("early_reject_unauthorized", {
        "user_id": user_id, "channel_id": channel_id, "is_dm": is_dm, "thread_id": thread_id})
    if is_raise(rejected):
        return None
    rej = T(rejected)
    if rej and not is_dm:
        return None
    source = ctx.call("build_source", {
        "chat_id": channel_id, "chat_type": "dm" if is_dm else "group",
        "user_id": user_id, "thread_id": thread_id,
        "scope_id": command.get("team_id") if team_id else None})
    if is_raise(source):
        return None
    prompt = ctx.call("channel_prompt_with_identity", {"channel_id": channel_id, "team_id": team_id})
    if is_raise(prompt):
        return None
    askill = ctx.call("resolve_channel_skills", {"extra": a.get("extra"), "channel_id": channel_id})
    if is_raise(askill):
        return None
    ev = {"text": txt, "message_type": "COMMAND" if txt.startswith("/") else "TEXT",
          "source": source, "raw_message": command, "channel_prompt": prompt, "auto_skill": askill}
    ec = ctx.call("event_command", ev)
    if is_raise(ec):
        return None
    cmd = ctx.call("resolve_command", {"command": ec})
    if is_raise(cmd):
        return None
    controls = isinstance(cmd, dict) and (
        S(cmd.get("busy_policy")) == "interrupt_then_dispatch" or
        S(cmd.get("name")) in ("approve", "deny", "pause"))
    lane = ahead = handoff = None
    if not rej and not controls:
        lane = ctx.call("event_session_key", {"event": ev})
        if is_raise(lane):
            return None
        res = ctx.call("slash_lanes_reserve", {"lane": lane})
        if is_raise(res):
            return None
        ahead = res[0] if isinstance(res, list) and len(res) > 0 else None
        handoff = res[1] if isinstance(res, list) and len(res) > 1 else None
    st = ctx.call("slash_user_id_set", {"user_id": user_id})
    if is_raise(st):
        return None
    exc = None
    try:
        if lane is not None:
            cn = ctx.call("resolve_channel_name", {"channel_id": channel_id, "team_id": team_id})
            if is_raise(cn):
                exc = cn
                raise _Stop()
            un = ctx.call("resolve_user_name", {"user_id": user_id, "chat_id": channel_id,
                                                "team_id": team_id})
            if is_raise(un):
                exc = un
                raise _Stop()
            r = ctx.call("source_set_fields", {"chat_name": S(cn), "user_name": S(un)})
            if is_raise(r):
                exc = r
                raise _Stop()
            if ahead is not None:
                r = ctx.call("slash_lanes_wait", {"ahead": ahead})
                if is_raise(r):
                    exc = r
                    raise _Stop()
        url = S(command.get("response_url"))
        if url and user_id and channel_id and txt.startswith("/"):
            r = ctx.call("stash_slash_context", {"team_id": team_id, "channel_id": channel_id,
                                                 "user_id": user_id, "response_url": url})
            if is_raise(r):
                exc = r
                raise _Stop()
        r = ctx.call("handle_message", {"event": ev})
        if is_raise(r):
            exc = r
            raise _Stop()
    except _Stop:
        pass
    finally:
        ctx.call("slash_user_id_reset", {"token": ""})
        ctx.call("slash_lanes_release", {"lane": lane, "ahead": ahead, "handed_off": handoff})
    return exc if exc is not None else None


class _Stop(Exception):
    pass


def m_handle_slack_message(ctx, a, state):
    event = a.get("event") or {}
    ts = strval(event.get("ts"))
    was_claimed = bool(ts) and shas(state, "_processed_message_ts", ts)
    ret = ctx.call("_handle_slack_message_impl", {"event": event, "payload": a.get("payload")})
    if is_raise(ret) and ts and not was_claimed and shas(state, "_processed_message_ts", ts):
        sdel(state, "_processed_message_ts", ts)
        ctx.call("logger.warning", {"name": S(a.get("name")), "ts": ts})
    return ret


def m_handle_slack_message_impl(ctx, a, state):
    acc = ctx.call("prefilter_inbound", {"event": a.get("event"), "payload": a.get("payload")})
    if is_raise(acc):
        return acc
    if not isinstance(acc, list) or len(acc) < 3:
        return None
    event = canon(acc[0])
    team_v = acc[1]
    channel_id = S(acc[2])
    orig = strval(event.get("text"))
    lstr = orig.lstrip(" ")
    probe = ctx.call("rewrite_known_bang_command", {"text": lstr})
    if is_raise(probe):
        return probe
    ps = S(probe)
    if ps != lstr:
        orig = ps
    is_command = ps.startswith("/")
    text = orig
    blocks = event.get("blocks")
    if blocks and T(blocks) and not is_command:
        text = ctx.call("append_block_text", {"text": orig, "blocks": blocks})
        if is_raise(text):
            return text
    text = ctx.call("append_forwards", {"text": text, "attachments": event.get("attachments")})
    if is_raise(text):
        return text
    text = ctx.call("append_link_unfurls", {"text": text, "attachments": event.get("attachments")})
    if is_raise(text):
        return text
    ts = S(event.get("ts"))
    meta = ctx.call("lookup_assistant_thread_metadata", {
        "event": event, "channel_id": channel_id, "thread_ts": event.get("thread_ts"),
        "team_id": team_v, "body": a.get("payload")})
    if is_raise(meta):
        return meta
    uid_v = event.get("user")
    if not T(uid_v):
        uid_v = meta.get("user_id")
    user_id = S(uid_v)
    if not channel_id:
        channel_id = S(meta.get("channel_id"))
    team_final = team_v
    if not T(team_final):
        r = ctx.call("team_in_install_set", {"team": meta.get("team_id")})
        if is_raise(r):
            return r
        if T(r):
            team_final = r
        else:
            cv = sget(state, "_channel_team", channel_id)
            team_final = ctx.call("team_in_install_set", {"team": cv})
            if is_raise(team_final):
                return team_final
    agent_ctx = ctx.call("agent_view_context_for_event", {
        "event": event, "team": S(team_final), "user": user_id})
    if is_raise(agent_ctx):
        return agent_ctx
    if T(team_final) and channel_id:
        ctx.call("remember_channel_team", {"channel_id": channel_id, "team_id": team_final})
    ctype = S(event.get("channel_type"))
    if not ctype:
        ctype = "im" if channel_id.startswith("D") else ""
    is_dm = ctype in ("im", "mpim")
    if is_dm:
        d = ctx.call("slack_disable_dms", {})
        if is_raise(d):
            return d
        if T(d):
            return None
    one_to_one = ctype == "im"
    thread_ts = ctx.call("session_thread_ts", {
        "event": event, "ts": ts, "is_dm": is_dm, "assistant_meta": meta})
    if is_raise(thread_ts):
        return thread_ts
    tts = S(thread_ts)
    decl = ctx.call("event_declares_bot_sender", {"event": event})
    if is_raise(decl):
        return decl
    sender_bot = T(decl)
    if not sender_bot and user_id and not T(event.get("client_msg_id")):
        r = ctx.call("resolve_user_is_bot", {"user": user_id, "chat_id": channel_id,
                                             "team_id": team_final})
        if is_raise(r):
            return r
        sender_bot = T(r)
    rej = ctx.call("early_reject_unauthorized", {
        "user_id": user_id, "channel_id": channel_id, "is_dm": is_dm,
        "thread_id": thread_ts, "is_bot": sender_bot})
    if is_raise(rej):
        return rej
    if T(rej):
        return None
    bot = ctx.call("team_bot_user_id", {"team_id": team_final})
    if is_raise(bot):
        return bot
    bot_uid = S(bot)
    det = ctx.call("slack_mention_detection_text", {"event": event})
    if is_raise(det):
        return det
    routing = S(det) or orig or ""
    is_mentioned = T(ctx.call("is_mentioned", {"routing_text": routing, "bot_uid": bot_uid}))
    etts = event.get("thread_ts")
    is_thread_reply = bool(T(etts) and S(etts) != ts)
    force = T(event.get("_hermes_force_process"))
    drop = ctx.call("peer_bot_drop", {
        "event": event, "user_id": user_id, "bot_uid": bot_uid, "channel_id": channel_id,
        "team_id": team_final, "is_mentioned": is_mentioned})
    if is_raise(drop):
        return drop
    if T(drop):
        return None
    if not one_to_one and bot_uid:
        gate = ctx.call("channel_gate_allows", {
            "channel_id": channel_id, "routing_text": routing, "bot_uid": bot_uid,
            "is_mentioned": is_mentioned, "is_thread_reply": is_thread_reply,
            "event_thread_ts": event.get("thread_ts"), "user_id": user_id,
            "team_id": team_final, "is_dm": is_dm, "force_process": force})
        if is_raise(gate):
            return gate
        if not T(gate):
            return None
    if ts:
        sset(state, "_processed_message_ts", ts, True)
    if is_mentioned:
        ar = ctx.call("apply_bot_mention", {
            "text": text, "original_text": orig, "command_probe_text": ps,
            "is_command_text": is_command, "bot_uid": "", "thread_ts": tts,
            "team_id": team_final})
        if is_raise(ar):
            return ar
        if isinstance(ar, list) and len(ar) >= 4:
            text = ar[0]
            orig = strval(ar[1])
    hctx = ctx.call("hydrate_thread_context", {
        "channel_id": channel_id, "event_thread_ts": event.get("thread_ts"), "ts": ts,
        "user_id": user_id, "team_id": team_final, "is_thread_reply": is_thread_reply,
        "is_mentioned": is_mentioned, "is_dm": is_dm})
    if is_raise(hctx):
        return hctx
    root_urls = hctx[1] if isinstance(hctx, list) and len(hctx) > 1 else None
    root_types = hctx[2] if isinstance(hctx, list) and len(hctx) > 2 else None
    cm = ctx.call("collect_inbound_media", {
        "event": event, "channel_id": channel_id, "team_id": team_final, "text": text,
        "root_urls": root_urls, "root_types": root_types})
    if is_raise(cm):
        return cm
    rexp = ctx.call("slack_reply_expected", {
        "routing_text": routing, "bot_uid": "", "channel_id": channel_id,
        "opens_own_session": tts == ts, "channel_thread_followup": is_thread_reply and not is_dm,
        "addressed": one_to_one or is_mentioned or is_command or force})
    if is_raise(rexp):
        return rexp
    msg_event = ctx.call("build_message_event", {
        "event": event, "text": text, "original_text": orig, "is_command_text": is_command,
        "channel_id": channel_id, "team_id": team_final, "ts": ts, "user_id": user_id,
        "thread_ts": tts, "is_dm": is_dm, "reply_expected": rexp})
    if is_raise(msg_event):
        return msg_event
    if one_to_one or is_mentioned:
        re_ = ctx.call("reactions_enabled", {})
        if is_raise(re_):
            return re_
        if T(re_):
            r = ctx.call("track_reacting_message", {"team_id": team_final, "ts": ts})
            if is_raise(r):
                return r
    cxid = S(agent_ctx.get("context_channel_id"))
    if cxid and cxid != channel_id and not is_command:
        full = "[Slack app context: user is viewing channel " + cxid + "]\n\n" + S(msg_event.get("text"))
        r = ctx.call("msg_event_set_text", {"text": full})
        if is_raise(r):
            return r
    if ts:
        sset(state, "_processed_message_ts", ts, True)
    r = ctx.call("handle_message", {"msg_event": msg_event})
    if is_raise(r):
        return r
    return None


def m_handle_slack_file_shared(ctx, a, state):
    event = a.get("event") or {}
    cv = event.get("channel_id")
    channel_id = S(cv) if T(cv) else S(event.get("channel"))
    ign = ctx.call("is_ignored_channel", {"channel_id": channel_id})
    if is_raise(ign):
        return ign
    if T(ign):
        r = ctx.call("logger.info", {"channel_id": channel_id})
        return r if is_raise(r) else None
    fv = event.get("file_id")
    file_id = S(fv) if T(fv) else S((event.get("file") or {}).get("id"))
    if not channel_id or not file_id:
        return None
    team_v = ctx.call("event_team_id", {"event": event, "body": a.get("body"),
                                       "install_set": a.get("install_set")})
    if is_raise(team_v):
        return team_v
    info = ctx.call("files_info", {"file": file_id, "team_id": team_v})
    if is_raise(info):
        if base_only(info):
            return info
        det = ctx.call("describe_slack_api_error", {"response": None,
                                                    "file_obj": {"id": file_id}})
        if is_raise(det):
            return det
        r = ctx.call("logger.warning", {"file_id": file_id, "detail": det})
        return r if is_raise(r) else None
    if not T(info.get("ok")):
        det = ctx.call("describe_slack_api_error", {
            "response": info, "file_obj": {"id": file_id}})
        if is_raise(det):
            return det
        r = ctx.call("logger.warning", {"file_id": file_id, "detail": det,
                                        "error": info.get("error")})
        return r if is_raise(r) else None
    file_obj = info.get("file")
    if not isinstance(file_obj, dict):
        file_obj = None
    mime = S(file_obj.get("mimetype")) if file_obj else ""
    if not mime.startswith("video/"):
        return None
    is_dm = channel_id.startswith("D") or S(event.get("channel_type")) == "im"
    if is_dm:
        sh = event.get("user_id") if T(event.get("user_id")) else (file_obj.get("user") if file_obj else None)
        sharer = S(sh)
        sender_team = S(event.get("user_team"))
        if not sender_team.startswith("T"):
            st_ = ctx.call("resolve_author_team", {"sharer": sharer, "channel_id": channel_id,
                                                   "team_id": team_v})
            if is_raise(st_):
                if not base_only(st_):
                    return st_
            else:
                sender_team = S(st_)
        if not sender_team:
            a1 = S(event.get("team"))
            b1 = S(file_obj.get("user_team")) if file_obj else ""
            sender_team = a1 if a1.startswith("T") else (b1 if b1.startswith("T") else "")
        foreign = ctx.call("is_foreign_author", {"sender_team": sender_team, "team_id": team_v})
        if is_raise(foreign):
            return foreign
        if T(foreign):
            r = ctx.call("logger.info", {"sharer": sharer, "sender_team": sender_team,
                                         "channel_id": channel_id})
            return r if is_raise(r) else None
    share = ctx.call("first_file_share", {"file_obj": file_obj if file_obj else None,
                                          "channel_id": channel_id})
    if is_raise(share):
        return share
    ts = S(share.get("ts")) or S(event.get("event_ts"))
    thread_ts = S(share.get("thread_ts"))
    sr = ctx.call("sleep", {"seconds": 0.75})
    if is_raise(sr):
        return sr
    if ts:
        wid = ctx.call("workspace_event_id", {"team_id": S(team_v), "ts": ts})
        if is_raise(wid):
            return wid
        dup = ctx.call("dedup_is_duplicate", {"id": wid})
        if is_raise(dup):
            return dup
        if T(dup):
            return None
    fe = {"type": "message", "subtype": "file_share", "text": "",
          "user": (event.get("user_id") if T(event.get("user_id")) else
                   (file_obj.get("user") if file_obj else None)),
          "channel": channel_id,
          "channel_type": "im" if channel_id.startswith("D") else "channel",
          "team": team_v, "ts": "", "files": [file_obj] if file_obj else []}
    if thread_ts and thread_ts != ts:
        fe["thread_ts"] = thread_ts
    r = ctx.call("handle_slack_message", {"event": fe})
    if is_raise(r):
        return None
    return None


def m_handle_slack_reaction(ctx, a, state):
    event = a.get("event") or {}
    item = event.get("item") or {}
    if S(item.get("type")) != "message":
        return None
    cid, mts = item.get("channel"), item.get("ts")
    uid, rv = event.get("user"), event.get("reaction")
    if not (T(cid) and T(mts) and T(uid) and T(rv)):
        return None
    channel_id, msg_ts = S(cid), S(mts)
    reaction, user_id = S(rv), S(uid)
    bot_uid = S(a.get("bot_user_id"))
    if bot_uid and user_id == bot_uid:
        return None
    team_v = ctx.call("channel_team", {"channel_id": cid})
    if is_raise(team_v):
        return team_v
    if not T(team_v):
        tc = ctx.call("team_clients", {})
        if is_raise(tc):
            return tc
        if T(tc):
            team_v = ctx.call("team_clients_first", {})
            if is_raise(team_v):
                return team_v
    team_id = S(team_v)
    client = ctx.call("team_clients_get", {"team_id": team_v})
    if is_raise(client):
        return client
    action = "removed" if a.get("removed", False) else "added"
    handler = ctx.call("reaction_handler_get", {})
    if is_raise(handler):
        return handler
    if handler is not None:
        hd = {"platform": "slack", "event_name": "reaction:" + action,
              "reaction": reaction, "user_id": user_id,
              "item_user_id": event.get("item_user"), "item_type": item.get("type"),
              "channel_id": channel_id, "message_ts": msg_ts, "team_id": team_id,
              "event_ts": event.get("event_ts"), "raw_event": event}
        hr = ctx.call("reaction_handler_call", hd)
        if is_raise(hr):
            if base_only(hr):
                return hr
            ctx.call("logger.debug", {})
    trig = ctx.call("slack_reaction_triggers", {})
    if is_raise(trig):
        return trig
    if trig is None:
        return None
    explicit = T(trig)
    if explicit:
        rstrip = reaction.strip(":")
        if not collection_has_str(trig, rstrip):
            return None
    thread_ts = ctx.call("reaction_thread_ts", {
        "client": client, "channel_id": channel_id, "msg_ts": msg_ts,
        "event": event, "team_id": team_v, "explicit_allowlist": explicit})
    if is_raise(thread_ts):
        return thread_ts
    if thread_ts is None:
        return None
    syn = ctx.call("synthetic_reaction_event", {
        "event": event, "action": action, "thread_ts": thread_ts, "team_id": team_v})
    if is_raise(syn):
        return syn
    if channel_id.startswith("D") or S(syn.get("channel")).startswith("D"):
        rt = ctx.call("resolve_author_team", {
            "user_id": user_id, "channel_id": channel_id, "team_id": team_v})
        if is_raise(rt):
            return rt
        if T(rt):
            ctx.call("synthetic_set", {"user_team": rt})
    r = ctx.call("handle_slack_message", {"event": syn})
    if is_raise(r):
        return None
    return None


MODELS = {
    "_handle_approval_action": m_handle_approval_action,
    "_handle_assistant_thread_lifecycle_event": m_handle_assistant_thread_lifecycle_event,
    "_handle_clarify_action": m_handle_clarify_action,
    "_handle_feedback_action": m_handle_feedback_action,
    "_handle_hermes_command": m_handle_hermes_command,
    "_handle_model_picker_action": m_handle_model_picker_action,
    "_handle_slack_file_shared": m_handle_slack_file_shared,
    "_handle_slack_message": m_handle_slack_message,
    "_handle_slack_message_impl": m_handle_slack_message_impl,
    "_handle_slack_reaction": m_handle_slack_reaction,
    "_handle_slash_command": m_handle_slash_command,
    "_handle_slash_confirm_action": m_handle_slash_confirm_action,
    "_has_active_session_for_thread": m_has_active_session_for_thread,
    "_hint_allow_bots": m_hint_allow_bots,
    "_slack_reaction_trigger_target": m_slack_reaction_trigger_target,
    "_socket_ping_pong_stale": m_socket_ping_pong_stale,
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
    csrc = open(os.path.join(_REPO, "src", "gateway", "platforms", "slack_adapter_flow10.c"),
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow10: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if '"' + n + '"' not in csrc]
    if missing_c:
        print("FATAL oracle_slack_adapter_flow10: seams absent from C port: " + str(missing_c))
        sys.exit(2)
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow10.jsonl")
    if os.path.exists(fix):
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print("FATAL oracle_slack_adapter_flow10: fixture io keys != SEAM_NAMES; "
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
    print("ORACLE slack_adapter_flow10: " + str(n) + " cases, " + str(m) + " mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow10.jsonl")
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
    print("SELFTEST slack_adapter_flow10: " + str(n) + " cases, " + str(bad) + " drive/script failures")
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))
    return 1 if bad or n == 0 else 0


def dump():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow10.jsonl")
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

