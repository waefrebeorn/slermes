#!/usr/bin/env python3
"""Generate tests/fixtures/telegram_adapter_flow.jsonl — decision-sequence
cases for the Telegram adapter FLOW port (45 funcs, >= 5 cases each:
branch + raise + state coverage, fail-closed and BaseException passthrough).

Case shape: {"func", "in": {"args", "state", "io", "errors",
"expect_fail_closed"}}. io scripts: plain value = constant result, bare
array = per-call queue, {"__script__": true, "ret": ...} wraps a list/dict
result, {"__script__": true, "raise": {"type", "message"}} scripts a raise.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "telegram_adapter_flow.jsonl")

CASES = []


def add(func, args=None, state=None, io=None, errors=None, fail_closed=False):
    inp = {"args": args or {}}
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = io
    if errors is not None:
        inp["errors"] = errors
    if fail_closed:
        inp["expect_fail_closed"] = True
    CASES.append({"func": func, "in": inp})


def raise_script(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def ret_script(value):
    return {"__script__": True, "ret": value}


PARSE = "parse_telegram_message_id"
PTB = "telegram_error_isinstance"
CORO = "iscoroutinefunction"
PAYLOAD = "rich_message_payload"

# ── 1. _metadata_thread_id ──
add("_metadata_thread_id", {"metadata": None})
add("_metadata_thread_id", {"metadata": {"thread_id": "5"}})
add("_metadata_thread_id", {"metadata": {"thread_id": 0, "message_thread_id": 7}})
add("_metadata_thread_id", {"metadata": {"thread_id": None, "message_thread_id": "9"}})
add("_metadata_thread_id", {"metadata": {"message_thread_id": 3}})
add("_metadata_thread_id", {"metadata": {"thread_id": True}})
add("_metadata_thread_id", {"metadata": {}})

# ── 2. _metadata_direct_messages_topic_id ──
add("_metadata_direct_messages_topic_id", {"metadata": None})
add("_metadata_direct_messages_topic_id", {"metadata": {"direct_messages_topic_id": "12"}})
add("_metadata_direct_messages_topic_id",
    {"metadata": {"direct_messages_topic_id": "", "telegram_direct_messages_topic_id": 8}})
add("_metadata_direct_messages_topic_id", {"metadata": {"telegram_direct_messages_topic_id": "4"}})
add("_metadata_direct_messages_topic_id", {"metadata": {"direct_messages_topic_id": 0}})
add("_metadata_direct_messages_topic_id", {"metadata": {}})

# ── 3. _metadata_reply_to_message_id ──
add("_metadata_reply_to_message_id", {"metadata": {"telegram_reply_to_message_id": "77"}}, io={PARSE: 77})
add("_metadata_reply_to_message_id", {"metadata": None}, io={PARSE: None})
add("_metadata_reply_to_message_id", {"metadata": {}}, io={PARSE: None})
add("_metadata_reply_to_message_id", {"metadata": {"telegram_reply_to_message_id": "x"}},
    io={PARSE: raise_script("ValueError", "bad id")})
add("_metadata_reply_to_message_id", {"metadata": {}}, io={}, fail_closed=True)
add("_metadata_reply_to_message_id", {"metadata": {}}, io={PARSE: raise_script("SystemExit", "bye")})

# ── 4. _dm_topic_fallback ──
add("_dm_topic_fallback", {"metadata": None})
add("_dm_topic_fallback", {"metadata": {}})
add("_dm_topic_fallback", {"metadata": {"telegram_dm_topic_reply_fallback": True}})
add("_dm_topic_fallback", {"metadata": {"telegram_dm_topic_reply_fallback": "yes"}})
add("_dm_topic_fallback", {"metadata": {"telegram_dm_topic_reply_fallback": 0}})
add("_dm_topic_fallback", {"metadata": {"telegram_dm_topic_reply_fallback": None}})

# ── 5. _is_private_dm_topic_send ──
add("_is_private_dm_topic_send",
    {"chat_id": "c", "thread_id": None,
     "metadata": {"direct_messages_topic_id": "5", "telegram_dm_topic_reply_fallback": True,
                  "telegram_reply_to_message_id": "9"}}, io={PARSE: 9})
add("_is_private_dm_topic_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"direct_messages_topic_id": "5"}}, io={})
add("_is_private_dm_topic_send",
    {"chat_id": "c", "thread_id": None,
     "metadata": {"direct_messages_topic_id": "5", "telegram_dm_topic_reply_fallback": True}},
    io={PARSE: None})
add("_is_private_dm_topic_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"telegram_dm_topic_created_for_send": True}})
add("_is_private_dm_topic_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"telegram_dm_topic_reply_fallback": True}})
add("_is_private_dm_topic_send",
    {"chat_id": "c", "thread_id": "", "metadata": {"telegram_dm_topic_reply_fallback": True}})
add("_is_private_dm_topic_send", {"chat_id": "c", "thread_id": "5", "metadata": {}})
add("_is_private_dm_topic_send",
    {"chat_id": "c", "thread_id": None,
     "metadata": {"direct_messages_topic_id": "5", "telegram_dm_topic_reply_fallback": True}},
    io={}, fail_closed=True)

# ── 6. _reply_to_message_id_for_send ──
add("_reply_to_message_id_for_send", {"reply_to": "10", "metadata": None}, io={PARSE: 10})
add("_reply_to_message_id_for_send",
    {"reply_to": None, "metadata": {"telegram_dm_topic_reply_fallback": True,
                                    "telegram_reply_to_message_id": "22"}, "reply_to_mode": "first"},
    io={PARSE: [None, 22]})
add("_reply_to_message_id_for_send",
    {"reply_to": None, "metadata": {"telegram_dm_topic_reply_fallback": True}, "reply_to_mode": "off"},
    io={PARSE: None})
add("_reply_to_message_id_for_send", {"reply_to": None, "metadata": {}}, io={PARSE: None})
add("_reply_to_message_id_for_send", {"reply_to": "x"}, io={PARSE: raise_script("RuntimeError", "boom")})
add("_reply_to_message_id_for_send", {"reply_to": "x"}, io={}, fail_closed=True)
add("_reply_to_message_id_for_send", {"reply_to": "x"}, io={PARSE: raise_script("KeyboardInterrupt", "stop")})

# ── 7. _message_thread_id_for_send ──
add("_message_thread_id_for_send", {"thread_id": None})
add("_message_thread_id_for_send", {"thread_id": ""})
add("_message_thread_id_for_send", {"thread_id": "1"})
add("_message_thread_id_for_send", {"thread_id": "5"})
add("_message_thread_id_for_send", {"thread_id": "0"})
add("_message_thread_id_for_send", {"thread_id": 7})
add("_message_thread_id_for_send", {"thread_id": " 12 "})
add("_message_thread_id_for_send", {"thread_id": "abc"})
add("_message_thread_id_for_send", {"thread_id": True})
add("_message_thread_id_for_send", {"thread_id": "1_0"})
add("_message_thread_id_for_send", {"thread_id": "_1"})

# ── 8. _message_thread_id_for_typing ──
add("_message_thread_id_for_typing", {"thread_id": None})
add("_message_thread_id_for_typing", {"thread_id": "1"})
add("_message_thread_id_for_typing", {"thread_id": "5"})
add("_message_thread_id_for_typing", {"thread_id": ""})
add("_message_thread_id_for_typing", {"thread_id": "abc"})
add("_message_thread_id_for_typing", {"thread_id": 0})

# ── 9. _direct_topic_kwargs ──
add("_direct_topic_kwargs", {"metadata": None})
add("_direct_topic_kwargs", {"metadata": {"direct_messages_topic_id": "5"}})
add("_direct_topic_kwargs", {"metadata": {"telegram_direct_messages_topic_id": "7"}})
add("_direct_topic_kwargs", {"metadata": {"direct_messages_topic_id": "abc"}})
add("_direct_topic_kwargs", {"metadata": {"direct_messages_topic_id": "1_2"}})
add("_direct_topic_kwargs", {"metadata": {"direct_messages_topic_id": 0}})

# ── 10. _thread_kwargs_for_send ──
add("_thread_kwargs_for_send", {"chat_id": "c", "thread_id": "5", "metadata": None})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"direct_messages_topic_id": "6"}})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"telegram_dm_topic_reply_fallback": True},
     "reply_to_message_id": 9, "reply_to_mode": "first"})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": "5",
     "metadata": {"telegram_dm_topic_reply_fallback": True, "telegram_reply_to_message_id": "22"},
     "reply_to_mode": "first"}, io={PARSE: 22})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"telegram_dm_topic_reply_fallback": True},
     "reply_to_mode": "first"}, io={PARSE: None})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": None,
     "metadata": {"telegram_dm_topic_reply_fallback": True, "direct_messages_topic_id": "6"},
     "reply_to_mode": "first"}, io={PARSE: None})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": None, "metadata": {"telegram_dm_topic_reply_fallback": True},
     "reply_to_mode": "first"}, io={PARSE: None})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"telegram_dm_topic_reply_fallback": True},
     "reply_to_mode": "off"})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": "abc", "metadata": {"telegram_dm_topic_reply_fallback": True},
     "reply_to_mode": "first"}, io={PARSE: None})
add("_thread_kwargs_for_send",
    {"chat_id": "c", "thread_id": "5", "metadata": {"telegram_dm_topic_reply_fallback": True},
     "reply_to_mode": "first"}, io={}, fail_closed=True)

# ── 11. _thread_kwargs_for_draft ──
add("_thread_kwargs_for_draft",
    {"chat_id": "c", "metadata": {"thread_id": "5", "telegram_dm_topic_reply_fallback": True,
                                  "telegram_reply_to_message_id": "9"}},
    state={"_reply_to_mode": "first"}, io={PARSE: [None, 9]})
add("_thread_kwargs_for_draft", {"chat_id": "c", "metadata": None},
    state={"_reply_to_mode": "first"}, io={PARSE: None})
add("_thread_kwargs_for_draft", {"chat_id": "c", "metadata": {"direct_messages_topic_id": "6"}},
    state={"_reply_to_mode": "first"}, io={PARSE: None})
add("_thread_kwargs_for_draft",
    {"chat_id": "c", "metadata": {"thread_id": "5", "telegram_dm_topic_reply_fallback": True,
                                  "telegram_reply_to_message_id": "9"}},
    state={"_reply_to_mode": "off"}, io={PARSE: None})
add("_thread_kwargs_for_draft",
    {"chat_id": "c", "metadata": {"thread_id": "5", "telegram_dm_topic_reply_fallback": True,
                                  "telegram_reply_to_message_id": "9"}},
    state={}, io={PARSE: [None, 9]})
add("_thread_kwargs_for_draft", {"chat_id": "c", "metadata": {"thread_id": "5"}},
    state={"_reply_to_mode": "first"}, io={}, fail_closed=True)

# ── 12. _should_thread_reply ──
add("_should_thread_reply", {"reply_to": None, "chunk_index": 0})
add("_should_thread_reply", {"reply_to": "5", "chunk_index": 0}, state={"_reply_to_mode": "off"})
add("_should_thread_reply", {"reply_to": "5", "chunk_index": 3}, state={"_reply_to_mode": "all"})
add("_should_thread_reply", {"reply_to": "5", "chunk_index": 0}, state={"_reply_to_mode": "first"})
add("_should_thread_reply", {"reply_to": "5", "chunk_index": 2}, state={"_reply_to_mode": "first"})
add("_should_thread_reply", {"reply_to": "5", "chunk_index": 0}, state={})
add("_should_thread_reply", {"reply_to": "", "chunk_index": 0}, state={"_reply_to_mode": "first"})

# ── 13. _chunk_reply_routing ──
add("_chunk_reply_routing",
    {"chat_id": "c", "reply_to": "10", "metadata": None, "thread_id": "5", "index": 0},
    state={"_reply_to_mode": "first"}, io={PARSE: [None, 10]})
add("_chunk_reply_routing",
    {"chat_id": "c", "reply_to": "10", "metadata": None, "thread_id": "5", "index": 2},
    state={"_reply_to_mode": "first"}, io={PARSE: [None, 10]})
add("_chunk_reply_routing",
    {"chat_id": "c", "reply_to": None, "metadata": {"telegram_dm_topic_reply_fallback": True},
     "thread_id": "5", "index": 0},
    state={"_reply_to_mode": "first"}, io={PARSE: [None, None]})
add("_chunk_reply_routing",
    {"chat_id": "c", "reply_to": None,
     "metadata": {"telegram_dm_topic_reply_fallback": True, "telegram_reply_to_message_id": "9"},
     "thread_id": "5", "index": 0},
    state={"_reply_to_mode": "off"}, io={PARSE: [9, None]})
add("_chunk_reply_routing",
    {"chat_id": "c", "reply_to": None,
     "metadata": {"direct_messages_topic_id": "3", "telegram_dm_topic_reply_fallback": True,
                  "telegram_reply_to_message_id": "9"}, "thread_id": None, "index": 0},
    state={"_reply_to_mode": "first"}, io={PARSE: [9, 9, None]})
add("_chunk_reply_routing",
    {"chat_id": "c", "reply_to": None, "metadata": {"telegram_dm_topic_reply_fallback": True},
     "thread_id": "5", "index": 0},
    state={}, io={PARSE: None})
add("_chunk_reply_routing",
    {"chat_id": "c", "reply_to": "10", "metadata": None, "thread_id": "5", "index": 0},
    state={"_reply_to_mode": "first"}, io={}, fail_closed=True)

# ── 14. _compute_single_send_routing ──
add("_compute_single_send_routing",
    {"chat_id": "c", "reply_to": "10", "metadata": None, "thread_id": "5"},
    state={"_reply_to_mode": "first"}, io={PARSE: [None, 10]})
add("_compute_single_send_routing",
    {"chat_id": "c", "reply_to": None, "metadata": {"telegram_dm_topic_reply_fallback": True},
     "thread_id": None},
    state={"_reply_to_mode": "first"}, io={PARSE: [None, None, None]})
add("_compute_single_send_routing",
    {"chat_id": "c", "reply_to": None,
     "metadata": {"telegram_dm_topic_reply_fallback": True, "telegram_reply_to_message_id": "9"},
     "thread_id": "5"},
    state={"_reply_to_mode": "first"}, io={PARSE: [9, None]})
add("_compute_single_send_routing",
    {"chat_id": "c", "reply_to": None,
     "metadata": {"direct_messages_topic_id": "3", "telegram_dm_topic_reply_fallback": True,
                  "telegram_reply_to_message_id": "9"}, "thread_id": None},
    state={"_reply_to_mode": "first"}, io={PARSE: [9, 9, None]})
add("_compute_single_send_routing",
    {"chat_id": "c", "reply_to": None, "metadata": {"telegram_dm_topic_reply_fallback": True},
     "thread_id": "5"},
    state={"_reply_to_mode": "off"}, io={PARSE: [None, None]})
add("_compute_single_send_routing",
    {"chat_id": "c", "reply_to": None, "metadata": None, "thread_id": "5"},
    state={}, io={PARSE: [None, None]})

# ── 15. _iter_exception_graph ──
add("_iter_exception_graph", {"error": "e1"}, errors={"e1": {"class": "TimedOut", "message": "x"}})
add("_iter_exception_graph", {"error": "e1"}, errors={
    "e1": {"class": "A", "message": "", "cause": "e2"},
    "e2": {"class": "B", "message": "", "cause": "e3"},
    "e3": {"class": "C", "message": ""}})
add("_iter_exception_graph", {"error": "e1"}, errors={
    "e1": {"class": "A", "message": "", "cause": "e2", "context": "e3"},
    "e2": {"class": "B", "message": ""}, "e3": {"class": "C", "message": ""}})
add("_iter_exception_graph", {"error": "e1"}, errors={
    "e1": {"class": "A", "message": "", "cause": "e2"},
    "e2": {"class": "B", "message": "", "cause": "e1"}})
add("_iter_exception_graph", {"error": "e1"}, errors={
    "e1": {"class": "A", "message": "", "cause": "e2", "context": "e3"},
    "e2": {"class": "B", "message": "", "context": "e3"},
    "e3": {"class": "C", "message": ""}})
add("_iter_exception_graph", {"error": "nope"}, errors={"e1": {"class": "A", "message": ""}})

# ── 16. _exception_graph_matches ──
add("_exception_graph_matches",
    {"error": "e1", "name_marker": "connecttimeout", "text_markers": []},
    errors={"e1": {"class": "ConnectTimeout", "message": ""}})
add("_exception_graph_matches",
    {"error": "e1", "name_marker": "connecttimeout",
     "text_markers": ["connect timeout", "connect timed out"]},
    errors={"e1": {"class": "TimedOut", "message": "connect timed out"}})
add("_exception_graph_matches",
    {"error": "e1", "name_marker": "connecttimeout", "text_markers": []},
    errors={"e1": {"class": "TimedOut", "message": "wrapper", "cause": "e2"},
            "e2": {"class": "ConnectTimeout", "message": ""}})
add("_exception_graph_matches",
    {"error": "e1", "name_marker": "pooltimeout", "text_markers": ["pool timeout"]},
    errors={"e1": {"class": "TimedOut", "message": "read timed out"}})
add("_exception_graph_matches",
    {"error": "e1", "name_marker": "zzz", "text_markers": ["occupied"]},
    errors={"e1": {"class": "A", "message": "", "context": "e2"},
            "e2": {"class": "B", "message": "pool occupied"}})
add("_exception_graph_matches",
    {"error": "e1", "name_marker": "zzz", "text_markers": []},
    errors={"e1": {"class": "A", "message": "plain"}})

# ── 17. _looks_like_connect_timeout ──
add("_looks_like_connect_timeout", {"error": "e1"}, errors={"e1": {"class": "ConnectTimeout", "message": ""}})
add("_looks_like_connect_timeout", {"error": "e1"}, errors={"e1": {"class": "TimedOut", "message": "Connect timeout"}})
add("_looks_like_connect_timeout", {"error": "e1"}, errors={
    "e1": {"class": "TimedOut", "message": "wrap", "cause": "e2"},
    "e2": {"class": "ConnectTimeout", "message": ""}})
add("_looks_like_connect_timeout", {"error": "e1"}, errors={"e1": {"class": "TimedOut", "message": "read timed out"}})
add("_looks_like_connect_timeout", {"error": "e1"}, errors={"e1": {"class": "TimedOut", "message": "connect timed out"}})

# ── 18. _looks_like_pool_timeout ──
add("_looks_like_pool_timeout", {"error": "e1"}, errors={"e1": {"class": "PoolTimeout", "message": ""}})
add("_looks_like_pool_timeout", {"error": "e1"}, errors={"e1": {"class": "TimedOut", "message": "pool timeout"}})
add("_looks_like_pool_timeout", {"error": "e1"}, errors={"e1": {"class": "X", "message": "connection pool is occupied"}})
add("_looks_like_pool_timeout", {"error": "e1"}, errors={"e1": {"class": "X", "message": "connection pool"}})
add("_looks_like_pool_timeout", {"error": "e1"}, errors={
    "e1": {"class": "TimedOut", "message": "", "context": "e2"},
    "e2": {"class": "PoolTimeout", "message": ""}})
add("_looks_like_pool_timeout", {"error": "e1"}, errors={"e1": {"class": "X", "message": "plain"}})

# ── 19. _is_thread_not_found_error ──
add("_is_thread_not_found_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "Thread not found"}})
add("_is_thread_not_found_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "message thread not found: 5"}})
add("_is_thread_not_found_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "bad request"}})
add("_is_thread_not_found_error", {"error": "e1"}, errors={"e1": {"class": "X", "message": ""}})
add("_is_thread_not_found_error", {"error": "e1"}, errors={"e1": {"class": "X", "message": "THREAD NOT FOUND"}})

# ── 20. _is_bad_request_error ──
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "x"}})
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "MyBadRequest", "message": "x"}})
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: True})
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: False})
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    state={"ptb_available": False})
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: raise_script("ImportError", "no telegram")})
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: raise_script("SystemExit", "bye")})
add("_is_bad_request_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={}, fail_closed=True)
add("_is_bad_request_error", {})

# ── 21. _is_reply_anchor_missing_error ──
add("_is_reply_anchor_missing_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "Message to be replied not found"}})
add("_is_reply_anchor_missing_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "reply message not found"}})
add("_is_reply_anchor_missing_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "thread not found"}})
add("_is_reply_anchor_missing_error", {"error": "e1"}, errors={"e1": {"class": "X", "message": ""}})
add("_is_reply_anchor_missing_error", {"error": "e1"}, errors={"e1": {"class": "X", "message": "REPLY MESSAGE NOT FOUND"}})

# ── 22. _should_retry_without_dm_topic_reply_anchor ──
FB = {"telegram_dm_topic_reply_fallback": True}
add("_should_retry_without_dm_topic_reply_anchor",
    {"error": "e1", "metadata": {}, "reply_to_message_id": 9},
    errors={"e1": {"class": "BadRequest", "message": "reply message not found"}})
add("_should_retry_without_dm_topic_reply_anchor",
    {"error": "e1", "metadata": FB, "reply_to_message_id": 9},
    errors={"e1": {"class": "BadRequest", "message": "reply message not found"}})
add("_should_retry_without_dm_topic_reply_anchor",
    {"error": "e1", "metadata": FB, "reply_to_message_id": 9},
    errors={"e1": {"class": "Other", "message": "reply message not found"}}, io={PTB: True})
add("_should_retry_without_dm_topic_reply_anchor",
    {"error": "e1", "metadata": FB, "reply_to_message_id": None},
    errors={"e1": {"class": "BadRequest", "message": "reply message not found"}})
add("_should_retry_without_dm_topic_reply_anchor",
    {"error": "e1", "metadata": FB, "reply_to_message_id": 9},
    errors={"e1": {"class": "BadRequest", "message": "other"}})
add("_should_retry_without_dm_topic_reply_anchor",
    {"error": "e1", "metadata": FB, "reply_to_message_id": 9},
    errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: raise_script("SystemExit", "bye")})
add("_should_retry_without_dm_topic_reply_anchor",
    {"error": "e1", "metadata": FB, "reply_to_message_id": 9},
    errors={"e1": {"class": "Other", "message": "x"}}, io={}, fail_closed=True)

# ── 23. _looks_like_auth_error ──
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "InvalidToken", "message": "x"}})
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "Forbidden", "message": "x"}})
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: [True]})
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: [False, True]})
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: [False, False]})
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    state={"ptb_available": False})
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: [raise_script("ImportError", "no telegram")]})
add("_looks_like_auth_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: [raise_script("SystemExit", "bye")]})

# ── 24. _looks_like_network_error ──
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "x"}})
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "NetworkError", "message": "x"}})
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "TimedOut", "message": "x"}})
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: [False, False, False, False, True]})
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: [True]})
add("_looks_like_network_error", {"error": "e1"},
    errors={"e1": {"class": "Other", "message": "x", "base": "OSError"}},
    io={PTB: [False, False, False, False, False, False]})
add("_looks_like_network_error", {"error": "e1"},
    errors={"e1": {"class": "Other", "message": "x", "base": "OSError"}},
    state={"ptb_available": False})
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    state={"ptb_available": False})
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: [raise_script("ImportError", "no telegram")]})
add("_looks_like_network_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: [raise_script("SystemExit", "bye")]})

# ── 25. _is_timed_out ──
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: True})
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "timed out"}}, io={PTB: False})
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "other"}}, io={PTB: False})
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "Timed out"}},
    state={"ptb_available": False})
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    state={"ptb_available": False})
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "timed out"}},
    state={"timedout_missing": True})
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: raise_script("ImportError", "late")})
add("_is_timed_out", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={}, fail_closed=True)

# ── 26. _is_rich_capability_error ──
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "EndpointNotFound", "message": "x"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "InvalidToken", "message": "x"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "AttributeError", "message": "x"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "MyErr", "message": "x", "base": "TypeError"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "MyErr", "message": "x", "base": "NotImplementedError"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x", "error_code": 404}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "Method not found"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "endpoint does not exist"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "no such method"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "bad request: parse"}})
add("_is_rich_capability_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x", "error_code": 400}})

# ── 27. _is_rich_fallback_error ──
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "BadRequest", "message": "x"}})
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "EndpointNotFound", "message": "x"}},
    io={PTB: False})
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "unsupported"}},
    io={PTB: False})
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "not implemented"}},
    io={PTB: False})
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "transient"}},
    io={PTB: False})
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    state={"ptb_available": False})
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}}, io={PTB: True})
add("_is_rich_fallback_error", {"error": "e1"}, errors={"e1": {"class": "Other", "message": "x"}},
    io={PTB: raise_script("SystemExit", "bye")})

# ── 28. _rich_delivery_enabled ──
add("_rich_delivery_enabled", {}, state={})
add("_rich_delivery_enabled", {}, state={"_rich_messages_enabled": True})
add("_rich_delivery_enabled", {}, state={"_rich_messages_enabled": False})
add("_rich_delivery_enabled", {}, state={"_rich_messages_enabled": 0})
add("_rich_delivery_enabled", {}, state={"_rich_messages_enabled": "yes"})

# ── 29. _bot_supports_rich ──
add("_bot_supports_rich", {}, state={"_bot": None}, io={CORO: False})
add("_bot_supports_rich", {}, state={"_bot": {"do_api_request": {"fn": "do_api_request"}}}, io={CORO: True})
add("_bot_supports_rich", {}, state={"_bot": {}}, io={CORO: False})
add("_bot_supports_rich", {}, state={"_bot": None}, io={}, fail_closed=True)
add("_bot_supports_rich", {}, state={"_bot": None}, io={CORO: raise_script("SystemExit", "bye")})
add("_bot_supports_rich", {}, state={})

# ── 30. _needs_rich_rendering ──
add("_needs_rich_rendering", {"content": ""})
add("_needs_rich_rendering", {"content": None})
add("_needs_rich_rendering", {"content": "| a | b |\n| --- | --- |"})
add("_needs_rich_rendering", {"content": "- [ ] task"})
add("_needs_rich_rendering", {"content": "- [x] done"})
add("_needs_rich_rendering", {"content": "<details>x</details>"})
add("_needs_rich_rendering", {"content": "$$x$$"})
add("_needs_rich_rendering", {"content": "plain text"})
add("_needs_rich_rendering", {"content": "| --- |"})
add("_needs_rich_rendering", {"content": "  |---|---|"})
add("_needs_rich_rendering", {"content": "<DETAILS>"})
add("_needs_rich_rendering", {"content": "text\n</summary>"})
add("_needs_rich_rendering", {"content": "- [ ]no-space"})

# ── 31. _has_telegram_desktop_cjk_rich_garble_shape ──
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": ""})
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": None})
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": "hello"})
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": "你好"})
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": "カナ"})
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": "한글"})
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": "\U00020000"})
add("_has_telegram_desktop_cjk_rich_garble_shape", {"content": "é"})

# ── 32. _has_telegram_desktop_details_math_crash_shape ──
add("_has_telegram_desktop_details_math_crash_shape", {"content": ""})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<details>plain</details>"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<details>$$x$$</details>"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<details>\\frac{a}{b}</details>"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<details>\\begin{align}x\\end{align}</details>"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<DETAILS>$$x$$</DETAILS>"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<details>x"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<detailsx>$$x$$</detailsx>"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "text <details>\\(y\\)</details> tail"})
add("_has_telegram_desktop_details_math_crash_shape", {"content": "<details>\\begin{aligned}</details>"})

# ── 33. _content_fits_rich_limits ──
add("_content_fits_rich_limits", {"content": "hi"}, io={PAYLOAD: {"markdown": "hello"}})
add("_content_fits_rich_limits", {"content": "hi"}, io={PAYLOAD: {"markdown": "x" * 32768}})
add("_content_fits_rich_limits", {"content": "hi"}, io={PAYLOAD: {"markdown": "x" * 32769}})
add("_content_fits_rich_limits", {"content": "hi"}, io={PAYLOAD: {}})
add("_content_fits_rich_limits", {"content": "hi"}, io={PAYLOAD: None})
add("_content_fits_rich_limits", {"content": "hi"}, io={}, fail_closed=True)
add("_content_fits_rich_limits", {"content": "hi"}, io={PAYLOAD: raise_script("ValueError", "bad")})
add("_content_fits_rich_limits", {"content": "hi"}, io={PAYLOAD: {"markdown": 5}})

# ── 34. _rich_content_ok ──
RICH_STATE = {"_bot": {"do_api_request": {"fn": "x"}}, "_allow_cjk_rich_messages": False}
add("_rich_content_ok", {"content": ""}, state=RICH_STATE)
add("_rich_content_ok", {"content": "   "}, state=RICH_STATE)
add("_rich_content_ok", {"content": "<details>$$x$$</details>"}, state=RICH_STATE)
add("_rich_content_ok", {"content": "你好"}, state=RICH_STATE)
add("_rich_content_ok", {"content": "你好"},
    state={**RICH_STATE, "_allow_cjk_rich_messages": True},
    io={PAYLOAD: {"markdown": "你好"}, CORO: True})
add("_rich_content_ok", {"content": "table |x|", "check_size": False},
    state=RICH_STATE, io={CORO: True})
add("_rich_content_ok", {"content": "hello"}, state=RICH_STATE,
    io={PAYLOAD: {"markdown": "x" * 32769}, CORO: True})
add("_rich_content_ok", {"content": "hello"}, state=RICH_STATE,
    io={PAYLOAD: {"markdown": "ok"}, CORO: False})
add("_rich_content_ok", {"content": "hello"}, state=RICH_STATE, io={}, fail_closed=True)

# ── 35. _rich_eligible ──
add("_rich_eligible", {"content": "| a | b |\n| --- | --- |"},
    state={"_rich_messages_enabled": False})
add("_rich_eligible", {"content": "| a | b |\n| --- | --- |"},
    state={"_rich_send_disabled": True})
add("_rich_eligible", {"content": "plain"}, state=RICH_STATE)
add("_rich_eligible", {"content": "| a | b |\n| --- | --- |"},
    state=RICH_STATE, io={PAYLOAD: {"markdown": "ok"}, CORO: True})
add("_rich_eligible", {"content": ""}, state=RICH_STATE)
add("_rich_eligible", {"content": "你好\n| --- | --- |"}, state=RICH_STATE)
add("_rich_eligible", {"content": "- [ ] task"}, state=RICH_STATE, io={}, fail_closed=True)

# ── 36. _should_attempt_rich ──
TABLE = "| a | b |\n| --- | --- |"
add("_should_attempt_rich", {"content": TABLE, "metadata": {"expect_edits": True}}, state=RICH_STATE)
add("_should_attempt_rich", {"content": TABLE, "metadata": {"expect_edits": False}},
    state=RICH_STATE, io={PAYLOAD: {"markdown": "ok"}, CORO: True})
add("_should_attempt_rich", {"content": TABLE, "metadata": None},
    state=RICH_STATE, io={PAYLOAD: {"markdown": "ok"}, CORO: True})
add("_should_attempt_rich", {"content": TABLE, "metadata": {"expect_edits": 1}}, state=RICH_STATE)
add("_should_attempt_rich", {"content": "plain", "metadata": None}, state=RICH_STATE)

# ── 37. prefers_fresh_final_streaming ──
add("prefers_fresh_final_streaming", {"content": TABLE, "metadata": None}, state=RICH_STATE)
add("prefers_fresh_final_streaming", {"content": TABLE, "metadata": {}}, state=RICH_STATE)
add("prefers_fresh_final_streaming",
    {"content": TABLE, "metadata": {"telegram_dm_topic_reply_fallback": True}},
    state=RICH_STATE, io={PAYLOAD: {"markdown": "ok"}, CORO: True})
add("prefers_fresh_final_streaming",
    {"content": TABLE, "metadata": {"direct_messages_topic_id": "3"}},
    state=RICH_STATE, io={PAYLOAD: {"markdown": "ok"}, CORO: True})
add("prefers_fresh_final_streaming",
    {"content": "plain", "metadata": {"telegram_dm_topic_reply_fallback": True}}, state=RICH_STATE)
add("prefers_fresh_final_streaming",
    {"content": TABLE, "metadata": {"telegram_dm_topic_reply_fallback": 0}}, state=RICH_STATE)

# ── 38. _rich_transport_available ──
add("_rich_transport_available", {}, state={"_bot": None}, io={CORO: True})
add("_rich_transport_available", {}, state={"_rich_messages_enabled": False})
add("_rich_transport_available", {}, state={"_rich_send_disabled": True})
add("_rich_transport_available", {}, state={"_bot": None}, io={CORO: False})
add("_rich_transport_available", {}, state={"_bot": None}, io={}, fail_closed=True)

# ── 39. streaming_overflow_limit ──
add("streaming_overflow_limit", {}, state={"_bot": None}, io={CORO: True})
add("streaming_overflow_limit", {}, state={"_bot": None}, io={CORO: False})
add("streaming_overflow_limit", {}, state={"_rich_messages_enabled": False})
add("streaming_overflow_limit", {}, state={"_rich_send_disabled": True})
add("streaming_overflow_limit", {}, state={"_bot": None}, io={CORO: raise_script("SystemExit", "bye")})

# ── 40. _normalize_chat_type ──
add("_normalize_chat_type", {"chat_type": "private", "is_forum": False})
add("_normalize_chat_type", {"chat_type": "PRIVATE ", "is_forum": False})
add("_normalize_chat_type", {"chat_type": "supergroup", "is_forum": True})
add("_normalize_chat_type", {"chat_type": "supergroup", "is_forum": False})
add("_normalize_chat_type", {"chat_type": "channel", "is_forum": False})
add("_normalize_chat_type", {"chat_type": None, "is_forum": False})
add("_normalize_chat_type", {"chat_type": "", "is_forum": False})
add("_normalize_chat_type", {"chat_type": "   ", "is_forum": False})
add("_normalize_chat_type", {"chat_type": 0, "is_forum": False})
add("_normalize_chat_type", {"chat_type": "Group", "is_forum": False})
add("_normalize_chat_type", {"chat_type": True, "is_forum": False})

# ── 41. _chat_type_str ──
add("_chat_type_str", {"chat": None})
add("_chat_type_str", {"chat": {}})
add("_chat_type_str", {"chat": {"type": "ChatType.SUPERGROUP"}})
add("_chat_type_str", {"chat": {"type": "private"}})
add("_chat_type_str", {"chat": {"x": 1}})
add("_chat_type_str", {"chat": {"type": None}})
add("_chat_type_str", {"chat": 5})

# ── 42. _gateway_chat_type ──
add("_gateway_chat_type", {"chat": {"type": "supergroup"}, "is_forum": True})
add("_gateway_chat_type", {"chat": {"type": "supergroup"}, "is_forum": False})
add("_gateway_chat_type", {"chat": {"type": "private"}, "is_forum": False})
add("_gateway_chat_type", {"chat": {"type": "channel"}, "is_forum": False})
add("_gateway_chat_type", {"chat": None, "is_forum": False})
add("_gateway_chat_type", {"chat": {"type": "weird"}, "is_forum": False})

# ── 43. _coerce_bool_extra ──
add("_coerce_bool_extra", {"key": "flag", "default": False}, state={"config": {"extra": {"flag": "true"}}})
add("_coerce_bool_extra", {"key": "flag", "default": False}, state={"config": {"extra": {"flag": " OFF "}}})
add("_coerce_bool_extra", {"key": "flag", "default": True}, state={"config": {"extra": {"flag": "maybe"}}})
add("_coerce_bool_extra", {"key": "flag", "default": True}, state={"config": {"extra": {}}})
add("_coerce_bool_extra", {"key": "flag", "default": True}, state={"config": None})
add("_coerce_bool_extra", {"key": "flag", "default": False}, state={"config": {"extra": {"flag": 1}}})
add("_coerce_bool_extra", {"key": "flag", "default": False}, state={"config": {"extra": {"flag": 0}}})
add("_coerce_bool_extra", {"key": "flag", "default": True}, state={"config": {"extra": {"flag": None}}})
add("_coerce_bool_extra", {"key": "flag", "default": False}, state={"config": {"extra": {"flag": ""}}})
add("_coerce_bool_extra", {"key": "flag", "default": False}, state={"config": {"extra": {"flag": []}}})
add("_coerce_bool_extra", {"key": "flag", "default": False}, state={"config": {"extra": {"flag": "YES"}}})

# ── 44. _notification_kwargs ──
add("_notification_kwargs", {"metadata": None}, state={"_notifications_mode": "important"})
add("_notification_kwargs", {"metadata": None}, state={"_notifications_mode": "all"})
add("_notification_kwargs", {"metadata": {"notify": True}}, state={"_notifications_mode": "important"})
add("_notification_kwargs", {"metadata": {"is_approval_prompt": True}}, state={"_notifications_mode": "important"})
add("_notification_kwargs", {"metadata": None}, state={})
add("_notification_kwargs", {"metadata": {}}, state={"_notifications_mode": "important"})
add("_notification_kwargs", {"metadata": {"notify": 0}}, state={"_notifications_mode": "important"})

# ── 45. _send_retry_is_final ──
add("_send_retry_is_final", {"result": {"raw_response": {"delivery_uncertain": True}}})
add("_send_retry_is_final", {"result": {"raw_response": {"delivery_uncertain": False}}})
add("_send_retry_is_final", {"result": {"raw_response": {}}})
add("_send_retry_is_final", {"result": {"raw_response": None}})
add("_send_retry_is_final", {"result": {"raw_response": "x"}})
add("_send_retry_is_final", {"result": {"raw_response": {"delivery_uncertain": "yes"}}})
add("_send_retry_is_final", {"result": None})
add("_send_retry_is_final", {"result": {}})
add("_send_retry_is_final", {"result": 5})


def validate():
    funcs = {}
    for c in CASES:
        funcs[c["func"]] = funcs.get(c["func"], 0) + 1
        inp = c["in"]
        errs = inp.get("errors") or {}
        for eid, e in errs.items():
            assert e.get("class") != "KeyError", f"KeyError registry class in {c['func']}"
            for ref in ("cause", "context"):
                if e.get(ref):
                    assert e[ref] in errs, f"dangling {ref} in {c['func']}"
    assert len(funcs) == 45, f"expected 45 funcs, got {len(funcs)}"
    short = {f: n for f, n in funcs.items() if n < 5}
    assert not short, f"funcs with <5 cases: {short}"
    return funcs


if __name__ == "__main__":
    funcs = validate()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases for {len(funcs)} funcs -> {OUT}")