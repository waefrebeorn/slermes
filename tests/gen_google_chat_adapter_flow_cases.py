#!/usr/bin/env python3
"""Generate tests/fixtures/google_chat_adapter_flow.jsonl for the Google Chat
adapter FLOW port (src/gateway/platforms/google_chat_adapter_flow.c).

Case shape: {"func": <live name>, "in": {"args": {...}, "state": {...},
"io": {seam-name: script}}}. A script is a plain JSON value (constant), a
LIST consumed one element per call (wrap list RESULTS one level deeper), or
{"__script__": true, "ret": v} / {"__script__": true, "raise": {type,...}}.
Missing scripts are fail-closed RuntimeErrors on both sides.

Every ported function gets >= 5 cases; error paths, BaseException
propagation and fail-closed scripts are covered per flow.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "google_chat_adapter_flow.jsonl")

CASES = []


def add(func, args=None, state=None, io=None, fc=False):
    c = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": io or {}}}
    if fc:
        c["fc"] = True
    CASES.append(c)


def rs(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


def exc(cls, msg="", attrs=None):
    return {"cls": cls, "msg": msg, "attrs": attrs or {}}


def obj(attrs):
    return {"__obj__": attrs}


# ── _thread_body ──
add("_thread_body", {"text": "hi", "thread_id": None})
add("_thread_body", {"text": "hi", "thread_id": "t"})
add("_thread_body", {"text": "", "thread_id": ""})
add("_thread_body", {"text": "x", "thread_id": 0})
add("_thread_body", {"text": None, "thread_id": "t"})
add("_thread_body", {"text": "x", "thread_id": "spaces/A/threads/B"})

# ── _create_kwargs ──
add("_create_kwargs", {"chat_id": "spaces/A", "body": {"text": "x"}})
add("_create_kwargs", {"chat_id": "spaces/A", "body": {"thread": {"name": "t"}}})
add("_create_kwargs", {"chat_id": "spaces/A", "body": {"thread": {"name": ""}}})
add("_create_kwargs", {"chat_id": "spaces/A", "body": {"thread": {}}})
add("_create_kwargs", {"chat_id": "spaces/A", "body": {}})
add("_create_kwargs", {"chat_id": "spaces/A", "body": {"thread": None}})
add("_create_kwargs", {"chat_id": "spaces/A", "body": {"text": "x", "thread": {"name": "t"}}})

# ── _mime_for_message_type ──
for mime in ("image/png", "audio/ogg", "video/mp4", "application/pdf", "",
             "image/", "IMAGE/png", "text/plain"):
    add("_mime_for_message_type", {"mime": mime})

# ── _http_status ──
add("_http_status", {"exc": exc("HttpError", "x", {"resp": {"status": 429}})})
add("_http_status", {"exc": exc("HttpError", "x", {"resp": {"status": 200}})})
add("_http_status", {"exc": exc("E", "x", {})})
add("_http_status", {"exc": exc("E", "x", {"resp": {}})})
add("_http_status", {"exc": exc("E", "x", {"resp": None})})
add("_http_status", {"exc": exc("E", "x", {"resp": {"status": None}})})

# ── _is_retryable_error ──
for st in (429, 500, 502, 503, 504, 400, 403):
    add("_is_retryable_error", {"exc": exc("E", "x")}, io={"_http_status": st})
for msg in ("timed out", "Connection reset by peer", "connection refused",
            "broken pipe", "remote disconnected", "bad request",
            "connection established", "Connection aborted"):
    add("_is_retryable_error", {"exc": exc("E", msg)}, io={"_http_status": None})
add("_is_retryable_error", {"exc": exc("E", "x")}, io={"_http_status": True})
add("_is_retryable_error", {"exc": exc("E", "x")}, io={"_http_status": rs("RuntimeError", "boom")})
add("_is_retryable_error", {"exc": exc("E", "x")}, io={"_http_status": rs("SystemExit", "bye")})
add("_is_retryable_error", {"exc": exc("E", "x")}, fc=True)

# ── _redact_sensitive ──
add("_redact_sensitive", {"text": "projects/p1/subscriptions/s1"})
add("_redact_sensitive", {"text": "projects/p1/topics/t1"})
add("_redact_sensitive", {"text": "a.b@proj.iam.gserviceaccount.com"})
add("_redact_sensitive", {"text": "projects/p1/subscriptions/s1 and a@proj.iam.gserviceaccount.com"})
add("_redact_sensitive", {"text": ""})
add("_redact_sensitive", {"text": "no secrets here"})
add("_redact_sensitive", {"text": "projects/p1/subscriptions/s1.extra"})
add("_redact_sensitive", {"text": "x@.iam.gserviceaccount.com"})
add("_redact_sensitive", {"text": "a@b.iam.gserviceaccount.com.evil.com"})
add("_redact_sensitive", {"text": None})

# ── _proxy_error ──
add("_proxy_error", {"exc": exc("E", "x"), "targets": []},
    io={"env_proxy_for_redaction": "P", "describe_proxy_error": "S"})
add("_proxy_error", {"exc": exc("E", "x"), "targets": ["https://x.com"]},
    io={"env_proxy_for_redaction": "P", "describe_proxy_error": "S"})
add("_proxy_error", {"exc": exc("E", "x"), "targets": []},
    io={"env_proxy_for_redaction": "P", "describe_proxy_error": None})
add("_proxy_error", {"exc": exc("E", "x"), "targets": []},
    io={"env_proxy_for_redaction": rs("RuntimeError", "p")})
add("_proxy_error", {"exc": exc("E", "x"), "targets": []},
    io={"env_proxy_for_redaction": "P", "describe_proxy_error": rs("ValueError", "d")})
add("_proxy_error", {"exc": exc("E", "x"), "targets": []},
    io={"env_proxy_for_redaction": "P", "describe_proxy_error": rs("SystemExit", "bye")})
add("_proxy_error", {"exc": exc("E", "x"), "targets": []}, fc=True)

# ── _exc_text ──
add("_exc_text", {"exc": exc("E", "secret projects/p1/subscriptions/s1"), "targets": []},
    io={"_proxy_error": "shown"})
add("_exc_text", {"exc": exc("E", "projects/p1/subscriptions/s1"), "targets": []},
    io={"_proxy_error": None, "_redact_sensitive": "redacted"})
add("_exc_text", {"exc": exc("E", "x"), "targets": []},
    io={"_proxy_error": "", "_redact_sensitive": "redacted"})
add("_exc_text", {"exc": exc("E", "x"), "targets": ["https://t.com"]},
    io={"_proxy_error": None, "_redact_sensitive": "r"})
add("_exc_text", {"exc": exc("E", "x"), "targets": []},
    io={"_proxy_error": rs("RuntimeError", "p")})
add("_exc_text", {"exc": exc("E", "x"), "targets": []},
    io={"_proxy_error": rs("SystemExit", "bye")})
add("_exc_text", {"exc": exc("E", "x"), "targets": []},
    io={"_proxy_error": None, "_redact_sensitive": rs("ValueError", "r")})
add("_exc_text", {"exc": exc("E", "x"), "targets": []}, fc=True)

# ── _is_google_owned_host ──
def host(scheme, hostname):
    return {"urlparse": obj({"scheme": scheme, "hostname": hostname})}
add("_is_google_owned_host", {"url": "u"}, io=host("https", "chat.googleapis.com"))
add("_is_google_owned_host", {"url": "u"}, io=host("https", "sub.chat.googleapis.com"))
add("_is_google_owned_host", {"url": "u"}, io=host("https", "googleapis.com"))
add("_is_google_owned_host", {"url": "u"}, io=host("https", "evil.com"))
add("_is_google_owned_host", {"url": "u"}, io=host("http", "chat.googleapis.com"))
add("_is_google_owned_host", {"url": "u"}, io=host("https", ""))
add("_is_google_owned_host", {"url": "u"}, io=host("https", None))
add("_is_google_owned_host", {"url": "u"}, io=host("https", "chat.googleapis.com.evil.com"))
add("_is_google_owned_host", {"url": "u"}, io=host("https", "lh3.googleusercontent.com"))
add("_is_google_owned_host", {"url": "u"}, io={"urlparse": rs("ValueError", "bad")})
add("_is_google_owned_host", {"url": "u"}, io={"urlparse": rs("SystemExit", "bye")})
add("_is_google_owned_host", {"url": "u"}, fc=True)  # RuntimeError caught -> False

# ── _adc_would_borrow_foreign_credentials ──
add("_adc_would_borrow_foreign_credentials", io={"is_multiplex_active": False})
add("_adc_would_borrow_foreign_credentials",
    io={"is_multiplex_active": True, "os.environ.get": ["x"]})
add("_adc_would_borrow_foreign_credentials",
    io={"is_multiplex_active": True, "os.environ.get": [None, "y"]})
add("_adc_would_borrow_foreign_credentials",
    io={"is_multiplex_active": True, "os.environ.get": [None, None]})
add("_adc_would_borrow_foreign_credentials",
    io={"is_multiplex_active": True, "os.environ.get": ["", "y"]})
add("_adc_would_borrow_foreign_credentials", io={"is_multiplex_active": None})
add("_adc_would_borrow_foreign_credentials", io={"is_multiplex_active": rs("RuntimeError", "m")})
add("_adc_would_borrow_foreign_credentials", io={"is_multiplex_active": rs("SystemExit", "bye")})
add("_adc_would_borrow_foreign_credentials",
    io={"is_multiplex_active": True, "os.environ.get": rs("RuntimeError", "e")})
add("_adc_would_borrow_foreign_credentials", fc=True)

# ── _validate_config ──
add("_validate_config", {"config": obj({"extra": {"http_events_url": "u"}})})
add("_validate_config", {"config": obj({"extra": {"project_id": "p", "subscription_name": "s"}})})
add("_validate_config", {"config": obj({"extra": {"project_id": "p"}})})
add("_validate_config", {"config": obj({"extra": {}})})
add("_validate_config", {"config": obj({"extra": None})})
add("_validate_config", {"config": obj({})})
add("_validate_config", {"config": obj({"extra": {"http_events_url": "", "project_id": "p", "subscription_name": "s"}})})
add("_validate_config", {"config": obj({"extra": {"project_id": "p", "subscription_name": ""}})})

# ── _is_connected ──
add("_is_connected", {"config": obj({"enabled": True})}, io={"_validate_config": True})
add("_is_connected", {"config": obj({"enabled": True})}, io={"_validate_config": False})
add("_is_connected", {"config": obj({"enabled": False})})
add("_is_connected", {"config": obj({})})
add("_is_connected", {"config": obj({"enabled": "yes"})}, io={"_validate_config": True})
add("_is_connected", {"config": obj({"enabled": True})}, io={"_validate_config": rs("RuntimeError", "v")})
add("_is_connected", {"config": obj({"enabled": True})}, io={"_validate_config": "x"})
add("_is_connected", {"config": obj({"enabled": True})}, fc=True)

# ── _env_inbound_settings ──
add("_env_inbound_settings", io={"get_scoped_secret": ["p", "s", "u"]})
add("_env_inbound_settings", io={"get_scoped_secret": [None, "p2", "s", "u"]})
add("_env_inbound_settings", io={"get_scoped_secret": ["p", None, "s2", "u"]})
add("_env_inbound_settings", io={"get_scoped_secret": [None, None, None, None, None, None]})
add("_env_inbound_settings", io={"get_scoped_secret": ["p", "s", None]})
add("_env_inbound_settings", io={"get_scoped_secret": rs("RuntimeError", "s")})
add("_env_inbound_settings", fc=True)

# ── _env_inbound_configured ──
add("_env_inbound_configured", io={"_env_inbound_settings": [["p", "s", "u"]]})
add("_env_inbound_configured", io={"_env_inbound_settings": [[None, None, "u"]]})
add("_env_inbound_configured", io={"_env_inbound_settings": [["p", "s", None]]})
add("_env_inbound_configured", io={"_env_inbound_settings": [[None, None, None]]})
add("_env_inbound_configured", io={"_env_inbound_settings": [["p", None, None]]})
add("_env_inbound_configured", io={"_env_inbound_settings": rs("RuntimeError", "s")})
add("_env_inbound_configured", io={"_env_inbound_settings": [[1, 2]]})
add("_env_inbound_configured", io={"_env_inbound_settings": None})
add("_env_inbound_configured", fc=True)

# ── check_google_chat_requirements ──
add("check_google_chat_requirements", io={"_load_google_modules": True})
add("check_google_chat_requirements", io={"_load_google_modules": False})
add("check_google_chat_requirements", io={"_load_google_modules": None})
add("check_google_chat_requirements", io={"_load_google_modules": rs("RuntimeError", "m")})
add("check_google_chat_requirements", io={"_load_google_modules": rs("SystemExit", "bye")})
add("check_google_chat_requirements", fc=True)

# ── _check_for_registry ──
add("_check_for_registry", io={"check_google_chat_requirements": True, "_env_inbound_configured": True})
add("_check_for_registry", io={"check_google_chat_requirements": True, "_env_inbound_configured": False})
add("_check_for_registry", io={"check_google_chat_requirements": False})
add("_check_for_registry", io={"check_google_chat_requirements": None})
add("_check_for_registry", io={"check_google_chat_requirements": rs("RuntimeError", "c")})
add("_check_for_registry", io={"check_google_chat_requirements": True, "_env_inbound_configured": rs("ValueError", "e")})
add("_check_for_registry", fc=True)

# ── _env_enablement ──
add("_env_enablement", io={"_env_inbound_configured": False})
add("_env_enablement", io={"_env_inbound_configured": True,
                           "_env_inbound_settings": [["p", "s", "u"]],
                           "get_scoped_secret": ["sa"],
                           "seed_extra_from_env": {"k": "v"}})
add("_env_enablement", io={"_env_inbound_configured": True,
                           "_env_inbound_settings": [[None, None, None]],
                           "get_scoped_secret": [None, "ac"],
                           "seed_extra_from_env": {}})
add("_env_enablement", io={"_env_inbound_configured": True,
                           "_env_inbound_settings": [["p", None, None]],
                           "get_scoped_secret": ["sa"],
                           "seed_extra_from_env": {}})
add("_env_enablement", io={"_env_inbound_configured": True,
                           "_env_inbound_settings": [["p", "s", "u"]],
                           "get_scoped_secret": ["sa"],
                           "seed_extra_from_env": None})
add("_env_enablement", io={"_env_inbound_configured": rs("RuntimeError", "c")})
add("_env_enablement", io={"_env_inbound_configured": True,
                           "_env_inbound_settings": rs("RuntimeError", "s")})
add("_env_enablement", fc=True)

# ── _standalone_error ──
add("_standalone_error", {"detail": "boom"}, io={"send_error": {"success": False}})
add("_standalone_error", {"detail": ""}, io={"send_error": {"ok": 1}})
add("_standalone_error", {"detail": None}, io={"send_error": None})
add("_standalone_error", {"detail": 123}, io={"send_error": {"ok": 1}})
add("_standalone_error", {"detail": "x"}, io={"send_error": rs("RuntimeError", "s")})
add("_standalone_error", {"detail": "x"}, fc=True)

# ── _int_setting ──
add("_int_setting", {"extra": {"k": "15"}, "key": "k", "env_name": "E", "default": 1})
add("_int_setting", {"extra": {"k": 20}, "key": "k", "env_name": "E", "default": 1})
add("_int_setting", {"extra": {"k": True}, "key": "k", "env_name": "E", "default": 1})
add("_int_setting", {"extra": {"k": 2.9}, "key": "k", "env_name": "E", "default": 1})
add("_int_setting", {"extra": {}, "key": "k", "env_name": "E", "default": 1},
    io={"get_scoped_secret": "7"})
add("_int_setting", {"extra": {}, "key": "k", "env_name": "E", "default": 1},
    io={"get_scoped_secret": "abc"})
add("_int_setting", {"extra": {}, "key": "k", "env_name": "E", "default": 1},
    io={"get_scoped_secret": None})
add("_int_setting", {"extra": {"k": "1_0"}, "key": "k", "env_name": "E", "default": 1})
add("_int_setting", {"extra": {"k": "1\x1c"}, "key": "k", "env_name": "E", "default": 1})
add("_int_setting", {"extra": {"k": " 12 "}, "key": "k", "env_name": "E", "default": 1})
add("_int_setting", {"extra": {"k": None}, "key": "k", "env_name": "E", "default": 1},
    io={"get_scoped_secret": "9"})
add("_int_setting", {"extra": {}, "key": "k", "env_name": "E", "default": 1},
    io={"get_scoped_secret": rs("ValueError", "v")})
add("_int_setting", {"extra": {}, "key": "k", "env_name": "E", "default": 1},
    io={"get_scoped_secret": rs("RuntimeError", "r")})
add("_int_setting", {"extra": {}, "key": "k", "env_name": "E", "default": 1},
    io={"get_scoped_secret": rs("SystemExit", "bye")})
add("_int_setting", {"extra": {}, "key": "k", "env_name": "E", "default": 1}, fc=True)

# ── _str_setting ──
add("_str_setting", {"extra": {"k": "  v  "}, "key": "k", "env_name": "E"})
add("_str_setting", {"extra": {"k": "v"}, "key": "k", "env_name": "E"})
add("_str_setting", {"extra": {}, "key": "k", "env_name": "E"},
    io={"get_scoped_secret": "  e  "})
add("_str_setting", {"extra": {}, "key": "k", "env_name": "E", "fallback": "fb"},
    io={"get_scoped_secret": ""})
add("_str_setting", {"extra": {}, "key": "k", "env_name": "E", "fallback": "fb"},
    io={"get_scoped_secret": None})
add("_str_setting", {"extra": {"k": 5}, "key": "k", "env_name": "E"})
add("_str_setting", {"extra": {"k": None}, "key": "k", "env_name": "E"},
    io={"get_scoped_secret": "s"})
add("_str_setting", {"extra": {}, "key": "k", "env_name": "E"},
    io={"get_scoped_secret": rs("RuntimeError", "r")})
add("_str_setting", {"extra": {}, "key": "k", "env_name": "E"},
    io={"get_scoped_secret": ""})  # absent fallback -> live default "" -> ""
add("_str_setting", {"extra": {}, "key": "k", "env_name": "E"}, fc=True)

# ── _extract_message_payload ──
add("_extract_message_payload", {"envelope": {"chat": {"messagePayload": {"message": {"text": "x"}, "space": {"name": "s"}}}}})
add("_extract_message_payload", {"envelope": {"chat": {"messagePayload": {"message": {"text": "x", "space": {"name": "ms"}}}}}})
add("_extract_message_payload", {"envelope": {"type": "MESSAGE", "message": {"text": "x"}, "space": {"name": "s"}}})
add("_extract_message_payload", {"envelope": {"type": "OTHER", "message": {}}})
add("_extract_message_payload", {"envelope": {"event_type": "MESSAGE", "sender_email": "a@b.com", "text": "hi"}})
add("_extract_message_payload", {"envelope": {"sender_email": "a@b.com", "sender_type": "bot"}})
add("_extract_message_payload", {"envelope": {"event_type": "OTHER", "sender_email": "a@b"}})
add("_extract_message_payload", {"envelope": {"sender_email": "a@b.com", "thread_name": "t", "space_name": "sp", "space_type": "DM", "sender_display_name": "Al", "message_name": "m"}})
add("_extract_message_payload", {"envelope": {"foo": "bar"}})
add("_extract_message_payload", {"envelope": {}})
add("_extract_message_payload", {"envelope": {"sender_email": "  A.B@x.com  ", "sender_type": "human"}})

# ── _prepare_inbound ──
def ext(msg, space, fmt="native_chat_api"):
    return {"_extract_message_payload": [[msg, space, fmt]]}
add("_prepare_inbound", {"envelope": {"a": 1}, "ce_type": None},
    io={"_extract_message_payload": None})
add("_prepare_inbound", {"envelope": {"a": 1}, "ce_type": "x"},
    io={"_extract_message_payload": None, "logger.debug": None})
add("_prepare_inbound", {"envelope": {}, "ce_type": None},
    io=ext({"sender": {"type": "BOT"}, "name": "m"}, {"name": "s"}))
add("_prepare_inbound", {"envelope": {"k": "v"}, "ce_type": "x"},
    io={**ext({"sender": {"type": "HUMAN"}, "name": "m"}, {"name": "s"}),
        "dedup.is_duplicate": False})
add("_prepare_inbound", {"envelope": {"k": "v"}, "ce_type": "x"},
    io={**ext({"sender": {"type": "HUMAN"}, "name": "m"}, {"name": "s"}),
        "dedup.is_duplicate": True, "logger.debug": None})
add("_prepare_inbound", {"envelope": {"k": "v"}, "ce_type": None},
    io={**ext({"sender": {"type": "HUMAN"}, "name": "m"}, {"name": "s"}),
        "dedup.is_duplicate": False})
add("_prepare_inbound", {"envelope": {"k": "v"}, "ce_type": None},
    io=ext({"sender": {"type": "HUMAN"}}, {"name": "s"}))
add("_prepare_inbound", {"envelope": {"space": {"name": "existing"}}, "ce_type": None},
    io={**ext({"sender": {"type": "HUMAN"}, "name": "m"}, {}),
        "dedup.is_duplicate": False})
add("_prepare_inbound", {"envelope": {}, "ce_type": None},
    io={"_extract_message_payload": rs("RuntimeError", "e")})
add("_prepare_inbound", {"envelope": {}, "ce_type": None},
    io={**ext({"sender": {"type": "HUMAN"}, "name": "m"}, {"name": "s"}),
        "dedup.is_duplicate": rs("SystemExit", "bye")})
add("_prepare_inbound", {"envelope": {}, "ce_type": None}, fc=True)

# ── _resolve_thread_id ──
LIT = {"_last_inbound_thread": {"c": "lt"}}
add("_resolve_thread_id", {"reply_to": None, "metadata": {"thread_id": "t"}, "chat_id": "c"}, LIT)
add("_resolve_thread_id", {"reply_to": None, "metadata": {"thread_name": "tn"}, "chat_id": "c"}, LIT)
add("_resolve_thread_id", {"reply_to": None, "metadata": {"thread_ts": "ts"}, "chat_id": "c"}, LIT)
add("_resolve_thread_id", {"reply_to": "spaces/A/threads/B", "metadata": {"thread_id": 0}, "chat_id": "c"}, LIT)
add("_resolve_thread_id", {"reply_to": "spaces/A/messages/B", "metadata": None, "chat_id": "c"}, LIT)
add("_resolve_thread_id", {"reply_to": None, "metadata": {"job_id": "j"}, "chat_id": "c"}, LIT)
add("_resolve_thread_id", {"reply_to": None, "metadata": None, "chat_id": None}, LIT)
add("_resolve_thread_id", {"reply_to": None, "metadata": None, "chat_id": "c"}, {"_last_inbound_thread": {}})
add("_resolve_thread_id", {"reply_to": None, "metadata": {"thread_id": 123}, "chat_id": "c"}, LIT)
add("_resolve_thread_id", {"reply_to": "spaces/A/threads/B", "metadata": {"job_id": "j"}, "chat_id": "c"}, LIT)

# ── _note_rate_limit ──
add("_note_rate_limit", {"chat_id": "c"}, {"_rate_limit_hits": {"c": 0}})
add("_note_rate_limit", {"chat_id": "c"}, {"_rate_limit_hits": {"c": 2}})
add("_note_rate_limit", {"chat_id": "c"}, {"_rate_limit_hits": {}})
add("_note_rate_limit", {"chat_id": "c"}, {})
add("_note_rate_limit", {"chat_id": "c"}, {"_rate_limit_hits": {"other": 5}})

# ── _invalidate_user_creds ──
add("_invalidate_user_creds", {"identity": "__legacy__"},
    {"_user_credentials": {"old": 1}, "_user_chat_api": {"api": 1}})
add("_invalidate_user_creds", {"identity": "a@b.com"},
    {"_user_creds_by_email": {"a@b.com": {"c": 1}}, "_user_chat_api_by_email": {"a@b.com": {"a": 1}}})
add("_invalidate_user_creds", {"identity": ""}, {"_user_credentials": {"keep": 1}})
add("_invalidate_user_creds", {"identity": None}, {})
add("_invalidate_user_creds", {"identity": "missing@x.com"},
    {"_user_creds_by_email": {"a@b.com": {}}, "_user_chat_api_by_email": {"a@b.com": {}}})
add("_invalidate_user_creds", {"identity": "__legacy__"}, {})

# ── stop_typing ──
add("stop_typing", {"chat_id": "c"}, {"_typing_messages": {"c": "<consumed>"}})
add("stop_typing", {"chat_id": "c"}, {"_typing_messages": {"c": "real_id"}})
add("stop_typing", {"chat_id": "c"}, {"_typing_messages": {}})
add("stop_typing", {"chat_id": "c"}, {"_typing_messages": {"other": "<consumed>"}})
add("stop_typing", {"chat_id": "c"}, {})

# ── _track_outbound_thread ──
add("_track_outbound_thread", {"chat_id": "c", "resp": {"thread": {"name": "t"}}},
    io={"thread_count_store.incr": 1})
add("_track_outbound_thread", {"chat_id": "c", "resp": {"thread": {"name": ""}}})
add("_track_outbound_thread", {"chat_id": "c", "resp": {}})
add("_track_outbound_thread", {"chat_id": "", "resp": {"thread": {"name": "t"}}})
add("_track_outbound_thread", {"chat_id": "c", "resp": {"thread": {"name": "t"}}},
    io={"thread_count_store.incr": rs("RuntimeError", "i"), "logger.debug": None})
add("_track_outbound_thread", {"chat_id": "c", "resp": {"thread": {"name": "t"}}},
    io={"thread_count_store.incr": rs("SystemExit", "bye")})
add("_track_outbound_thread", {"chat_id": "c", "resp": {"thread": {"name": "t"}}},
    io={"logger.debug": None}, fc=True)  # incr fail-closed -> except -> debug

# ── _release_unadmitted ──
add("_release_unadmitted", {"msg": {"name": "m", "thread": {"name": "t"}}, "envelope": {"space": {"name": "s"}}},
    io={"dedup.discard": None, "thread_count_store.decr": None})
add("_release_unadmitted", {"msg": {"name": "m"}, "envelope": {}},
    io={"dedup.discard": None})
add("_release_unadmitted", {"msg": {}, "envelope": {}}, io={"dedup.discard": None})
add("_release_unadmitted", {"msg": {"name": "m", "space": {"name": "s"}, "thread": {"name": "t"}}, "envelope": {}},
    io={"dedup.discard": None, "thread_count_store.decr": None})
add("_release_unadmitted", {"msg": {"name": "m"}, "envelope": {}},
    io={"dedup.discard": rs("RuntimeError", "d")})
add("_release_unadmitted", {"msg": {"name": "m", "thread": {"name": "t"}}, "envelope": {"space": {"name": "s"}}},
    io={"dedup.discard": None, "thread_count_store.decr": rs("SystemExit", "bye")})
add("_release_unadmitted", {"msg": {}, "envelope": {}}, fc=True)

# ── _connect_failed ──
add("_connect_failed", {"log_fmt": "fmt %s", "exc": exc("E", "x"), "code": "c", "retryable": False},
    io={"_exc_text": "msg", "logger.error": None, "_set_fatal_error": None})
add("_connect_failed", {"log_fmt": "f", "exc": exc("E", "x"), "code": "c", "retryable": True},
    io={"_exc_text": "msg", "logger.error": None, "_set_fatal_error": None})
add("_connect_failed", {"log_fmt": "f", "exc": exc("E", "x"), "code": "c", "retryable": False},
    io={"_exc_text": rs("RuntimeError", "e")})
add("_connect_failed", {"log_fmt": "f", "exc": exc("E", "x"), "code": "c", "retryable": False},
    io={"_exc_text": "msg", "logger.error": rs("ValueError", "l")})
add("_connect_failed", {"log_fmt": "f", "exc": exc("E", "x"), "code": "c", "retryable": False},
    io={"_exc_text": "msg", "logger.error": None, "_set_fatal_error": rs("SystemExit", "bye")})
add("_connect_failed", {"log_fmt": "f", "exc": exc("E", "x"), "code": "c", "retryable": False}, fc=True)

# ── _chunk_text ──
add("_chunk_text", {"text": "short"})
add("_chunk_text", {"text": ""})
add("_chunk_text", {"text": "a" * 4001})
add("_chunk_text", {"text": "b" * 3000 + "\n" + "c" * 1500})
add("_chunk_text", {"text": "d" * 1000 + "\n" + "e" * 4000})
add("_chunk_text", {"text": "f" * 8500})
add("_chunk_text", {"text": "g" * 4000 + "\n   " + "h" * 100})
add("_chunk_text", {"text": "x" * 4000 + "\n" + "y" * 4000 + "\n" + "z" * 10})

# ── verify_http_event_request ──
VS = {"_http_events_audience": "aud", "_http_events_service_account_email": "a@b.com"}
add("verify_http_event_request", {"auth_header": "Bearer tok"}, {})
add("verify_http_event_request", {"auth_header": "Bearer tok"}, VS,
    io={"_verify_google_id_token": {"email": "a@b.com"}})
add("verify_http_event_request", {"auth_header": "Basic x"}, VS)
add("verify_http_event_request", {"auth_header": "Bearer "}, VS)
add("verify_http_event_request", {"auth_header": "Bearer tok"}, VS,
    io={"_verify_google_id_token": {"email": "other@x.com"}})
add("verify_http_event_request", {"auth_header": "Bearer tok"}, VS,
    io={"_verify_google_id_token": rs("RuntimeError", "bad"), "_exc_text": "shown", "logger.warning": None})
add("verify_http_event_request", {"auth_header": "Bearer tok"}, VS,
    io={"_verify_google_id_token": rs("SystemExit", "bye")})
add("verify_http_event_request", {"auth_header": "Bearer tok"},
    {"_http_events_audience": "aud", "_http_events_service_account_email": "A@B.com, c@d.com"},
    io={"_verify_google_id_token": {"email": "a@b.com"}})
add("verify_http_event_request", {"auth_header": "Bearer tok"}, VS,
    io={"_verify_google_id_token": {"email": None}})
add("verify_http_event_request", {"auth_header": "Bearer tok"}, VS,
    io={"_exc_text": "shown", "logger.warning": None}, fc=True)

# ── _load_cached_bot_id ──
def bot(text):
    return {"_bot_id_cache_path": "/h/bot.json", "bot_id_cache.read_text": text}
add("_load_cached_bot_id", io=bot('{"bot_user_id":"users/1"}'))
add("_load_cached_bot_id", io=bot('{"bot_user_id":""}'))
add("_load_cached_bot_id", io=bot('{}'))
add("_load_cached_bot_id", io=bot('{"bot_user_id":null}'))
add("_load_cached_bot_id", io=bot(''))
add("_load_cached_bot_id", io=bot('{bad'))
add("_load_cached_bot_id", io=bot('5'))
add("_load_cached_bot_id", io=bot('null'))
add("_load_cached_bot_id", io=bot('"s"'))
add("_load_cached_bot_id", io=bot('\ufeff{"bot_user_id":"u"}'))
add("_load_cached_bot_id", io={"_bot_id_cache_path": "/h/bot.json",
                               "bot_id_cache.read_text": rs("OSError", "read")})
add("_load_cached_bot_id", io={"_bot_id_cache_path": "/h/bot.json",
                               "bot_id_cache.read_text": rs("RuntimeError", "read")})
add("_load_cached_bot_id", io={"_bot_id_cache_path": rs("OSError", "path")})
add("_load_cached_bot_id", io={"_bot_id_cache_path": rs("RuntimeError", "path")})
add("_load_cached_bot_id", fc=True)

# ── _log_background_failure ──
add("_log_background_failure", io={"future.result": None})
add("_log_background_failure", io={"future.result": "x"})
add("_log_background_failure", io={"future.result": rs("RuntimeError", "f"), "logger.exception": None})
add("_log_background_failure", io={"future.result": rs("SystemExit", "bye")})
add("_log_background_failure", io={"future.result": rs("KeyboardInterrupt", "ki")})
add("_log_background_failure", io={"logger.exception": None}, fc=True)

# ── _loop_accepts_callbacks ──
add("_loop_accepts_callbacks", {"loop": None})
add("_loop_accepts_callbacks", {"loop": obj({})})
add("_loop_accepts_callbacks", {"loop": obj({"is_closed": "seam"})}, io={"loop.is_closed": True})
add("_loop_accepts_callbacks", {"loop": obj({"is_closed": "seam"})}, io={"loop.is_closed": False})
add("_loop_accepts_callbacks", {"loop": obj({"is_closed": "seam"})}, io={"loop.is_closed": rs("RuntimeError", "c")})
add("_loop_accepts_callbacks", {"loop": obj({"is_closed": "seam"})}, fc=True)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    few = [k for k, v in counts.items() if v < 5]
    print(f"funcs: {len(counts)}; funcs with <5 cases: {few}")


if __name__ == "__main__":
    main()