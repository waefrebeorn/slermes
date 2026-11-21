#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 13.

Independent Python reference MODEL of the 16 LIVE
plugins/platforms/slack/adapter.py remainders ported by the C file. Each model
is a faithful transcription of the live function's DECISION logic with every
injected/unported callable replaced by a recording stub fed the SAME io
scripts the C harness feeds sa13_call. No quarry import chain is imported.

Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and echoed state ("state"). Summary:
'ORACLE slack_adapter_flow13: N cases, M mismatches'.

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

SEAM_NAMES = [
    "build",
    "build_source",
    "button",
    "chat_stopStream",
    "clear_thread_status_quietly",
    "describe_slack_api_error",
    "dm_target",
    "emit_media_warning",
    "exists",
    "files_info",
    "format_chunks",
    "gateway_trust_env",
    "get_client",
    "get_session_store",
    "has_app",
    "has_app_token",
    "import_aiohttp",
    "is_running",
    "logger.debug",
    "logger.error",
    "logger.warning",
    "metadata_team_id",
    "new_session",
    "note_attachment_failure",
    "parse_users_info",
    "post",
    "post_ephemeral_fallback",
    "post_interactive_blocks",
    "read_error_text_limited",
    "resolve_thread_ts",
    "seal_stream",
    "send_failure_notice",
    "send_interactive_prompt",
    "send_slash_ephemeral",
    "session_status_method",
    "set_status",
    "slack_json_post",
    "standalone_proxy_kwargs",
    "start_socket_mode_handler",
    "stop_socket_mode_handler",
    "time.time",
    "to_thread",
    "trim_oldest_dict_entries",
    "trim_slack_dm_cache",
    "upload_with_retry",
    "users_info_payload",
    "workspace_message_marker",
]

