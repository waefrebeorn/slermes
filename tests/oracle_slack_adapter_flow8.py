#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 8
(src/gateway/platforms/slack_adapter_flow8.c).

Independent Python reference MODEL of the 16 LIVE
plugins/platforms/slack/adapter.py remainders ported by the C file.  The model
is a faithful transcription of each live function's decision logic with every
injected/unported callable replaced by a recording stub fed the SAME io scripts
the C harness feeds sa8_call.  No quarry import chain is imported.

Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace") plus the final return ("ret") and echoed state ("state").  Summary:
'ORACLE slack_adapter_flow8: N cases, M mismatches'.

Fail-closed: missing/exhausted seam scripts and missing funcs => exit 2.
BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
CancelledError) PASS THROUGH every except-Exception site; Exception-family
raises are caught exactly where the live body catches them.

Representation contract (shared with the C port):
- NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
- A raise is {"__raise__": cls, "message": str(exc), ...extra}.
- io values: a plain JSON value, a LIST consumed in order, or
  {"__script__":true,"ret":v} / {"__script__":true,"raise":{...}}.
- Seam-name SET EQUALITY across C source / this file / fixture io keys.
"""
import asyncio
import copy
import json
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Seam names MUST match the C port and the fixture io-key union exactly (SET EQUALITY).
SEAM_NAMES = [
    "logger.debug", "logger.info", "logger.warning",
    "task.done", "task.cancel", "asyncio.wait", "task.cancelled", "task.exception",
    "app_client", "close", "aclose", "await_close",
    "attachment_label", "getattr_response", "describe_api_error",
    "is_http_status_error", "status_code", "http_template", "format_template", "str_exc",
    "metadata_team_id", "get_client", "ensure_dm_conversation",
    "stop_typing",
    "client_fn", "method_call", "is_block_rejection",
    "pop_stream", "seal_stream", "maybe_blocks", "edit_message", "send_result",
    "extra_flag",
    "allowed_channels", "ignore_other_user_mentions", "mentions_self",
    "addressed_to_other_user", "thread_require_mention", "is_free_channel",
    "strict_mention", "should_wake",
    "download_file", "resolve_audio_ext", "download_bytes", "cache_video", "cache_document",
    "files_with_forwarded", "resolve_stub", "file_kind", "cache_slack_file",
    "inline_document_text", "describe_failure", "note_failure",
    "conversations_replies", "sleep",
    "thread_cache_key", "thread_root_message",
    "is_safe_url", "is_slack_cdn_url", "resolve_download_token", "client_get",
    "redirect_target", "raise_for_status",
    "cache_media",
]

TARGETS = [
    "_cancel_socket_tasks", "_call_with_block_fallback", "_cancel_socket_tasks",
]
# de-dup, keep first
TARGETS = list(dict.fromkeys(TARGETS))
TARGETS = [
    "_cancel_socket_tasks", "_call_with_block_fallback", "_channel_gate_allows",
    "_clear_thread_status_quietly", "_client_for", "_close_workspace_clients",
    "_collect_inbound_media", "_collect_thread_root_images", "_commit_stream",
    "_conversations_replies_with_backoff", "_describe_slack_download_failure",
    "_dm_target", "_dm_top_level_threads_as_sessions", "_download_slack_file",
    "_download_slack_file_bytes", "_cache_slack_file",
]

# ── exception table (mirrors C sa8) ─────────────────────────────────────────
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
    # synthetic HTTP/transport kinds used by the download flow (Exception family)
    "TimeoutException": Exception, "HTTPStatusError": Exception,
}


def canon(v):
    return json.loads(json.dumps(v, ensure_ascii=False, default=str))


def T(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, tuple, set, dict)):
        return len(v) > 0
    return False


def norm_raise(spec):
    d = {"__raise__": spec.get("type", "RuntimeError"),
         "message": spec.get("message", "")}
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


_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        args = canon(args)
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            spec = {"type": "RuntimeError", "message": "no script for " + name}
            rec = norm_raise(spec)
            self.trace.append({"name": name, "args": args, "ret": rec})
            raise make_exc(spec)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                spec = {"type": "RuntimeError", "message": "script exhausted for " + name}
                rec = norm_raise(spec)
                self.trace.append({"name": name, "args": args, "ret": rec})
                raise make_exc(spec)
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                spec = used["raise"]
                rec = norm_raise(spec)
                self.trace.append({"name": name, "args": args, "ret": rec})
                raise make_exc(spec)
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def spec_of(exc):
    return getattr(exc, "spec", {"__raise__": type(exc).__name__, "message": str(exc)})


# ── faithful decision models ────────────────────────────────────────────────
def m_cancel_socket_tasks(ctx, a, state):
    tasks = a.get("tasks") or []
    live = []
    for t in tasks:
        if t is None:
            continue
        if not T(t.get("has_cancel", False)):
            continue
        if T(t.get("has_done", False)):
            if T(ctx.call("task.done", {"tag": t.get("tag")})):
                continue
        live.append(t)
    for t in live:
        ctx.call("task.cancel", {"tag": t.get("tag")})
    if not live:
        return None
    w = ctx.call("asyncio.wait", {"tags": [t.get("tag") for t in live], "timeout": 3.0})
    done = (w or {}).get("done") or []
    still = (w or {}).get("still") or []
    for tag in done:
        if T(ctx.call("task.cancelled", {"tag": tag})):
            continue
        ex = ctx.call("task.exception", {"tag": tag})
        if ex is not None:
            ctx.call("logger.debug", {"tag": tag})
    if still:
        ctx.call("logger.warning", {"count": len(still), "timeout": 3.0})
    return None


def m_close_workspace_clients(ctx, a, state):
    primary = ctx.call("app_client", {})
    clients = []
    if primary is not None:
        clients.append(primary)
    clients += list(state.get("_team_clients") or [])
    seen = []
    for c in clients:
        ident = c.get("ident")
        if ident in seen:
            continue
        seen.append(ident)
        methods = c.get("methods") or []
        for mname in ("close", "aclose"):
            if mname not in methods:
                continue
            r = ctx.call(mname, {"ident": ident})
            if isinstance(r, dict) and T(r.get("__await__")):
                ctx.call("await_close", {"ident": ident})
            break
    return None


def m_describe_slack_download_failure(ctx, a, state):
    file_obj = a.get("file_obj")
    exc = a.get("exc") or {}
    file_label = ctx.call("attachment_label", {"file_obj": file_obj})
    response = ctx.call("getattr_response", {"exc": exc})
    api_detail = ctx.call("describe_api_error", {"response": response, "file_obj": file_obj})
    if T(api_detail):
        return api_detail
    if T(ctx.call("is_http_status_error", {"exc": exc})):
        status = ctx.call("status_code", {"exc": exc})
        tmpl = ctx.call("http_template", {"status": status})
        if T(tmpl):
            return ctx.call("format_template", {"template": tmpl, "file_label": file_label})
    message = ctx.call("str_exc", {"exc": exc})
    msg = message if isinstance(message, str) else str(message)
    if "Slack returned HTML instead of media" in msg or "non-image data" in msg:
        return ("Slack attachment access failed for " + str(file_label) +
                ": Slack returned an HTML/login or non-media response. "
                "This usually means a scope, auth, or file-permission problem.")
    return None


def m_client_for(ctx, a, state):
    tid = ctx.call("metadata_team_id", {"metadata": a.get("metadata")})
    return ctx.call("get_client", {"chat_id": a.get("chat_id"), "team_id": tid})


def m_dm_target(ctx, a, state):
    tid = ctx.call("metadata_team_id", {"metadata": a.get("metadata")})
    return ctx.call("ensure_dm_conversation", {"chat_id": a.get("chat_id"), "team_id": tid})


def m_clear_thread_status_quietly(ctx, a, state):
    try:
        ctx.call("stop_typing", {"chat_id": a.get("chat_id"), "metadata": a.get("metadata")})
    except Exception as e:
        ctx.call("logger.debug", {"message": str(e)})
    return None


def m_call_with_block_fallback(ctx, a, state):
    kwargs = a.get("kwargs") or {}
    verb = a.get("verb", "")
    method = a.get("method", "")
    ctx.call("client_fn", {})
    try:
        return ctx.call("method_call", {"method": method, "kwargs": kwargs})
    except Exception as exc:
        blocks = kwargs.get("blocks")
        if T(blocks) and T(ctx.call("is_block_rejection", {"exc": spec_of(exc)})):
            retry = dict(kwargs)
            if verb == "edit":
                retry["blocks"] = []
            else:
                retry.pop("blocks", None)
            ctx.call("logger.info", {"verb": verb})
            ctx.call("client_fn", {})
            return ctx.call("method_call", {"method": method, "kwargs": retry})
        raise


def m_commit_stream(ctx, a, state):
    key = a.get("key") or []
    stream = a.get("stream") or {}
    text = a.get("text", "")
    delta = a.get("delta", "")
    replace = T(a.get("replace", False))
    metadata = a.get("metadata")
    chat_id = key[1] if len(key) > 1 else None
    ts = stream.get("ts")
    ctx.call("pop_stream", {"key": key})
    sealed = ctx.call("seal_stream", {"ts": ts, "delta": delta})
    if not T(sealed) and not (delta and str(delta).strip()):
        sealed = ctx.call("seal_stream", {"ts": ts, "delta": ""})
    if T(sealed) and not replace and not T(ctx.call("maybe_blocks", {"text": text})):
        committed = True
    else:
        base = stream.get("base", 0)
        shown = text if replace else str(text)[int(base):]
        edited = ctx.call("edit_message", {"chat_id": chat_id, "ts": ts, "shown": shown})
        esuccess = T(edited.get("success")) if isinstance(edited, dict) else T(edited)
        committed = esuccess or (T(sealed) and not replace)
        if not T(sealed):
            ctx.call("logger.warning", {"chat_id": chat_id, "ts": ts})
    if not committed:
        return None
    ctx.call("stop_typing", {"chat_id": chat_id})
    return ctx.call("send_result", {"success": True, "message_id": ts})


def m_dm_top_level_threads_as_sessions(ctx, a, state):
    return ctx.call("extra_flag", {"key": "dm_top_level_threads_as_sessions", "default": True})


def m_channel_gate_allows(ctx, a, state):
    channel_id = a.get("channel_id", "")
    routing_text = a.get("routing_text", "")
    bot_uid = a.get("bot_uid", "")
    mentioned = T(a.get("is_mentioned", False))
    thread_reply = T(a.get("is_thread_reply", False))
    user_id = a.get("user_id", "")
    team_id = a.get("team_id", "")
    is_dm = T(a.get("is_dm", False))
    force = T(a.get("force_process", False))
    allowed = ctx.call("allowed_channels", {})
    if T(allowed) and channel_id not in (allowed or []):
        return False
    bot_user_id = state.get("_bot_user_id")
    self_uids = [u for u in (bot_uid, bot_user_id) if u]
    if (T(ctx.call("ignore_other_user_mentions", {})) and not mentioned
            and not T(ctx.call("mentions_self", {"text": routing_text, "uids": self_uids}))
            and T(ctx.call("addressed_to_other_user", {"text": routing_text, "uids": self_uids}))):
        return False
    thread_gated = T(ctx.call("thread_require_mention", {})) and thread_reply and not mentioned
    if force:
        return True
    free = T(ctx.call("is_free_channel", {"channel_id": channel_id}))
    if not free and T(ctx.call("strict_mention", {})) and not mentioned:
        return False
    if thread_gated:
        ctx.call("logger.debug", {"channel_id": channel_id})
        return False
    if free:
        return True
    if not mentioned:
        return ctx.call("should_wake", {"channel_id": channel_id, "user_id": user_id,
                                        "team_id": team_id, "is_thread_reply": thread_reply,
                                        "chat_type": "dm" if is_dm else "group"})
    return True


def m_cache_slack_file(ctx, a, state):
    kind = a.get("kind", "")
    f = a.get("f") or {}
    url = a.get("url", "")
    mimetype = a.get("mimetype", "")
    team_id = a.get("team_id", "")
    if kind == "image":
        ext = "." + str(mimetype).split("/")[-1].split(";")[0]
        if ext not in (".jpg", ".jpeg", ".png", ".gif", ".webp"):
            ext = ".jpg"
        cached = ctx.call("download_file", {"url": url, "ext": ext, "team_id": team_id})
        return [cached, mimetype, ""]
    if kind in ("audio", "voice clip"):
        ext = ctx.call("resolve_audio_ext", {"f": f, "mimetype": mimetype})
        cached = ctx.call("download_file", {"url": url, "ext": ext, "audio": True,
                                            "team_id": team_id})
        if kind == "audio":
            return [cached, mimetype, ""]
        ctx.call("logger.debug", {"mimetype": mimetype, "cached": cached})
        amap = state.get("_SLACK_EXT_TO_AUDIO_MIME") or {}
        return [cached, amap.get(ext, "audio/mp4"), ""]
    if kind == "video":
        name = f.get("name", "")
        ext = os.path.splitext(name)[1].lower()
        vtypes = state.get("SUPPORTED_VIDEO_TYPES") or {}
        if ext not in vtypes:
            mime_to_ext = {v: k for k, v in vtypes.items()}
            ext = mime_to_ext.get(str(mimetype).split(";", 1)[0].lower(), ".mp4")
        raw = ctx.call("download_bytes", {"url": url, "team_id": team_id})
        cached = ctx.call("cache_video", {"raw_bytes": raw, "ext": ext})
        ctx.call("logger.debug", {"cached": cached})
        return [cached, vtypes.get(ext, mimetype or "video/mp4"), ""]
    return ctx.call("cache_document", {"f": f, "url": url, "mimetype": mimetype,
                                       "team_id": team_id})


def m_collect_inbound_media(ctx, a, state):
    event = a.get("event") or {}
    text = a.get("text", "")
    urls = list(a.get("thread_root_media_urls") or [])
    types = list(a.get("thread_root_media_types") or [])
    inlined = [False] * len(urls)
    notices = []
    files = ctx.call("files_with_forwarded", {"files": event.get("files"),
                                              "attachments": event.get("attachments")}) or []
    for f in files:
        if f.get("file_access") == "check_file_info":
            f = ctx.call("resolve_stub", {"f": f})
            if f is None:
                continue
        mimetype = f.get("mimetype", "unknown")
        url = f.get("url_private_download") or f.get("url_private", "")
        if not url:
            continue
        kind = ctx.call("file_kind", {"f": f, "mimetype": mimetype})
        try:
            cached = ctx.call("cache_slack_file", {"kind": kind, "f": f, "url": url,
                                                   "mimetype": mimetype})
            if cached is None:
                continue
            cp, mt, injection = cached[0], cached[1], cached[2]
            urls.append(cp)
            types.append(mt)
            ir = ctx.call("inline_document_text", {"text": text, "injection": injection})
            text = ir.get("text") if isinstance(ir, dict) else text
            iv = ir.get("inlined", [False])[0] if isinstance(ir, dict) else False
            inlined.append(iv)
        except Exception as e:
            diag = ctx.call("describe_failure", {"exc": spec_of(e), "file_obj": f})
            ctx.call("note_failure", {"notice": diag})
            notices.append(diag)
    if notices:
        block = "[Slack attachment notice]\n" + "\n".join("- " + str(n) for n in notices)
        text = (block + "\n\n" + text) if text else block
    return [urls, types, inlined, text]


def m_conversations_replies_with_backoff(ctx, a, state):
    channel_id = a.get("channel_id", "")
    thread_ts = a.get("thread_ts", "")
    limit = a.get("limit", 200)
    team_id = a.get("team_id", "")
    ctx.call("get_client", {"channel_id": channel_id, "team_id": team_id})
    for attempt in range(3):
        try:
            return ctx.call("conversations_replies", {"channel": channel_id, "ts": thread_ts,
                                                       "limit": limit, "inclusive": True})
        except Exception as exc:
            err = spec_of(exc)
            err_str = str(err.get("message", "")).lower()
            is_rate = ("ratelimited" in err_str or "429" in err_str
                       or "rate_limited" in err_str)
            if is_rate and attempt < 2:
                ctx.call("sleep", {"seconds": 1.0 * (2 ** attempt)})
                continue
            raise
    return None


def m_collect_thread_root_images(ctx, a, state):
    channel_id = a.get("channel_id", "")
    thread_ts = a.get("thread_ts", "")
    team_id = a.get("team_id", "")
    urls, types = [], []
    try:
        key = ctx.call("thread_cache_key", {"channel_id": channel_id, "thread_ts": thread_ts,
                                            "team_id": team_id})
        cache = state.get("_thread_context_cache") or {}
        cached = cache.get(str(key))
        root = ctx.call("thread_root_message", {"messages": (cached or {}).get("messages") or [],
                                                "thread_ts": thread_ts}) if cached else None
        files = ctx.call("files_with_forwarded", {"files": (root or {}).get("files"),
                                                  "attachments": (root or {}).get("attachments")}) if root else []
        if not files:
            return [urls, types]
        maximg = state.get("_THREAD_ROOT_IMAGE_MAX", 4)
        for f in files:
            if len(urls) >= maximg:
                break
            if not isinstance(f, dict):
                continue
            if f.get("file_access") == "check_file_info":
                f = ctx.call("resolve_stub", {"f": f})
                if f is None:
                    continue
            mimetype = str(f.get("mimetype") or "")
            url = f.get("url_private_download") or f.get("url_private", "")
            if not mimetype.startswith("image/") or not url:
                continue
            try:
                cached_file = ctx.call("cache_slack_file", {"kind": "image", "f": f, "url": url,
                                                            "mimetype": mimetype,
                                                            "team_id": team_id})
                urls.append(cached_file[0])
                types.append(cached_file[1])
            except Exception:
                ctx.call("logger.warning", {"id": f.get("id") or f.get("name") or "unknown"})
    except Exception:
        ctx.call("logger.debug", {"stage": "thread_root_recovery"})
    return [urls, types]


def m_download_slack_file_bytes(ctx, a, state):
    url = a.get("url", "")
    team_id = a.get("team_id", "")
    html_label = a.get("html_label", "file bytes")
    if not T(ctx.call("is_safe_url", {"url": url})):
        raise make_exc({"type": "ValueError",
                        "message": "Blocked unsafe Slack file URL (SSRF protection): " + str(url)})
    if not T(ctx.call("is_slack_cdn_url", {"url": url})):
        raise make_exc({"type": "ValueError",
                        "message": "Blocked non-Slack-CDN file URL (token-exfiltration protection): " + str(url)})
    token = ctx.call("resolve_download_token", {"url": url, "team_id": team_id})
    for attempt in range(3):
        try:
            download_url = url
            rc = 0
            while True:
                resp = ctx.call("client_get", {"url": download_url, "token": token,
                                               "auth": "Bearer " + str(token)})
                status = resp.get("status_code", 200) if isinstance(resp, dict) else 200
                if status not in (301, 302, 303, 307, 308):
                    break
                target = ctx.call("redirect_target", {"response": resp})
                if not target:
                    break
                if not T(ctx.call("is_safe_url", {"url": target})):
                    raise make_exc({"type": "ValueError",
                                    "message": "Blocked unsafe Slack file redirect (SSRF protection): " + str(target)})
                if not T(ctx.call("is_slack_cdn_url", {"url": target})):
                    raise make_exc({"type": "ValueError",
                                    "message": "Blocked non-Slack-CDN file redirect (token-exfiltration protection): " + str(target)})
                if rc == 3:
                    raise make_exc({"type": "ValueError",
                                    "message": "Too many Slack file redirects (maximum 3)"})
                download_url = target
                rc += 1
            ctx.call("raise_for_status", {"status": status})
            ct = (resp.get("headers", {}) or {}).get("content-type", "") if isinstance(resp, dict) else ""
            if "text/html" in str(ct):
                raise make_exc({"type": "ValueError",
                                "message": ("Slack returned HTML instead of " + str(html_label) +
                                            " (content-type: " + str(ct) + "); check bot token scopes and file permissions, or a cross-origin redirect dropped the token (Enterprise Grid files-origin)")})
            return resp.get("content") if isinstance(resp, dict) else None
        except Exception as exc:
            err = spec_of(exc)
            kind = err.get("__raise__")
            if kind == "HTTPStatusError" and int(err.get("status_code", 600)) < 429:
                raise
            if kind not in ("TimeoutException", "HTTPStatusError"):
                raise
            if attempt < 2:
                ctx.call("sleep", {"seconds": 1.5 * (attempt + 1)})
                continue
            raise


def m_download_slack_file(ctx, a, state):
    url = a.get("url", "")
    ext = a.get("ext", "")
    audio = T(a.get("audio", False))
    team_id = a.get("team_id", "")
    data = ctx.call("download_bytes", {"url": url, "team_id": team_id, "html_label": "media"})
    return ctx.call("cache_media", {"data": data, "ext": ext, "audio": audio})


MODELS = {
    "_cancel_socket_tasks": m_cancel_socket_tasks,
    "_call_with_block_fallback": m_call_with_block_fallback,
    "_channel_gate_allows": m_channel_gate_allows,
    "_clear_thread_status_quietly": m_clear_thread_status_quietly,
    "_client_for": m_client_for,
    "_close_workspace_clients": m_close_workspace_clients,
    "_collect_inbound_media": m_collect_inbound_media,
    "_collect_thread_root_images": m_collect_thread_root_images,
    "_commit_stream": m_commit_stream,
    "_conversations_replies_with_backoff": m_conversations_replies_with_backoff,
    "_describe_slack_download_failure": m_describe_slack_download_failure,
    "_dm_target": m_dm_target,
    "_dm_top_level_threads_as_sessions": m_dm_top_level_threads_as_sessions,
    "_download_slack_file": m_download_slack_file,
    "_download_slack_file_bytes": m_download_slack_file_bytes,
    "_cache_slack_file": m_cache_slack_file,
}


def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    state_out = copy.deepcopy(state)
    try:
        ret = MODELS[func](ctx, args, state_out)
    except BaseException as exc:
        ret = spec_of(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── seam-name cross-check (SET EQUALITY across C / oracle / fixtures) ────────
def cross_check():
    csrc = open(os.path.join(_REPO, "src", "gateway", "platforms", "slack_adapter_flow8.c"),
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow8: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if '"' + n + '"' not in csrc]
    if missing_c:
        print("FATAL oracle_slack_adapter_flow8: seams absent from C port: " + str(missing_c))
        sys.exit(2)
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow8.jsonl")
    if os.path.exists(fix):
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print("FATAL oracle_slack_adapter_flow8: fixture io keys != SEAM_NAMES; "
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
        print("  ret got =" + json.dumps(got.get("ret"), ensure_ascii=False)[:500])
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print("  trace[" + str(i) + "] want=" + json.dumps(w, ensure_ascii=False)[:500])
        print("  trace[" + str(i) + "] got =" + json.dumps(g, ensure_ascii=False)[:500])
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        print("  state want=" + json.dumps(ws, ensure_ascii=False)[:300])
        print("  state got =" + json.dumps(gs, ensure_ascii=False)[:300])


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
    print("ORACLE slack_adapter_flow8: " + str(n) + " cases, " + str(m) + " mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow8.jsonl")
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
    print("SELFTEST slack_adapter_flow8: " + str(n) + " cases, " + str(bad) + " drive/script failures")
    for f in sorted(counts):
        print("  " + f + ": " + str(counts[f]))
    return 1 if bad or n == 0 else 0


def dump():
    fix = os.path.join(_REPO, "tests", "fixtures", "slack_adapter_flow8.jsonl")
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
