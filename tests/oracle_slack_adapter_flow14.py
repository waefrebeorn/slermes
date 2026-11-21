#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 13.

Independent Python reference MODEL of the 16 LIVE
plugins/platforms/slack/adapter.py remainders ported by the C file. Each model
is a faithful transcription of the live function's DECISION logic with every
injected/unported callable replaced by a recording stub fed the SAME io
scripts the C harness feeds sa14_call. No quarry import chain is imported.

Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and echoed state ("state"). Summary:
'ORACLE slack_adapter_flow14: N cases, M mismatches'.

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
    "apply_slack_proxy",
    "assistant_suggested_prompts",
    "assistant_thread_title_enabled",
    "assistant_threads_setSuggestedPrompts",
    "async_web_client",
    "await",
    "bot_authored_thread_root",
    "build_thread_session_key",
    "client_session",
    "config_extra_get",
    "describe_proxy_error",
    "elide",
    "evict_oldest_by_ts",
    "exists",
    "fetch_thread_parent_text",
    "format_mrkdwn",
    "get_client",
    "get_scoped_secret",
    "get_secret",
    "get_session_store",
    "has_active_session_for_thread",
    "import_aiohttp",
    "import_slack_sdk",
    "is_connected",
    "json_dumps",
    "json_loads",
    "load_slack_bot_tokens",
    "logger.debug",
    "logger.error",
    "logger.info",
    "logger.warning",
    "next_",
    "pconfig_extra",
    "pconfig_token",
    "post",
    "re_compile",
    "reaction_trigger_target",
    "register_mentioned_thread",
    "repr",
    "request_never_left",
    "resolve_proxy_url",
    "resolve_slack_user_dm",
    "resp_json",
    "restart_socket_mode",
    "send_error",
    "session_store_method",
    "session_title_method",
    "set_thread_title",
    "slack_json_post",
    "slack_strict_mention",
    "slack_unfurl_kwargs",
    "sleep",
    "socket_ping_pong_stale",
    "socket_transport_connected",
    "standalone_post_kwargs",
    "standalone_post_text",
    "standalone_proxy_kwargs",
    "standalone_send_media",
    "standalone_upload_file",
    "workspace_message_marker",
    "workspace_thread_key",
]

