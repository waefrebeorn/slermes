#!/usr/bin/env python3
"""Generate tests/fixtures/google_chat_flow2.jsonl for google_chat_flow2.c.

Case shape: {"func": <live name>, "in": {"args": {...}, "state": {...},
"io": {seam-name: script}}}. A script is a plain JSON value (constant), a
LIST consumed one element per call (wrap list/tuple RESULTS one level deeper),
or {"__script__": true, "ret": v} / {"__script__": true, "raise": {type,...}}.
Missing scripts are fail-closed RuntimeErrors on both sides.

Every ported function gets >= 6 cases; error paths, BaseException propagation
and fail-closed scripts are covered per flow. ASCII corpus; integral numbers.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "google_chat_flow2.jsonl")
CASES = []


def add(func, args=None, state=None, io=None, fc=False):
    c = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": io or {}}}
    if fc:
        c["fc"] = True
    CASES.append(c)


def rs(kind, msg, **extra):
    r = {"type": kind, "message": msg}
    r.update(extra)
    return {"__script__": True, "raise": r}


def ret(v):
    return {"__script__": True, "ret": v}


def obj(attrs):
    return {"__obj__": attrs}


def sr(success=True, mid=None, error=None, **kw):
    d = {"success": success}
    if mid is not None:
        d["message_id"] = mid
    if error is not None:
        d["error"] = error
    d.update(kw)
    return d


# ── format_message ──
add("format_message", {"content": "**hi**"}, io={"_format_message": "*hi*"})
add("format_message", {"content": ""}, io={"_format_message": ""})
add("format_message", {"content": "a\nb"}, io={"_format_message": "a\nb"})
add("format_message", {"content": "<x>"}, io={"_format_message": "&lt;x&gt;"})
add("format_message", {"content": "x"}, io={"_format_message": rs("RuntimeError", "fmt boom")})
add("format_message", {"content": "x"}, io={"_format_message": rs("SystemExit", "bye")})
add("format_message", {"content": "x"}, fc=True)

# ── _verify_google_id_token ──
VIOK = {"import.google.oauth2": None, "_get_google_id_token_request": "REQ",
        "id_token.verify_oauth2_token": {"sub": "1"}}
add("_verify_google_id_token", {"token": "tok", "audience": "aud"}, io=VIOK)
add("_verify_google_id_token", {"token": "tok", "audience": "aud"},
    io={"import.google.oauth2": rs("ImportError", "no google")})
add("_verify_google_id_token", {"token": "tok", "audience": "aud"},
    io={"import.google.oauth2": None, "_get_google_id_token_request": rs("RuntimeError", "req boom")})
add("_verify_google_id_token", {"token": "tok", "audience": "aud"},
    io={"import.google.oauth2": None, "_get_google_id_token_request": "REQ",
        "id_token.verify_oauth2_token": rs("ValueError", "bad token")})
add("_verify_google_id_token", {"token": "tok", "audience": "aud"},
    io={"import.google.oauth2": None, "_get_google_id_token_request": "REQ",
        "id_token.verify_oauth2_token": rs("KeyboardInterrupt", "stop")})
add("_verify_google_id_token", {"token": "tok", "audience": "aud"}, fc=True)

# ── _get_google_id_token_request ──
add("_get_google_id_token_request", state={"_google_id_token_request": "CACHED"})
add("_get_google_id_token_request",
    io={"import.google.auth.transport": None, "google_requests.Request": "R",
        "_CachedGoogleAuthRequest": "NEW"})
add("_get_google_id_token_request",
    state={"_google_id_token_request": None},
    io={"import.google.auth.transport": rs("ImportError", "no module")})
add("_get_google_id_token_request",
    io={"import.google.auth.transport": None, "google_requests.Request": rs("OSError", "x", errno=5)})
add("_get_google_id_token_request",
    io={"import.google.auth.transport": None, "google_requests.Request": "R",
        "_CachedGoogleAuthRequest": rs("RuntimeError", "ctor")})
add("_get_google_id_token_request",
    io={"import.google.auth.transport": rs("SystemExit", "bye")})
add("_get_google_id_token_request", fc=True)

# ── _load_google_modules ──
LM_IO = {
    "importlib.import_module": ["m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7"],
    "getattr": ["A", "B", "C", "D"],
}
add("_load_google_modules", io=LM_IO)
add("_load_google_modules", state={"_google_modules_loaded": True, "GOOGLE_CHAT_AVAILABLE": True})
add("_load_google_modules", state={"_google_modules_loaded": True, "GOOGLE_CHAT_AVAILABLE": False})
add("_load_google_modules", io={"importlib.import_module": rs("ImportError", "missing dep")})
add("_load_google_modules", io={"importlib.import_module": ["m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7"],
                                "getattr": rs("AttributeError", "no attr")})
add("_load_google_modules", io={"importlib.import_module": rs("SystemExit", "bye")})
add("_load_google_modules", io={"importlib.import_module": rs("RuntimeError", "boom")})
add("_load_google_modules", fc=True)

# ── send_animation ──
add("send_animation", {"chat_id": "c", "animation_url": "u"},
    io={"send_image": sr(mid="m1")})
add("send_animation", {"chat_id": "c", "animation_url": "u", "caption": "cap", "reply_to": "r", "metadata": {"k": 1}},
    io={"send_image": sr(mid="m2")})
add("send_animation", {"chat_id": "c", "animation_url": "u"},
    io={"send_image": {"success": False, "error": "no"}})
add("send_animation", {"chat_id": "c", "animation_url": "u"},
    io={"send_image": rs("RuntimeError", "send boom")})
add("send_animation", {"chat_id": "c", "animation_url": "u"},
    io={"send_image": rs("SystemExit", "bye")})
add("send_animation", {"chat_id": "c", "animation_url": "u"}, fc=True)

# ── file-reply family ──
for fn, kw in (("send_document", {"mime_hint": None}),
               ("send_image_file", {}), ("send_video", {}), ("send_voice", {})):
    base_args = {"chat_id": "c", "path": "/p", "caption": "cap", "reply_to": "r", "kwargs": {"x": 1}}
    base_args.update(kw)
    add(fn, base_args, io={"_send_file_reply": sr(mid="m")})
    add(fn, dict(base_args, kwargs={}), io={"_send_file_reply": sr(success=False, error="no")})
    add(fn, base_args, io={"_send_file_reply": rs("RuntimeError", "boom")})
    add(fn, base_args, io={"_send_file_reply": rs("SystemExit", "bye")})
    add(fn, base_args, fc=True)
    add(fn, dict(base_args, caption=None, kwargs={}),
        io={"_send_file_reply": sr(mid="m3")})

# ── _send_file_reply ──
SFR = {"chat_id": "c", "path": "/p", "caption": "cap", "reply_to": "r",
       "kwargs": {"metadata": {"thread": "t"}}, "mime_hint": "image/*",
       "override_filename": "n"}
add("_send_file_reply", SFR,
    io={"_resolve_thread_id": "thr", "_send_file": sr(mid="m")})
add("_send_file_reply", dict(SFR, kwargs={}),
    io={"_resolve_thread_id": None, "_send_file": sr(mid="m2")})
add("_send_file_reply", SFR, io={"_resolve_thread_id": rs("RuntimeError", "resolve")})
add("_send_file_reply", SFR, io={"_resolve_thread_id": "thr",
                                 "_send_file": rs("RuntimeError", "send")})
add("_send_file_reply", SFR, io={"_resolve_thread_id": rs("SystemExit", "bye")})
add("_send_file_reply", SFR, fc=True)

# ── _new_authed_http ──
add("_new_authed_http", state={"_credentials": "CRED"},
    io={"httplib2.Http": "HTTP", "AuthorizedHttp": "AUTH"})
add("_new_authed_http", state={"_credentials": None},
    io={"httplib2.Http": "HTTP", "AuthorizedHttp": "AUTH2"})
add("_new_authed_http", io={"httplib2.Http": rs("OSError", "x", errno=5)})
add("_new_authed_http", io={"httplib2.Http": "HTTP",
                            "AuthorizedHttp": rs("RuntimeError", "auth")})
add("_new_authed_http", io={"httplib2.Http": rs("SystemExit", "bye")})
add("_new_authed_http", fc=True)

# ── _handle_setup_files_command ──
SF = {"chat_id": "c", "thread_id": "t", "raw_text": "/setup-files", "sender_email": "e@x"}
add("_handle_setup_files_command", SF,
    io={"import.setup_files": None, "handle_setup_files_command": True})
add("_handle_setup_files_command", dict(SF, sender_email=None),
    io={"import.setup_files": None, "handle_setup_files_command": False})
add("_handle_setup_files_command", SF, io={"import.setup_files": rs("ImportError", "missing")})
add("_handle_setup_files_command", SF,
    io={"import.setup_files": None, "handle_setup_files_command": rs("RuntimeError", "boom")})
add("_handle_setup_files_command", SF,
    io={"import.setup_files": None, "handle_setup_files_command": rs("SystemExit", "bye")})
add("_handle_setup_files_command", SF, fc=True)

# ── _refresh_or_none ──
add("_refresh_or_none", {"creds": "C", "email": "e@x", "log_msg": "refresh failed"},
    io={"import.oauth": None, "asyncio.to_thread": "FRESH"})
add("_refresh_or_none", {"creds": "C", "email": None, "log_msg": "refresh failed"},
    io={"import.oauth": None, "asyncio.to_thread": None})
add("_refresh_or_none", {"creds": "C", "email": "e@x", "log_msg": "m"},
    io={"import.oauth": None, "asyncio.to_thread": rs("RuntimeError", "refresh"),
        "_proxy_error": False, "logger.debug": None})
add("_refresh_or_none", {"creds": "C", "email": "e@x", "log_msg": "m"},
    io={"import.oauth": None, "asyncio.to_thread": rs("RuntimeError", "refresh"),
        "_proxy_error": True, "logger.debug": None})
add("_refresh_or_none", {"creds": "C", "email": "e@x", "log_msg": "m"},
    io={"import.oauth": None, "asyncio.to_thread": rs("SystemExit", "bye")})
add("_refresh_or_none", {"creds": "C", "email": "e@x", "log_msg": "m"},
    io={"import.oauth": rs("ImportError", "no oauth")})
add("_refresh_or_none", {"creds": "C", "email": "e@x", "log_msg": "m"}, fc=True)

# ── _patch_quietly ──
add("_patch_quietly", {"message_name": "spaces/A/messages/M", "text": "hi", "log_msg": "patch failed", "log_args": ["a"]},
    io={"_patch_message": sr(mid="M")})
add("_patch_quietly", {"message_name": "M", "text": "hi", "log_msg": "m", "log_args": []},
    io={"_patch_message": rs("RuntimeError", "patch") , "_proxy_error": False, "logger.debug": None})
add("_patch_quietly", {"message_name": "M", "text": "hi", "log_msg": "m", "log_args": ["x", 2]},
    io={"_patch_message": rs("RuntimeError", "patch"), "_proxy_error": True, "logger.debug": None})
add("_patch_quietly", {"message_name": "M", "text": "hi", "log_msg": "m", "log_args": []},
    io={"_patch_message": rs("SystemExit", "bye")})
add("_patch_quietly", {"message_name": "M", "text": "hi", "log_msg": "m", "log_args": []}, fc=True)
add("_patch_quietly", {"message_name": "M", "text": "", "log_msg": "m", "log_args": []},
    io={"_patch_message": sr(mid="M")})

# ── _on_pubsub_message ──
add("_on_pubsub_message", {"message": {"id": "1"}},
    io={"contextvars.copy_context": "CTX", "context.run": None})
add("_on_pubsub_message", {"message": {"id": "2"}}, state={"_scope_ctx": "S"},
    io={"scope_ctx.copy": "CTX", "context.run": None})
add("_on_pubsub_message", {"message": {"id": "3"}},
    io={"contextvars.copy_context": "CTX", "context.run": rs("RuntimeError", "run")})
add("_on_pubsub_message", {"message": {"id": "4"}},
    io={"contextvars.copy_context": rs("RuntimeError", "ctx")})
add("_on_pubsub_message", {"message": {"id": "5"}},
    io={"contextvars.copy_context": rs("SystemExit", "bye")})
add("_on_pubsub_message", {"message": {"id": "6"}}, fc=True)

# ── _post_attachment_fallback ──
TIO = {
    "t": ["H", "E", "A", "P"],
    "warning_text": "BODY",
    "_thread_body": "TB",
    "_create_message": sr(mid="M"),
}
PF = {"chat_id": "c", "path": "/p", "filename": "f.bin", "caption": "cap", "thread_id": "t"}
add("_post_attachment_fallback", PF, io=TIO)
add("_post_attachment_fallback", dict(PF, caption=None), io=TIO)
add("_post_attachment_fallback", PF,
    io={"t": ["H", "E", "A", "P"], "warning_text": ""})
add("_post_attachment_fallback", PF,
    io={"t": ["H", "E", "A", "P"], "warning_text": "BODY", "_thread_body": rs("RuntimeError", "tb"),
        "_proxy_error": False, "logger.debug": None})
add("_post_attachment_fallback", PF,
    io={"t": ["H", "E", "A", "P"], "warning_text": "BODY", "_thread_body": "TB",
        "_create_message": rs("RuntimeError", "send"), "_proxy_error": True, "logger.debug": None})
add("_post_attachment_fallback", PF, io={"t": rs("SystemExit", "bye")})
add("_post_attachment_fallback", PF, fc=True)

# ── _consume_typing_card_with_text ──
add("_consume_typing_card_with_text", {"chat_id": "c", "text": "hi"},
    state={"_typing_messages": {"c": "spaces/A/messages/M"}},
    io={"_patch_message": sr(mid="M")})
add("_consume_typing_card_with_text", {"chat_id": "c", "text": "hi"},
    state={"_typing_messages": {"c": "<consumed>"}})
add("_consume_typing_card_with_text", {"chat_id": "c", "text": "hi"},
    state={"_typing_messages": {}})
add("_consume_typing_card_with_text", {"chat_id": "c", "text": "hi"},
    state={"_typing_messages": {"c": "M"}},
    io={"_patch_message": rs("HttpError", "gone"), "_http_status": 404})
add("_consume_typing_card_with_text", {"chat_id": "c", "text": "hi"},
    state={"_typing_messages": {"c": "M"}},
    io={"_patch_message": rs("HttpError", "denied"), "_http_status": 403})
add("_consume_typing_card_with_text", {"chat_id": "c", "text": "hi"},
    state={"_typing_messages": {"c": "M"}},
    io={"_patch_message": rs("SystemExit", "bye")})
add("_consume_typing_card_with_text", {"chat_id": "c", "text": "hi"},
    state={"_typing_messages": {"c": "M"}}, fc=True)

# ── _acquire_user_chat_api ──
add("_acquire_user_chat_api", {"sender_email": "e@x"},
    io={"_load_per_user_chat_api": "API"})
add("_acquire_user_chat_api", {"sender_email": "e@x"},
    io={"_load_per_user_chat_api": None})
add("_acquire_user_chat_api", {"sender_email": None},
    state={"_user_chat_api": "LEGACY", "_user_credentials": "C"},
    io={"_refresh_or_none": "FRESH"})
add("_acquire_user_chat_api", {"sender_email": None},
    state={"_user_chat_api": "LEGACY", "_user_credentials": "C"},
    io={"_refresh_or_none": None, "logger.warning": None, "_invalidate_user_creds": None})
add("_acquire_user_chat_api", {"sender_email": None},
    state={"_user_chat_api": None})
add("_acquire_user_chat_api", {"sender_email": "e@x"},
    io={"_load_per_user_chat_api": rs("SystemExit", "bye")})
add("_acquire_user_chat_api", {"sender_email": "e@x"}, fc=True)

# ── _load_per_user_chat_api ──
add("_load_per_user_chat_api", {"email": "e@x"},
    io={"import.oauth": None,
        "asyncio.to_thread": ["CREDS", "API"]})
add("_load_per_user_chat_api", {"email": "e@x"},
    io={"import.oauth": None, "asyncio.to_thread": [None]})
add("_load_per_user_chat_api", {"email": "e@x"},
    state={"_user_chat_api_by_email": {"e@x": "API"}, "_user_creds_by_email": {"e@x": "C"}},
    io={"import.oauth": None, "_refresh_or_none": "FRESH"})
add("_load_per_user_chat_api", {"email": "e@x"},
    state={"_user_chat_api_by_email": {"e@x": "API"}, "_user_creds_by_email": {"e@x": "C"}},
    io={"import.oauth": None, "_refresh_or_none": None, "_invalidate_user_creds": None})
ADD = {"email": "e@x"}
add("_load_per_user_chat_api", ADD,
    io={"import.oauth": None,
        "asyncio.to_thread": [rs("RuntimeError", "load"), rs("RuntimeError", "build")],
        "_proxy_error": False, "logger.debug": None})
add("_load_per_user_chat_api", ADD,
    io={"import.oauth": None, "asyncio.to_thread": [rs("SystemExit", "bye")]})
add("_load_per_user_chat_api", ADD,
    io={"import.oauth": rs("ImportError", "no oauth")})
add("_load_per_user_chat_api", ADD, fc=True)

# ── _submit_on_loop ──
def _submit(io, state=None, fc=False):
    add("_submit_on_loop", {"coro": "C"}, state=state or {"_loop": "L"}, io=io, fc=fc)


_submit({"_loop_accepts_callbacks": True, "import.agent.async_utils": None,
         "scope_ctx.copy": "CTX", "context.run": "FUT", "future.add_done_callback": None},
        state={"_loop": "L", "_scope_ctx": "S"})
_submit({"_loop_accepts_callbacks": True, "import.agent.async_utils": None,
         "scope_ctx.copy": "CTX", "context.run": None},
        state={"_loop": "L", "_scope_ctx": "S"})
_submit({"_loop_accepts_callbacks": True, "import.agent.async_utils": None,
         "contextvars.copy_context": "CTX", "context.run": "FUT", "future.add_done_callback": None})
_submit({"_loop_accepts_callbacks": False, "logger.warning": None})
_submit({"_loop_accepts_callbacks": True, "import.agent.async_utils": None,
         "scope_ctx.copy": "CTX", "context.run": rs("RuntimeError", "closed"),
         "logger.warning": None},
        state={"_loop": "L", "_scope_ctx": "S"})
_submit({"_loop_accepts_callbacks": rs("SystemExit", "bye")})
_submit({"_loop_accepts_callbacks": True, "import.agent.async_utils": rs("ImportError", "x")})
_submit({}, fc=True)

# ── write ──
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for case in CASES:
        f.write(json.dumps(case, ensure_ascii=False) + "\n")

from collections import Counter
counts = Counter(c["func"] for c in CASES)
print(f"wrote {OUT}: {len(CASES)} cases, {len(counts)} funcs")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
