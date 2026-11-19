#!/usr/bin/env python3
"""Generate tests/fixtures/run_inbound_flow.jsonl — scripted-seam cases for the
run_inbound_flow oracle. Each case carries the function name, its args, the
initial self/world state, and the io scripts (seam-name -> value | per-call list
| {"__script__": ret/raise}). The io sequences were derived by walking the live
decision chains in call order (short-circuits and suppress blocks included); the
oracle cross-checks every io key against the C port's seam registry.

Domains (documented in the port header): adapter/pending/outcome scripts are
null|dict(|str for adapters); approval_input_words results are lists of strings;
event.timestamp / raw_message.edit_date are epoch numbers; GATEWAY_KNOWN_COMMANDS
is the 87-name snapshot. ASCII-only folding corpus.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "run_inbound_flow.jsonl")

CASES = []


def add(func, args, state=None, io=None):
    CASES.append({"func": func, "in": {"args": args, "state": state or {}, "io": io or {}}})


def src(platform="telegram", user_id="u1", chat_id="c1", **kw):
    d = {"platform": platform, "user_id": user_id, "chat_id": chat_id}
    d.update(kw)
    return d


def ev(text="hi", message_type="text", media_urls=None, media_types=None, source=None, **kw):
    d = {"text": text, "message_type": message_type,
         "media_urls": [] if media_urls is None else media_urls,
         "media_types": [] if media_types is None else media_types}
    if source is not None:
        d["source"] = source
    d.update(kw)
    return d


def rs(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def st_(ret):
    return {"__script__": True, "ret": ret}


def states(key="k1", turn=None, persistent=None, conversation=None):
    node = {}
    if turn is not None:
        node["turn"] = turn
    if persistent is not None:
        node["persistent"] = persistent
    if conversation is not None:
        node["conversation"] = conversation
    return {"session_states": {key: node}}


# ══════════════ _prompt_admin_ids ══════════════
add("_prompt_admin_ids", {"event": ev(source=src())}, {},
    {"policy_for_runner_source": {"admin_user_ids": ["a1", "a2"]}})
add("_prompt_admin_ids", {"event": ev(source=src())}, {}, {"policy_for_runner_source": None})
add("_prompt_admin_ids", {"event": ev(source=src())}, {}, {"policy_for_runner_source": {}})
add("_prompt_admin_ids", {"event": ev()})  # source absent -> AttributeError('source')
add("_prompt_admin_ids", {"event": ev(source=src())}, {},
    {"policy_for_runner_source": rs("RuntimeError", "boom")})
add("_prompt_admin_ids", {"event": ev(source=src())}, {},
    {"policy_for_runner_source": rs("SystemExit", "bye")})
add("_prompt_admin_ids", {"event": ev(source=src())})  # fail-closed

# ══════════════ _hm_update_reply_refused ══════════════
UR_IO = {"policy_for_runner_source": {"admin_user_ids": ["a1"]}, "update_prompt_refusal": None}
add("_hm_update_reply_refused", {"event": ev(user_id="u9", source=src())}, {}, UR_IO)
add("_hm_update_reply_refused", {"event": ev(user_id="u9", source=src())}, {},
    {"policy_for_runner_source": {"admin_user_ids": ["a1"]}, "update_prompt_refusal": "refused"})
add("_hm_update_reply_refused", {"event": ev(user_id="", source=src(user_id="u2"))}, {},
    {"policy_for_runner_source": {"admin_user_ids": []}, "update_prompt_refusal": None})
add("_hm_update_reply_refused", {"event": ev(user_id="u9", source=src())}, {},
    {"policy_for_runner_source": None})  # AttributeError NoneType admin_user_ids
add("_hm_update_reply_refused", {"event": ev(user_id="u9", source=src())}, {},
    {"policy_for_runner_source": {}})  # AttributeError NS admin_user_ids
add("_hm_update_reply_refused", {"event": ev(source=src())})  # user_id absent
add("_hm_update_reply_refused", {"event": ev(user_id=None, source=src())}, {},
    {"policy_for_runner_source": {"admin_user_ids": []}})  # source.user_id missing
add("_hm_update_reply_refused", {"event": ev(user_id="u9", source=src())}, {},
    {"policy_for_runner_source": {"admin_user_ids": []}, "update_prompt_refusal": rs("SystemExit", "x")})
add("_hm_update_reply_refused", {"event": ev(user_id="u9", source=src())})  # fail-closed

# ══════════════ _refused_clarify ══════════════
RC_IO = {"event.sender_id": "s1", "policy_for_runner_source": {"admin_user_ids": ["a1"]},
         "answer_admitted": True, "clarify_refusal": "req", "refusal_text": "localized"}
add("_refused_clarify", {"event": ev(source=src()), "clarify_id": "cl1"}, {}, RC_IO)
add("_refused_clarify", {"event": ev(source=src()), "clarify_id": "cl1"}, {},
    dict(RC_IO, clarify_refusal=None))  # requester falsy -> None, no refusal_text
add("_refused_clarify", {"event": ev(source=src()), "clarify_id": "cl1"}, {},
    dict(RC_IO, **{"event.sender_id": None}))  # sender "" 
add("_refused_clarify", {"event": ev(source=src()), "clarify_id": "cl1"}, {},
    dict(RC_IO, clarify_refusal=rs("RuntimeError", "boom")))
add("_refused_clarify", {"event": ev(source=src()), "clarify_id": "cl1"}, {},
    dict(RC_IO, answer_admitted=rs("SystemExit", "bye")))
add("_refused_clarify", {"event": ev(source=src()), "clarify_id": "cl1"}, {},
    dict(RC_IO, policy_for_runner_source=None))  # admin_ids null
add("_refused_clarify", {"event": ev(source=src()), "clarify_id": "cl1"})  # fail-closed

# ══════════════ _refused_slash_confirm ══════════════
RS_IO = {"event.sender_id": "s1", "policy_for_runner_source": {"admin_user_ids": ["a1"]},
         "answer_admitted": False, "slash_confirm_refusal": "req", "refusal_text": "localized"}
add("_refused_slash_confirm", {"event": ev(source=src()), "session_key": "k1", "confirm_id": "cf1"}, {}, RS_IO)
add("_refused_slash_confirm", {"event": ev(source=src()), "session_key": "k1", "confirm_id": "cf1"}, {},
    dict(RS_IO, slash_confirm_refusal=None))
add("_refused_slash_confirm", {"event": ev(source=src()), "session_key": "k1", "confirm_id": "cf1"}, {},
    dict(RS_IO, slash_confirm_refusal=""))  # empty requester falsy -> None
add("_refused_slash_confirm", {"event": ev(source=src()), "session_key": "k1", "confirm_id": "cf1"}, {},
    dict(RS_IO, slash_confirm_refusal=rs("ValueError", "bad")))
add("_refused_slash_confirm", {"event": ev(source=src()), "session_key": "k1", "confirm_id": "cf1"}, {},
    dict(RS_IO, refusal_text=rs("KeyboardInterrupt", "")))
add("_refused_slash_confirm", {"event": ev(source=src()), "session_key": "k1", "confirm_id": "cf1"})  # fail-closed

# ══════════════ _slash_confirm_text_choices ══════════════
add("_slash_confirm_text_choices", {}, {},
    {"approval_input_words": [["yes", "ok"], ["always"], ["cancel", "no"]]})
add("_slash_confirm_text_choices", {}, {},
    {"approval_input_words": [["yes"], ["yes", "always"], ["yes"]]})  # first wins
add("_slash_confirm_text_choices", {}, {}, {"approval_input_words": [[], [], []]})
add("_slash_confirm_text_choices", {}, {}, {"approval_input_words": [None, None, None]})
add("_slash_confirm_text_choices", {}, {}, {"approval_input_words": ["ab", "cd", "ef"]})
add("_slash_confirm_text_choices", {}, {}, {"approval_input_words": rs("RuntimeError", "boom")})
add("_slash_confirm_text_choices", {})  # fail-closed

# ══════════════ _release_pending_clarify ══════════════
RP_ARGS = {"pending": {"clarify_id": "cl1"}, "source": src()}
add("_release_pending_clarify", RP_ARGS, {}, {"clarify_gateway.resolve_gateway_clarify": False})
add("_release_pending_clarify", RP_ARGS, {},
    {"clarify_gateway.resolve_gateway_clarify": True, "self._delivery_adapter_for": None})
add("_release_pending_clarify", RP_ARGS, {},
    {"clarify_gateway.resolve_gateway_clarify": True,
     "self._delivery_adapter_for": {"id": "a1", "has_retire_card": True},
     "t": "cancelled text", "adapter.retire_clarify_card": None})
add("_release_pending_clarify", RP_ARGS, {},
    {"clarify_gateway.resolve_gateway_clarify": True, "self._delivery_adapter_for": {"id": "a1"}})
add("_release_pending_clarify", RP_ARGS, {},
    {"clarify_gateway.resolve_gateway_clarify": True,
     "self._delivery_adapter_for": {"id": "a1", "has_retire_card": True},
     "t": "txt", "adapter.retire_clarify_card": rs("RuntimeError", "card fail")})  # swallowed -> True
add("_release_pending_clarify", RP_ARGS, {},
    {"clarify_gateway.resolve_gateway_clarify": True,
     "self._delivery_adapter_for": {"id": "a1", "has_retire_card": True},
     "t": "txt", "adapter.retire_clarify_card": rs("SystemExit", "bye")})
add("_release_pending_clarify", {"pending": None, "source": src()})  # AttributeError NoneType
add("_release_pending_clarify", {"pending": {}, "source": src()})  # AttributeError NS clarify_id
add("_release_pending_clarify", RP_ARGS, {},
    {"clarify_gateway.resolve_gateway_clarify": True,
     "self._delivery_adapter_for": {"id": "a1", "has_retire_card": True},
     "t": rs("ValueError", "t fail")})  # t raise swallowed -> True
add("_release_pending_clarify", RP_ARGS)  # fail-closed

# ══════════════ _release_clarify_owning_turn ══════════════
RO_ARGS = {"session_key": "k1", "source": src()}
add("_release_clarify_owning_turn", RO_ARGS, {}, {"clarify_gateway.get_pending_for_session": None})
add("_release_clarify_owning_turn", RO_ARGS, {},
    {"clarify_gateway.get_pending_for_session": {"clarify_id": "cl1"},
     "clarify_gateway.resolve_gateway_clarify": True, "self._delivery_adapter_for": None})
add("_release_clarify_owning_turn", RO_ARGS, {},
    {"clarify_gateway.get_pending_for_session": {"clarify_id": "cl1"},
     "clarify_gateway.resolve_gateway_clarify": False})
add("_release_clarify_owning_turn", RO_ARGS, {},
    {"clarify_gateway.get_pending_for_session": rs("RuntimeError", "boom")})
add("_release_clarify_owning_turn", RO_ARGS, {},
    {"clarify_gateway.get_pending_for_session": {"clarify_id": "cl1"},
     "clarify_gateway.resolve_gateway_clarify": True,
     "self._delivery_adapter_for": {"id": "a1", "has_retire_card": True},
     "t": "txt", "adapter.retire_clarify_card": rs("SystemExit", "bye")})
add("_release_clarify_owning_turn", RO_ARGS)  # fail-closed

# ══════════════ _hm_evict_running_agent ══════════════
ER_ARGS = {"key": "k1", "reason": "stale_running_agent_eviction"}
add("_hm_evict_running_agent", ER_ARGS, {},
    {"self._interrupt_running_turn": 5, "self._drop_turn_slot": None})
add("_hm_evict_running_agent", ER_ARGS, {},
    {"self._interrupt_running_turn": None, "self._drop_turn_slot": None})
add("_hm_evict_running_agent", ER_ARGS, {}, {"self._interrupt_running_turn": rs("RuntimeError", "boom")})
add("_hm_evict_running_agent", ER_ARGS, {}, {"self._interrupt_running_turn": rs("SystemExit", "bye")})
add("_hm_evict_running_agent", ER_ARGS, {},
    {"self._interrupt_running_turn": 3, "self._drop_turn_slot": rs("ValueError", "drop fail")})
add("_hm_evict_running_agent", ER_ARGS)  # fail-closed

# ══════════════ _hm_evict_reaped_agent ══════════════
add("_hm_evict_reaped_agent", {"key": "k1"}, {"session_store": None})
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}},
    {"store.peek_session_id": "s1", "store._is_session_ended_in_db": True,
     "self._interrupt_running_turn": 2, "self._drop_turn_slot": None})
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}},
    {"store.peek_session_id": "s1", "store._is_session_ended_in_db": False})
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}}, {"store.peek_session_id": ""})
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}}, {"store.peek_session_id": 5})
add("_hm_evict_reaped_agent", {"key": "k1"}, {"session_store": {"has_peek": False}})
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}},
    {"store.peek_session_id": "s1", "store._is_session_ended_in_db": 1})  # is True identity
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}},
    {"store.peek_session_id": rs("RuntimeError", "peek fail")})  # swallowed
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}},
    {"store.peek_session_id": rs("SystemExit", "bye")})
add("_hm_evict_reaped_agent", {"key": "k1"},
    {"session_store": {"has_peek": True, "has_ended": True}},
    {"store.peek_session_id": "s1", "store._is_session_ended_in_db": True,
     "self._interrupt_running_turn": rs("RuntimeError", "evict fail")})  # swallowed

# ══════════════ _hm_merge_pending_for_source ══════════════
MP_ARGS = {"source": src(), "key": "k1", "event": ev(), "merge_text": False}
add("_hm_merge_pending_for_source", MP_ARGS, {}, {"self._delivery_adapter_for": None})
add("_hm_merge_pending_for_source", MP_ARGS, {},
    {"self._delivery_adapter_for": {"id": "a1", "pending_messages": {"x": 1}},
     "merge_pending_message_event": None})
add("_hm_merge_pending_for_source", MP_ARGS, {},
    {"self._delivery_adapter_for": {"id": "a1"}, "merge_pending_message_event": None})
add("_hm_merge_pending_for_source", dict(MP_ARGS, merge_text=True), {},
    {"self._delivery_adapter_for": {"id": "a1"}, "merge_pending_message_event": None})
add("_hm_merge_pending_for_source", MP_ARGS, {}, {"self._delivery_adapter_for": ""})
add("_hm_merge_pending_for_source", MP_ARGS, {},
    {"self._delivery_adapter_for": {"id": "a1"}, "merge_pending_message_event": rs("RuntimeError", "boom")})
add("_hm_merge_pending_for_source", MP_ARGS, {}, {"self._delivery_adapter_for": "a1"})  # str adapter
add("_hm_merge_pending_for_source", MP_ARGS)  # fail-closed

# ══════════════ _hm_drop_held_input ══════════════
add("_hm_drop_held_input", {"event": ev(source=src())}, {}, {"self._intake_adapter_for": None})
add("_hm_drop_held_input", {"event": ev(source=src())}, {},
    {"self._intake_adapter_for": {"id": "a1"}, "adapter._event_session_key": "ek",
     "adapter._discard_text_debounce": None})
add("_hm_drop_held_input", {"event": ev(source=src())}, {}, {"self._intake_adapter_for": ""})
add("_hm_drop_held_input", {"event": ev()})  # source absent
add("_hm_drop_held_input", {"event": ev(source=src())}, {},
    {"self._intake_adapter_for": {"id": "a1"}, "adapter._event_session_key": rs("RuntimeError", "boom")})
add("_hm_drop_held_input", {"event": ev(source=src())}, {},
    {"self._intake_adapter_for": {"id": "a1"}, "adapter._event_session_key": "ek",
     "adapter._discard_text_debounce": rs("SystemExit", "bye")})
add("_hm_drop_held_input", {"event": ev(source=src())})  # fail-closed

# ══════════════ _hm_text_only ══════════════
add("_hm_text_only", {"event": ev(message_type="text", media_urls=[], media_types=[])})
add("_hm_text_only", {"event": ev(message_type="text", media_urls=["u"], media_types=[])})
add("_hm_text_only", {"event": ev(message_type="text", media_urls=[], media_types=["image"])})
add("_hm_text_only", {"event": ev(message_type="photo")})
add("_hm_text_only", {"event": {"media_urls": [], "media_types": []}})  # message_type absent
add("_hm_text_only", {"event": {"message_type": "text", "media_types": []}})  # media_urls absent
add("_hm_text_only", {"event": {"message_type": "text", "media_urls": []}})  # media_types absent

# ══════════════ _hm_in_telegram_grace ══════════════
TG_STATE = states(turn={"started_ts": 999.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text", timestamp=1000.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": 3.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text", timestamp=1005.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": 3.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text"), "source": src(platform="discord"), "key": "k1"},
    TG_STATE, {"_float_env": 3.0, "time.time": 1000.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="voice", timestamp=1000.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": 3.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="photo", timestamp=1000.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": 3.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text", timestamp=1000.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": 0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text", timestamp=1000.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, states(turn={"started_ts": 0}), {"_float_env": 3.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text", timestamp=999.0, raw_message={"edit_date": 1000.0}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": 3.0})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text"), "source": src(platform="telegram"), "key": "k1"},
    TG_STATE, {"_float_env": 3.0, "time.time": 1000.5})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text"), "source": src(platform="telegram"), "key": "k1"},
    states(turn={}), {"_float_env": 3.0, "time.time": 1000.0})  # started_ts absent
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text", timestamp=1000.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": None})
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text", timestamp=990.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"}, TG_STATE, {"_float_env": 3.0})  # diff -9 < -1
add("_hm_in_telegram_grace",
    {"event": ev(message_type="text"), "source": src(platform="telegram"), "key": "k1"}, TG_STATE)  # fail-closed

# ══════════════ _hm_busy_telegram_grace_queue ══════════════
GQ_EV = ev(message_type="text", timestamp=1000.0, raw_message={"edit_date": None})
add("_hm_busy_telegram_grace_queue",
    {"event": GQ_EV, "source": src(platform="discord"), "key": "k1", "effective_busy_input_mode": "queue"},
    TG_STATE, {"_float_env": 3.0, "time.time": 1000.0})  # not in grace
add("_hm_busy_telegram_grace_queue",
    {"event": GQ_EV, "source": src(platform="telegram"), "key": "k1", "effective_busy_input_mode": "steer"},
    TG_STATE, {"_float_env": 3.0, "self._delivery_adapter_for": {"id": "a1"},
               "merge_pending_message_event": None})
add("_hm_busy_telegram_grace_queue",
    {"event": GQ_EV, "source": src(platform="telegram"), "key": "k1", "effective_busy_input_mode": "queue"},
    TG_STATE, {"_float_env": 3.0, "self._delivery_adapter_for": {"id": "a1"}, "self._enqueue_fifo": None})
add("_hm_busy_telegram_grace_queue",
    {"event": GQ_EV, "source": src(platform="telegram"), "key": "k1", "effective_busy_input_mode": "queue"},
    TG_STATE, {"_float_env": 3.0, "self._delivery_adapter_for": None})
add("_hm_busy_telegram_grace_queue",
    {"event": GQ_EV, "source": src(platform="telegram"), "key": "k1", "effective_busy_input_mode": "interrupt"},
    TG_STATE, {"_float_env": 3.0, "self._delivery_adapter_for": {"id": "a1"},
               "merge_pending_message_event": None})
add("_hm_busy_telegram_grace_queue",
    {"event": GQ_EV, "source": src(platform="telegram"), "key": "k1", "effective_busy_input_mode": "steer"},
    TG_STATE, {"_float_env": 3.0, "self._delivery_adapter_for": {"id": "a1"},
               "merge_pending_message_event": rs("SystemExit", "bye")})
add("_hm_busy_telegram_grace_queue",
    {"event": GQ_EV, "source": src(platform="telegram"), "key": "k1", "effective_busy_input_mode": "queue"},
    TG_STATE)  # fail-closed

# ══════════════ _hm_busy_steer ══════════════
BS_EV = ev(text="  hi  ", message_type="text", media_urls=[], media_types=[])
BS_AGENT = {"id": "a1", "has_steer": True}
add("_hm_busy_steer", {"event": BS_EV, "agent": BS_AGENT, "key": "k1"}, {},
    {"self._steer_text_with_origin": "origin", "self._steer_running_agent": True,
     "self._fold_into_running_turn": None})
add("_hm_busy_steer", {"event": BS_EV, "agent": BS_AGENT, "key": "k1"}, {},
    {"self._steer_text_with_origin": "origin", "self._steer_running_agent": False,
     "self._queue_or_replace_pending_event": None})
add("_hm_busy_steer", {"event": BS_EV, "agent": None, "key": "k1"}, {},
    {"self._queue_or_replace_pending_event": None})
add("_hm_busy_steer", {"event": ev(text="   ", message_type="text", media_urls=[], media_types=[]),
                       "agent": BS_AGENT, "key": "k1"}, {},
    {"self._queue_or_replace_pending_event": None})
add("_hm_busy_steer", {"event": ev(text="hi", message_type="text", media_urls=["u"], media_types=[]),
                       "agent": BS_AGENT, "key": "k1"}, {},
    {"self._queue_or_replace_pending_event": None})
add("_hm_busy_steer", {"event": BS_EV, "agent": BS_AGENT, "key": "k1"}, {},
    {"self._steer_text_with_origin": "origin", "self._steer_running_agent": rs("RuntimeError", "steer fail"),
     "self._queue_or_replace_pending_event": None})
add("_hm_busy_steer", {"event": BS_EV, "agent": BS_AGENT, "key": "k1"}, {},
    {"self._steer_text_with_origin": "origin", "self._steer_running_agent": rs("SystemExit", "bye")})
add("_hm_busy_steer", {"event": BS_EV, "agent": BS_AGENT, "key": "k1"}, {},
    {"self._steer_text_with_origin": rs("ValueError", "origin fail"),
     "self._queue_or_replace_pending_event": None})
add("_hm_busy_steer", {"event": ev(message_type="text", media_urls=[], media_types=[]),
                       "agent": BS_AGENT, "key": "k1"})  # text absent
add("_hm_busy_steer", {"event": BS_EV, "agent": BS_AGENT, "key": "k1"})  # fail-closed

# ══════════════ _hm_busy_interrupt ══════════════
BI_EV = ev(text="hi", message_type="text", media_urls=[], media_types=[])
BI_AGENT = {"id": "a1", "supports_redirect": True, "has_redirect": True}
add("_hm_busy_interrupt", {"event": BI_EV, "agent": BI_AGENT, "key": "k1"}, {},
    {"self._redirect_active_turn": True})
add("_hm_busy_interrupt", {"event": BI_EV, "agent": BI_AGENT, "key": "k1"}, {},
    {"self._redirect_active_turn": False, "self._pending_event_audio_paths": False,
     "self._busy_interrupt_text": "itext", "agent.interrupt": None})
add("_hm_busy_interrupt", {"event": BI_EV, "agent": BI_AGENT, "key": "k1"}, {},
    {"self._redirect_active_turn": False, "self._pending_event_audio_paths": True,
     "self._queue_or_replace_pending_event": None, "self._busy_interrupt_text": "itext",
     "agent.interrupt": None})
add("_hm_busy_interrupt", {"event": BI_EV, "agent": None, "key": "k1"}, {},
    {"self._pending_event_audio_paths": False, "self._busy_interrupt_text": "itext"})  # NoneType interrupt
add("_hm_busy_interrupt", {"event": BI_EV, "agent": {"sentinel": True}, "key": "k1"}, {},
    {"self._pending_event_audio_paths": False, "self._busy_interrupt_text": "itext"})  # object interrupt
add("_hm_busy_interrupt", {"event": BI_EV, "agent": {"id": "a1", "supports_redirect": 1, "has_redirect": True},
                           "key": "k1"}, {},
    {"self._pending_event_audio_paths": False, "self._busy_interrupt_text": "itext", "agent.interrupt": None})
add("_hm_busy_interrupt", {"event": BI_EV, "agent": BI_AGENT, "key": "k1"}, {},
    {"self._redirect_active_turn": rs("SystemExit", "bye")})
add("_hm_busy_interrupt", {"event": ev(message_type="text", media_urls=[], media_types=[]),
                           "agent": BI_AGENT, "key": "k1"})  # text absent in redirect branch
add("_hm_busy_interrupt", {"event": BI_EV, "agent": BI_AGENT, "key": "k1"})  # fail-closed

# ══════════════ _clarify_takeover ══════════════
add("_clarify_takeover", {"event": ev(_clarify_superseded=5), "key": "k1"}, {},
    {"self._current_session_run_generation": 5})
add("_clarify_takeover", {"event": ev(_clarify_superseded=5), "key": "k1"}, {},
    {"self._current_session_run_generation": 6})
add("_clarify_takeover", {"event": ev(), "key": "k1"})  # absent -> False, no seam
add("_clarify_takeover", {"event": ev(_clarify_superseded=None), "key": "k1"})
add("_clarify_takeover", {"event": ev(_clarify_superseded=5), "key": "k1"}, {},
    {"self._current_session_run_generation": rs("RuntimeError", "boom")})
add("_clarify_takeover", {"event": ev(_clarify_superseded="x"), "key": "k1"}, {},
    {"self._current_session_run_generation": "x"})
add("_clarify_takeover", {"event": ev(_clarify_superseded=5), "key": "k1"})  # fail-closed

# ══════════════ _hm_busy_preempt ══════════════
BP_EV = ev(message_type="text", media_urls=[], media_types=[], internal=False)
BP_STATE = {"session_states": {"k1": {"turn": {"agent": {"id": "a1"}}}}, "_draining": False}
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE,
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": {"id": "a1"},
     "self._resolve_busy_steer_or_redirect": {"steered": True, "redirected": False},
     "note_busy_outcome": None, "self._busy_interrupt_and_ack": None})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE,
    {"self._effective_busy_input_mode": "queue", "self._delivery_adapter_for": {"id": "a1"},
     "self._effective_busy_text_mode": "queue"})  # demote -> False
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"},
    dict(BP_STATE, _draining=True), {"self._effective_busy_input_mode": "steer",
                                     "self._delivery_adapter_for": {"id": "a1"}})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE,
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": None})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"},
    {"session_states": {"k1": {"turn": {"agent": None}}}},
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": {"id": "a1"}})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"},
    {"session_states": {"k1": {"turn": {"agent": {"sentinel": True}}}}},
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": {"id": "a1"}})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE,
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": {"id": "a1"},
     "self._resolve_busy_steer_or_redirect": {"steered": False, "redirected": True},
     "note_busy_outcome": None, "self._busy_interrupt_and_ack": None})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE,
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": {"id": "a1"},
     "self._resolve_busy_steer_or_redirect": {"steered": False, "redirected": False},
     "note_busy_outcome": None, "self._busy_interrupt_and_ack": None})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE,
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": {"id": "a1"},
     "self._resolve_busy_steer_or_redirect": None,
     "note_busy_outcome": None, "self._busy_interrupt_and_ack": None})  # NoneType steered
add("_hm_busy_preempt",
    {"event": ev(message_type="text", media_urls=[], media_types=[], internal=False,
                 timestamp=1000.0, raw_message={"edit_date": None}),
     "source": src(platform="telegram"), "key": "k1"},
    {"session_states": {"k1": {"turn": {"agent": {"id": "a1"}, "started_ts": 999.0}}}},
    {"self._effective_busy_input_mode": "interrupt", "event.get_command": None, "_float_env": 3.0})  # grace -> False
add("_hm_busy_preempt", {"event": ev(message_type="text", media_urls=[], media_types=[],
                                     _clarify_superseded=5), "source": src(), "key": "k1"}, BP_STATE,
    {"self._current_session_run_generation": 5, "self._delivery_adapter_for": {"id": "a1"},
     "self._resolve_busy_steer_or_redirect": {"steered": True, "redirected": False},
     "note_busy_outcome": None, "self._busy_interrupt_and_ack": None})  # superseded mode interrupt
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE,
    {"self._effective_busy_input_mode": "steer", "self._delivery_adapter_for": {"id": "a1"},
     "self._resolve_busy_steer_or_redirect": rs("SystemExit", "bye")})
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"},
    {"session_states": {"k1": {}}}, {"self._effective_busy_input_mode": "steer"})  # turn absent
add("_hm_busy_preempt", {"event": BP_EV, "source": src(), "key": "k1"}, BP_STATE)  # fail-closed

# ══════════════ _hm_send_payload_as_turn ══════════════
add("_hm_send_payload_as_turn", {"event": ev(media_urls=[]), "usage": "usage hint"}, {},
    {"event.get_command_args": "  hi  "})
add("_hm_send_payload_as_turn", {"event": ev(media_urls=[]), "usage": "usage hint"}, {},
    {"event.get_command_args": ""})
add("_hm_send_payload_as_turn", {"event": ev(media_urls=["u"]), "usage": "usage hint"}, {},
    {"event.get_command_args": ""})
add("_hm_send_payload_as_turn", {"event": ev(media_urls=[]), "usage": "usage hint"}, {},
    {"event.get_command_args": None})  # NoneType strip
add("_hm_send_payload_as_turn", {"event": ev(media_urls=[]), "usage": "usage hint"}, {},
    {"event.get_command_args": "   "})
add("_hm_send_payload_as_turn", {"event": ev(media_urls=[], text_readonly=True), "usage": "u"}, {},
    {"event.get_command_args": "hi"})  # readonly swallowed
add("_hm_send_payload_as_turn",
    {"event": ev(media_urls=[], text_raise={"type": "SystemExit", "message": "bye"}), "usage": "u"}, {},
    {"event.get_command_args": "hi"})
add("_hm_send_payload_as_turn",
    {"event": ev(media_urls=[], text_raise={"type": "ValueError", "message": "x"}), "usage": "u"}, {},
    {"event.get_command_args": "hi"})  # Exception swallowed
add("_hm_send_payload_as_turn", {"event": {"message_type": "text"}, "usage": "u"}, {},
    {"event.get_command_args": ""})  # media_urls absent
add("_hm_send_payload_as_turn", {"event": ev(media_urls=[]), "usage": "u"})  # fail-closed

# ══════════════ _hm_unknown_slash_reply ══════════════
add("_hm_unknown_slash_reply", {"command": "status", "source": src()})
add("_hm_unknown_slash_reply", {"command": "reload_mcp", "source": src()})
add("_hm_unknown_slash_reply", {"command": "foo", "source": src()}, {},
    {"_check_unavailable_skill": None, "t": "unknown notice"})
add("_hm_unknown_slash_reply", {"command": "foo", "source": src()}, {},
    {"_check_unavailable_skill": "skill msg"})
add("_hm_unknown_slash_reply", {"command": "foo", "source": None}, {},
    {"_check_unavailable_skill": None})  # NoneType platform
add("_hm_unknown_slash_reply", {"command": "foo", "source": {}}, {},
    {"_check_unavailable_skill": None})  # NS platform
add("_hm_unknown_slash_reply", {"command": "foo", "source": src(platform=None)}, {},
    {"_check_unavailable_skill": None, "t": "unknown notice"})
add("_hm_unknown_slash_reply", {"command": "foo", "source": src()}, {},
    {"_check_unavailable_skill": rs("SystemExit", "bye")})
add("_hm_unknown_slash_reply", {"command": "foo", "source": src()})  # fail-closed

# ══════════════ _hm_bundle_slash_rewrite ══════════════
BB_ARGS = {"event": ev(), "source": src(), "key": "k1", "command": "bundle"}
add("_hm_bundle_slash_rewrite", BB_ARGS, {}, {"resolve_bundle_command_key": None})
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "  instr  ",
     "build_bundle_invocation_message": ["prompt", ["s"], []]})
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr",
     "build_bundle_invocation_message": ["prompt", ["s"], ["missing1"]]})
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr",
     "build_bundle_invocation_message": ["prompt", ["s"]]})  # unpack len2
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr",
     "build_bundle_invocation_message": ["prompt", ["s"], ["m"], "extra"]})  # len4
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr",
     "build_bundle_invocation_message": None})
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr",
     "build_bundle_invocation_message": ["prompt", ["s"], [1]]})  # join TypeError
add("_hm_bundle_slash_rewrite", dict(BB_ARGS, source=None), {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr"})  # platform raise
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": None})  # strip raise
add("_hm_bundle_slash_rewrite", BB_ARGS, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr",
     "build_bundle_invocation_message": rs("SystemExit", "bye")})
add("_hm_bundle_slash_rewrite",
    {"event": ev(text_raise={"type": "SystemExit", "message": "bye"}), "source": src(),
     "key": "k1", "command": "bundle"}, {},
    {"resolve_bundle_command_key": "bk", "event.get_command_args": "instr",
     "build_bundle_invocation_message": ["prompt", ["s"], []]})
add("_hm_bundle_slash_rewrite", BB_ARGS)  # fail-closed

# ══════════════ _consume_pending_native_image_paths ══════════════
add("_consume_pending_native_image_paths", {"session_key": "k1"},
    states(persistent={"native_image_paths": ["a", "b"]}))
add("_consume_pending_native_image_paths", {"session_key": "k1"},
    states(persistent={"native_image_paths": []}))
add("_consume_pending_native_image_paths", {"session_key": "k1"},
    states(persistent={"native_image_paths": None}))
add("_consume_pending_native_image_paths", {"session_key": "k1"}, {})  # state null
add("_consume_pending_native_image_paths", {"session_key": "k1"}, states(turn={}))  # persistent absent
add("_consume_pending_native_image_paths", {"session_key": "k1"}, states(persistent={}))  # nip absent
add("_consume_pending_native_image_paths", {"session_key": "k1"},
    states(persistent={"native_image_paths": "ab"}))
add("_consume_pending_native_image_paths", {"session_key": "k1"},
    states(persistent={"native_image_paths": 5}))  # not iterable

# ══════════════ _inbound_original_names ══════════════
add("_inbound_original_names", {"event": ev(media_urls=["p1", "p2"], media_filenames=["n1", "n2"])})
add("_inbound_original_names", {"event": ev(media_urls=["p1", "p2"], media_filenames=["n1", ""])})
add("_inbound_original_names", {"event": ev(media_urls=["p1", "p2"])})  # names absent
add("_inbound_original_names", {"event": ev(media_urls=[])})
add("_inbound_original_names", {"event": ev(media_urls=["p1", "p2"], media_filenames=["n1"])})
add("_inbound_original_names", {"event": {"message_type": "text"}})  # media_urls absent
add("_inbound_original_names", {"event": ev(media_urls=["p1", "p1"], media_filenames=["n1", "n2"])})
add("_inbound_original_names", {"event": ev(media_urls=["p1"], media_filenames=None)})

# ══════════════ _restore_pending_one_turn_model_override ══════════════
OTR_STATE = states(conversation={"one_turn_restore": {"model": "m"}})
add("_restore_pending_one_turn_model_override", {"session_key": "", "run_generation": None})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    OTR_STATE, {"self._restore_session_model_override": None})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": 5},
    OTR_STATE, {"self._is_session_run_current": True, "self._restore_session_model_override": None})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": 5},
    OTR_STATE, {"self._is_session_run_current": False})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    states(conversation={"one_turn_restore": None}))
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    states(turn={}))  # conversation absent
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    OTR_STATE, {"self._restore_session_model_override": rs("RuntimeError", "restore fail")})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    OTR_STATE, {"self._restore_session_model_override": rs("SystemExit", "bye")})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    dict(OTR_STATE, _pending_model_notes={"other": "x"}),
    {"self._restore_session_model_override": None})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    dict(OTR_STATE, _pending_model_notes="str"), {"self._restore_session_model_override": None})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None}, {})
add("_restore_pending_one_turn_model_override", {"session_key": "k1", "run_generation": None},
    OTR_STATE)  # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    # per-func coverage report
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for func in sorted(counts):
        flag = "" if counts[func] >= 5 else "  <-- UNDER 5"
        print(f"  {func}: {counts[func]}{flag}")
    under = [f for f, n in counts.items() if n < 5]
    if under:
        raise SystemExit(f"FATAL: funcs under 5 cases: {under}")


if __name__ == "__main__":
    main()
