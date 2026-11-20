#!/usr/bin/env python3
"""Generate tests/fixtures/signal_adapter_flow2.jsonl for the Signal adapter FLOW
remainder port. Deterministic; >=5 cases per ported function (24 functions).
Honours the corpus restrictions documented in
src/gateway/platforms/signal_adapter_flow2.c (ASCII scope; reaction emoji are
opaque pass-through literals; resp.json scripts an object; _sig_secret scripts a
string; int() tokens in _styles_for_chunk are clean; integral time values).

Run: python tests/gen_signal_adapter_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "signal_adapter_flow2.jsonl")

ACCT = "+155****4567"
UUID = "u-15551234567"
SID = "u:abc123"            # service id with u: prefix (valid without uuid seam)
HTTP = "http://127.0.0.1:8080"
CASES = []


def case(func, args, state=None, io=None):
    inp = {"args": args}
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = io
    CASES.append({"func": func, "in": inp})


def raise_spec(kind, msg="", **extra):
    r = {"type": kind, "message": msg}
    r.update(extra)
    return {"__script__": True, "raise": r}


def sc(ret):
    return {"__script__": True, "ret": ret}


def sr(success, error=None, raw=None, message_id=None):
    """A full SendResult projection (dataclasses.asdict, all 8 fields)."""
    return {"success": success, "message_id": message_id, "error": error,
            "raw_response": raw, "retryable": False, "retry_after": None,
            "continuation_message_ids": [], "error_kind": None}


def typing_state(**over):
    base = {"account": ACCT, "_typing_tasks": {}, "_typing_failures": {},
            "_typing_skip_until": {}, "client": {"m": 1}, "http_url": HTTP}
    base.update(over)
    return base


def rpc_state(**over):
    base = {"account": ACCT, "client": {"m": 1}, "http_url": HTTP}
    base.update(over)
    return base


def recipient_state(**over):
    base = {"account": ACCT, "_recipient_uuid_by_number": {}, "_recipient_number_by_uuid": {},
            "_recipient_cache_lock": {"lock": 1}, "client": {"m": 1}, "http_url": HTTP}
    base.update(over)
    return base


def reaction_state(**over):
    base = {"account": ACCT, "dm_allow_from": ["*"]}
    base.update(over)
    return base


def send_state(**over):
    base = typing_state(MAX_MESSAGE_LENGTH=8000)
    base.update(over)
    return base


# the _stop_typing_indicator REAL child needs cancel_task + _with_target + the
# _rpc chain (client.post / resp.raise_for_status / resp.json) scripted.
STOP_IO = {"cancel_task": None, "_with_target": {"account": ACCT, "recipient": [ACCT]},
           "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {}}}


def sio(extra):
    """STOP_IO merged with extra seam scripts (keys may contain dots)."""
    d = dict(STOP_IO)
    d.update(extra)
    return d


def ev_full(**over):
    e = {"raw_message": {"sender": ACCT, "timestamp_ms": 100},
         "source": {"user_id": ACCT, "chat_type": "dm", "chat_id": ACCT}}
    e.update(over)
    return e


# ── _is_signal_service_id ──
case("_is_signal_service_id", {"value": ""})
case("_is_signal_service_id", {"value": None})
case("_is_signal_service_id", {"value": "PNI:abc"})
case("_is_signal_service_id", {"value": "u:xyz"})
case("_is_signal_service_id", {"value": "12345678-1234-1234-1234-123456789abc"}, io={"uuid.UUID": {}})
case("_is_signal_service_id", {"value": "not-a-uuid"}, io={"uuid.UUID": raise_spec("ValueError", "badly formed")})
case("_is_signal_service_id", {"value": "x"}, io={"uuid.UUID": raise_spec("TypeError", "wrong type")})
case("_is_signal_service_id", {"value": "x"}, io={"uuid.UUID": raise_spec("AttributeError", "no attr")})
case("_is_signal_service_id", {"value": "x"}, io={"uuid.UUID": raise_spec("RuntimeError", "boom")})
case("_is_signal_service_id", {"value": "x"}, io={"uuid.UUID": raise_spec("SystemExit", "bye")})
case("_is_signal_service_id", {"value": "x"}, io={})

# ── validate_signal_config ──
case("validate_signal_config", {"config": {"extra": {"http_url": "http://x", "account": ACCT}}})
case("validate_signal_config", {"config": {"extra": {"http_url": "", "account": ""}}},
     io={"_sig_secret": ["http://y", ACCT]})
case("validate_signal_config", {"config": {"extra": {"http_url": "http://x"}}},
     io={"_sig_secret": ["http://y", ""]})
case("validate_signal_config", {"config": {"extra": {}}}, io={"_sig_secret": ["", ""]})
case("validate_signal_config", {"config": {"extra": None}}, io={"_sig_secret": ["http://y", ACCT]})
case("validate_signal_config", {"config": {}}, io={"_sig_secret": ["http://y", ACCT]})
case("validate_signal_config", {"config": {"extra": {"http_url": "  http://x  ", "account": " " + ACCT + " "}}})
case("validate_signal_config", {"config": {"extra": {"http_url": "http://x", "account": "   "}}},
     io={"_sig_secret": ["", ""]})
case("validate_signal_config", {"config": {"extra": {"http_url": "", "account": ACCT}}},
     io={"_sig_secret": raise_spec("RuntimeError", "secret down")})
case("validate_signal_config", {"config": {"extra": {"http_url": "", "account": ACCT}}}, io={})

# ── _remember_recipient_identifiers ──
case("_remember_recipient_identifiers", {"number": ACCT, "service_id": SID}, state=recipient_state())
case("_remember_recipient_identifiers", {"number": ACCT, "service_id": "bad"},
     state=recipient_state(), io={"uuid.UUID": raise_spec("ValueError", "bad")})
case("_remember_recipient_identifiers", {"number": None, "service_id": SID}, state=recipient_state())
case("_remember_recipient_identifiers", {"number": ACCT, "service_id": None}, state=recipient_state())
case("_remember_recipient_identifiers", {"number": ACCT, "service_id": "12345678-1234-1234-1234-123456789abc"},
     state=recipient_state(), io={"uuid.UUID": {}})
case("_remember_recipient_identifiers", {"number": ACCT, "service_id": SID},
     state={"_recipient_number_by_uuid": {}})
case("_remember_recipient_identifiers", {"number": ACCT, "service_id": "x"},
     state=recipient_state(), io={"uuid.UUID": raise_spec("SystemExit", "bye")})
case("_remember_recipient_identifiers", {"number": ACCT, "service_id": SID},
     state=recipient_state(_recipient_uuid_by_number={ACCT: "old"}, _recipient_number_by_uuid={"old": ACCT}))

# ── _extract_contact_uuid ──
case("_extract_contact_uuid", {"contact": {"uuid": SID, "number": ACCT}, "phone_number": ACCT}, state={})
case("_extract_contact_uuid", {"contact": {"serviceId": SID, "recipient": ACCT}, "phone_number": ACCT}, state={})
case("_extract_contact_uuid", {"contact": {"profile": {"serviceId": SID}, "number": ACCT}, "phone_number": ACCT}, state={})
case("_extract_contact_uuid", {"contact": {"profile": {"uuid": SID}, "number": ACCT}, "phone_number": ACCT}, state={})
case("_extract_contact_uuid", {"contact": {"uuid": SID, "number": "+1999"}, "phone_number": ACCT}, state={})
case("_extract_contact_uuid", {"contact": {"uuid": "bad", "number": ACCT}, "phone_number": ACCT},
     state={}, io={"uuid.UUID": raise_spec("ValueError", "bad")})
case("_extract_contact_uuid", {"contact": "not-dict", "phone_number": ACCT}, state={})
case("_extract_contact_uuid", {"contact": {"uuid": SID}, "phone_number": ACCT}, state={})
case("_extract_contact_uuid", {"contact": {"uuid": "x", "number": ACCT}, "phone_number": ACCT},
     state={}, io={"uuid.UUID": raise_spec("SystemExit", "bye")})

# ── _quote_references_own_message ──
case("_quote_references_own_message", {"reply_to_id": "100", "reply_to_author": None},
     state={"_sent_message_timestamps": {"100": None}})
case("_quote_references_own_message", {"reply_to_id": "999", "reply_to_author": None},
     state={"_sent_message_timestamps": {"100": None}})
case("_quote_references_own_message", {"reply_to_id": None, "reply_to_author": ACCT},
     state={"_account_normalized": ACCT, "_sent_message_timestamps": {}})
case("_quote_references_own_message", {"reply_to_id": None, "reply_to_author": " " + ACCT + " "},
     state={"_account_normalized": ACCT, "_sent_message_timestamps": {}})
case("_quote_references_own_message", {"reply_to_id": None, "reply_to_author": UUID},
     state={"_account_normalized": ACCT, "_sent_message_timestamps": {}, "_recipient_uuid_by_number": {ACCT: UUID}})
case("_quote_references_own_message", {"reply_to_id": None, "reply_to_author": ACCT},
     state={"_account_normalized": UUID, "_sent_message_timestamps": {}, "_recipient_uuid_by_number": {},
            "_recipient_number_by_uuid": {ACCT: UUID}})
case("_quote_references_own_message", {"reply_to_id": None, "reply_to_author": "+1999"},
     state={"_account_normalized": ACCT, "_sent_message_timestamps": {}, "_recipient_uuid_by_number": {},
            "_recipient_number_by_uuid": {}})
case("_quote_references_own_message", {"reply_to_id": None, "reply_to_author": ACCT},
     state={"_account_normalized": "", "_sent_message_timestamps": {}})
case("_quote_references_own_message", {"reply_to_id": "100", "reply_to_author": None}, state={})
case("_quote_references_own_message", {"reply_to_id": None, "reply_to_author": "+1999"},
     state={"_account_normalized": ACCT, "_sent_message_timestamps": {}})

# ── _extract_reaction_target ──
case("_extract_reaction_target", {"event": {"raw_message": {"sender": ACCT, "timestamp_ms": 100}}}, state={})
case("_extract_reaction_target", {"event": {"raw_message": {"sender": "", "timestamp_ms": 100}}}, state={})
case("_extract_reaction_target", {"event": {"raw_message": {"sender": ACCT, "timestamp_ms": 0}}}, state={})
case("_extract_reaction_target", {"event": {"raw_message": {}}}, state={})
case("_extract_reaction_target", {"event": {"raw_message": "not-dict"}}, state={})
case("_extract_reaction_target", {"event": {}}, state={})
case("_extract_reaction_target", {"event": {"raw_message": {"sender": ACCT, "timestamp_ms": 100, "quote": {}}}}, state={})

# ── format_message ──
case("format_message", {"content": "hello"}, state={})
case("format_message", {"content": ""}, state={})
case("format_message", {"content": None}, state={})
case("format_message", {"content": "**bold** _x_"}, state={})
case("format_message", {"content": "  spaced  "}, state={})
case("format_message", {"content": 123}, state={})

# ── _styles_for_chunk ──
case("_styles_for_chunk", {"text_styles": ["0:5:BOLD"], "chunk_start": 0, "chunk_end": 10})
case("_styles_for_chunk", {"text_styles": ["0:5:BOLD"], "chunk_start": 2, "chunk_end": 8})
case("_styles_for_chunk", {"text_styles": ["0:5:BOLD"], "chunk_start": 5, "chunk_end": 10})
case("_styles_for_chunk", {"text_styles": ["bad"], "chunk_start": 0, "chunk_end": 10}, io={"logger.debug": None})
case("_styles_for_chunk", {"text_styles": ["x:5:BOLD"], "chunk_start": 0, "chunk_end": 10}, io={"logger.debug": None})
case("_styles_for_chunk", {"text_styles": ["0:5:BOLD", "2:3:ITALIC"], "chunk_start": 0, "chunk_end": 10})
case("_styles_for_chunk", {"text_styles": ["0:5:BOLD:extra"], "chunk_start": 0, "chunk_end": 10})
case("_styles_for_chunk", {"text_styles": [], "chunk_start": 0, "chunk_end": 10})
case("_styles_for_chunk", {"text_styles": ["bad"], "chunk_start": 0, "chunk_end": 10},
     io={"logger.debug": raise_spec("SystemExit", "bye")})

# ── _utf16_offsets ──
for t in ["", "abc", "a", "hello", "ab cd", "Signal"]:
    case("_utf16_offsets", {"text": t})

# ── _split_signal_formatted_message ──
case("_split_signal_formatted_message", {"plain_text": "hi", "text_styles": [], "max_length": 8000})
case("_split_signal_formatted_message", {"plain_text": "hi", "text_styles": ["0:2:BOLD"], "max_length": 8000})
case("_split_signal_formatted_message", {"plain_text": "abcdefghijklm", "text_styles": [], "max_length": 12})
case("_split_signal_formatted_message", {"plain_text": "abcdefghijklm", "text_styles": ["0:13:BOLD"], "max_length": 12})
case("_split_signal_formatted_message", {"plain_text": "abc", "text_styles": ["bad"], "max_length": 2},
     io={"logger.debug": None})
case("_split_signal_formatted_message", {"plain_text": "x", "text_styles": [], "max_length": 1})
case("_split_signal_formatted_message", {"plain_text": "abc", "text_styles": [], "max_length": 10})

# ── _reactions_enabled ──
case("_reactions_enabled", {"event": None}, state=reaction_state(), io={"_sig_secret": "true"})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": True})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(), io={"_sig_secret": "false"})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(), io={"_sig_secret": "0"})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(), io={"_sig_secret": "NO"})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(dm_allow_from=["+1999"]),
     io={"_sig_secret": "true"})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": False})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": None})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": raise_spec("SystemExit", "bye")})
case("_reactions_enabled", {"event": ev_full()}, state={"account": ACCT}, io={"_sig_secret": "true"})
case("_reactions_enabled", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": raise_spec("RuntimeError", "secret down")})

# ── send_reaction ──
case("send_reaction", {"chat_id": ACCT, "emoji": "x", "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": True})
case("send_reaction", {"chat_id": ACCT, "emoji": "x", "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": False, "logger.debug": None})
case("send_reaction", {"chat_id": ACCT, "emoji": "x", "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": raise_spec("RuntimeError", "conn lost")})
case("send_reaction", {"chat_id": ACCT, "emoji": "x", "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": raise_spec("SystemExit", "bye")})
case("send_reaction", {"chat_id": ACCT, "emoji": "x", "target_author": "+1", "target_timestamp": 5}, state={})
case("send_reaction", {"chat_id": "group:g1", "emoji": "\U0001F440", "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": True})
case("send_reaction", {"chat_id": ACCT, "emoji": "x", "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": False, "logger.debug": raise_spec("RuntimeError", "log down")})

# ── remove_reaction ──
case("remove_reaction", {"chat_id": ACCT, "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": True})
case("remove_reaction", {"chat_id": ACCT, "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": False})
case("remove_reaction", {"chat_id": ACCT, "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": raise_spec("SystemExit", "bye")})
case("remove_reaction", {"chat_id": ACCT, "target_author": "+1", "target_timestamp": 5}, state={})
case("remove_reaction", {"chat_id": "group:g1", "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": {}})
case("remove_reaction", {"chat_id": ACCT, "target_author": "+1", "target_timestamp": 5},
     state=reaction_state(), io={"_send_reaction_rpc": None})

# ── on_processing_start ──
case("on_processing_start", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": True, "_send_reaction_rpc": True})
case("on_processing_start", {"event": ev_full()}, state=reaction_state(), io={"_sig_secret": "false"})
case("on_processing_start", {"event": ev_full(raw_message={})}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": True})
case("on_processing_start", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": True, "_send_reaction_rpc": False, "logger.debug": None})
case("on_processing_start", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": True, "_send_reaction_rpc": raise_spec("SystemExit", "bye")})
case("on_processing_start", {"event": ev_full(source={"user_id": ACCT, "chat_type": "dm"})},
     state=reaction_state(), io={"_sig_secret": "true", "_is_sender_authorized": True})
case("on_processing_start", {"event": ev_full()}, state=reaction_state(),
     io={"_sig_secret": "true", "_is_sender_authorized": False})

# ── on_processing_complete ──
case("on_processing_complete", {"event": ev_full(_signal_eyes_sent=True), "outcome": "success"},
     state=reaction_state(), io={"_sig_secret": "true", "_is_sender_authorized": True, "_send_reaction_rpc": True})
case("on_processing_complete", {"event": ev_full(_signal_eyes_sent=True), "outcome": "cancelled"},
     state=reaction_state(), io={})
case("on_processing_complete", {"event": ev_full(_signal_eyes_sent=False), "outcome": "success"},
     state=reaction_state(), io={})
case("on_processing_complete", {"event": ev_full(), "outcome": "success"}, state=reaction_state(), io={})
case("on_processing_complete", {"event": ev_full(_signal_eyes_sent=True), "outcome": "failure"},
     state=reaction_state(), io={"_sig_secret": "true", "_is_sender_authorized": True, "_send_reaction_rpc": True})
case("on_processing_complete", {"event": ev_full(_signal_eyes_sent=True, raw_message={}), "outcome": "success"},
     state=reaction_state(), io={})
case("on_processing_complete", {"event": ev_full(_signal_eyes_sent=True), "outcome": "success"},
     state=reaction_state(), io={"_sig_secret": "false", "_send_reaction_rpc": True})
case("on_processing_complete", {"event": ev_full(_signal_eyes_sent=True), "outcome": "success"},
     state=reaction_state(), io={"_send_reaction_rpc": raise_spec("SystemExit", "bye")})

# ── _stop_typing_indicator ──
case("_stop_typing_indicator", {"chat_id": ACCT},
     state=typing_state(_typing_tasks={ACCT: {"task": 1}}), io=dict(STOP_IO))
case("_stop_typing_indicator", {"chat_id": ACCT}, state=typing_state(), io=dict(STOP_IO))
case("_stop_typing_indicator", {"chat_id": ACCT},
     state=typing_state(_typing_failures={ACCT: 2}, _typing_skip_until={ACCT: 50}),
     io={"cancel_task": None, "_with_target": raise_spec("RuntimeError", "resolve down")})
case("_stop_typing_indicator", {"chat_id": ACCT}, state=typing_state(),
     io={"cancel_task": None, "_with_target": raise_spec("SystemExit", "bye")})
case("_stop_typing_indicator", {"chat_id": ACCT}, state=typing_state(),
     io={"cancel_task": raise_spec("RuntimeError", "cancel down")})
case("_stop_typing_indicator", {"chat_id": ACCT},
     state={"_typing_tasks": {}, "_typing_failures": {ACCT: 1}, "_typing_skip_until": {ACCT: 5},
            "client": {"m": 1}, "http_url": HTTP}, io={"cancel_task": None})
case("_stop_typing_indicator", {"chat_id": ACCT}, state={"account": ACCT}, io={"cancel_task": None})
case("_stop_typing_indicator", {"chat_id": ACCT}, state=typing_state(),
     io={"cancel_task": None, "_with_target": {"account": ACCT, "recipient": [ACCT]},
         "client.post": raise_spec("RuntimeError", "net down"), "logger.log": None})

# ── stop_typing ──
case("stop_typing", {"chat_id": ACCT}, state=typing_state(), io=dict(STOP_IO))
case("stop_typing", {"chat_id": ACCT}, state=typing_state(),
     io={"cancel_task": None, "_with_target": raise_spec("RuntimeError", "resolve down")})
case("stop_typing", {"chat_id": ACCT}, state=typing_state(),
     io={"cancel_task": raise_spec("SystemExit", "bye")})
case("stop_typing", {"chat_id": ACCT},
     state={"account": ACCT, "_typing_tasks": {}, "_typing_skip_until": {}, "client": {"m": 1}, "http_url": HTTP},
     io=dict(STOP_IO))
case("stop_typing", {"chat_id": "group:g1"}, state=typing_state(),
     io={"cancel_task": None, "_with_target": {"account": ACCT, "groupId": "g1"},
         "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {}}})

# ── send_typing ──
case("send_typing", {"chat_id": ACCT},
     state=typing_state(_typing_failures={ACCT: 1}, _typing_skip_until={ACCT: 50}),
     io={"time.monotonic": 100, "_with_target": {"account": ACCT, "recipient": [ACCT]},
         "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {}}})
case("send_typing", {"chat_id": ACCT}, state=typing_state(_typing_skip_until={ACCT: 50}),
     io={"time.monotonic": 10})
case("send_typing", {"chat_id": ACCT}, state=typing_state(),
     io={"time.monotonic": 100, "_with_target": {"account": ACCT, "recipient": [ACCT]},
         "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": None}})
case("send_typing", {"chat_id": ACCT}, state=typing_state(_typing_failures={ACCT: 2}),
     io={"time.monotonic": 100, "_with_target": {"account": ACCT, "recipient": [ACCT]},
         "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": None}})
case("send_typing", {"chat_id": ACCT}, state=typing_state(_typing_failures={ACCT: 3}),
     io={"time.monotonic": 100, "_with_target": {"account": ACCT, "recipient": [ACCT]},
         "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": None}})
case("send_typing", {"chat_id": ACCT}, state=typing_state(_typing_failures={ACCT: 5}),
     io={"time.monotonic": 100, "_with_target": {"account": ACCT, "recipient": [ACCT]},
         "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": None}})
case("send_typing", {"chat_id": ACCT}, state=typing_state(), io={"time.monotonic": raise_spec("SystemExit", "bye")})
case("send_typing", {"chat_id": ACCT}, state={"account": ACCT, "client": {"m": 1}, "http_url": HTTP},
     io={"time.monotonic": 100})
case("send_typing", {"chat_id": ACCT}, state=typing_state(), io={})

# ── _resolve_recipient ──
case("_resolve_recipient", {"chat_id": "group:g1"}, state=recipient_state())
case("_resolve_recipient", {"chat_id": ""}, state=recipient_state())
case("_resolve_recipient", {"chat_id": ACCT}, state=recipient_state(), io={"_looks_like_e164_number": False})
case("_resolve_recipient", {"chat_id": ACCT}, state=recipient_state(_recipient_uuid_by_number={ACCT: UUID}),
     io={"_looks_like_e164_number": True})
case("_resolve_recipient", {"chat_id": ACCT}, state=recipient_state(),
     io={"_looks_like_e164_number": True, "time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None,
         "resp.json": {"result": [{"number": ACCT, "uuid": SID}]}})
case("_resolve_recipient", {"chat_id": ACCT}, state=recipient_state(),
     io={"_looks_like_e164_number": True, "time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None,
         "resp.json": {"result": []}})
case("_resolve_recipient", {"chat_id": ACCT}, state=recipient_state(),
     io={"_looks_like_e164_number": True, "time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None,
         "resp.json": {"result": None}})
case("_resolve_recipient", {"chat_id": ACCT}, state=recipient_state(),
     io={"_looks_like_e164_number": raise_spec("SystemExit", "bye")})
case("_resolve_recipient", {"chat_id": ACCT},
     state={"_recipient_uuid_by_number": {}, "_recipient_number_by_uuid": {}, "_recipient_cache_lock": {"lock": 1}},
     io={"_looks_like_e164_number": True})
case("_resolve_recipient", {"chat_id": ACCT}, state=recipient_state(),
     io={"_looks_like_e164_number": True, "time.time": 1000, "client.post": raise_spec("RuntimeError", "net down"), "logger.log": None})

# ── _rpc ──
case("_rpc", {"method": "send", "params": {"account": ACCT}, "rpc_id": "r1"}, state=rpc_state(),
     io={"client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {"timestamp": 5}}})
case("_rpc", {"method": "send", "params": {}}, state=rpc_state(),
     io={"time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {}}})
case("_rpc", {"method": "send", "params": {}}, state=rpc_state(client=None), io={"logger.warning": None})
case("_rpc", {"method": "send", "params": {}}, state={"http_url": HTTP})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1"}, state=rpc_state(),
     io={"client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"error": {"message": "boom"}},
         "logger.log": None})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1", "raise_on_rate_limit": True}, state=rpc_state(),
     io={"client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"error": {"message": "[429] rate"}},
         "_is_signal_rate_limit_error": True, "_extract_retry_after_seconds": 4})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1", "raise_on_rate_limit": True}, state=rpc_state(),
     io={"client.post": {"m": 1}, "resp.raise_for_status": None,
         "resp.json": {"result": {"results": [{"type": "RATE_LIMIT_FAILURE", "retryAfterSeconds": 7}]}}})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1"}, state=rpc_state(),
     io={"client.post": raise_spec("RuntimeError", "net down"), "logger.log": None})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1"}, state=rpc_state(),
     io={"client.post": raise_spec("SystemExit", "bye")})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1"}, state=rpc_state(),
     io={"client.post": {"m": 1}, "resp.raise_for_status": raise_spec("RuntimeError", "status bad"), "logger.log": None})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1", "log_failures": False}, state=rpc_state(),
     io={"client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"error": {"message": "boom"}},
         "logger.log": None})
case("_rpc", {"method": "send", "params": {}, "rpc_id": "r1"}, state=rpc_state(), io={})

# ── get_chat_info ──
case("get_chat_info", {"chat_id": "group:g1"}, state=rpc_state())
case("get_chat_info", {"chat_id": ACCT}, state=rpc_state(),
     io={"time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {"name": "Alice"}}})
case("get_chat_info", {"chat_id": ACCT}, state=rpc_state(),
     io={"time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {"profileName": "Bob"}}})
case("get_chat_info", {"chat_id": ACCT}, state=rpc_state(),
     io={"time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": {}}})
case("get_chat_info", {"chat_id": ACCT}, state=rpc_state(),
     io={"time.time": 1000, "client.post": {"m": 1}, "resp.raise_for_status": None, "resp.json": {"result": None}})
case("get_chat_info", {"chat_id": ACCT}, state={"client": {"m": 1}, "http_url": HTTP})
case("get_chat_info", {"chat_id": ACCT}, state=rpc_state(),
     io={"time.time": 1000, "client.post": raise_spec("SystemExit", "bye")})

# ── send_image ──
case("send_image", {"chat_id": ACCT, "image_url": "http://x/a.png", "caption": "hi"}, state=send_state(),
     io=sio({"_resolve_image_path": sc(["/cache/a.png", None, None]), "_send_file": sr(True)}))
case("send_image", {"chat_id": ACCT, "image_url": "http://x/a.png", "caption": None}, state=send_state(),
     io=sio({"_resolve_image_path": sc([None, "missing", None])}))
case("send_image", {"chat_id": ACCT, "image_url": "http://x/a.png", "caption": None}, state=send_state(),
     io=sio({"_resolve_image_path": sc([None, "download", "net down"]), "logger.warning": None}))
case("send_image", {"chat_id": ACCT, "image_url": "http://x/a.png", "caption": None}, state=send_state(),
     io=sio({"_resolve_image_path": sc([None, "oversize", 200000000])}))
case("send_image", {"chat_id": ACCT, "image_url": "http://x/a.png", "caption": None}, state=send_state(),
     io=sio({"_resolve_image_path": raise_spec("SystemExit", "bye")}))
case("send_image", {"chat_id": ACCT, "image_url": "http://x/a.png", "caption": None}, state=send_state(),
     io={"cancel_task": raise_spec("RuntimeError", "cancel down")})
case("send_image", {"chat_id": ACCT, "image_url": "http://x/a.png", "caption": None}, state=send_state(),
     io=sio({"_resolve_image_path": sc([None, "bogus", None])}))

# ── _send_attachment ──
case("_send_attachment", {"chat_id": ACCT, "file_path": "/f.png", "media_label": "Image", "caption": None},
     state=send_state(), io=sio({"Path.stat": {"st_size": 100}, "_send_file": sr(True)}))
case("_send_attachment", {"chat_id": ACCT, "file_path": "/f.png", "media_label": "Image", "caption": None},
     state=send_state(), io=sio({"Path.stat": raise_spec("FileNotFoundError", "no file")}))
case("_send_attachment", {"chat_id": ACCT, "file_path": "/f.png", "media_label": "Image", "caption": None},
     state=send_state(), io=sio({"Path.stat": {"st_size": 200000000}}))
case("_send_attachment", {"chat_id": ACCT, "file_path": "/f.png", "media_label": "Image", "caption": None},
     state=send_state(), io=sio({"Path.stat": raise_spec("SystemExit", "bye")}))
case("_send_attachment", {"chat_id": ACCT, "file_path": "/f.pdf", "media_label": "File", "caption": "cap"},
     state=send_state(), io=sio({"Path.stat": {"st_size": 100}, "_send_file": sr(True)}))
case("_send_attachment", {"chat_id": ACCT, "file_path": "/f.png", "media_label": "Image", "caption": None},
     state=send_state(), io={"cancel_task": raise_spec("RuntimeError", "cancel down")})
case("_send_attachment", {"chat_id": ACCT, "file_path": "/f.png", "media_label": "Image", "caption": None},
     state=send_state(), io=sio({"Path.stat": raise_spec("PermissionError", "denied")}))

# ── send ──
case("send", {"chat_id": ACCT, "content": "hi"}, state=send_state(),
     io=sio({"markdown_to_signal": sc(["hi", []]), "logger.info": None, "_rpc_send": sc([{"timestamp": 5}, None])}))
case("send", {"chat_id": ACCT, "content": ""}, state=send_state(), io=dict(STOP_IO))
case("send", {"chat_id": ACCT, "content": "   "}, state=send_state(), io=dict(STOP_IO))
case("send", {"chat_id": ACCT, "content": "hi"}, state=send_state(),
     io=sio({"markdown_to_signal": sc(["hi", []]), "logger.info": None, "_rpc_send": sc([None, sr(False, error="RPC send failed")])}))
case("send", {"chat_id": ACCT, "content": "hi"}, state=send_state(),
     io=sio({"markdown_to_signal": raise_spec("SystemExit", "bye")}))
case("send", {"chat_id": ACCT, "content": "hi"}, state=send_state(),
     io=sio({"markdown_to_signal": sc(["hi", []]), "logger.info": None, "_rpc_send": raise_spec("RuntimeError", "send down")}))
case("send", {"chat_id": ACCT, "content": "abcdefghijklm"}, state=send_state(MAX_MESSAGE_LENGTH=12),
     io=sio({"markdown_to_signal": sc(["abcdefghijklm", []]), "logger.info": None, "_rpc_send": sc([{"timestamp": 5}, None])}))
case("send", {"chat_id": ACCT, "content": None}, state=send_state(), io=dict(STOP_IO))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    low = [k for k, v in counts.items() if v < 5]
    print(f"funcs: {len(counts)}; under-5: {low}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()