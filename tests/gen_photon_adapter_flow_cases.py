#!/usr/bin/env python3
"""Generate tests/fixtures/photon_adapter_flow.jsonl for the Photon adapter
FLOW port (src/gateway/platforms/photon_adapter_flow.c).

Case shape: {"func": <live name>, "in": {"args": {...}, "state": {...},
"io": {seam-name: script}}}. A script is a plain JSON value (constant), a
LIST consumed one element per call (wrap list RESULTS one level deeper), or
{"__script__": true, "ret": v} / {"__script__": true, "raise": {type,...}}.
Missing scripts are fail-closed RuntimeErrors on both sides (fc=True marks an
intentional gap). Every ported function gets >= 5 cases; error paths,
BaseException propagation and fail-closed scripts are covered per flow.

Corpus restrictions mirror the C port header: str()-computed slots carry
scalars only; lower()/regex text is ASCII; bracketed hosts are out of domain;
sidecar_call success scripts are dicts; attachment paths use '/' separators.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "photon_adapter_flow.jsonl")
CASES = []


def add(func, args=None, state=None, io=None, fc=False):
    c = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": io or {}}}
    if fc:
        c["fc"] = True
    CASES.append(c)


def raise_script(kind, msg, **extra):
    r = {"type": kind, "message": msg}
    r.update(extra)
    return {"__script__": True, "raise": r}


def ret_script(v):
    return {"__script__": True, "ret": v}


def pser(ec, retry, msg="Photon sidecar /send returned 500 (sidecar_error, retryable=False): boom"):
    return raise_script("PhotonSidecarError", msg, error_class=ec, retryable=retry,
                        path="/send", status_code=500, error="boom")


# ── _url_only_candidate ──
for t in ("https://x.com", "  http://h/p  ", "http:///path", "ftp://x.com",
          "http://", "https://x y", "HTTP://H", "http://h?q", "",
          "https://u:p@h:8080/a", "http://h#f", "not a url"):
    add("_url_only_candidate", {"text": t})

# ── _richlink_url_from_content ──
add("_richlink_url_from_content", {"content": {"type": "text", "text": "https://a.com"}})
add("_richlink_url_from_content", {"content": {"type": "text", "text": "prose https://a.com"}})
add("_richlink_url_from_content", {"content": {"type": "richlink", "url": "https://b.com"}})
add("_richlink_url_from_content", {"content": {"type": "richlink", "url": ""}})
add("_richlink_url_from_content", {"content": {"type": "group", "items": [
    {"content": {"type": "text", "text": "no"}},
    {"content": {"type": "text", "text": "https://c.com"}}]}})
add("_richlink_url_from_content", {"content": {"type": "group", "items": [
    {"content": {"type": "group", "items": [{"content": {"type": "richlink", "url": "https://d.com"}}]}}]}})
add("_richlink_url_from_content", {"content": {"type": "attachment"}})
add("_richlink_url_from_content", {"content": {"type": "group", "items": []}})

# ── _group_item_contents ──
add("_group_item_contents", {"content": {"items": [
    {"content": {"type": "text"}}, "notdict", {"content": "str"}, {"nocontent": 1}]}})
add("_group_item_contents", {"content": {"items": []}})
add("_group_item_contents", {"content": {"items": None}})
add("_group_item_contents", {"content": {}})
add("_group_item_contents", {"content": {"items": [{"content": {"a": 1}}, {"content": {"b": 2}}]}})

# ── _format_richlink_content ──
add("_format_richlink_content", {"content": {"url": "https://x", "title": "T", "summary": "S"}})
add("_format_richlink_content", {"content": {"url": "https://x", "title": "T", "summary": "T"}})
add("_format_richlink_content", {"content": {"url": "https://x"}})
add("_format_richlink_content", {"content": {"title": "  T  ", "summary": "  "}})
add("_format_richlink_content", {"content": {}})
add("_format_richlink_content", {"content": {"url": "", "title": "", "summary": ""}})
add("_format_richlink_content", {"content": {"url": "https://x", "summary": "S"}})

# ── _is_richlink_preview_attachment ──
SUF = ".pluginpayloadattachment"
add("_is_richlink_preview_attachment", {"payload": {"type": "attachment", "name": "x" + SUF}})
add("_is_richlink_preview_attachment", {"payload": {"type": "attachment", "id": "Y" + SUF.upper()}})
add("_is_richlink_preview_attachment", {"payload": {"type": "attachment", "name": "plain.png"}})
add("_is_richlink_preview_attachment", {"payload": {"type": "text", "name": "x" + SUF}})
add("_is_richlink_preview_attachment", {"payload": {"type": "attachment"}})
add("_is_richlink_preview_attachment", {"payload": {"type": "attachment", "name": None, "id": 123}})

# ── _richlink_preview_label ──
add("_richlink_preview_label", {"content": {"type": "attachment", "name": "n.png"}})
add("_richlink_preview_label", {"content": {"type": "attachment", "id": "id1"}})
add("_richlink_preview_label", {"content": {"type": "attachment"}})
add("_richlink_preview_label", {"content": {"type": "group", "items": [
    {"content": {"name": "a"}}, {"content": {"id": "b"}}, {"content": {}}]}})
add("_richlink_preview_label", {"content": {"type": "group", "items": []}})
add("_richlink_preview_label", {"content": {"type": "text"}})

# ── _is_richlink_preview_content ──
add("_is_richlink_preview_content", {"content": {"type": "attachment", "name": "x" + SUF}})
add("_is_richlink_preview_content", {"content": {"type": "group", "items": [
    {"content": {"type": "attachment", "name": "a" + SUF}},
    {"content": {"type": "attachment", "id": "b" + SUF}}]}})
add("_is_richlink_preview_content", {"content": {"type": "group", "items": [
    {"content": {"type": "attachment", "name": "a" + SUF}},
    {"content": {"type": "attachment", "name": "plain"}}]}})
add("_is_richlink_preview_content", {"content": {"type": "group", "items": []}})
add("_is_richlink_preview_content", {"content": {"type": "group", "items": ["notdict"]}})
add("_is_richlink_preview_content", {"content": {"type": "text"}})

# ── _is_voice_payload ──
add("_is_voice_payload", {"payload": {"type": "voice"}})
add("_is_voice_payload", {"payload": {"type": "attachment", "name": "note.CAF"}})
add("_is_voice_payload", {"payload": {"type": "attachment", "mimeType": "audio/x-caf"}})
add("_is_voice_payload", {"payload": {"type": "attachment", "name": "x.png"}})
add("_is_voice_payload", {"payload": {"type": "text"}})
add("_is_voice_payload", {"payload": {"type": "attachment", "name": None}})

# ── _cached_media_type ──
add("_cached_media_type", {"payload": {}, "mime": "image/png", "is_voice": False})
add("_cached_media_type", {"payload": {}, "mime": "", "is_voice": False})
add("_cached_media_type", {"payload": {}, "mime": "AUDIO/mp4", "is_voice": True})
add("_cached_media_type", {"payload": {"name": "n.caf"}, "mime": "application/octet-stream", "is_voice": True})
add("_cached_media_type", {"payload": {"name": "n.bin"}, "mime": "", "is_voice": True})
add("_cached_media_type", {"payload": {}, "mime": "audio/x-caf", "is_voice": True})

# ── _attachment_message_type ──
for m in ("image/png", "VIDEO/mp4", "audio/mpeg", "application/pdf", "", "IMAGE/GIF"):
    add("_attachment_message_type", {"mime": m})

# ── _normalize_binary_payload ──
add("_normalize_binary_payload", {"payload": {"type": "attachment", "name": "x.png", "mimeType": "image/png", "data": "x"}},
    io={"_cache_inbound_attachment": "/cache/x.png"})
add("_normalize_binary_payload", {"payload": {"type": "attachment", "name": "x.png", "mimeType": "image/png"}},
    io={"_cache_inbound_attachment": None})
add("_normalize_binary_payload", {"payload": {"type": "voice", "name": "n.caf"}},
    io={"_cache_inbound_attachment": "/cache/n.caf"})
add("_normalize_binary_payload", {"payload": {"type": "attachment", "duration": 5}},
    io={"_cache_inbound_attachment": None})
add("_normalize_binary_payload", {"payload": {"type": "attachment", "name": "f", "duration": "long"}},
    io={"_cache_inbound_attachment": None})
add("_normalize_binary_payload", {"payload": {"type": "voice"}},
    io={"_cache_inbound_attachment": None})

# ── _normalize_group_content ──
add("_normalize_group_content", {"content": {"type": "group", "items": [
    {"content": {"type": "text", "text": "hello"}}, {"content": {"type": "text", "text": "world"}}]}})
add("_normalize_group_content", {"content": {"type": "group", "items": [
    {"content": {"type": "text", "text": "see"}},
    {"content": {"type": "attachment", "name": "x.png", "mimeType": "image/png"}}]}},
    io={"_cache_inbound_attachment": "/cache/x.png"})
add("_normalize_group_content", {"content": {"type": "group", "items": [
    {"content": {"type": "attachment", "name": "x.png"}}]}},
    io={"_cache_inbound_attachment": None})
add("_normalize_group_content", {"content": {"type": "group", "items": [
    {"content": {"type": "richlink", "url": "https://x", "title": "T"}}]}})
add("_normalize_group_content", {"content": {"type": "group", "items": [
    {"content": {"type": "sticker"}}]}})
add("_normalize_group_content", {"content": {"type": "group", "items": []}})
add("_normalize_group_content", {"content": {"type": "group", "items": [
    {"content": {"type": "voice", "name": "n.caf"}},
    {"content": {"type": "attachment", "name": "p.jpg", "mimeType": "image/jpeg"}}]}},
    io={"_cache_inbound_attachment": ["/cache/n.caf", "/cache/p.jpg"]})

# ── _reply_inner ──
add("_reply_inner", {"content": {"type": "reply", "content": {"type": "text", "text": "hi"}}})
add("_reply_inner", {"content": {"type": "reply", "content": "str"}})
add("_reply_inner", {"content": {"type": "reply"}})
add("_reply_inner", {"content": {"type": "text", "text": "hi"}})
add("_reply_inner", {"content": {"type": "reply", "content": None}})

# ── _carries_voice ──
add("_carries_voice", {"content": {"type": "voice"}})
add("_carries_voice", {"content": {"type": "group", "items": [
    {"content": {"type": "text"}}, {"content": {"type": "voice"}}]}})
add("_carries_voice", {"content": {"type": "group", "items": [{"content": {"type": "text"}}]}})
add("_carries_voice", {"content": {"type": "attachment", "name": "n.caf"}})
add("_carries_voice", {"content": {"type": "text"}})

# ── _mention_gate_text ──
add("_mention_gate_text", {"content": {"type": "text", "text": "hey"}})
add("_mention_gate_text", {"content": {"type": "richlink", "url": "https://x", "title": "T"}})
add("_mention_gate_text", {"content": {"type": "group", "items": [
    {"content": {"type": "text", "text": "a"}},
    {"content": {"type": "group", "items": [{"content": {"type": "text", "text": "b"}}]}},
    {"content": {"type": "attachment"}}]}})
add("_mention_gate_text", {"content": {"type": "attachment"}})
add("_mention_gate_text", {"content": {"type": "text"}})
add("_mention_gate_text", {"content": {"type": "unknown"}})

# ── _normalize_content ──
add("_normalize_content", {"content": {"type": "text", "text": "hi"}})
add("_normalize_content", {"content": {"type": "attachment", "name": "x.png", "mimeType": "image/png"}},
    io={"_cache_inbound_attachment": None})
add("_normalize_content", {"content": {"type": "voice", "name": "n.caf"}},
    io={"_cache_inbound_attachment": "/cache/n.caf"})
add("_normalize_content", {"content": {"type": "richlink", "url": "https://x"}})
add("_normalize_content", {"content": {"type": "group", "items": [{"content": {"type": "text", "text": "g"}}]}})
add("_normalize_content", {"content": {"type": "sticker"}})
add("_normalize_content", {"content": {"type": None}})
add("_normalize_content", {"content": {"type": 123}})
add("_normalize_content", {"content": {}})

# ── _markdown_enabled ──
for v in ("true", "false", "0", "no", "YES", "  False  ", "1"):
    add("_markdown_enabled", {}, io={"get_scoped_secret": v})

# ── _richlink_candidate ──
add("_richlink_candidate", {"text": "https://x.com"}, io={"get_scoped_secret": "true"})
add("_richlink_candidate", {"text": "https://x.com"}, io={"get_scoped_secret": "false"})
add("_richlink_candidate", {"text": "prose"}, io={"get_scoped_secret": "true"})
add("_richlink_candidate", {"text": "  https://x.com/p  "}, io={"get_scoped_secret": "true"})
add("_richlink_candidate", {"text": ""}, io={"get_scoped_secret": "true"})

# ── _sidecar_payload_text ──
add("_sidecar_payload_text", {"text": "hello **bold**", "send_markdown": True})
add("_sidecar_payload_text", {"text": "see https://x.com now", "send_markdown": True},
    io={"strip_markdown": "see https://x.com now"})
add("_sidecar_payload_text", {"text": "**bold**", "send_markdown": False},
    io={"strip_markdown": "bold"})
add("_sidecar_payload_text", {"text": "", "send_markdown": True})
add("_sidecar_payload_text", {"text": "http://h", "send_markdown": True},
    io={"strip_markdown": "http://h"})

# ── _attachment_label ──
for k, n in (("attachment", "x.png"), ("voice", "n.caf"), ("attachment", ""), ("", "n"), ("video", "v.mp4")):
    add("_attachment_label", {"kind": k, "name": n})

# ── _attachment_body ──
add("_attachment_body", {"space_id": "c", "safe_path": "/a.png", "kind": "attachment"})
add("_attachment_body", {"space_id": "c", "safe_path": "/a.caf", "kind": "voice", "name": "n", "mime_type": "audio/x-caf", "caption": "cap"})
add("_attachment_body", {"space_id": "c", "safe_path": "/a.png", "kind": "attachment", "name": "", "mime_type": None, "caption": ""})
add("_attachment_body", {"space_id": "c", "safe_path": "/a.png", "kind": "other", "caption": "only cap"})
add("_attachment_body", {"space_id": "c", "safe_path": "/a.png", "kind": "voice", "mime_type": "image/png"})
# ── _normalize_chat_key ──
for c in ("any;-;+15551234567", "+15551234567", "any;-;+155512", "any;-;+15551",
          "any;-;15551234567", "guid-x", ""):
    add("_normalize_chat_key", {"chat_id": c})

# ── _record_sent_message ──
add("_record_sent_message", {"message_id": "m1"}, {"_sent_message_ids": {}},
    {"time.time": 100.0, "bounded_put": None})
add("_record_sent_message", {"message_id": None}, {"_sent_message_ids": {}}, {})
add("_record_sent_message", {"message_id": ""}, {"_sent_message_ids": {"a": 1}}, {})
add("_record_sent_message", {"message_id": "m2"}, {"_sent_message_ids": {"a": 1, "b": 2}, "_SENT_IDS_MAX": 2},
    {"time.time": 50.0, "bounded_put": None})
add("_record_sent_message", {"message_id": "a"}, {"_sent_message_ids": {"a": 1, "b": 2}},
    {"time.time": 9.0, "bounded_put": None})
add("_record_sent_message", {"message_id": "m1"}, {}, {}, fc=True)  # absent store -> AttributeError

# ── _put_by_chat ──
add("_put_by_chat", {"store": {}, "chat_id": "c", "value": "m1"}, {}, {"bounded_put": None})
add("_put_by_chat", {"store": {}, "chat_id": "any;-;+15551234567", "value": "m2"}, {}, {"bounded_put": None})
add("_put_by_chat", {"store": {"a": 1, "b": 2}, "chat_id": "c", "value": 3},
    {"_LAST_INBOUND_CHATS_MAX": 2}, {"bounded_put": None})
add("_put_by_chat", {"store": {"x": "old"}, "chat_id": "x", "value": "new"}, {}, {"bounded_put": None})
add("_put_by_chat", {"store": {}, "chat_id": "c", "value": None}, {}, {"bounded_put": None})

# ── _record_last_inbound ──
add("_record_last_inbound", {"chat_id": "c", "message_id": "m1"}, {"_last_inbound_by_chat": {}}, {"bounded_put": None})
add("_record_last_inbound", {"chat_id": None, "message_id": "m1"}, {"_last_inbound_by_chat": {}}, {})
add("_record_last_inbound", {"chat_id": "c", "message_id": None}, {"_last_inbound_by_chat": {}}, {})
add("_record_last_inbound", {"chat_id": "any;-;+15551234567", "message_id": "m2"},
    {"_last_inbound_by_chat": {}}, {"bounded_put": None})
add("_record_last_inbound", {"chat_id": "", "message_id": ""}, {"_last_inbound_by_chat": {}}, {})

# ── _record_recent_richlink ──
add("_record_recent_richlink", {"chat_id": "c", "text": "https://x.com"},
    {"_recent_richlinks_by_chat": {}}, {"time.time": 10.0, "bounded_put": None})
add("_record_recent_richlink", {"chat_id": "c", "text": "prose"}, {"_recent_richlinks_by_chat": {}}, {})
add("_record_recent_richlink", {"chat_id": None, "text": "https://x.com"}, {"_recent_richlinks_by_chat": {}}, {})
add("_record_recent_richlink", {"chat_id": "any;-;+15551234567", "text": "https://y.com"},
    {"_recent_richlinks_by_chat": {}}, {"time.time": 20.0, "bounded_put": None})
add("_record_recent_richlink", {"chat_id": "c", "text": "http:///path"}, {"_recent_richlinks_by_chat": {}}, {})

# ── _is_recent_richlink_preview ──
PRE = {"type": "attachment", "name": "x" + SUF}
add("_is_recent_richlink_preview", {"chat_id": "c", "content": PRE},
    {"_recent_richlinks_by_chat": {"c": 100.0}}, {"time.time": 110.0})
add("_is_recent_richlink_preview", {"chat_id": "c", "content": PRE},
    {"_recent_richlinks_by_chat": {"c": 100.0}}, {"time.time": 200.0})
add("_is_recent_richlink_preview", {"chat_id": "c", "content": PRE},
    {"_recent_richlinks_by_chat": {}}, {})
add("_is_recent_richlink_preview", {"chat_id": "c", "content": {"type": "text"}},
    {"_recent_richlinks_by_chat": {"c": 100.0}}, {})
add("_is_recent_richlink_preview", {"chat_id": None, "content": PRE},
    {"_recent_richlinks_by_chat": {"c": 100.0}}, {})
add("_is_recent_richlink_preview", {"chat_id": "any;-;+15551234567", "content": PRE},
    {"_recent_richlinks_by_chat": {"+15551234567": 100.0}}, {"time.time": 105.0})

# ── _reactions_enabled ──
for v in ("true", "false", "1", "yes", "on", "no", "  TRUE  "):
    add("_reactions_enabled", {}, io={"get_scoped_secret": v})

# ── _is_retryable_error ──
add("_is_retryable_error", {"error": "anything"}, io={"base_is_retryable_error": True})
add("_is_retryable_error", {"error": "upstream connect error"}, io={"base_is_retryable_error": False})
add("_is_retryable_error", {"error": "retryable=false upstream unavailable"}, io={"base_is_retryable_error": False})
add("_is_retryable_error", {"error": "auth_or_config problem"}, io={"base_is_retryable_error": False})
add("_is_retryable_error", {"error": ""}, io={"base_is_retryable_error": False})
add("_is_retryable_error", {"error": "totally random"}, io={"base_is_retryable_error": False})
add("_is_retryable_error", {"error": "UPSTREAM_OVERFLOW"}, io={"base_is_retryable_error": False})
add("_is_retryable_error", {"error": None}, io={"base_is_retryable_error": False})

# ── _is_permanent_sidecar_failure ──
add("_is_permanent_sidecar_failure", {"result": {"raw_response": {"retryable": False, "error_class": "auth_or_config"}}})
add("_is_permanent_sidecar_failure", {"result": {"raw_response": {"retryable": False, "error_class": "target_not_allowed"}}})
add("_is_permanent_sidecar_failure", {"result": {"raw_response": {"retryable": True, "error_class": "auth_or_config"}}})
add("_is_permanent_sidecar_failure", {"result": {"raw_response": {"retryable": 0, "error_class": "auth_or_config"}}})
add("_is_permanent_sidecar_failure", {"result": {"raw_response": "notdict"}})
add("_is_permanent_sidecar_failure", {"result": {"raw_response": {"retryable": False, "error_class": "sidecar_error"}}})
add("_is_permanent_sidecar_failure", {"result": {"raw_response": None}})

# ── _send_retry_is_final ──
add("_send_retry_is_final", {"result": {"raw_response": {"retryable": False, "error_class": "target_not_allowed"}}})
add("_send_retry_is_final", {"result": {"raw_response": {"retryable": False, "error_class": "auth_or_config"}}})
add("_send_retry_is_final", {"result": {"raw_response": {"retryable": True, "error_class": "auth_or_config"}}})
add("_send_retry_is_final", {"result": {"raw_response": None}})
add("_send_retry_is_final", {"result": {"raw_response": {"retryable": False, "error_class": "other"}}})

# ── _sidecar_try ──
add("_sidecar_try", {"path": "/typing", "body": {"spaceId": "c"}, "what": "send_typing"},
    io={"sidecar_call": {"ok": True}})
add("_sidecar_try", {"path": "/react", "body": {}, "what": "add_reaction"},
    io={"sidecar_call": raise_script("RuntimeError", "boom")})
add("_sidecar_try", {"path": "/react", "body": {}, "what": "add_reaction"},
    io={"sidecar_call": pser("sidecar_error", False)})
add("_sidecar_try", {"path": "/typing", "body": {}, "what": "send_typing"},
    io={"sidecar_call": raise_script("SystemExit", "bye")})
add("_sidecar_try", {"path": "/typing", "body": {}, "what": "send_typing"}, fc=True)
# ── _post_send ──
PS_STATE = {"_sent_message_ids": {}}
PS_OK = {"sidecar_call": {"ok": True, "messageId": "m9"}, "time.time": 5.0,
         "bounded_put": None, "record_sent_text_async": None}
add("_post_send", {"path": "/send", "body": {"spaceId": "c", "text": "hi"},
                   "structured": True, "sent_text": "hi"}, PS_STATE, PS_OK)
add("_post_send", {"path": "/send", "body": {"spaceId": "c"}, "structured": True, "sent_text": "hi"},
    PS_STATE, {"sidecar_call": {"ok": True}, "record_sent_text_async": None})
add("_post_send", {"path": "/send", "body": {"spaceId": "c"}, "structured": True, "sent_text": None},
    PS_STATE, {"sidecar_call": {"ok": True, "messageId": "m1"}, "time.time": 1.0, "bounded_put": None})
add("_post_send", {"path": "/send", "body": {"spaceId": "c"}, "structured": True, "sent_text": "x"},
    PS_STATE, {"sidecar_call": pser("auth_or_config", False)})
add("_post_send", {"path": "/send", "body": {"spaceId": "c"}, "structured": False, "sent_text": "x"},
    PS_STATE, {"sidecar_call": pser("target_not_allowed", False)})
add("_post_send", {"path": "/send", "body": {"spaceId": "c"}, "structured": True, "sent_text": "x"},
    PS_STATE, {"sidecar_call": raise_script("RuntimeError", "transport down")})
add("_post_send", {"path": "/send", "body": {"spaceId": "c"}, "structured": True, "sent_text": "x"},
    PS_STATE, {"sidecar_call": raise_script("KeyboardInterrupt", "stop")})
add("_post_send", {"path": "/send", "body": {"spaceId": "c"}, "structured": True, "sent_text": "x"},
    PS_STATE, fc=True)

# ── _sidecar_send_poll ──
POLL_OK = {"sidecar_call": {"ok": True, "messageId": "p1"}, "time.time": 1.0,
           "bounded_put": None, "record_sent_text_async": None}
add("_sidecar_send_poll", {"space_id": "c", "title": "Pick", "options": ["a", "b"]}, PS_STATE, POLL_OK)
add("_sidecar_send_poll", {"space_id": "c", "title": "", "options": ["a", "b"]}, PS_STATE, {})
add("_sidecar_send_poll", {"space_id": "c", "title": "   ", "options": ["a", "b"]}, PS_STATE, {})
add("_sidecar_send_poll", {"space_id": "c", "title": "Pick", "options": ["a"]}, PS_STATE, {})
add("_sidecar_send_poll", {"space_id": "c", "title": "Pick", "options": ["a", "  ", "", "b", 3]}, PS_STATE, POLL_OK)
add("_sidecar_send_poll", {"space_id": "c", "title": "ABCDEFGHIJ", "options": ["a", "b"]},
    {"_sent_message_ids": {}, "MAX_MESSAGE_LENGTH": 5}, POLL_OK)
add("_sidecar_send_poll", {"space_id": "c", "title": None, "options": ["a", "b"]}, PS_STATE, {})

# ── _sidecar_send_attachment ──
ATT_OK = {"validate_media_delivery_path": "/a/b.png", "_guess_mime": "image/png",
          "sidecar_call": {"ok": True, "messageId": "a1"}, "time.time": 1.0,
          "bounded_put": None, "record_sent_text_async": None}
add("_sidecar_send_attachment", {"space_id": "c", "path": "/a/b.png"}, PS_STATE, ATT_OK)
add("_sidecar_send_attachment", {"space_id": "c", "path": "/unsafe"}, PS_STATE,
    {"validate_media_delivery_path": None})
add("_sidecar_send_attachment", {"space_id": "c", "path": "/a/b.png", "mime_type": "image/jpeg"}, PS_STATE,
    {"validate_media_delivery_path": "/a/b.png", "sidecar_call": {"ok": True, "messageId": "a2"},
     "time.time": 1.0, "bounded_put": None, "record_sent_text_async": None})
add("_sidecar_send_attachment", {"space_id": "c", "path": "/a/b.png", "caption": "my cap"}, PS_STATE, ATT_OK)
add("_sidecar_send_attachment", {"space_id": "c", "path": "/a/n.caf", "kind": "voice", "name": "note"}, PS_STATE,
    {"validate_media_delivery_path": "/a/n.caf", "_guess_mime": "audio/x-caf",
     "sidecar_call": {"ok": True, "messageId": "a3"}, "time.time": 1.0,
     "bounded_put": None, "record_sent_text_async": None})
add("_sidecar_send_attachment", {"space_id": "c", "path": "/a/b.bin"}, PS_STATE,
    {"validate_media_delivery_path": "/a/b.bin", "_guess_mime": None,
     "sidecar_call": {"ok": True, "messageId": "a4"}, "time.time": 1.0,
     "bounded_put": None, "record_sent_text_async": None})
add("_sidecar_send_attachment", {"space_id": "c", "path": "/a/b.png"}, PS_STATE,
    {"validate_media_delivery_path": raise_script("PermissionError", "denied")})

# ── _sidecar_send ──
SEND_OK = {"get_scoped_secret": "true", "sidecar_call": {"ok": True, "messageId": "m1"},
           "time.time": 1.0, "bounded_put": None, "record_sent_text_async": None}
add("_sidecar_send", {"space_id": "c", "text": "hello"}, PS_STATE, SEND_OK)
add("_sidecar_send", {"space_id": "c", "text": "https://x.com"}, PS_STATE,
    {"get_scoped_secret": "true", "sidecar_call": {"ok": True, "messageId": "r1"},
     "time.time": 1.0, "bounded_put": None, "record_sent_text_async": None})
add("_sidecar_send", {"space_id": "c", "text": "https://x.com"}, PS_STATE,
    {"get_scoped_secret": "true", "sidecar_call": [pser("sidecar_error", True), {"ok": True, "messageId": "m2"}],
     "strip_markdown": "https://x.com", "time.time": 1.0, "bounded_put": None, "record_sent_text_async": None})
add("_sidecar_send", {"space_id": "c", "text": "**bold**", "markdown": False}, PS_STATE,
    {"get_scoped_secret": "true", "strip_markdown": "bold", "sidecar_call": {"ok": True, "messageId": "m3"},
     "time.time": 1.0, "bounded_put": None, "record_sent_text_async": None})
add("_sidecar_send", {"space_id": "c", "text": "abcdefghij"},
    {"_sent_message_ids": {}, "MAX_MESSAGE_LENGTH": 5}, SEND_OK)
add("_sidecar_send", {"space_id": "c", "text": "hello", "richlink": False}, PS_STATE, SEND_OK)
add("_sidecar_send", {"space_id": "c", "text": "hello"}, PS_STATE, fc=True)

# ── write ──
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {OUT}")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
short = [fn for fn, n in counts.items() if n < 5]
if short:
    print(f"WARNING funcs with <5 cases: {short}")