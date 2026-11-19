#!/usr/bin/env python3
"""Generate tests/fixtures/telegram_adapter_flow2.jsonl for the Telegram
adapter FLOW remainder port. >=5 cases per ported func: normal scripted-seam
paths, branch variants, state-mutation echoes, attribute-absent raises,
exception swallow/pass-through, and one fail-closed (empty io) case per flow
that fires a seam. Deterministic; ASCII corpus; integral-int counters; short
exactly-representable float clocks (libjson %.17g round-trip).

Run:  python tests/gen_telegram_adapter_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "telegram_adapter_flow2.jsonl")

CASES = []


def add(func, args=None, state=None, io=None, errors=None, fail_closed=False):
    inp = {}
    if args is not None:
        inp["args"] = args
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = io
    if errors is not None:
        inp["errors"] = errors
    if fail_closed:
        inp["expect_fail_closed"] = True
    CASES.append({"func": func, "in": inp})


def sc(raise_type=None, message="", ret=None):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": message}
    else:
        d["ret"] = ret
    return d


def poll_state(**over):
    st = {
        "platform": {"value": "telegram"},
        "_polling_teardown_started": False,
        "_polling_progress_accepting": True,
        "_polling_generation": 3,
        "_polling_progress_event": {"id": "ev1", "set": False},
        "_polling_network_error_count": 0,
        "_fatal_error_message": None,
        "_polling_error_task": None,
        "_webhook_mode": False,
        "_updates_received_total": 0,
        "_updates_dispatched_total": 0,
        "_ingress_dispatched_seen": 0,
        "_ingress_stalled_heartbeats": 0,
    }
    st.update(over)
    return st


def progress_io(**over):
    io = {
        "event_is_set": False,
        "logger.info": None,
        "event_set": None,
        "monotonic": 100.5,
        "self._note_conflict_quiet_progress": None,
        "mark_send_path_recovered": None,
    }
    io.update(over)
    return io


# ── _fence_polling ──
add("_fence_polling", state={})
add("_fence_polling", state={"_polling_progress_accepting": True, "_send_path_degraded": False})
add("_fence_polling", state={"_polling_progress_accepting": False, "_send_path_degraded": True})
add("_fence_polling", state={"_polling_progress_accepting": True, "_keep": "keep", "_send_path_degraded": False})
add("_fence_polling", state={"_polling_progress_accepting": True, "_polling_generation": 9})

# ── _begin_polling_generation ──
add("_begin_polling_generation",
    state=poll_state(_polling_progress_verifier_task=None),
    io={"event_create": "ev2", "self._bank_conflict_quiet_window": None, "monotonic": 100.5})
add("_begin_polling_generation",
    state=poll_state(_polling_teardown_started=True), io={})
add("_begin_polling_generation",
    state=poll_state(_polling_teardown_started=True, _polling_progress_event=None),
    io={"event_create": "ev9"})
add("_begin_polling_generation",
    state=poll_state(_polling_progress_verifier_task={"id": "t1"}),
    io={"task_done": False, "task_cancel": None, "event_create": "ev2",
        "self._bank_conflict_quiet_window": None, "monotonic": 100.5})
add("_begin_polling_generation",
    state=poll_state(_polling_progress_verifier_task={"id": "t1"}),
    io={"task_done": True, "event_create": "ev2",
        "self._bank_conflict_quiet_window": None, "monotonic": 100.5})
add("_begin_polling_generation", state=poll_state(), io={}, fail_closed=True)
add("_begin_polling_generation", state=poll_state(),
    io={"event_create": sc("SystemExit", "bye")})
add("_begin_polling_generation", state=poll_state(_polling_generation=None),
    io={"event_create": "ev2", "self._bank_conflict_quiet_window": None, "monotonic": 100.5})

# ── _record_polling_progress ──
add("_record_polling_progress", args={"generation": 3}, state=poll_state(), io=progress_io())
add("_record_polling_progress", args={"generation": 3},
    state=poll_state(_polling_network_error_count=5), io=progress_io())
add("_record_polling_progress", args={"generation": 3},
    state=poll_state(_polling_progress_event={"id": "ev1", "set": True}),
    io=progress_io(event_is_set=True))
add("_record_polling_progress", args={"generation": 3},
    state=poll_state(_polling_teardown_started=True), io={})
add("_record_polling_progress", args={"generation": 3},
    state=poll_state(_polling_progress_accepting=False), io={})
add("_record_polling_progress", args={"generation": 99}, state=poll_state(), io={})
add("_record_polling_progress", args={"generation": 3}, state=poll_state(), io={}, fail_closed=True)
add("_record_polling_progress", args={"generation": 3}, state=poll_state(),
    io=progress_io(monotonic=sc("SystemExit", "bye")))
add("_record_polling_progress", args={"generation": 3},
    state=poll_state(_polling_progress_accepting=None), io={})

# ── _observe_polling_request_result ──
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": [200, "p"]},
    state=poll_state(),
    io={"request_parse_json_payload": {"ok": True, "result": [{"u": 1}, {"u": 2}]},
        "event_is_set": True, "event_set": None, "monotonic": 100.5,
        "self._note_conflict_quiet_progress": None, "mark_send_path_recovered": None})
add("_observe_polling_request_result",
    args={"request": {}, "generation": None, "result": [200, "p"]}, state=poll_state(), io={})
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": [404, "p"]}, state=poll_state(), io={})
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": [1]}, state=poll_state(), io={})
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": None}, state=poll_state(), io={})
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": [200, "p"]}, state=poll_state(),
    io={"request_parse_json_payload": sc("ValueError", "bad json")})
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": [200, "p"]}, state=poll_state(),
    io={"request_parse_json_payload": sc("SystemExit", "bye")})
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": [200, "p"]}, state=poll_state(),
    io={"request_parse_json_payload": {"ok": False, "result": []}})
add("_observe_polling_request_result",
    args={"request": None, "generation": 3, "result": [200, "p"]}, state=poll_state(), io={})
add("_observe_polling_request_result",
    args={"request": {}, "generation": 3, "result": [200, "p"]}, state=poll_state(),
    io={}, fail_closed=True)

# ── _record_updates_received ──
add("_record_updates_received", args={"result": [1, 2, 3]},
    state={"_updates_received_total": 10})
add("_record_updates_received", args={"result": []}, state={"_updates_received_total": 10})
add("_record_updates_received", args={"result": None}, state={"_updates_received_total": 10})
add("_record_updates_received", args={"result": "x"}, state={"_updates_received_total": 10})
add("_record_updates_received", args={"result": [1]}, state={})
add("_record_updates_received", args={"result": [1]}, state={"_updates_received_total": None})

# ── _verifier_stale ──
add("_verifier_stale", args={"generation": 3, "progress": {"id": "ev1"}},
    state=poll_state(), io={"event_is_set": False})
add("_verifier_stale", args={"generation": 3, "progress": {"id": "ev1"}},
    state=poll_state(), io={"event_is_set": True})
add("_verifier_stale", args={"generation": 3, "progress": {"id": "ev1"}},
    state=poll_state(_polling_teardown_started=True), io={})
add("_verifier_stale", args={"generation": 99, "progress": {"id": "ev1"}},
    state=poll_state(), io={"event_is_set": False})
add("_verifier_stale", args={"generation": 3, "progress": {"id": "ev2"}},
    state=poll_state(), io={"event_is_set": False})
add("_verifier_stale", args={"generation": 3, "progress": {"id": "ev1"}},
    state=poll_state(_polling_progress_accepting=False), io={"event_is_set": False})
add("_verifier_stale", args={"generation": 3, "progress": None}, state=poll_state(), io={})
add("_verifier_stale", args={"generation": 3, "progress": {"id": "ev1"}},
    state=poll_state(), io={}, fail_closed=True)

# ── _recovery_in_flight ──
add("_recovery_in_flight", state={"_polling_error_task": None})
add("_recovery_in_flight", state={"_polling_error_task": {"id": "t1"}}, io={"task_done": False})
add("_recovery_in_flight", state={"_polling_error_task": {"id": "t1"}}, io={"task_done": True})
add("_recovery_in_flight", state={})
add("_recovery_in_flight", state={"_polling_error_task": {"id": "t1"}}, io={}, fail_closed=True)
add("_recovery_in_flight", state={"_polling_error_task": {"id": "t1"}},
    io={"task_done": sc("SystemExit", "bye")})

# ── _check_ingress_dispatch_stall ──
add("_check_ingress_dispatch_stall", state=poll_state(_webhook_mode=True), io={})
add("_check_ingress_dispatch_stall",
    state=poll_state(_updates_received_total=10, _updates_dispatched_total=10), io={})
add("_check_ingress_dispatch_stall",
    state=poll_state(_updates_received_total=10, _updates_dispatched_total=5,
                     _ingress_dispatched_seen=5, _ingress_stalled_heartbeats=0), io={})
add("_check_ingress_dispatch_stall",
    state=poll_state(_updates_received_total=10, _updates_dispatched_total=5,
                     _ingress_dispatched_seen=5, _ingress_stalled_heartbeats=3),
    io={"self._schedule_polling_recovery": None})
add("_check_ingress_dispatch_stall",
    state=poll_state(_updates_received_total=10, _updates_dispatched_total=5,
                     _ingress_dispatched_seen=5, _ingress_stalled_heartbeats=4), io={})
add("_check_ingress_dispatch_stall",
    state=poll_state(_updates_received_total=10, _updates_dispatched_total=5,
                     _ingress_dispatched_seen=5, _ingress_stalled_heartbeats=3,
                     _polling_error_task={"id": "t1"}),
    io={"task_done": False})
add("_check_ingress_dispatch_stall",
    state=poll_state(_fatal_error_message="dead"), io={})
add("_check_ingress_dispatch_stall",
    state=poll_state(_updates_received_total=10, _updates_dispatched_total=5,
                     _ingress_dispatched_seen=5, _ingress_stalled_heartbeats=3),
    io={}, fail_closed=True)
add("_check_ingress_dispatch_stall",
    state=poll_state(_updates_received_total=10, _updates_dispatched_total=5,
                     _ingress_dispatched_seen=2, _ingress_stalled_heartbeats=3), io={})

# ── _current_bot_username ──
add("_current_bot_username", state={"_bot_username_observed": "foo"})
add("_current_bot_username", state={"_bot": {"username": "@Bar"}})
add("_current_bot_username", state={"_bot": None})
add("_current_bot_username", state={})
add("_current_bot_username", state={"_bot": {"username": 5}})
add("_current_bot_username", state={"_bot_username_observed": "", "_bot": {"username": "@X"}})
add("_current_bot_username", state={"_bot": {"username": "@@Deep"}})

# ── _note_bot_username ──
add("_note_bot_username", args={"username": "@New"}, state={"platform": {"value": "telegram"}},
    io={"monotonic": 100.5})
add("_note_bot_username", args={"username": "@New"},
    state={"platform": {"value": "telegram"}, "_bot_username_observed": "old"},
    io={"monotonic": 100.5, "logger.info": None})
add("_note_bot_username", args={"username": None}, state={}, io={})
add("_note_bot_username", args={"username": ""}, state={}, io={})
add("_note_bot_username", args={"username": "same"},
    state={"_bot_username_observed": "same"}, io={})
add("_note_bot_username", args={"username": "@New"}, state={}, io={}, fail_closed=True)
add("_note_bot_username", args={"username": 5}, state={}, io={})
add("_note_bot_username", args={"username": "@New"}, state={},
    io={"monotonic": sc("SystemExit", "bye")})

# ── _observe_bot_identity_from_message ──
add("_observe_bot_identity_from_message",
    args={"message": {"from_user": {"id": 5, "username": "@Bot"}}},
    state={"_bot": {"id": 5}, "platform": {"value": "telegram"}}, io={"monotonic": 100.5})
add("_observe_bot_identity_from_message",
    args={"message": {"from_user": {"id": 5}}}, state={"_bot": {"id": None}}, io={})
add("_observe_bot_identity_from_message", args={"message": {}}, state={})
add("_observe_bot_identity_from_message",
    args={"message": {"from_user": {"id": 9, "username": "@Other"}}},
    state={"_bot": {"id": 5}}, io={})
add("_observe_bot_identity_from_message",
    args={"message": {"reply_to_message": {"from_user": {"id": 5, "username": "@Bot"}}}},
    state={"_bot": {"id": 5}, "platform": {"value": "telegram"}}, io={"monotonic": 100.5})
add("_observe_bot_identity_from_message", args={"message": None}, state={"_bot": {"id": 5}}, io={})
add("_observe_bot_identity_from_message",
    args={"message": {"from_user": {"id": 5, "username": 7}}},
    state={"_bot": {"id": 5}}, io={})

# ── _bot_identity_is_fresh ──
add("_bot_identity_is_fresh", state={})
add("_bot_identity_is_fresh", state={"_bot_identity_checked_at": None})
add("_bot_identity_is_fresh", state={"_bot_identity_checked_at": 100.5}, io={"monotonic": 200.5})
add("_bot_identity_is_fresh", state={"_bot_identity_checked_at": 100.5}, io={"monotonic": 500.5})
add("_bot_identity_is_fresh", state={"_bot_identity_checked_at": "x"}, io={"monotonic": 200.5})
add("_bot_identity_is_fresh", state={"_bot_identity_checked_at": 100.5}, io={}, fail_closed=True)
add("_bot_identity_is_fresh", state={"_bot_identity_checked_at": 100.5}, io={"monotonic": 400.5})

# ── _is_reply_to_bot ──
add("_is_reply_to_bot", args={"message": {"reply_to_message": {"from_user": {"id": 5}}}},
    state={"_bot": {"id": 5}})
add("_is_reply_to_bot", args={"message": {"reply_to_message": {"from_user": {"id": 5}}}},
    state={"_bot": None})
add("_is_reply_to_bot", args={"message": {}}, state={})
add("_is_reply_to_bot", args={"message": {"reply_to_message": None}}, state={"_bot": {"id": 5}})
add("_is_reply_to_bot",
    args={"message": {"message_thread_id": 7,
                      "reply_to_message": {"forum_topic_created": {}, "message_id": 7,
                                           "from_user": {"id": 5}}}},
    state={"_bot": {"id": 5}})
add("_is_reply_to_bot", args={"message": {"reply_to_message": {"from_user": {"id": 9}}}},
    state={"_bot": {"id": 5}})
add("_is_reply_to_bot", args={"message": {"reply_to_message": {}}}, state={"_bot": {"id": 5}})

# ── _effective_message_thread_id ──
add("_effective_message_thread_id",
    args={"message": {"chat": {"type": "supergroup", "is_forum": True}}},
    io={"self._chat_type_str": "supergroup"})
add("_effective_message_thread_id",
    args={"message": {"chat": {"type": "supergroup", "is_forum": True}, "message_thread_id": 7}},
    io={"self._chat_type_str": "supergroup"})
add("_effective_message_thread_id",
    args={"message": {"chat": {"type": "group"}, "message_thread_id": 7, "is_topic_message": True}},
    io={"self._chat_type_str": "group"})
add("_effective_message_thread_id",
    args={"message": {"chat": {"type": "group"}, "message_thread_id": 7}},
    io={"self._chat_type_str": "group"})
add("_effective_message_thread_id",
    args={"message": {"chat": {"type": "private"}, "message_thread_id": 7, "is_topic_message": True}},
    io={"self._chat_type_str": "private"})
add("_effective_message_thread_id",
    args={"message": {"chat": {"type": "private"}, "message_thread_id": 7}},
    io={"self._chat_type_str": "private"})
add("_effective_message_thread_id", args={"message": {"chat": {"type": "dm"}}},
    io={"self._chat_type_str": "dm"})
add("_effective_message_thread_id", args={"message": {}}, io={}, fail_closed=True)
add("_effective_message_thread_id", args={"message": {}},
    io={"self._chat_type_str": sc("SystemExit", "bye")})

# ── _extra_bool ──
add("_extra_bool", args={"key": "require_mention", "env_name": "TELEGRAM_REQUIRE_MENTION", "default": "false"},
    state={"config": {"extra": {}}}, io={"_extra_or_secret": "true"})
add("_extra_bool",
    args={"key": "observe", "env_name": "E", "default": "false", "fallback_keys": ["alt"]},
    state={"config": {"extra": {"alt": "yes"}}}, io={"_extra_or_secret": None})
add("_extra_bool", args={"key": "k", "env_name": "E", "default": "false"},
    state={"config": {"extra": {}}}, io={"_extra_or_secret": None})
add("_extra_bool", args={"key": "k", "env_name": "E", "default": "true"},
    state={"config": {"extra": {}}}, io={"_extra_or_secret": False})
add("_extra_bool", args={"key": "k", "env_name": "E", "default": "false"},
    state={"config": {"extra": {}}}, io={"_extra_or_secret": " ON "})
add("_extra_bool", args={"key": "k", "env_name": "E", "default": "true"},
    state={"config": {"extra": {}}}, io={"_extra_or_secret": "0"})
add("_extra_bool", args={"key": "k", "env_name": "E", "default": "false"}, state={})
add("_extra_bool", args={"key": "k", "env_name": "E", "default": "false"},
    state={"config": {"extra": {}}}, io={}, fail_closed=True)
add("_extra_bool",
    args={"key": "k", "env_name": "E", "default": "false", "fallback_keys": ["alt"]},
    state={"config": {"extra": None}}, io={"_extra_or_secret": None})

# ── _extra_str_set ──
add("_extra_str_set", args={"key": "allowed_chats", "env_name": "E"},
    state={"config": {"extra": {}}},
    io={"_extra_or_secret": "a,b,,c", "_decode_json_list_literal": "a,b,,c"})
add("_extra_str_set", args={"key": "k", "env_name": "E"}, state={"config": {"extra": {}}},
    io={"_extra_or_secret": '["x","y"]', "_decode_json_list_literal": [["x", "y"]]})
add("_extra_str_set", args={"key": "k", "env_name": "E"}, state={"config": {"extra": {}}},
    io={"_extra_or_secret": None, "_decode_json_list_literal": None})
add("_extra_str_set", args={"key": "k", "env_name": "E"}, state={"config": {"extra": {}}},
    io={"_extra_or_secret": [" a ", "", "b"], "_decode_json_list_literal": [[" a ", "", "b"]]})
add("_extra_str_set", args={"key": "k", "env_name": "E"}, state={"config": {"extra": {}}},
    io={"_extra_or_secret": "", "_decode_json_list_literal": ""})
add("_extra_str_set", args={"key": "k", "env_name": "E"}, state={"config": {"extra": {}}},
    io={}, fail_closed=True)
add("_extra_str_set", args={"key": "k", "env_name": "E"}, state={"config": {"extra": {}}},
    io={"_extra_or_secret": "a,a,b", "_decode_json_list_literal": "a,a,b"})

# ── _telegram_ignored_threads ──
add("_telegram_ignored_threads", state={"config": {"extra": {}}, "platform": {"value": "telegram"}},
    io={"_extra_or_secret": "1,2,3", "_decode_json_list_literal": "1,2,3"})
add("_telegram_ignored_threads", state={"config": {"extra": {}}, "platform": {"value": "telegram"}},
    io={"_extra_or_secret": "1,abc,3", "_decode_json_list_literal": "1,abc,3", "logger.warning": None})
add("_telegram_ignored_threads", state={"config": {"extra": {}}, "platform": {"value": "telegram"}},
    io={"_extra_or_secret": [1, "x", 2], "_decode_json_list_literal": [[1, "x", 2]], "logger.warning": None})
add("_telegram_ignored_threads", state={"config": {"extra": {}}, "platform": {"value": "telegram"}},
    io={"_extra_or_secret": " 1_0 , -5 ", "_decode_json_list_literal": " 1_0 , -5 "})
add("_telegram_ignored_threads", state={"config": {"extra": {}}, "platform": {"value": "telegram"}},
    io={"_extra_or_secret": None, "_decode_json_list_literal": None, "logger.warning": None})
add("_telegram_ignored_threads", state={"config": {"extra": {}}}, io={}, fail_closed=True)
add("_telegram_ignored_threads", state={"config": {"extra": {}}, "platform": {"value": "telegram"}},
    io={"_extra_or_secret": "", "_decode_json_list_literal": ""})
add("_telegram_ignored_threads", state={"config": {"extra": {}}},
    io={"_extra_or_secret": "abc", "_decode_json_list_literal": "abc"})

# ── _telegram_observe_allowed_chats ──
add("_telegram_observe_allowed_chats",
    io={"self._telegram_group_allowed_chats": [["a", "b"]], "self._telegram_allowed_chats": [["b", "c"]]})
add("_telegram_observe_allowed_chats", io={"self._telegram_group_allowed_chats": [[]]})
add("_telegram_observe_allowed_chats",
    io={"self._telegram_group_allowed_chats": [["a"]], "self._telegram_allowed_chats": [[]]})
add("_telegram_observe_allowed_chats",
    io={"self._telegram_group_allowed_chats": [["a", "b"]], "self._telegram_allowed_chats": [["a", "b"]]})
add("_telegram_observe_allowed_chats", io={}, fail_closed=True)
add("_telegram_observe_allowed_chats", io={"self._telegram_group_allowed_chats": None})

# ── _telegram_is_free_response_topic ──
add("_telegram_is_free_response_topic",
    args={"message": {"chat": {"id": -100, "type": "supergroup", "is_forum": True}}},
    io={"self._telegram_free_response_topics": [["-100:1"]], "self._chat_type_str": "supergroup"})
add("_telegram_is_free_response_topic", args={"message": {"chat": {"id": 5}}},
    io={"self._telegram_free_response_topics": [[]]})
add("_telegram_is_free_response_topic", args={"message": {"chat": {}}},
    io={"self._telegram_free_response_topics": [["x"]]})
add("_telegram_is_free_response_topic",
    args={"message": {"chat": {"id": 5, "type": "private"}}},
    io={"self._telegram_free_response_topics": [["5:7"]], "self._chat_type_str": "private"})
add("_telegram_is_free_response_topic",
    args={"message": {"chat": {"id": 5, "type": "private"}, "message_thread_id": 7, "is_topic_message": True}},
    io={"self._telegram_free_response_topics": [["5:7"]], "self._chat_type_str": "private"})
add("_telegram_is_free_response_topic",
    args={"message": {"chat": {"id": 5, "type": "supergroup", "is_forum": True}}},
    io={"self._telegram_free_response_topics": [["5:1"]]}, fail_closed=True)
add("_telegram_is_free_response_topic",
    args={"message": {"chat": {"id": 5, "type": "group", "is_forum": True}}},
    io={"self._telegram_free_response_topics": [["5:1"]], "self._chat_type_str": "group"})

# ── _int_or_zero ──
add("_int_or_zero", args={"value": "123"})
add("_int_or_zero", args={"value": None})
add("_int_or_zero", args={"value": "abc"})
add("_int_or_zero", args={"value": 1.9})
add("_int_or_zero", args={"value": True})
add("_int_or_zero", args={"value": [1]})
add("_int_or_zero", args={"value": ""})
add("_int_or_zero", args={"value": " 1_0 "})
add("_int_or_zero", args={"value": -5})

# ── _telegram_media_too_large_note ──
add("_telegram_media_too_large_note", args={"label": "document", "file_size": 25000000, "max_bytes": 20971520})
add("_telegram_media_too_large_note", args={"label": "document", "file_size": "abc", "max_bytes": 20971520})
add("_telegram_media_too_large_note", args={"label": "video", "file_size": None, "max_bytes": 20971520})
add("_telegram_media_too_large_note", args={"label": "image", "file_size": 5000000, "max_bytes": 1048576})
add("_telegram_media_too_large_note", args={"label": "image", "file_size": 5000000, "max_bytes": 0})
add("_telegram_media_too_large_note", args={"label": "doc", "file_size": [1], "max_bytes": 20971520})
add("_telegram_media_too_large_note", args={"label": "doc", "file_size": 100, "max_bytes": "x"})

# ── _telegram_media_size_allowed ──
add("_telegram_media_size_allowed", args={"source": {"file_size": 100}, "label": "document"}, state={})
add("_telegram_media_size_allowed", args={"source": {"file_size": 25000000}, "label": "document"}, state={})
add("_telegram_media_size_allowed", args={"source": None, "label": "document"}, state={})
add("_telegram_media_size_allowed", args={"source": {"file_size": "abc"}, "label": "document"}, state={})
add("_telegram_media_size_allowed", args={"source": {"file_size": 200}, "label": "document"},
    state={"_max_doc_bytes": "100"})
add("_telegram_media_size_allowed", args={"source": {"file_size": 200}, "label": "document"},
    state={"_max_doc_bytes": "abc"})
add("_telegram_media_size_allowed", args={"source": {"file_size": 0}, "label": "document"}, state={})
add("_telegram_media_size_allowed", args={"source": {"file_size": 100}, "label": "document"},
    state={"_max_doc_bytes": None})

# ── _missing_media_path_error ──
add("_missing_media_path_error", args={"label": "Image", "path": "/workspace/x.png"})
add("_missing_media_path_error", args={"label": "Image", "path": "/output/x.png"})
add("_missing_media_path_error", args={"label": "Image", "path": "/outputs/x.png"})
add("_missing_media_path_error", args={"label": "Image", "path": "/home/x.png"})
add("_missing_media_path_error", args={"label": "Image", "path": None})
add("_missing_media_path_error", args={"label": "Doc", "path": "rel/x.pdf"})

# ── _media_send_kwargs ──
add("_media_send_kwargs", args={"chat_id": "-100", "reply_to": "77", "metadata": {}},
    state={"_reply_to_mode": "all"},
    io={"self._reply_to_message_id_for_send": 77, "self._metadata_thread_id": "5",
        "self._thread_kwargs_for_send": {"message_thread_id": "5"},
        "normalize_telegram_chat_id": "-100", "self._notification_kwargs": {}})
add("_media_send_kwargs", args={"chat_id": "5", "reply_to": None, "metadata": {"notify": True}},
    state={"_reply_to_mode": "all"},
    io={"self._reply_to_message_id_for_send": None, "self._metadata_thread_id": None,
        "self._thread_kwargs_for_send": {},
        "normalize_telegram_chat_id": "5", "self._notification_kwargs": {"disable_notification": True}})
add("_media_send_kwargs", args={"chat_id": "5", "reply_to": None, "metadata": {}},
    state={"_reply_to_mode": "off"},
    io={"self._reply_to_message_id_for_send": None, "self._metadata_thread_id": None,
        "self._thread_kwargs_for_send": {"chat_id": "override"},
        "normalize_telegram_chat_id": "5", "self._notification_kwargs": {}})
add("_media_send_kwargs", args={"chat_id": "5", "reply_to": None, "metadata": {}},
    state={"_reply_to_mode": "all"},
    io={"self._reply_to_message_id_for_send": None, "self._metadata_thread_id": None,
        "self._thread_kwargs_for_send": {},
        "normalize_telegram_chat_id": "5", "self._notification_kwargs": {}})
add("_media_send_kwargs", args={"chat_id": "5", "reply_to": None, "metadata": {}},
    state={"_reply_to_mode": "all"}, io={}, fail_closed=True)
add("_media_send_kwargs", args={"chat_id": "5", "reply_to": None, "metadata": {}}, state={})
add("_media_send_kwargs", args={"chat_id": "5", "reply_to": None, "metadata": {}},
    state={"_reply_to_mode": "all"},
    io={"self._reply_to_message_id_for_send": 77, "self._metadata_thread_id": "5",
        "self._thread_kwargs_for_send": None, "normalize_telegram_chat_id": "5"})
add("_media_send_kwargs", args={"chat_id": "5", "reply_to": None, "metadata": {}},
    state={"_reply_to_mode": "all"},
    io={"self._reply_to_message_id_for_send": 77, "self._metadata_thread_id": "5",
        "self._thread_kwargs_for_send": {}, "normalize_telegram_chat_id": sc("SystemExit", "bye")})

# ── _sniff_raster_format ──
add("_sniff_raster_format", args={"image_path": "/x.png"},
    io={"open": {}, "file_read": "89504e470d0a1a0a0000000d49484452"})
add("_sniff_raster_format", args={"image_path": "/x.gif"},
    io={"open": {}, "file_read": "47494638396100000000"})
add("_sniff_raster_format", args={"image_path": "/x.webp"},
    io={"open": {}, "file_read": "524946460000000057454250"})
add("_sniff_raster_format", args={"image_path": "/x.bmp"},
    io={"open": {}, "file_read": "424d00000000"})
add("_sniff_raster_format", args={"image_path": "/x.tiff"},
    io={"open": {}, "file_read": "49492a0000000000"})
add("_sniff_raster_format", args={"image_path": "/x.tiff"},
    io={"open": {}, "file_read": "4d4d002a00000000"})
add("_sniff_raster_format", args={"image_path": "/x.bin"},
    io={"open": {}, "file_read": "00010203"})
add("_sniff_raster_format", args={"image_path": "/x.png"},
    io={"open": sc("OSError", "no file")})
add("_sniff_raster_format", args={"image_path": "/x.png"}, io={}, fail_closed=True)
add("_sniff_raster_format", args={"image_path": "/x.png"},
    io={"open": {}, "file_read": sc("OSError", "read fail")})
add("_sniff_raster_format", args={"image_path": "/x.png"},
    io={"open": sc("SystemExit", "bye")})

# ── _cold_boot_drop_pending ──
add("_cold_boot_drop_pending", args={"is_reconnect": False},
    state={"platform": {"value": "telegram"}, "_drop_pending_on_cold_boot": True},
    io={"logger.info": None})
add("_cold_boot_drop_pending", args={"is_reconnect": False},
    state={"platform": {"value": "telegram"}, "_drop_pending_on_cold_boot": False},
    io={"logger.info": None})
add("_cold_boot_drop_pending", args={"is_reconnect": True}, state={})
add("_cold_boot_drop_pending", args={"is_reconnect": False},
    state={"platform": {"value": "telegram"}})
add("_cold_boot_drop_pending", args={"is_reconnect": False},
    state={"platform": {"value": "telegram"}, "_drop_pending_on_cold_boot": True},
    io={}, fail_closed=True)
add("_cold_boot_drop_pending", args={"is_reconnect": False},
    state={"platform": {"value": "telegram"}, "_drop_pending_on_cold_boot": True},
    io={"logger.info": sc("SystemExit", "bye")})

# ── _looks_like_polling_conflict ──
add("_looks_like_polling_conflict", args={"error": "e1"},
    errors={"e1": {"class": "Conflict", "message": "x"}})
add("_looks_like_polling_conflict", args={"error": "e1"},
    errors={"e1": {"class": "BadRequest", "message": "Terminated by other getUpdates request"}})
add("_looks_like_polling_conflict", args={"error": "e1"},
    errors={"e1": {"class": "NetworkError", "message": "another bot instance is running"}})
add("_looks_like_polling_conflict", args={"error": "e1"},
    errors={"e1": {"class": "NetworkError", "message": "unrelated"}})
add("_looks_like_polling_conflict", args={"error": "e1"},
    errors={"e1": {"class": "Exception", "message": ""}})
add("_looks_like_polling_conflict", args={"error": "e1"},
    errors={"e1": {"class": "CONFLICT", "message": "y"}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        raise SystemExit(f"generator: funcs with <5 cases: {thin}")
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"wrote {len(CASES)} cases ({len(counts)} funcs) -> {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()
