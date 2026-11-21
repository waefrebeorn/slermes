#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 11.

Independent Python reference MODEL of the 16 LIVE
plugins/platforms/slack/adapter.py remainders ported by the C file. Each model
is a faithful transcription of the live function's DECISION logic with every
injected/unported callable replaced by a recording stub fed the SAME io
scripts the C harness feeds sa11_call. No quarry import chain is imported.

Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and echoed state ("state"). Summary:
'ORACLE slack_adapter_flow12: N cases, M mismatches'.

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
    "app_action",
    "app_command",
    "app_event",
    "chat_postEphemeral",
    "chat_postMessage",
    "conversations_info",
    "conversations_replies",
    "drop_bot_sender",
    "event_team_id",
    "extract_additional_text_from_slack_blocks",
    "extract_text_from_slack_attachments",
    "extract_urls_from_slack_blocks",
    "get_client",
    "get_scoped_secret",
    "get_slack_action_handlers",
    "has_app",
    "is_duplicate",
    "is_enabled_debug",
    "is_foreign_author",
    "is_ignored_channel",
    "logger.debug",
    "logger.info",
    "logger.warning",
    "metadata_team_id",
    "normalize_changed_message",
    "read_chunk",
    "re_compile",
    "re_search",
    "re_split",
    "reactions_add",
    "reactions_remove",
    "refusal_text",
    "register_plugin_action_handlers",
    "register_vault_code_handlers",
    "release_response",
    "resolve_thread_ts",
    "resolve_user_name",
    "response_text",
    "sanitize_blocks",
    "slack_file_marker",
    "slack_native_slashes",
    "slack_response_payload",
    "time.time",
    "trim_bot_message_timestamps",
    "trim_oldest_dict_entries",
    "unescape_slack_entities",
    "users_info",
    "wire_plugin_handlers",
    "workspace_event_id",
    "workspace_message_marker",
]