TARGETS = [
    "_resolve_file_stub",
    "_resolve_slack_user_dm",
    "_resolve_user_is_bot",
    "_resolve_user_name",
    "_restart_socket_mode",
    "_seal_stale_streams",
    "_seal_stream",
    "_seed_dm_session",
    "_send_exec_approval_prompt",
    "_send_failure_notice",
    "_send_interactive_prompt",
    "_send_local_file",
    "_send_slash_ephemeral",
    "_send_slash_reply",
    "_set_thread_status",
    "_set_thread_watermark",
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



# ── helper: seam call returning raise-spec as a VALUE (mirrors sa13_call) ──
def call(ctx, name, args):
    try:
        return ctx.call(name, args)
    except BaseException as exc:
        return spec_of(exc)


def sd(v, d):
    return v if isinstance(v, str) else d


def state_obj(state, name):
    d = state.get(name)
    if not isinstance(d, dict):
        d = {}
        state[name] = d
    return d


# ── wave-13 models (faithful transcripts of the live decision logic) ──────
def m_resolve_file_stub(ctx, a, state):
    f = a.get("f") if isinstance(a.get("f"), dict) else {}
    fid = S(f.get("id"))
    if not fid:
        return None
    notices = a.get("notices")
    has = notices is not None
    client = call(ctx, "get_client", {"channel_id": S(a.get("channel_id")), "team_id": canon(a.get("team_id"))})
    info = client if is_raise(client) else call(ctx, "files_info", {"file": fid, "client": canon(client)})
    if is_raise(info):
        if base_only(info):
            return info
        if has:
            detail = call(ctx, "describe_slack_api_error", {"error": canon(info.get("message")), "file": canon(f)})
            nr = call(ctx, "note_attachment_failure", {"notices": canon(notices), "detail": canon(detail),
                     "label": "files.info error for %s: %s", "arg": fid, "exc": canon(info.get("message"))})
            if is_raise(nr) and base_only(nr):
                return nr
        return None
    if T(info.get("ok")):
        return canon(info.get("file"))
    if has:
        detail = call(ctx, "describe_slack_api_error", {"error": canon(info), "file": canon(f)})
        nr = call(ctx, "note_attachment_failure", {"notices": canon(notices), "detail": canon(detail),
                 "label": "files.info failed for %s: %s", "arg": fid, "extra": canon(info.get("error"))})
        if is_raise(nr) and base_only(nr):
            return nr
    return None


def m_resolve_slack_user_dm(ctx, a, state):
    token = S(a.get("token"))
    uid = S(a.get("user_id"))
    key = token + ":" + uid
    cache = state_obj(state, "_slack_dm_cache")
    if key in cache:
        return canon(cache[key])
    imp = call(ctx, "import_aiohttp", {})
    if is_raise(imp):
        return imp if base_only(imp) else None
    if not T(imp):
        return None
    sesskw = call(ctx, "standalone_proxy_kwargs", {"token": canon(token), "user_id": canon(uid)})
    if is_raise(sesskw):
        return sesskw if base_only(sesskw) else None
    data = call(ctx, "slack_json_post", {"token": canon(token), "method": "conversations.open",
                "users": canon(uid), "kwargs": canon(sesskw)})
    if is_raise(data):
        if base_only(data):
            return data
        lr = call(ctx, "logger.warning", {"label": "[Slack] conversations.open exception for %s: %s",
                  "arg": canon(uid), "error": canon(data.get("message"))})
        return lr if (is_raise(lr) and base_only(lr)) else None
    ch = data.get("channel") if isinstance(data, dict) else None
    cid = sd(ch.get("id"), "") if isinstance(ch, dict) else ""
    if T(data.get("ok")) and cid:
        cache[key] = canon(cid)
        tr = call(ctx, "trim_slack_dm_cache", {"cache": canon(cache)})
        return tr if (is_raise(tr) and base_only(tr)) else cid
    lr = call(ctx, "logger.warning", {"label": "[Slack] conversations.open failed for %s: %s",
              "arg": canon(uid), "error": canon(sd(data.get("error"), "unknown") if isinstance(data, dict) else "unknown")})
    return lr if (is_raise(lr) and base_only(lr)) else None


def m_resolve_user_is_bot(ctx, a, state):
    uidv = a.get("user_id")
    if not T(uidv):
        return False
    uids = strval(uidv)
    ct = state.get("_channel_team") if isinstance(state.get("_channel_team"), dict) else {}
    tid = S(a.get("team_id")) or sd(ct.get(a.get("chat_id")), "")
    key = tid + "|" + uids
    bc = state_obj(state, "_user_is_bot_cache")
    if key in bc:
        return canon(bc[key])
    ap = call(ctx, "has_app", {})
    if is_raise(ap):
        if base_only(ap):
            return ap
    elif not T(ap):
        bc[key] = False
        return False
    pw = {"user_id": canon(uidv), "chat_id": canon(a.get("chat_id")), "team_id": canon(tid)}
    payload = call(ctx, "users_info_payload", pw)
    if is_raise(payload):
        if base_only(payload):
            return payload
        lr = call(ctx, "logger.debug", {"label": "[Slack] users.info bot check failed for %s: %s",
                  "arg": canon(uids), "error": canon(payload.get("message"))})
        if is_raise(lr) and base_only(lr):
            return lr
        bc[key] = False
        return False
    if not T(payload):
        bc[key] = False
        nc = state_obj(state, "_user_name_cache")
        if key not in nc:
            nc[key] = canon(uidv)
        return False
    parsed = call(ctx, "parse_users_info", {"payload": canon(payload), "user_id": canon(uidv)})
    if is_raise(parsed) and base_only(parsed):
        return parsed
    nm = parsed[0] if (isinstance(parsed, list) and len(parsed) > 0) else None
    ib = parsed[1] if (isinstance(parsed, list) and len(parsed) > 1) else None
    isbot = T(ib)
    name = strval(nm) if (isinstance(parsed, list) and len(parsed) > 0) else strval(uidv)
    bc[key] = isbot
    tr = call(ctx, "trim_oldest_dict_entries", {"items": canon(bc), "max_size": num(state.get("_USER_NAME_CACHE_MAX"), 5000)})
    if is_raise(tr) and base_only(tr):
        return tr
    nc = state_obj(state, "_user_name_cache")
    nc[key] = canon(name)
    return isbot


def m_resolve_user_name(ctx, a, state):
    uidv = a.get("user_id")
    if not T(uidv):
        return ""
    uids = strval(uidv)
    ct = state.get("_channel_team") if isinstance(state.get("_channel_team"), dict) else {}
    tid = S(a.get("team_id")) or sd(ct.get(a.get("chat_id")), "")
    key = tid + "|" + uids
    nc = state_obj(state, "_user_name_cache")
    hit = nc.get(key, _MISSING)
    if hit is not _MISSING and hit is not None:
        return canon(hit)
    ap = call(ctx, "has_app", {})
    if is_raise(ap):
        if base_only(ap):
            return ap
    elif not T(ap):
        return uids
    pw = {"user_id": canon(uidv), "chat_id": canon(a.get("chat_id")), "team_id": canon(tid)}
    payload = call(ctx, "users_info_payload", pw)
    if is_raise(payload):
        if base_only(payload):
            return payload
        lr = call(ctx, "logger.debug", {"label": "[Slack] users.info failed for %s: %s",
                  "arg": canon(uids), "error": canon(payload.get("message"))})
        if is_raise(lr) and base_only(lr):
            return lr
        name = uids
    elif not T(payload):
        bc = state_obj(state, "_user_is_bot_cache")
        bc[key] = False
        nc[key] = canon(uidv)
        name = uids
    else:
        parsed = call(ctx, "parse_users_info", {"payload": canon(payload), "user_id": canon(uidv)})
        if is_raise(parsed) and base_only(parsed):
            return parsed
        nm = parsed[0] if (isinstance(parsed, list) and len(parsed) > 0) else None
        ib = parsed[1] if (isinstance(parsed, list) and len(parsed) > 1) else None
        bc = state_obj(state, "_user_is_bot_cache")
        bc[key] = canon(ib if ib is not None else False)
        name = strval(nm) if (isinstance(parsed, list) and len(parsed) > 0) else strval(uidv)
    nc[key] = canon(name)
    tr = call(ctx, "trim_oldest_dict_entries", {"items": canon(nc), "max_size": num(state.get("_USER_NAME_CACHE_MAX"), 5000)})
    return tr if (is_raise(tr) and base_only(tr)) else name


def m_restart_socket_mode(ctx, a, state):
    r1 = call(ctx, "is_running", {})
    if is_raise(r1):
        if base_only(r1):
            return r1
    elif not T(r1):
        return None
    r2 = call(ctx, "is_running", {})
    if is_raise(r2):
        if base_only(r2):
            return r2
    elif not T(r2):
        return None
    app = call(ctx, "has_app", {})
    if is_raise(app):
        if base_only(app):
            return app
    elif not T(app):
        return None
    tok = call(ctx, "has_app_token", {})
    if is_raise(tok):
        if base_only(tok):
            return tok
    elif not T(tok):
        return None
    lr = call(ctx, "logger.warning", {"label": "[Slack] Socket Mode unhealthy (%s); reconnecting", "arg": canon(a.get("reason"))})
    if is_raise(lr) and base_only(lr):
        return lr
    sr = call(ctx, "stop_socket_mode_handler", {})
    if is_raise(sr) and base_only(sr):
        return sr
    st = call(ctx, "start_socket_mode_handler", {})
    if is_raise(st):
        if base_only(st):
            return st
        er = call(ctx, "logger.error", {"label": "[Slack] Socket Mode reconnect failed: %s", "error": canon(st.get("message"))})
        if is_raise(er) and base_only(er):
            return er
    return None


def m_seal_stale_streams(ctx, a, state):
    now = call(ctx, "time.time", {})
    if is_raise(now):
        return now if base_only(now) else None
    cutoff = num(now, 0) - num(state.get("_STREAM_MAX_AGE_S"), 0)
    streams = state.get("_active_streams")
    if not isinstance(streams, dict):
        return None
    for key in list(streams.keys()):
        stream = streams[key]
        started = num(stream.get("started"), 0) if isinstance(stream, dict) else 0
        if started < cutoff:
            streams.pop(key, None)
            r = call(ctx, "seal_stream", {"key": canon(key), "stream": canon(stream)})
            if is_raise(r) and base_only(r):
                return r
    return None


def m_seal_stream(ctx, a, state):
    key = a.get("key") if isinstance(a.get("key"), list) else []
    team = strval(key[0]) if len(key) > 0 else ""
    chat = strval(key[1]) if len(key) > 1 else ""
    stream = a.get("stream") if isinstance(a.get("stream"), dict) else {}
    delta = a.get("delta")
    add_delta = T(delta) and strip(delta) != ""
    client = call(ctx, "get_client", {"channel": canon(chat), "team_id": canon(team if team else None)})
    if is_raise(client):
        if base_only(client):
            return client
        lr = call(ctx, "logger.debug", {"label": "[Slack] chat.stopStream failed for %s/%s: %s",
                  "args": [canon(chat), canon(stream.get("ts")), canon(client.get("message"))]})
        return lr if (is_raise(lr) and base_only(lr)) else False
    kwargs = {"channel": canon(chat), "ts": canon(stream.get("ts"))}
    if add_delta:
        kwargs["markdown_text"] = canon(S(delta))
    kwargs["client"] = canon(client)
    r = call(ctx, "chat_stopStream", kwargs)
    if is_raise(r):
        if base_only(r):
            return r
        lr = call(ctx, "logger.debug", {"label": "[Slack] chat.stopStream failed for %s/%s: %s",
                  "args": [canon(chat), canon(stream.get("ts")), canon(r.get("message"))]})
        return lr if (is_raise(lr) and base_only(lr)) else False
    return True


def m_seed_dm_session(ctx, a, state):
    ss0 = call(ctx, "get_session_store", {})
    if is_raise(ss0) and base_only(ss0):
        return ss0
    have = T(ss0)
    meta = a.get("metadata") if isinstance(a.get("metadata"), dict) else {}
    cid = S(meta.get("channel_id"))
    uid = S(meta.get("user_id"))
    if not have or not cid or not uid:
        return None
    nc0 = state.get("_channel_name_cache") if isinstance(state.get("_channel_name_cache"), dict) else {}
    tid = S(meta.get("team_id"))
    ck = tid + "|" + cid
    cname = sd(nc0.get(ck), cid) if ck in nc0 else cid
    sw = {"chat_id": canon(cid), "chat_name": canon(cname), "chat_type": "dm", "user_id": canon(uid),
          "thread_id": canon(a.get("thread_ts")),
          "chat_topic": canon(meta.get("context_channel_id") if T(meta.get("context_channel_id")) else None),
          "scope_id": canon(meta.get("team_id") if T(meta.get("team_id")) else None)}
    src = call(ctx, "build_source", sw)
    if is_raise(src) and base_only(src):
        return src
    tw = {"method": "get_or_create_session", "args": canon([canon(src)])}
    r = call(ctx, "to_thread", tw)
    if is_raise(r):
        if base_only(r):
            return r
        fl = a.get("fail_log") if isinstance(a.get("fail_log"), list) else []
        lr = call(ctx, "logger.debug", {"args": canon(fl), "exc_info": True})
        return lr if (is_raise(lr) and base_only(lr)) else None
    return None


def m_send_failure_notice(ctx, a, state):
    return call(ctx, "emit_media_warning", {"chat_id": canon(a.get("chat_id")), "notice": canon(a.get("notice")),
                "caption": canon(a.get("caption")), "reply_to": canon(a.get("reply_to")),
                "metadata": canon(a.get("metadata")), "shown_metadata": canon(a.get("metadata"))})


def m_send_interactive_prompt(ctx, a, state):
    ap = call(ctx, "has_app", {})
    if is_raise(ap):
        if base_only(ap):
            return ap
    elif not T(ap):
        return sr(False, "Not connected")
    cidv = call(ctx, "dm_target", {"chat_id": canon(a.get("chat_id")), "metadata": canon(a.get("metadata"))})
    if is_raise(cidv) and base_only(cidv):
        return cidv
    built = call(ctx, "build", {"chat_id": canon(cidv), "metadata": canon(a.get("metadata"))})
    if is_raise(built) and base_only(built):
        return built
    text = built[0] if (isinstance(built, list) and len(built) > 0) else None
    blocks = built[1] if (isinstance(built, list) and len(built) > 1) else None
    pw = {"chat_id": canon(cidv), "text": canon(text), "blocks": canon(blocks),
          "metadata": canon(a.get("metadata")), "sanitize": canon(a.get("sanitize")),
          "team_scoped": canon(a.get("team_scoped_key"))}
    result = call(ctx, "post_interactive_blocks", pw)
    if is_raise(result):
        if base_only(result):
            return result
        lr = call(ctx, "logger.error", {"label": canon(a.get("label")), "error": canon(result.get("message"))})
        return lr if (is_raise(lr) and base_only(lr)) else sr(False, None)
    ts = S(result.get("ts"))
    resolved = a.get("resolved")
    rd = canon(resolved) if isinstance(resolved, dict) else None
    if ts and rd is not None:
        key = ts
        scoped = a.get("team_scoped_key") if isinstance(a.get("team_scoped_key"), bool) else True
        if scoped:
            team = call(ctx, "metadata_team_id", {"metadata": canon(a.get("metadata"))})
            if is_raise(team) and base_only(team):
                return team
            marker = call(ctx, "workspace_message_marker", {"team_id": canon(team), "ts": canon(ts)})
            if is_raise(marker) and base_only(marker):
                return marker
            key = strval(marker)
        rd[strval(key)] = False
        tr = call(ctx, "trim_oldest_dict_entries", {"items": canon(rd), "max_size": num(a.get("resolved_max"), 0)})
        if is_raise(tr) and base_only(tr):
            return tr
    out = sr(True, None)
    out["message_id"] = canon(ts)
    out["raw_response"] = canon(result)
    return out


def m_send_local_file(ctx, a, state):
    ap = call(ctx, "has_app", {})
    if is_raise(ap):
        if base_only(ap):
            return ap
    elif not T(ap):
        return sr(False, "Not connected")
    ex = call(ctx, "exists", {"path": canon(a.get("file_path"))})
    if is_raise(ex):
        if base_only(ex):
            return ex
    elif not T(ex):
        return sr(False, S(a.get("not_found_error")))
    cidv = call(ctx, "dm_target", {"chat_id": canon(a.get("chat_id")), "metadata": canon(a.get("metadata"))})
    if is_raise(cidv) and base_only(cidv):
        return cidv
    ts = call(ctx, "resolve_thread_ts", {"reply_to": canon(a.get("reply_to")), "metadata": canon(a.get("metadata"))})
    if is_raise(ts) and base_only(ts):
        return ts
    kind = S(a.get("kind"))
    label = kind.capitalize() + " upload"
    r = call(ctx, "upload_with_retry", {"chat_id": canon(cidv), "file_path": canon(a.get("file_path")),
              "filename": canon(a.get("filename")), "caption": canon(a.get("caption")),
              "thread_ts": canon(ts), "metadata": canon(a.get("metadata")), "label": canon(label)})
    if is_raise(r):
        if base_only(r):
            return r
        lw = call(ctx, "logger.error", {"label": "[%s] Failed to send %s %s: %s",
                  "args": [S((state.get("config") or {}).get("name")), canon(a.get("kind")),
                           canon(a.get("file_path")), canon(r.get("message"))]})
        if is_raise(lw) and base_only(lw):
            return lw
        return call(ctx, "send_failure_notice", {"chat_id": canon(cidv), "caption": canon(a.get("caption")),
                    "notice": canon(a.get("failure_notice")), "reply_to": canon(a.get("reply_to")),
                    "metadata": canon(a.get("metadata"))})
    return r


def m_send_exec_approval_prompt(ctx, a, state):
    prompt = a.get("prompt") if isinstance(a.get("prompt"), dict) else {}
    acts = prompt.get("actions") if isinstance(prompt.get("actions"), list) else []
    ea = state.get("_EA_ACTION_IDS") if isinstance(state.get("_EA_ACTION_IDS"), dict) else {}
    actions = []
    for it in acts:
        if not isinstance(it, list) or len(it) < 3:
            continue
        label, choice, style = it[0], it[1], it[2]
        aid = S(ea.get(strval(choice)))
        b = call(ctx, "button", {"label": canon(label), "action_id": canon(aid),
                 "session_key": canon(prompt.get("session_key")), "style": canon(style)})
        if is_raise(b) and base_only(b):
            return b
        actions.append([canon(label), canon(aid), canon(style), None])
    blocks = [{"type": "section", "text": {"type": "mrkdwn", "text": canon(prompt.get("text"))}},
              {"type": "actions", "elements": canon(actions)}]
    fallback = "platform.slack.approval.fallback_text"  # t() identity
    iw = {"chat_id": canon(prompt.get("chat_id")), "metadata": canon(prompt.get("metadata")),
          "fallback_text": fallback, "blocks": canon(blocks), "label": "send_exec_approval",
          "resolved": canon(a.get("resolved")), "resolved_max": num(a.get("resolved_max"), 5000)}
    result = call(ctx, "send_interactive_prompt", iw)
    if is_raise(result) and base_only(result):
        return result
    ok = T(result.get("success"))
    rid = prompt.get("request_id")
    if ok and T(rid):
        team = call(ctx, "metadata_team_id", {"metadata": canon(prompt.get("metadata"))})
        if is_raise(team) and base_only(team):
            return result
        marker = call(ctx, "workspace_message_marker", {"team_id": canon(team), "ts": canon(result.get("message_id"))})
        if is_raise(marker) and base_only(marker):
            return result
        ad = state_obj(state, "_approval_request_ids")
        ad[strval(marker)] = canon(rid)
        tr = call(ctx, "trim_oldest_dict_entries", {"items": canon(ad), "max_size": num(a.get("resolved_max"), 5000)})
        if is_raise(tr) and base_only(tr):
            return result
    return result


def m_send_slash_ephemeral(ctx, a, state):
    ctxa = a.get("ctx") if isinstance(a.get("ctx"), dict) else {}
    content = S(a.get("content"))
    chunksv = call(ctx, "format_chunks", {"content": canon(content)})
    if is_raise(chunksv) and base_only(chunksv):
        return chunksv
    chunks = list(chunksv) if isinstance(chunksv, list) else []
    if len(chunks) > 5:
        last = S(chunks[4]).rstrip()
        chunks = chunks[:4] + [last + "platform.slack.slash.reply_truncated"]
    tv = call(ctx, "gateway_trust_env", {})
    if is_raise(tv) and base_only(tv):
        return tv
    sess = call(ctx, "new_session", {"trust_env": canon(tv)})
    if is_raise(sess) and base_only(sess):
        return sess
    for idx, chunk in enumerate(chunks):
        pl = {"response_type": "ephemeral", "replace_original": idx == 0, "text": canon(chunk)}
        resp = call(ctx, "post", {"url": canon(ctxa.get("response_url")), "json": canon(pl)})
        if is_raise(resp):
            if base_only(resp):
                return resp
            lr = call(ctx, "logger.warning", {"label": "[Slack] response_url POST failed: %s", "arg": canon(resp.get("message"))})
            return lr if (is_raise(lr) and base_only(lr)) else sr(False, None)
        status = num(resp.get("status"), 0)
        if status != 200:
            body = call(ctx, "read_error_text_limited", {"response": canon(resp)})
            if is_raise(body) and base_only(body):
                return body
            bt = strval(body)[:200]
            msg = "response_url POST returned " + pystr(status)
            ww = {"label": "[Slack] response_url POST returned %s: %s", "args": [canon(status), canon(bt)]}
            lr = call(ctx, "logger.warning", ww)
            return lr if (is_raise(lr) and base_only(lr)) else sr(False, msg)
    return sr(True, None)


def m_send_slash_reply(ctx, a, state):
    er = call(ctx, "send_slash_ephemeral", {"ctx": canon(a.get("slash_ctx")), "content": canon(a.get("content"))})
    if is_raise(er) and base_only(er):
        return er
    if T(er.get("success")):
        cr = call(ctx, "clear_thread_status_quietly", {"chat_id": canon(a.get("chat_id")), "metadata": canon(a.get("metadata"))})
        return cr if (is_raise(cr) and base_only(cr)) else er
    lr = call(ctx, "logger.warning", {"label": "[Slack] response_url slash reply failed (%s); retrying via chat.postEphemeral",
              "arg": canon(er.get("error"))})
    if is_raise(lr) and base_only(lr):
        return lr
    fr = call(ctx, "post_ephemeral_fallback", {"chat_id": canon(a.get("chat_id")),
              "slash_ctx": canon(a.get("slash_ctx")), "content": canon(a.get("content"))})
    if is_raise(fr) and base_only(fr):
        return fr
    if T(fr.get("success")):
        cr = call(ctx, "clear_thread_status_quietly", {"chat_id": canon(a.get("chat_id")), "metadata": canon(a.get("metadata"))})
        return cr if (is_raise(cr) and base_only(cr)) else fr
    er2 = call(ctx, "logger.error", {"label": "[Slack] Ephemeral slash reply failed on both response_url and chat.postEphemeral (%s); dropping rather than posting publicly",
               "arg": canon(fr.get("error"))})
    return er2 if (is_raise(er2) and base_only(er2)) else fr


def m_set_thread_status(ctx, a, state):
    client = call(ctx, "get_client", {"chat_id": canon(a.get("chat_id")), "team_id": canon(a.get("team_id"))})
    if is_raise(client):
        if base_only(client):
            return client
        lr = call(ctx, "logger.debug", {"label": "[Slack] assistant.threads.setStatus %s: %s",
                  "args": [canon(a.get("fail_label")), canon(client.get("message"))]})
        return lr if (is_raise(lr) and base_only(lr)) else None
    setter = call(ctx, "session_status_method", {"client": canon(client)})
    if is_raise(setter):
        if base_only(setter):
            return setter
        lr = call(ctx, "logger.debug", {"label": "[Slack] assistant.threads.setStatus %s: %s",
                  "args": [canon(a.get("fail_label")), canon(setter.get("message"))]})
        return lr if (is_raise(lr) and base_only(lr)) else None
    r = call(ctx, "set_status", {"setter": canon(setter), "channel_id": canon(a.get("chat_id")),
             "thread_ts": canon(a.get("thread_ts")), "status": canon(a.get("status"))})
    if is_raise(r):
        if base_only(r):
            return r
        lr = call(ctx, "logger.debug", {"label": "[Slack] assistant.threads.setStatus %s: %s",
                  "args": [canon(a.get("fail_label")), canon(r.get("message"))]})
        return lr if (is_raise(lr) and base_only(lr)) else None
    return None


def m_set_thread_watermark(ctx, a, state):
    if not T(a.get("watermark_ts")):
        return None
    tw = {"method": "set_session_metadata", "channel_id": canon(a.get("chat_id")),
          "thread_ts": canon(a.get("thread_ts")), "user_id": canon(a.get("user_id")),
          "team_id": canon(a.get("team_id")), "args": canon([a.get("watermark_ts")])}
    r = call(ctx, "to_thread", tw)
    if is_raise(r):
        if base_only(r):
            return r
        lr = call(ctx, "logger.debug", {"label": "[Slack] Failed to persist thread watermark", "exc_info": True})
        return lr if (is_raise(lr) and base_only(lr)) else None
    return None


MODELS = {
    "_resolve_file_stub": m_resolve_file_stub,
    "_resolve_slack_user_dm": m_resolve_slack_user_dm,
    "_resolve_user_is_bot": m_resolve_user_is_bot,
    "_resolve_user_name": m_resolve_user_name,
    "_restart_socket_mode": m_restart_socket_mode,
    "_seal_stale_streams": m_seal_stale_streams,
    "_seal_stream": m_seal_stream,
    "_seed_dm_session": m_seed_dm_session,
    "_send_exec_approval_prompt": m_send_exec_approval_prompt,
    "_send_failure_notice": m_send_failure_notice,
    "_send_interactive_prompt": m_send_interactive_prompt,
    "_send_local_file": m_send_local_file,
    "_send_slash_ephemeral": m_send_slash_ephemeral,
    "_send_slash_reply": m_send_slash_reply,
    "_set_thread_status": m_set_thread_status,
    "_set_thread_watermark": m_set_thread_watermark,
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
    csrc = open(os.path.join(_REPO, "src", "gateway", "platforms", "slack_adapter_flow13.c"),
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow13: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if '"' + n + '"' not in csrc]
    if missing_c:
        print("FATAL oracle_slack_adapter_flow13: seams absent from C port: " + str(missing_c))
        sys.exit(2)
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow13.jsonl")
    if os.path.exists(fix):
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print("FATAL oracle_slack_adapter_flow13: fixture io keys != SEAM_NAMES; "
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
    print("ORACLE slack_adapter_flow13: " + str(n) + " cases, " + str(m) + " mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow13.jsonl")
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
    print("SELFTEST slack_adapter_flow13: " + str(n) + " cases, " + str(bad) + " drive/script failures")
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))
    return 1 if bad or n == 0 else 0


def dump():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow13.jsonl")
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