TARGETS = [
    "_serialize_slack_blocks_for_agent",
    "_set_assistant_suggested_prompts",
    "_set_assistant_thread_title",
    "_should_wake_on_unmentioned_message",
    "_slack_api_human_users",
    "_slack_json_post",
    "_slack_mention_patterns",
    "_slack_message_addressed_to_other_user",
    "_slack_message_mentions_self",
    "_slack_per_request_proxy_middleware",
    "_socket_transport_connected",
    "_socket_watchdog_loop",
    "_standalone_send",
    "_standalone_send_media",
    "_synthetic_reaction_event",
    "_thread_watermark_io",
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
    "TimeoutException": Exception, "HTTPStatusError": Exception, "re.error": Exception,
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



# ── helper: seam call returning raise-spec as a VALUE (mirrors sa14_call) ──
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


# ── extra local helpers mirroring wave-14 C port ──────────────────────────
def pytype(v):
    if v is None: return "NoneType"
    if isinstance(v, bool): return "bool"
    if isinstance(v, (int, float)): return "int"
    if isinstance(v, str): return "str"
    if isinstance(v, list): return "list"
    if isinstance(v, dict): return "dict"
    return "object"


def contains(hay, needle):
    return isinstance(hay, str) and isinstance(needle, str) and needle in hay


def objget(o, k, d=None):
    return o.get(k, d) if isinstance(o, dict) else d


def strset_from(arr):
    """Mirror sa14_set_from: strip each str, drop empties, sorted unique."""
    if not isinstance(arr, list): return []
    vals = []
    for x in arr:
        v = strip(x)
        if v: vals.append(v)
    return sorted(set(vals))


def sanitize_empty(v):
    if v is None: return True
    if isinstance(v, str): return v == ""
    if isinstance(v, (list, dict)): return len(v) == 0
    return False


# ── wave-14 models (faithful transcripts of the live decision logic) ──────
def m_serialize_slack_blocks_for_agent(ctx, a, state):
    blocks = a.get("blocks") or []
    sk = a.get("scalar_keys") or []
    rk = a.get("recursive_keys") or []
    maxc = a.get("max_chars", 6000)
    def sanitize(v):
        if isinstance(v, list):
            out = []
            for x in v:
                it = sanitize(x)
                if not sanitize_empty(it): out.append(it)
            return out
        if isinstance(v, dict):
            out = {}
            for k, item in v.items():
                if k in sk: out[k] = item
                elif k in rk:
                    c = sanitize(item)
                    if not sanitize_empty(c): out[k] = c
            return out
        return v
    inspectable = []
    for b in blocks:
        ty = objget(b, "type") if isinstance(b, dict) else None
        if ty in ("rich_text", "table"): continue
        inspectable.append(b)
    if not inspectable: return ""
    san = sanitize(inspectable)
    r = call(ctx, "json_dumps", {"value": san})
    if is_raise(r):
        if base_only(r): return r
        pr = call(ctx, "repr", {"value": inspectable})
        if is_raise(pr): return pr
        payload = strval(pr)
    else:
        payload = strval(r)
    er = call(ctx, "elide", {"text": payload, "max_chars": maxc})
    if is_raise(er): return er
    return "[Slack Block Kit payload for this message]\n```json\n" + strval(er) + "\n```"


def m_set_assistant_suggested_prompts(ctx, a, state):
    channel = S(a.get("channel_id")); team = S(a.get("team_id")); thread = S(a.get("thread_ts"))
    if not T(state.get("_app")) or not channel: return None
    r = call(ctx, "assistant_suggested_prompts", {})
    if is_raise(r): return r
    title = r[0] if isinstance(r, list) and len(r) > 0 else None
    prompts = r[1] if isinstance(r, list) and len(r) > 1 else None
    if not T(prompts): return None
    kwargs = {"channel_id": channel, "prompts": prompts}
    if T(title): kwargs["title"] = title
    if thread: kwargs["thread_ts"] = thread
    cl = call(ctx, "get_client", {"channel_id": channel, "team_id": team})
    sr = cl if is_raise(cl) else call(ctx, "assistant_threads_setSuggestedPrompts", kwargs)
    if is_raise(sr):
        if base_only(sr): return sr
        la = {"fmt": "[Slack] assistant.threads.setSuggestedPrompts failed: %s"}
        if "message" in sr: la["error"] = sr["message"]
        lr = call(ctx, "logger.debug", la)
        if is_raise(lr): return lr
    return None


def m_set_assistant_thread_title(ctx, a, state):
    channel = S(a.get("channel_id")); thread = S(a.get("thread_ts"))
    source = S(a.get("title_source")); team = S(a.get("team_id"))
    if (not T(state.get("_app")) or not channel or not thread or not source): return None
    er = call(ctx, "assistant_thread_title_enabled", {})
    if is_raise(er): return er
    if not T(er): return None
    kr = call(ctx, "workspace_thread_key", {"team_id": team, "channel_id": channel, "thread_ts": thread})
    if is_raise(kr): return kr
    key = S(kr)
    titled = state.get("_titled_assistant_threads")
    if not key or set_has(state, "_titled_assistant_threads", key): return None
    title_src = collapse_ws(source)
    if not title_src or title_src.startswith("/"): return None
    if len(title_src) > 80:
        final = title_src[:77].rstrip() + "..."
    else:
        final = title_src
    cl = call(ctx, "get_client", {"channel_id": channel, "team_id": team})
    sm = cl
    if not is_raise(cl):
        ma = {"client": cl}
        mr = call(ctx, "session_title_method", ma)
        sm = mr
        if not is_raise(mr):
            sm = call(ctx, "set_thread_title", {"channel_id": channel, "thread_ts": thread, "title": final})
    if is_raise(sm):
        if base_only(sm): return sm
        la = {"fmt": "[Slack] session title set failed: %s"}
        if "message" in sm: la["error"] = sm["message"]
        lr = call(ctx, "logger.debug", la)
        if is_raise(lr): return lr
        return None
    set_add(state, "_titled_assistant_threads", key)
    vr = call(ctx, "evict_oldest_by_ts", {"field": "e[2]"})
    if is_raise(vr): return vr
    return None


def m_should_wake_on_unmentioned_message(ctx, a, state):
    ets = S(a.get("event_thread_ts"))
    if not ets: return False
    team = S(a.get("team_id")); channel = S(a.get("channel_id"))
    user = S(a.get("user_id")); ctype = S(a.get("chat_type"), "group")
    reply = T(a.get("is_thread_reply"))
    mr = call(ctx, "workspace_message_marker", {"team_id": team, "ts": ets})
    if is_raise(mr): return mr
    marker = S(mr)
    if reply and ((marker and set_has(state, "_bot_message_ts", marker)) or set_has(state, "_bot_message_ts", ets)):
        return True
    if (marker and set_has(state, "_mentioned_threads", marker)) or set_has(state, "_mentioned_threads", ets):
        return True
    if reply:
        ar = call(ctx, "has_active_session_for_thread", {"channel_id": channel, "thread_ts": ets,
                   "user_id": user, "team_id": team, "chat_type": ctype})
        if is_raise(ar): return ar
        if T(ar): return True
        br = call(ctx, "bot_authored_thread_root", dict(a))
        if is_raise(br): return br
        if T(br): return True
    if reply:
        ids = state.get("_team_bot_user_ids")
        uid = None
        if isinstance(ids, dict) and team in ids and isinstance(ids[team], str): uid = ids[team]
        if uid is None:
            b = state.get("_bot_user_id")
            uid = b if isinstance(b, str) else None
        if uid:
            pr = call(ctx, "fetch_thread_parent_text", {"channel_id": channel, "thread_ts": ets,
                      "team_id": team, "strip_bot_mention": False})
            if is_raise(pr): return pr
            if contains(S(pr), "<@" + uid + ">"):
                sr = call(ctx, "slack_strict_mention", {})
                if is_raise(sr): return sr
                if not T(sr):
                    rr = call(ctx, "register_mentioned_thread", {})
                    if is_raise(rr): return rr
                return True
    return False


def m_slack_api_human_users(ctx, a, state):
    cached = state.get("_api_human_users_cache")
    if cached is not None: return cached
    raw = call(ctx, "config_extra_get", {"key": "api_human_users"})
    if is_raise(raw): return raw
    if raw is None:
        raw = call(ctx, "get_scoped_secret", {"key": "SLACK_API_HUMAN_USERS", "default": ""})
        if is_raise(raw): return raw
    parts = raw if isinstance(raw, (list, dict)) else strval(raw).split(",")
    cached = {"__set__": strset_from([strval(p) for p in (parts if isinstance(parts, list) else list(parts.values()))])}
    state["_api_human_users_cache"] = cached
    return cached


def m_slack_json_post(ctx, a, state):
    token = S(a.get("token")); method = S(a.get("method"))
    payload = a.get("payload"); reqkw = a.get("req_kw")
    headers = {"Authorization": "Bearer " + token, "Content-Type": "application/json"}
    pa = {"url": "https://slack.com/api/" + method, "headers": headers, "json": payload}
    if isinstance(reqkw, dict):
        for k, v in reqkw.items(): pa[k] = v
    resp = call(ctx, "post", pa)
    if is_raise(resp): return resp
    return call(ctx, "resp_json", {"resp": resp})


def m_slack_mention_patterns(ctx, a, state):
    cached = state.get("_compiled_mention_patterns")
    if cached is not None: return cached
    patterns = call(ctx, "config_extra_get", {"key": "mention_patterns"})
    if is_raise(patterns): return patterns
    if patterns is None:
        raw = call(ctx, "get_scoped_secret", {"key": "SLACK_MENTION_PATTERNS", "default": ""})
        if is_raise(raw): return raw
        raws = strip(raw)
        if raws:
            jr = call(ctx, "json_loads", {"raw": raws})
            if is_raise(jr):
                if base_only(jr): return jr
                patterns = [p for p in (x.strip() for x in raws.replace("\n", ",").split(",")) if p]
            else:
                patterns = jr
        else:
            patterns = None
    if isinstance(patterns, str): patterns = [patterns]
    compiled = []
    if isinstance(patterns, list):
        for pat in patterns:
            if not (isinstance(pat, str) and pat.strip()): continue
            cr = call(ctx, "re_compile", {"pattern": pat, "flags": "IGNORECASE"})
            if is_raise(cr):
                if base_only(cr): return cr
                wa = {"fmt": "[Slack] Invalid mention pattern %r: %s", "pat": pat}
                if "message" in cr: wa["error"] = cr["message"]
                wr = call(ctx, "logger.warning", wa)
                if is_raise(wr): return wr
                continue
            compiled.append(cr)
    elif patterns is not None:
        wr = call(ctx, "logger.warning", {"fmt": "[Slack] mention_patterns must be a list or string; got %s",
                  "type": pytype(patterns)})
        if is_raise(wr): return wr
    if compiled:
        ir = call(ctx, "logger.info", {"fmt": "[Slack] Loaded %d mention pattern(s)", "n": len(compiled)})
        if is_raise(ir): return ir
    state["_compiled_mention_patterns"] = compiled
    return compiled


def m_slack_message_addressed_to_other_user(ctx, a, state):
    text = a.get("text") or ""
    uids = a.get("self_uids") or []
    m = re.match(r"\s*<@([^>|\s]+)(?:\|[^>]*)?>", text) if text else None
    return bool(m) and (m.group(1) not in uids)


def m_slack_message_mentions_self(ctx, a, state):
    text = a.get("text") or ""
    uids = a.get("self_uids") or []
    if not text: return False
    for u in uids:
        if not isinstance(u, str): continue
        if re.search(r"<@" + re.escape(u) + r"(?:\|[^>]*)?>", text): return True
    return False


def m_slack_per_request_proxy_middleware(ctx, a, state):
    proxy = S(a.get("proxy_url"))
    pa = {"client": a.get("client"), "proxy_url": proxy}
    pr = call(ctx, "apply_slack_proxy", pa)
    if is_raise(pr): return pr
    return call(ctx, "next_", {})


def m_socket_transport_connected(ctx, a, state):
    handler = state.get("_handler")
    client = objget(handler, "client")
    st = objget(client, "is_connected")
    if st is None: return None
    value = st
    if isinstance(value, dict) and "__callable__" in value:
        value = call(ctx, "is_connected", {})
        if is_raise(value):
            if base_only(value): return value
            lr = call(ctx, "logger.debug", {"fmt": "[Slack] Could not inspect Socket Mode transport state", "exc_info": True})
            if is_raise(lr): return lr
            return None
    if isinstance(value, dict) and "__coroutine__" in value:
        value = call(ctx, "await", {})
        if is_raise(value):
            if base_only(value): return value
            lr = call(ctx, "logger.debug", {"fmt": "[Slack] Could not inspect Socket Mode transport state", "exc_info": True})
            if is_raise(lr): return lr
            return None
    if is_raise(value): return value
    return T(value)



def rmsg(r):
    return S(objget(r, "message"))


def exc_family(r, family):
    kind = r.get("__raise__", "Exception")
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    fam = EXC_CLASSES.get(family) or Exception
    return issubclass(cls, fam)


def send_error(ctx, err, nothing=False):
    d = {"error": err}
    if nothing: d["nothing_sent"] = True
    return call(ctx, "send_error", d)


def describe_proxy(ctx, msg):
    pr = call(ctx, "resolve_proxy_url", {})
    if is_raise(pr): return pr
    return call(ctx, "describe_proxy_error", {"error": msg, "proxy": pr})


def se_full(ctx, r):
    if base_only(r): return r
    msg = rmsg(r)
    pr = call(ctx, "resolve_proxy_url", {})
    if is_raise(pr): return pr
    dr = call(ctx, "describe_proxy_error", {"error": msg, "proxy": pr})
    if is_raise(dr): return dr
    desc = dr if isinstance(dr, str) else None
    shown = desc if desc else msg
    nr = call(ctx, "request_never_left", {"error": msg})
    if is_raise(nr): return nr
    return send_error(ctx, "Slack send failed: " + shown, T(nr))


def se_simple(ctx, r, proxy):
    if base_only(r): return r
    msg = rmsg(r)
    dr = call(ctx, "describe_proxy_error", {"error": msg, "proxy": proxy})
    if is_raise(dr): return dr
    desc = dr if isinstance(dr, str) else None
    shown = desc if desc else msg
    return send_error(ctx, "Slack send failed: " + shown, False)


def wd_exc(ctx, r):
    if base_only(r): return r
    wr = call(ctx, "logger.warning", {"fmt": "[Slack] Socket Mode watchdog iteration failed; continuing", "exc_info": True})
    if is_raise(wr): return wr
    return None


def m_socket_watchdog_loop(ctx, a, state):
    guard = 0
    while T(state.get("_running")):
        guard += 1
        if guard > 24: break
        sr = call(ctx, "sleep", {"seconds": a.get("interval_s", 30)})
        if is_raise(sr):
            h = wd_exc(ctx, sr)
            if h is not None: return h
            continue
        if isinstance(sr, dict) and sr.get("stop"): state["_running"] = False
        if not T(state.get("_running")): break
        task = state.get("_socket_mode_task")
        tnull = task is None
        done = isinstance(task, dict) and T(task.get("done"))
        reason = None
        if tnull: reason = "socket task missing"
        elif done: reason = "socket task stopped"
        if reason:
            rr = call(ctx, "restart_socket_mode", {"reason": reason})
            if is_raise(rr):
                h = wd_exc(ctx, rr)
                if h is not None: return h
            continue
        cr = call(ctx, "socket_transport_connected", {})
        if is_raise(cr):
            h = wd_exc(ctx, cr)
            if h is not None: return h
            continue
        disc = (cr is False)
        stale = False
        if not disc:
            pr = call(ctx, "socket_ping_pong_stale", {})
            if is_raise(pr):
                h = wd_exc(ctx, pr)
                if h is not None: return h
                continue
            stale = T(pr)
        if disc or stale:
            rr = call(ctx, "restart_socket_mode", {"reason": "transport disconnected" if disc else "ping/pong stale"})
            if is_raise(rr):
                h = wd_exc(ctx, rr)
                if h is not None: return h
    return None


def m_standalone_send(ctx, a, state):
    media = a.get("media_files") or []
    has_media = isinstance(media, list) and len(media) > 0
    chat = S(a.get("chat_id"))
    message = a.get("message"); caption = a.get("caption"); thread = a.get("thread_id")
    wrong = a.get("wrong_workspace_errors") or []
    raw_token = call(ctx, "pconfig_token", {})
    if is_raise(raw_token): return raw_token
    if not T(raw_token):
        raw_token = call(ctx, "get_secret", {"key": "SLACK_BOT_TOKEN", "default": ""})
        if is_raise(raw_token): return raw_token
    rts = strval(raw_token) if T(raw_token) else ""
    tokens = call(ctx, "load_slack_bot_tokens", {"raw": rts, "quiet": True})
    if is_raise(tokens): return tokens
    if not T(tokens):
        return send_error(ctx, "Slack send failed: SLACK_BOT_TOKEN not configured", True)
    token = S(tokens[0]) if isinstance(tokens, list) and len(tokens) > 0 else ""
    if chat[:1] in ("U", "W"):
        resolved = None; found = False
        for tok in tokens:
            rr = call(ctx, "resolve_slack_user_dm", {"token": S(tok), "chat_id": chat})
            if is_raise(rr): return rr
            if rr is not None:
                resolved = rr; token = S(tok); found = True; break
        if not found:
            return {"error": "Slack user ID resolution failed for " + chat +
                    " (conversations.open — check the bot's im:write scope)"}
        chat = S(resolved)
    formatted = call(ctx, "format_mrkdwn", {"text": message}) if T(message) else message
    if is_raise(formatted): return formatted
    formatted_caption = call(ctx, "format_mrkdwn", {"text": caption}) if T(caption) else caption
    if is_raise(formatted_caption): return formatted_caption
    extra = call(ctx, "pconfig_extra", {})
    if is_raise(extra): return extra
    unfurl = call(ctx, "slack_unfurl_kwargs", {"extra": extra})
    if is_raise(unfurl): return unfurl
    if has_media:
        return call(ctx, "standalone_send_media", {"token": token, "chat_id": chat,
                    "media_files": media, "thread_id": thread, "formatted": formatted,
                    "formatted_caption": formatted_caption, "unfurl_kwargs": unfurl})
    fstr = strip(strval(formatted))
    if (not T(formatted)) or not fstr:
        ir = call(ctx, "logger.debug", {"fmt": "[Slack] _standalone_send: skipping empty/whitespace message"})
        if is_raise(ir): return ir
        return {"success": True, "platform": "slack", "skipped": "empty_text"}
    im = call(ctx, "import_aiohttp", {})
    if is_raise(im):
        if exc_family(im, "ImportError"):
            return send_error(ctx, "aiohttp not installed. Run: pip install aiohttp", False)
        return im
    pk = call(ctx, "standalone_proxy_kwargs", {})
    if is_raise(pk): return se_full(ctx, pk)
    sesskw = pk[0] if isinstance(pk, list) and len(pk) > 0 else None
    reqkw = pk[1] if isinstance(pk, list) and len(pk) > 1 else None
    ca = {"timeout": {"total": 30}}
    if isinstance(sesskw, dict): ca.update(sesskw)
    sess = call(ctx, "client_session", ca)
    if is_raise(sess): return se_full(ctx, sess)
    pa = {"chat_id": chat, "formatted": formatted, "unfurl_kwargs": unfurl, "thread_id": thread}
    payload = call(ctx, "standalone_post_kwargs", pa)
    if is_raise(payload): return se_full(ctx, payload)
    last_error = "unknown"
    result = _MISSING
    for tok in tokens:
        data = call(ctx, "slack_json_post", {"session": sess, "token": S(tok), "method": "chat.postMessage",
                    "payload": payload, "req_kw": reqkw})
        if is_raise(data):
            result = se_full(ctx, data); break
        if T(objget(data, "ok")):
            ts = objget(data, "ts")
            result = {"success": True, "platform": "slack", "chat_id": chat,
                      "message_id": ts if ts is not None else None}
            break
        ev = objget(data, "error")
        last_error = ev if isinstance(ev, str) else "unknown"
        if last_error not in wrong: break
    if result is _MISSING:
        result = send_error(ctx, "Slack API error: " + last_error, False)
    return result


def m_standalone_send_media(ctx, a, state):
    token = S(a.get("token")); chat = S(a.get("chat_id"))
    media = a.get("media_files") or []
    thread = a.get("thread_id"); formatted = a.get("formatted")
    caption = a.get("formatted_caption"); unfurl = a.get("unfurl_kwargs")
    im = call(ctx, "import_slack_sdk", {})
    if is_raise(im):
        if exc_family(im, "ImportError"):
            return {"error": "slack_sdk not installed. Run: pip install 'slack-sdk' (required for Slack MEDIA delivery via send_message)"}
        return im
    client = call(ctx, "async_web_client", {"token": token})
    if is_raise(client): return client
    proxy = call(ctx, "resolve_proxy_url", {})
    if is_raise(proxy): return proxy
    apr = call(ctx, "apply_slack_proxy", {"client": client, "proxy_url": proxy})
    if is_raise(apr): return apr
    cap_as_comment = T(caption) and not T(unfurl)
    if cap_as_comment: text_to_send = ""
    elif T(caption): text_to_send = caption
    elif T(formatted): text_to_send = formatted
    else: text_to_send = ""
    tstr = strip(strval(text_to_send))
    last_mid = _MISSING
    uploaded_any = False
    result = _MISSING
    warnings = []
    if tstr:
        pp = call(ctx, "standalone_post_text", {"client": client, "chat_id": chat,
                  "text": text_to_send, "unfurl_kwargs": unfurl, "thread_id": thread})
        if is_raise(pp):
            result = se_simple(ctx, pp, proxy)
        else:
            ok = objget(pp, "ok")
            if ok is not None and not T(ok):
                e = objget(pp, "error")
                e = e if isinstance(e, str) else "unknown"
                result = send_error(ctx, "Slack API error: " + e, False)
            elif "ts" in pp:
                last_mid = pp["ts"]
    caption_pending = cap_as_comment
    if result is _MISSING and isinstance(media, list):
        for row in media:
            path = S(row[0]) if isinstance(row, list) and len(row) > 0 else ""
            ex = call(ctx, "exists", {"path": path})
            if is_raise(ex): result = ex; break
            if not T(ex):
                w = "Media file not found, skipping: " + path
                wr = call(ctx, "logger.warning", {"fmt": "[Slack] %s", "msg": w})
                if is_raise(wr): result = wr; break
                warnings.append(w)
                if caption_pending:
                    fb = call(ctx, "standalone_post_text", {"client": client, "chat_id": chat,
                              "text": caption, "unfurl_kwargs": unfurl, "thread_id": thread})
                    if is_raise(fb):
                        pr = call(ctx, "resolve_proxy_url", {})
                        shown = not T(pr)
                        lr = call(ctx, "logger.warning", {"fmt": "[Slack] Caption-fallback send failed for missing media", "exc_info": shown})
                        if is_raise(lr): result = lr; break
                    else:
                        ok = objget(fb, "ok")
                        if ok is None or T(ok):
                            ts = objget(fb, "ts")
                            if T(ts):
                                last_mid = ts; caption_pending = False
                continue
            up = call(ctx, "standalone_upload_file", {"client": client, "chat_id": chat,
                      "path": path, "initial_comment": caption if caption_pending else "", "thread_id": thread})
            if is_raise(up):
                pr = call(ctx, "resolve_proxy_url", {})
                if is_raise(pr): result = pr; break
                dr = call(ctx, "describe_proxy_error", {"error": rmsg(up), "proxy": pr})
                shown = dr if isinstance(dr, str) else None
                w = "Failed to send media " + path + ": " + (shown if shown else rmsg(up))
                la = {"fmt": "[Slack] %s", "msg": w, "exc_info": (shown is None)}
                lr = call(ctx, "logger.error", la)
                if is_raise(lr): result = lr; break
                warnings.append(w)
                continue
            errv = objget(up, "error")
            if T(errv):
                warnings.append("Failed to send media " + path + ": " + S(errv))
                continue
            uploaded_any = True; caption_pending = False
            mid = objget(up, "message_id")
            if T(mid): last_mid = mid
    if result is _MISSING:
        ts2 = strip(strval(text_to_send))
        notext = not ts2
        if last_mid is _MISSING and not uploaded_any and notext:
            result = {"error": "No deliverable text or media remained after processing"}
        else:
            result = {"success": True, "platform": "slack", "chat_id": chat,
                      "message_id": (None if last_mid is _MISSING else last_mid)}
        if warnings: result["warnings"] = warnings
    return result


def m_synthetic_reaction_event(ctx, a, state):
    event = a.get("event") or {}
    action = S(a.get("action")); thread = S(a.get("thread_ts")); team = S(a.get("team_id"))
    emap = a.get("emoji_map")
    item = objget(event, "item")
    channel = S(objget(item, "channel")); msgts = S(objget(item, "ts"))
    rname = S(objget(event, "reaction")); uid = objget(event, "user")
    mapped = objget(emap, rname)
    emoji = mapped if isinstance(mapped, str) else rname
    ets = objget(event, "event_ts")
    ts = ets if T(ets) else "reaction-" + msgts + "-" + rname + "-" + strval(uid)
    syn = {"type": "message", "user": (uid if uid is not None else None),
           "text": "reaction:" + action + ":" + emoji, "channel": channel, "ts": ts,
           "thread_ts": thread, "_hermes_force_process": True,
           "_hermes_reaction": {"name": rname, "action": action, "reacted_to_ts": msgts,
                                "event_ts": (ets if ets is not None else None)}}
    if team: syn["team"] = team
    tr = call(ctx, "reaction_trigger_target", {})
    if is_raise(tr): return tr
    tchan = tr[0] if isinstance(tr, list) and len(tr) > 0 and isinstance(tr[0], str) else None
    tthr = tr[1] if isinstance(tr, list) and len(tr) > 1 and isinstance(tr[1], str) else None
    if tchan:
        syn["channel"] = tchan
        syn["channel_type"] = "im" if tchan[:1] == "D" else "channel"
        syn["_hermes_reaction_source_channel"] = channel
        if tthr: syn["thread_ts"] = tthr
        else:
            syn.pop("thread_ts", None)
            syn["_hermes_no_thread_response"] = True
    return syn


def m_thread_watermark_io(ctx, a, state):
    method = S(a.get("method")); channel = S(a.get("channel_id"))
    thread = S(a.get("thread_ts")); user = S(a.get("user_id")); team = S(a.get("team_id"))
    extra = a.get("args")
    sr = call(ctx, "get_session_store", {})
    if is_raise(sr): return sr
    has_method = T(sr) and isinstance(sr, dict) and method in sr
    if not has_method: return None
    kr = call(ctx, "build_thread_session_key", {"channel_id": channel, "thread_ts": thread,
              "user_id": user, "team_id": team})
    if is_raise(kr): return kr
    skey = S(kr)
    if not skey: return None
    mkey = "slack_thread_watermark:" + channel + ":" + thread
    return call(ctx, "session_store_method", {"method": method, "session_key": skey,
                "meta_key": mkey, "args": extra})


MODELS = {
    "_serialize_slack_blocks_for_agent": m_serialize_slack_blocks_for_agent,
    "_set_assistant_suggested_prompts": m_set_assistant_suggested_prompts,
    "_set_assistant_thread_title": m_set_assistant_thread_title,
    "_should_wake_on_unmentioned_message": m_should_wake_on_unmentioned_message,
    "_slack_api_human_users": m_slack_api_human_users,
    "_slack_json_post": m_slack_json_post,
    "_slack_mention_patterns": m_slack_mention_patterns,
    "_slack_message_addressed_to_other_user": m_slack_message_addressed_to_other_user,
    "_slack_message_mentions_self": m_slack_message_mentions_self,
    "_slack_per_request_proxy_middleware": m_slack_per_request_proxy_middleware,
    "_socket_transport_connected": m_socket_transport_connected,
    "_socket_watchdog_loop": m_socket_watchdog_loop,
    "_standalone_send": m_standalone_send,
    "_standalone_send_media": m_standalone_send_media,
    "_synthetic_reaction_event": m_synthetic_reaction_event,
    "_thread_watermark_io": m_thread_watermark_io,
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
    csrc = open(os.path.join(_REPO, "src", "gateway", "platforms", "slack_adapter_flow14.c"),
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow14: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if '"' + n + '"' not in csrc]
    if missing_c:
        print("FATAL oracle_slack_adapter_flow14: seams absent from C port: " + str(missing_c))
        sys.exit(2)
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow14.jsonl")
    if os.path.exists(fix):
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print("FATAL oracle_slack_adapter_flow14: fixture io keys != SEAM_NAMES; "
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
    print("ORACLE slack_adapter_flow14: " + str(n) + " cases, " + str(m) + " mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow14.jsonl")
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
    print("SELFTEST slack_adapter_flow14: " + str(n) + " cases, " + str(bad) + " drive/script failures")
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))
    return 1 if bad or n == 0 else 0


def dump():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow14.jsonl")
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