TARGETS = [
    "_post_interactive_blocks",
    "_prefilter_inbound",
    "_react",
    "_reaction_thread_ts",
    "_read_error_text_limited",
    "_record_uploaded_file_thread",
    "_refuse_other_requester",
    "_register_bolt_handlers",
    "_register_plugin_action_handlers",
    "_remember_processed_message_ts",
    "_remove_reaction",
    "_render_message_text",
    "_resolve_author_team",
    "_resolve_channel_name",
    "_resolve_download_token",
    "_slack_reaction_triggers",
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
    """Mirror sa12_truthy."""
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


# ── models (faithful transcripts of the live decision logic) ────────────────
def m_read_error_text_limited(ctx, a, state):
    resp = a.get("response") if isinstance(a.get("response"), dict) else {}
    limit = int(num(a.get("limit"), 8192))
    content = resp.get("content")
    read = content.get("read") if isinstance(content, dict) else None
    if T(read):
        chunks = []
        total = 0
        while total <= limit:
            size = min(4096, limit + 1 - total)
            chunk = ctx.call("read_chunk", {"size": size})
            if not T(chunk):
                break
            data = strval(chunk)
            chunks.append(data)
            total += len(data)
        if total > limit and T(resp.get("release")):
            ctx.call("release_response", {})
        return "".join(chunks)[:limit]
    return strval(ctx.call("response_text", {}))[:limit]


def m_react(ctx, a, state, remove=False):
    if not T(ctx.call("has_app", {})):
        return False
    client = ctx.call("get_client", {"channel": a.get("channel"),
                                     "team_id": a.get("team_id") or None})
    try:
        if remove:
            ctx.call("reactions_remove", {"channel": a.get("channel"),
                     "timestamp": a.get("timestamp"), "name": a.get("emoji"),
                     "client": canon(client)})
        else:
            ctx.call("reactions_add", {"channel": a.get("channel"),
                     "timestamp": a.get("timestamp"), "name": a.get("emoji"),
                     "client": canon(client)})
        return True
    except BaseException as exc:
        sp = spec_of(exc)
        if base_only(sp):
            raise
        ctx.call("logger.debug", {
            "msg": "[Slack] reactions.%s failed (%s): %s",
            "args": ["remove" if remove else "add", a.get("emoji"), sp.get("message", "")]})
        return False


def m_remove_reaction(ctx, a, state):
    return m_react(ctx, a, state, remove=True)


def m_record_uploaded_file_thread(ctx, a, state):
    if not T(a.get("thread_ts")):
        return None
    team = ctx.call("metadata_team_id", {"metadata": canon(a.get("metadata"))})
    marker = ctx.call("workspace_message_marker", {"team_id": team, "ts": a.get("thread_ts")})
    set_add(state, "_bot_message_ts", marker)
    ctx.call("trim_bot_message_timestamps", {"items": canon(state.get("_bot_message_ts", []))})
    return None


def m_remember_processed_message_ts(ctx, a, state):
    ts = a.get("ts")
    if not T(ts):
        return None
    now = ctx.call("time.time", {})
    m = state.get("_processed_message_ts")
    if not isinstance(m, dict):
        m = {}
        state["_processed_message_ts"] = m
    m[ts] = canon(now)
    mx = int(num(a.get("max_size"), 5000))
    if len(m) > mx:
        items = sorted(m.items(), key=lambda item: num(item[1]))
        state["_processed_message_ts"] = dict(items[-mx:])
    return None


def m_refuse_other_requester(ctx, a, state):
    req = a.get("requester")
    if not T(req):
        return False
    body = a.get("body") if isinstance(a.get("body"), dict) else {}
    ch = (body.get("channel") or {}).get("id", "")
    uid = (body.get("user") or {}).get("id", "")
    team = ctx.call("event_team_id", {"event": {}, "body": canon(body)})
    client = ctx.call("get_client", {"channel_id": ch, "team_id": team or None})
    text = ctx.call("refusal_text", {"requester": req})
    ctx.call("chat_postEphemeral", {"channel": ch, "user": uid, "text": text,
             "thread_ts": (body.get("message") or {}).get("thread_ts"), "client": canon(client)})
    return True


def m_post_interactive_blocks(ctx, a, state):
    san = a.get("sanitize", True)
    san = san if isinstance(san, bool) else True
    tsc = a.get("team_scoped", True)
    tsc = tsc if isinstance(tsc, bool) else True
    kwargs = {"channel": a.get("chat_id"), "text": a.get("text")}
    if san:
        kwargs["blocks"] = ctx.call("sanitize_blocks", {"blocks": canon(a.get("blocks"))})
    else:
        kwargs["blocks"] = canon(a.get("blocks"))
    ts = ctx.call("resolve_thread_ts", {"event": None, "metadata": canon(a.get("metadata"))})
    if T(ts):
        kwargs["thread_ts"] = ts
    if tsc:
        team = ctx.call("metadata_team_id", {"metadata": canon(a.get("metadata"))})
    else:
        team = None
    client = ctx.call("get_client", {"chat_id": a.get("chat_id"), "team_id": team})
    kwargs["client"] = canon(client)
    return ctx.call("chat_postMessage", kwargs)


def m_render_message_text(ctx, a, state):
    msg = a.get("msg") if isinstance(a.get("msg"), dict) else {}
    bot_uid = S(a.get("bot_uid"))
    mt = strip(msg.get("text") or "")
    if bot_uid:
        mt = strip(replace(mt, "<@" + bot_uid + ">", ""))
    blocks = msg.get("blocks")
    extras = []

    def unseen(piece, base):
        return piece not in base and all(piece not in e for e in extras)

    if T(blocks):
        rt = strip(ctx.call("extract_additional_text_from_slack_blocks",
                            {"blocks": canon(blocks), "msg_text": mt, "bot_uid": bot_uid}))
        if rt:
            extras.append(rt)
        for blk in blocks:
            if not isinstance(blk, dict):
                continue
            if S(blk.get("type")) not in ("section", "header", "context"):
                continue
            to = blk.get("text") or {}
            if not isinstance(to, dict):
                continue
            st = strip(to.get("text") or "")
            if st and unseen(st, mt):
                extras.append(st)
    attachments = msg.get("attachments") or []
    at = strip(ctx.call("extract_text_from_slack_attachments",
                        {"attachments": canon(attachments), "msg_text": mt}))
    if at and unseen(at, mt):
        extras.append(at)
    if T(blocks):
        raw = ctx.call("unescape_slack_entities", {"text": mt})
        urls = ctx.call("extract_urls_from_slack_blocks", {"blocks": canon(blocks)})
        new_urls = [strval(u) for u in (urls if isinstance(urls, list) else [])
                    if unseen(strval(u), strval(raw))]
        if new_urls:
            extras.append("URLs: " + ", ".join(new_urls))
    files = msg.get("files") if isinstance(msg.get("files"), list) else []
    markers = [strval(ctx.call("slack_file_marker", {"file": canon(f)}))
               for f in files if isinstance(f, dict)]
    if markers:
        extras.append(" ".join(markers))
    if extras:
        addendum = "\n".join(extras)
        mt = (mt + "\n" + addendum).strip() if mt else addendum
    return mt


def m_resolve_author_team(ctx, a, state):
    uid = S(a.get("user_id"))
    if not uid:
        return ""
    cache = state.get("_user_team_cache")
    if not isinstance(cache, dict):
        cache = {}
        state["_user_team_cache"] = cache
    key = S(a.get("team_id")) + "|" + uid
    if key in cache:
        return cache[key]
    tc = state.get("_team_clients")
    tid = S(a.get("team_id"))
    client = tc.get(tid) if (T(tid) and isinstance(tc, dict)) else None

    def fetch():
        c = canon(client) if T(client) else ctx.call("get_client", {"channel_id": a.get("channel_id")})
        info = ctx.call("users_info", {"user": uid, "client": c})
        return ctx.call("slack_response_payload", {"response": canon(info)})

    try:
        payload = fetch()
    except BaseException as exc:
        sp = spec_of(exc)
        if base_only(sp):
            raise
        ctx.call("logger.debug", {"label": "users.info team lookup", "user_id": uid,
                                  "error": sp.get("message", "")})
        return ""
    user = payload.get("user") if isinstance(payload, dict) else None
    ut = (user or {}).get("team_id") if isinstance(user, dict) else None
    if not (isinstance(ut, str) and ut.startswith("T")):
        return ""
    cache[key] = ut
    ctx.call("trim_oldest_dict_entries", {"items": canon(cache), "max_size": 5000})
    return ut


def m_reaction_thread_ts(ctx, a, state):
    tts = S(a.get("msg_ts"))
    event = a.get("event") if isinstance(a.get("event"), dict) else {}
    item_user = S(event.get("item_user"))
    client = a.get("client")
    if client is not None:
        try:
            hist = ctx.call("conversations_replies", {
                "channel": a.get("channel_id"), "ts": a.get("msg_ts"),
                "limit": 1, "inclusive": True, "client": canon(client)})
            msgs = (hist or {}).get("messages") or [] if isinstance(hist, dict) else []
            if msgs:
                first = msgs[0] if isinstance(msgs[0], dict) else {}
                tts = S(first.get("thread_ts")) or S(first.get("ts")) or tts
                item_user = item_user or S(first.get("user"))
            else:
                return tts
        except BaseException as exc:
            sp = spec_of(exc)
            if base_only(sp):
                raise
            ctx.call("logger.debug", {"label": "reaction thread_ts lookup",
                     "msg_ts": a.get("msg_ts"), "error": sp.get("message", "")})
            return tts
    allow = a.get("explicit_allowlist")
    allow = allow if isinstance(allow, bool) else False
    if not allow:
        tb = state.get("_team_bot_user_ids")
        bot = (tb.get(S(a.get("team_id"))) if isinstance(tb, dict) else None) or state.get("_bot_user_id")
        if item_user and bot and item_user != bot:
            return None
    return tts


def m_resolve_channel_name(ctx, a, state):
    cid = S(a.get("channel_id"))
    if not cid:
        return cid
    ct = state.get("_channel_team")
    tid = S(a.get("team_id")) or (ct.get(cid) if isinstance(ct, dict) else "") or ""
    tid = S(tid)
    cache = state.get("_channel_name_cache")
    if not isinstance(cache, dict):
        cache = {}
        state["_channel_name_cache"] = cache
    key = tid + "|" + cid
    if cache.get(key) is not None:
        return cache[key]
    if not T(ctx.call("has_app", {})):
        return cid

    def fetch():
        client = ctx.call("get_client", {"channel_id": cid, "team_id": tid or None})
        resp = ctx.call("conversations_info", {"channel": cid, "client": canon(client)})
        payload = ctx.call("slack_response_payload", {"response": canon(resp)})
        ch = (payload.get("channel") or {}) if isinstance(payload, dict) else {}
        if not isinstance(ch, dict):
            ch = {}
        if not (isinstance(payload, dict) and T(payload.get("ok"))):
            return cid
        if T(ch.get("is_im")):
            peer = S(ch.get("user"))
            if peer:
                return ctx.call("resolve_user_name", {"user": peer, "chat_id": cid, "team_id": tid})
            return cid
        return S(ch.get("name")) or S(ch.get("name_normalized")) or cid

    try:
        name = fetch()
    except BaseException as exc:
        sp = spec_of(exc)
        if base_only(sp):
            raise
        ctx.call("logger.debug", {"label": "conversations.info", "channel_id": cid,
                                  "error": sp.get("message", "")})
        name = cid
    cache[key] = canon(name)
    ctx.call("trim_oldest_dict_entries", {"items": canon(cache), "max_size": 5000})
    return name


def m_prefilter_inbound(ctx, a, state):
    event = canon(a.get("event") if isinstance(a.get("event"), dict) else {})
    payload = canon(a.get("payload"))
    if T(ctx.call("is_enabled_debug", {})):
        bp = event.get("bot_profile") or {}
        ctx.call("logger.debug", {"msg": "[Slack] event received", "event": event,
                 "bot_name": S(bp.get("name")) if isinstance(bp, dict) else ""})
    if S(event.get("subtype")) == "message_changed":
        event = ctx.call("normalize_changed_message", {"event": event})
        if event is None:
            return None
        event = canon(event)
    ets = S(event.get("_slack_changed_event_ts")) or S(event.get("ts"))
    tc = state.get("_team_clients")
    has_tc = isinstance(tc, dict) and len(tc) > 0
    tid = S(ctx.call("event_team_id", {"event": event, "payload": payload, "install_set": has_tc}))
    if has_tc and not tid:
        ctx.call("logger.info", {"msg": "[Slack] Ignoring event with no authenticated workspace"})
        return None
    if ets:
        wid = ctx.call("workspace_event_id", {"team_id": tid, "ts": ets})
        if T(ctx.call("is_duplicate", {"id": wid})):
            return None
    cid = S(event.get("channel"))
    if T(ctx.call("is_ignored_channel", {"channel_id": cid})):
        ctx.call("logger.info", {"msg": "[Slack] Ignoring message in configured ignored channel",
                                 "channel_id": cid})
        return None
    author = ""
    for fld in ("user_team", "team"):
        v = event.get(fld)
        if isinstance(v, str) and v.startswith("T"):
            author = v
            break
    is_dm = (S(event.get("channel_type")) == "im" or cid.startswith("D")
             or S(event.get("_hermes_reaction_source_channel")).startswith("D"))
    if is_dm and T(ctx.call("is_foreign_author", {"author_team": author, "team_id": tid})):
        ctx.call("logger.info", {"msg": "[Slack] Ignoring DM from external-org user"})
        return None
    if T(ctx.call("drop_bot_sender", {"event": event})):
        return None
    st = event.get("subtype")
    if st not in (None, "", "file_share", "thread_broadcast", "me_message",
                  "bot_message", "document_mention"):
        ctx.call("logger.debug", {"label": "drop subtype", "subtype": st, "channel_id": cid})
        return None
    return [event, tid, cid]


def m_register_bolt_handlers(ctx, a, state):
    pairs = [
        ("message", "_handle_slack_message"), ("app_mention", "_handle_slack_message"),
        ("app_home_opened", "_handle_app_home_opened"),
        ("app_context_changed", "_handle_app_context_changed"),
        ("file_shared", "_handle_slack_file_shared"), ("file_created", "_noop"),
        ("file_change", "_noop"), ("reaction_added", "_reaction(False)"),
        ("reaction_removed", "_reaction(True)"),
        ("assistant_thread_started", "_handle_assistant_thread_lifecycle_event"),
        ("assistant_thread_context_changed", "_handle_assistant_thread_lifecycle_event")]
    for et, h in pairs:
        ctx.call("app_event", {"event_type": et, "handler": h})
    pat = ctx.call("re_compile", {"pattern": ".*"})
    ctx.call("app_event", {"event_type": canon(pat), "handler": "handle_unhandled_event"})
    names = ctx.call("slack_native_slashes", {})
    names = names if isinstance(names, list) else []
    slash_pat = ctx.call("re_compile", {"names": canon(names)})
    ctx.call("app_command", {"pattern": canon(slash_pat), "handler": "_handle_hermes_command"})
    for aid in state.get("_APPROVAL_CHOICES", []) or []:
        ctx.call("app_action", {"action_id": aid, "handler": "_handle_approval_action"})
    for aid in state.get("_CONFIRM_CHOICES", []) or []:
        ctx.call("app_action", {"action_id": aid, "handler": "_handle_slash_confirm_action"})
    ctx.call("app_action", {"action_id": "hermes_feedback", "handler": "_handle_feedback_action"})
    clar = ctx.call("re_compile", {"pattern": "^hermes_clarify_choice_\\d+$"})
    ctx.call("app_action", {"action_id": canon(clar), "handler": "_handle_clarify_action"})
    ctx.call("app_action", {"action_id": "hermes_clarify_other", "handler": "_handle_clarify_action"})
    for aid in state.get("_MODEL_PICKER_ACTION_IDS", []) or []:
        ctx.call("app_action", {"action_id": aid, "handler": "_handle_model_picker_action"})
    ctx.call("register_vault_code_handlers", {})
    ctx.call("register_plugin_action_handlers", {})
    ctx.call("wire_plugin_handlers", {})
    return None


def m_register_plugin_action_handlers(ctx, a, state):
    try:
        handlers = ctx.call("get_slack_action_handlers", {})
    except BaseException as exc:
        sp = spec_of(exc)
        if base_only(sp):
            raise
        ctx.call("logger.warning", {"label": "Could not load plugin action handlers",
                                    "error": sp.get("message", "")})
        handlers = []
    if not isinstance(handlers, list):
        handlers = []
    app = state.get("_app")
    if state.get("_plugin_actions_app") != app:
        state["_plugin_actions_app"] = canon(app)
        state["_plugin_actions_wired"] = {}
    wired = state.get("_plugin_actions_wired")
    if not isinstance(wired, dict):
        wired = {}
        state["_plugin_actions_wired"] = wired
    fresh = []
    for h in handlers:
        if not isinstance(h, list) or len(h) < 3:
            continue
        aid, cb, name = h[0], h[1], h[2]
        key = strval(aid) + "|" + strval(name)
        if key not in wired:
            fresh.append((aid, cb, name))
    for aid, cb, name in fresh:
        ctx.call("app_action", {"action_id": aid, "handler": "_make_wrapper"})
        wired[strval(aid) + "|" + strval(name)] = True
        ctx.call("logger.debug", {"action_id": aid, "plugin_name": name})
    if fresh:
        ctx.call("logger.info", {"count": len(fresh)})
    return None


def m_resolve_download_token(ctx, a, state):
    tc = state.get("_team_clients") if isinstance(state.get("_team_clients"), dict) else {}
    tid = S(a.get("team_id"))
    if tid and tid in tc:
        e = tc[tid]
        return S(e.get("token")) if isinstance(e, dict) else ""
    try:
        m = ctx.call("re_search", {"pattern": "/files-pri/(T[A-Z0-9]+)-", "url": S(a.get("url"))})
        g = S(m.get("group1")) if isinstance(m, dict) else ""
        if g and g in tc:
            e = tc[g]
            return S(e.get("token")) if isinstance(e, dict) else ""
    except BaseException as exc:
        sp = spec_of(exc)
        if base_only(sp):
            raise
    cfg = state.get("config") if isinstance(state.get("config"), dict) else {}
    return S(cfg.get("token"))


def m_slack_reaction_triggers(ctx, a, state):
    cfg = state.get("config") if isinstance(state.get("config"), dict) else {}
    extra = cfg.get("extra") if isinstance(cfg.get("extra"), dict) else {}
    raw = extra.get("reaction_triggers")
    if raw is None:
        sec = ctx.call("get_scoped_secret", {"key": "SLACK_REACTION_TRIGGERS"})
        raw = sec if sec is not None else None
    if raw is None:
        return None
    if isinstance(raw, bool):
        return {"__set__": []} if raw else None
    if isinstance(raw, (list, tuple, set)):
        vals = sorted({p for x in raw for p in [strval(x).strip().strip(":")] if p})
        return {"__set__": vals}
    text = strip(pystr(raw) if T(raw) else "")
    if not text or text.lower() in {"false", "0", "no", "off"}:
        return None
    if text.lower() in {"true", "1", "yes", "on", "all", "*"}:
        return {"__set__": []}
    parts = ctx.call("re_split", {"text": text, "pattern": "[,\\s]+"})
    vals = sorted({p for x in (parts if isinstance(parts, list) else [])
                   for p in [strval(x).strip().strip(":")] if p})
    return {"__set__": vals}



MODELS = {
    "_post_interactive_blocks": m_post_interactive_blocks,
    "_prefilter_inbound": m_prefilter_inbound,
    "_react": m_react,
    "_reaction_thread_ts": m_reaction_thread_ts,
    "_read_error_text_limited": m_read_error_text_limited,
    "_record_uploaded_file_thread": m_record_uploaded_file_thread,
    "_refuse_other_requester": m_refuse_other_requester,
    "_register_bolt_handlers": m_register_bolt_handlers,
    "_register_plugin_action_handlers": m_register_plugin_action_handlers,
    "_remember_processed_message_ts": m_remember_processed_message_ts,
    "_remove_reaction": m_remove_reaction,
    "_render_message_text": m_render_message_text,
    "_resolve_author_team": m_resolve_author_team,
    "_resolve_channel_name": m_resolve_channel_name,
    "_resolve_download_token": m_resolve_download_token,
    "_slack_reaction_triggers": m_slack_reaction_triggers,
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
    csrc = open(os.path.join(_REPO, "src", "gateway", "platforms", "slack_adapter_flow12.c"),
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow12: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if '"' + n + '"' not in csrc]
    if missing_c:
        print("FATAL oracle_slack_adapter_flow12: seams absent from C port: " + str(missing_c))
        sys.exit(2)
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow12.jsonl")
    if os.path.exists(fix):
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print("FATAL oracle_slack_adapter_flow12: fixture io keys != SEAM_NAMES; "
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
    print("ORACLE slack_adapter_flow12: " + str(n) + " cases, " + str(m) + " mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow12.jsonl")
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
    print("SELFTEST slack_adapter_flow12: " + str(n) + " cases, " + str(bad) + " drive/script failures")
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))
    return 1 if bad or n == 0 else 0


def dump():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow12.jsonl")
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

