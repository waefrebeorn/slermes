#!/usr/bin/env python3
"""Generate tests/fixtures/run_inbound_flow3.jsonl — the scripted-seam case
corpus for the run_inbound_flow3 FLOW port (18 unclaimed methods of
gateway/run_inbound.py). >=5 cases per func: happy paths, every branch,
scripted Exception-family raises (swallowed where the live code catches),
BaseException passthrough (SystemExit), and fail-closed missing-script cases.

Deterministic, stdlib-only. The oracle AST-extracts the LIVE defs and replays
each case against the same scripts; the C harness feeds them to ri3_call.

Domain notes (shared with the port header): ASCII text; integral numbers at
str() sites; no NUL strings (SKIPPED responses excluded — behaviorally ==
falsy response); stores/adapters/agents/authorities scripted null or
NON-EMPTY records; pair-returning seams scripted one level deeper ([[a,b]]).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "run_inbound_flow3.jsonl")
CASES = []


def case(func, args=None, state=None, io=None):
    CASES.append({"func": func, "in": {"args": args or {}, "state": state or {},
                                       "io": io or {}}})


def rs(ret=None, raise_=None):
    d = {"__script__": True}
    if raise_ is not None:
        d["raise"] = raise_
    else:
        d["ret"] = ret
    return d


def rais(kind, msg="boom", **kw):
    d = {"type": kind, "message": msg}
    d.update(kw)
    return d


def ev(**kw):
    d = {"text": "hello", "message_type": "text"}
    d.update(kw)
    return d


def src(**kw):
    d = {"platform": "telegram", "chat_id": "c1", "user_id": "u1",
         "user_name": "Uma"}
    d.update(kw)
    return d


EV = ev()
SRC = src()
KEY = "telegram:c1"
AD = {"id": "a1", "retire": False}
AD_RETIRE = {"id": "a1", "retire": True}
SENT = "__agent_pending_sentinel__"


def turn(agent=None, ts=1000, ctx=None, **kw):
    d = {"started_ts": ts, "agent": agent}
    if ctx is not None:
        d["ctx"] = ctx
    d.update(kw)
    return d


def sess(agent=None, ts=1000, ctx=None, persistent=None):
    node = {"turn": turn(agent, ts, ctx)}
    if persistent is not None:
        node["persistent"] = persistent
    return {"session_states": {KEY: node}}


AG = {"id": "ag1", "activity": True}
AG_NOACT = {"id": "ag1", "activity": False}
AUTH = {"id": "auth1", "profile_id": "p1", "sessions": {"s1": {}},
        "_ordered_writes": None}
ROW_Q = {"status": "queued", "principal_id": "messaging:u1", "admission_id": "ad1"}
ROW_U = {"status": "unknown", "principal_id": "messaging:u2", "admission_id": "ad2"}
HANDLE_RUN = {"execution_state": "running", "execution_generation": 7}
HANDLE_IDLE = {"execution_state": "idle", "execution_generation": 3}
PENDING = {"clarify_id": "cl1", "response": None}

# ── _hm_offer_pairing_code ──
PC_IO = {"self._pairing_store_for": "ps1",
         "pairing_store._is_rate_limited": False,
         "pairing_store.generate_code": "CODE123",
         "self._delivery_adapter_for": AD,
         "pairing_profile_arg": "prof",
         "pairing_code_reply": "pair text",
         "adapter.send": None}
case("_hm_offer_pairing_code", {"source": SRC}, io=PC_IO)
case("_hm_offer_pairing_code", {"source": SRC},
     io={"self._pairing_store_for": None})
case("_hm_offer_pairing_code", {"source": SRC},
     io=dict(PC_IO, **{"pairing_store._is_rate_limited": True}))
case("_hm_offer_pairing_code", {"source": SRC},
     io=dict(PC_IO, **{"pairing_store.generate_code": rs(raise_=rais("OSError", "lock", errno=5))}))
case("_hm_offer_pairing_code", {"source": SRC},
     io=dict(PC_IO, **{"pairing_store.generate_code": rs(raise_=rais("TimeoutError", "slow"))}))
case("_hm_offer_pairing_code", {"source": SRC},
     io=dict(PC_IO, **{"pairing_store.generate_code": rs(raise_=rais("SystemExit", "1"))}))
case("_hm_offer_pairing_code", {"source": SRC},
     io={"self._pairing_store_for": "ps1",
         "pairing_store._is_rate_limited": False,
         "pairing_store.generate_code": None,
         "self._delivery_adapter_for": AD,
         "pairing_rate_limited_reply": "limited text",
         "adapter.send": None,
         "pairing_store._record_rate_limit": None})
case("_hm_offer_pairing_code", {"source": SRC},
     io=dict(PC_IO, **{"self._delivery_adapter_for": None}))
case("_hm_offer_pairing_code", {"source": SRC}, io={})   # fail-closed
case("_hm_offer_pairing_code", {"source": SRC},
     io=dict(PC_IO, **{"pairing_store.generate_code": "",
                       "pairing_rate_limited_reply": "limited",
                       "pairing_store._record_rate_limit": None}))
case("_hm_offer_pairing_code", {"source": SRC},
     io=dict(PC_IO, **{"adapter.send": rs(raise_=rais("RuntimeError"))}))
case("_hm_offer_pairing_code", {"source": src(platform=None)}, io=PC_IO)

# ── _hm_report_ignored_dm ──
IG_IO = {"display_hermes_home": "/home/h",
         "unauthorized_owner_hint": "hint-text",
         "UnauthorizedOwnerNotifier": "n1",
         "notifier.first_time": True,
         "notifier.notify": None}
case("_hm_report_ignored_dm", {"source": SRC}, state={"config": {}}, io=IG_IO)
case("_hm_report_ignored_dm", {"source": SRC},
     state={"_unauthorized_owner_notifier": "n0", "config": {}},
     io=dict(IG_IO, **{"notifier.first_time": False}))
case("_hm_report_ignored_dm", {"source": SRC}, io=IG_IO)   # no config
case("_hm_report_ignored_dm", {"source": SRC}, state={"config": None}, io=IG_IO)
case("_hm_report_ignored_dm", {"source": SRC}, state={"config": {}},
     io=dict(IG_IO, **{"UnauthorizedOwnerNotifier": None}))
case("_hm_report_ignored_dm", {"source": SRC},
     io=dict(IG_IO, **{"unauthorized_owner_hint": rs(raise_=rais("SystemExit", "2"))}))
case("_hm_report_ignored_dm", {"source": SRC}, io={})   # fail-closed
case("_hm_report_ignored_dm", {"source": SRC}, state={"config": {}},
     io=dict(IG_IO, **{"notifier.notify": rs(raise_=rais("ValueError"))}))

# ── _hm_write_update_response ──
WR_IO = {"path.write_text": None, "path.replace": None, "path.unlink": None}
case("_hm_write_update_response", {"response_text": "y"},
     state={"hermes_home": "/home/h"}, io=WR_IO)
case("_hm_write_update_response", {"response_text": "n"},
     state={"hermes_home": "/home/h"},
     io=dict(WR_IO, **{"path.write_text": rs(raise_=rais("PermissionError", "Permission denied", errno=13))}))
case("_hm_write_update_response", {"response_text": "y"},
     state={"hermes_home": "/home/h"},
     io=dict(WR_IO, **{"path.replace": rs(raise_=rais("OSError", "replace failed", errno=5))}))
case("_hm_write_update_response", {"response_text": "y"},
     state={"hermes_home": "/home/h"},
     io=dict(WR_IO, **{"path.unlink": rs(raise_=rais("FileNotFoundError", "gone", errno=2))}))
case("_hm_write_update_response", {"response_text": "y"},
     state={"hermes_home": "/home/h"},
     io=dict(WR_IO, **{"path.write_text": rs(raise_=rais("SystemExit", "3"))}))
case("_hm_write_update_response", {"response_text": "y"},
     state={"hermes_home": "/home/h"},
     io=dict(WR_IO, **{"path.write_text": rs(raise_=rais("ValueError"))}))
case("_hm_write_update_response", {"response_text": "y"},
     state={"hermes_home": "/home/h"}, io={})   # fail-closed RuntimeError passes
case("_hm_write_update_response", {"response_text": ""}, io=WR_IO)   # home ""
case("_hm_write_update_response", {"response_text": "y"},
     state={"hermes_home": "/home/h/"}, io=WR_IO)

# ── _hm_clarify_reply ──
CL_BASE = {"clarify.get_pending_for_session": PENDING,
           "self._refused_clarify": None,
           "self._pending_event_audio_paths": None,
           "clarify.attempt_text_response": "resolved",
           "self._delivery_adapter_for": AD_RETIRE,
           "adapter.resume_typing_for_chat": None,
           "t": "answered",
           "adapter.retire_clarify_card": None}
CL_ARGS = {"event": ev(text="answer"), "source": SRC, "key": KEY}
case("_hm_clarify_reply", CL_ARGS, io=CL_BASE)
case("_hm_clarify_reply", CL_ARGS,
     io={"clarify.get_pending_for_session": None})
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"self._refused_clarify": "refused"}))
case("_hm_clarify_reply", {"event": ev(text=""), "source": SRC, "key": KEY},
     io=dict(CL_BASE, **{"self._pending_event_audio_paths": rs(ret=["p1"]),
                         "self._transcribe_pending_audio_event_once": [[None, ["", "  "]]]}))
case("_hm_clarify_reply", {"event": ev(text="/stop"), "source": SRC, "key": KEY},
     io=CL_BASE)
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"clarify.attempt_text_response": "rejected_selection"}))
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"clarify.attempt_text_response": "rejected_prose",
                         "self._current_session_run_generation": 42,
                         "self._release_pending_clarify": True}))
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"clarify.attempt_text_response": "rejected_prose",
                         "self._current_session_run_generation": 42,
                         "self._release_pending_clarify": False}))
case("_hm_clarify_reply", CL_ARGS,
     io={"clarify.get_pending_for_session": rs(raise_=rais("ValueError"))})
case("_hm_clarify_reply", CL_ARGS,
     io={"clarify.get_pending_for_session": rs(raise_=rais("SystemExit", "4"))})
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"self._delivery_adapter_for": None}))
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"self._delivery_adapter_for": AD}))
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"adapter.resume_typing_for_chat": rs(raise_=rais("ValueError"))}))
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"adapter.retire_clarify_card": rs(raise_=rais("RuntimeError"))}))
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"adapter.retire_clarify_card": rs(raise_=rais("SystemExit", "5"))}))
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"clarify.get_pending_for_session": {"clarify_id": "cl1", "response": "typed answer"}}))
case("_hm_clarify_reply", CL_ARGS, io={})   # fail-closed -> except Exception -> None
case("_hm_clarify_reply", CL_ARGS,
     io={"clarify.get_pending_for_session": {"response": None}})   # clarify_id miss
case("_hm_clarify_reply", CL_ARGS,
     io=dict(CL_BASE, **{"clarify.get_pending_for_session": {"clarify_id": "cl1"}}))  # response miss swallowed
case("_hm_clarify_reply", {"event": ev(text="  "), "source": SRC, "key": KEY},
     io=dict(CL_BASE, **{"self._pending_event_audio_paths": rs(ret=["p1"]),
                         "self._transcribe_pending_audio_event_once": [[None, ["  "]]]}))

# ── _hm_evict_idle_stale_agent ──
EV_IO = {"_float_env": 1800, "time.time": [2000, 2000],
         "agent.get_activity_summary": {"last_activity_desc": "tool x",
                                        "api_call_count": 3, "max_iterations": 10},
         "resolve_session_idle_seconds_from_activity": 1900,
         "awake_idle.measure": 1900,
         "self._hm_evict_running_agent": None}
EV_CTX = {"awake_idle": {"id": "aw1"}}
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000, EV_CTX), io=EV_IO)
case("_hm_evict_idle_stale_agent", {"key": KEY}, io={"_float_env": 1800})
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(None, 1000), io={"_float_env": 1800})
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 0), io={"_float_env": 1800})
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(SENT, 1000),
     io={"_float_env": 1800, "time.time": 100000})   # sentinel never evicted
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG_NOACT, 1000),
     io={"_float_env": 1800, "time.time": 100000,
         "self._hm_evict_running_agent": None})   # wall TTL fallback
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000, EV_CTX),
     io=dict(EV_IO, **{"resolve_session_idle_seconds_from_activity": 10,
                       "awake_idle.measure": 10}))   # idle below timeout
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000),
     io={"_float_env": 1800, "time.time": [100000, 100000],
         "agent.get_activity_summary": {},
         "resolve_session_idle_seconds_from_activity": None,
         "self._hm_evict_running_agent": None})   # resolve None -> fallback
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000),
     io={"_float_env": 1800, "time.time": 100000,
         "agent.get_activity_summary": rs(raise_=rais("ValueError")),
         "self._hm_evict_running_agent": None})
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000),
     io={"_float_env": 1800, "time.time": 2000,
         "agent.get_activity_summary": rs(raise_=rais("SystemExit", "6"))})
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000, EV_CTX),
     io=dict(EV_IO, **{"awake_idle.measure": rs(raise_=rais("ValueError"))}))
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000),   # ctx missing -> AttributeError suppressed
     io={"_float_env": 1800, "time.time": [2000, 2000],
         "agent.get_activity_summary": {},
         "resolve_session_idle_seconds_from_activity": 1900,
         "self._hm_evict_running_agent": None})
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG_NOACT, 1000),
     io={"_float_env": 0, "time.time": 100000})   # timeout 0 -> wall inf
case("_hm_evict_idle_stale_agent", {"key": KEY}, io={})   # fail-closed
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state={"session_states": {KEY: {}}}, io={"_float_env": 1800})   # turn miss
case("_hm_evict_idle_stale_agent", {"key": KEY},
     state=sess(AG, 1000, {"awake_idle": None}),
     io=dict(EV_IO, **{"awake_idle.measure": 1900}))   # awake null -> no measure

# ── _hm_stop_owner_turn ──
ST_IO = {"active_authority": AUTH,
         "session_store.peek_session_id": "s1",
         "authority.logical_owner": "s1",
         "authority_persistence": "st1",
         "store.admissions.list": [[ROW_Q], [ROW_Q]],
         "authority.fence_route": None,
         "authority.cancel_queued": None,
         "authority._handle": HANDLE_RUN,
         "authority.interrupt": None}
ST_STATE = {"session_store": "ss1"}
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE, io=ST_IO)
case("_hm_stop_owner_turn", {"key": KEY}, io={"active_authority": None})
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority.logical_owner": None}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority.logical_owner": "sX"}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io={"active_authority": AUTH,
         "session_store.peek_session_id": "s1",
         "authority.logical_owner": "s1",
         "authority_persistence": "st1",
         "store.admissions.list": [[ROW_U]],
         "authority._handle": HANDLE_IDLE})   # paused
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority_persistence": None}))   # StoreUnavailable
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority.cancel_queued": rs(raise_=rais("AuthorityRefusal", "stale", reason="stale_generation"))}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority.cancel_queued": rs(raise_=rais("ValueError"))}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority.interrupt": rs(raise_=rais("AuthorityRefusal", "stale", reason="stale_generation"))}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority.interrupt": rs(raise_=rais("SystemExit", "7"))}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"active_authority": dict(AUTH, _ordered_writes="ow1"),
                       "ordered.__aenter__": None, "ordered.__aexit__": None}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"active_authority": dict(AUTH, _ordered_writes="ow1"),
                       "ordered.__aenter__": rs(raise_=rais("RuntimeError"))}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"authority._handle": HANDLE_IDLE}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"store.admissions.list": [[dict(ROW_Q, principal_id="local:u1")],
                                                 [dict(ROW_Q, principal_id="local:u1")]]}))
case("_hm_stop_owner_turn", {"key": KEY}, state={"session_store": None},
     io={"active_authority": AUTH})   # None.peek_session_id
case("_hm_stop_owner_turn", {"key": KEY}, io={})   # fail-closed
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"store.admissions.list": rs(raise_=rais("OSError", "db"))}))
case("_hm_stop_owner_turn", {"key": KEY}, state=ST_STATE,
     io=dict(ST_IO, **{"store.admissions.list": 5}))   # not iterable

# ── _hm_handle_running_session_message ──
RS_ARGS = {"event": EV, "source": SRC, "key": KEY}
RS_IO = {"self._hm_busy_slash_or_photo": [[False, None]],
         "self._busy_skill_turn": [[False, None]],
         "self._clarify_takeover": False,
         "self._effective_busy_input_mode": "interrupt",
         "self._hm_busy_telegram_grace_queue": False,
         "self._agent_has_active_subagents": False,
         "self._session_has_compression_in_flight": False,
         "self._hm_busy_interrupt": None}
RS_STATE = sess(AG, 1000)
case("_hm_handle_running_session_message",
     {"event": ev(internal=True), "source": SRC, "key": KEY},
     io={"event.get_command": None, "self._queue_or_replace_pending_event": None})
case("_hm_handle_running_session_message", RS_ARGS,
     io={"self._hm_busy_slash_or_photo": [[True, "slash-result"]]})
case("_hm_handle_running_session_message", RS_ARGS,
     io={"self._hm_busy_slash_or_photo": [[False, None]],
         "self._busy_skill_turn": [[True, "skill-result"]]})
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE, io=RS_IO)
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE,
     io=dict(RS_IO, **{"self._effective_busy_input_mode": "queue",
                       "self._queue_or_replace_pending_event": None}))
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE,
     io=dict(RS_IO, **{"self._effective_busy_input_mode": "steer",
                       "self._hm_busy_steer": None}))
case("_hm_handle_running_session_message",
     {"event": EV, "source": SRC, "key": KEY},
     state=sess(SENT, 1000),
     io=dict(RS_IO, **{"event.get_command": "stop",
                       "cancel_preagent_setup": None,
                       "self._release_running_agent_state": None,
                       "t": "force-stopped"}))
case("_hm_handle_running_session_message", RS_ARGS,
     state=sess(SENT, 1000),
     io=dict(RS_IO, **{"event.get_command": None,
                       "self._hm_merge_pending_for_source": None,
                       "pending_setup_busy_notice": "notice"}))
case("_hm_handle_running_session_message", RS_ARGS,
     state=dict(RS_STATE, _draining=True),
     io=dict(RS_IO, **{"self._queue_during_drain_enabled": True,
                       "self._queue_or_replace_pending_event": None,
                       "self._status_action_gerund": "running",
                       "t": "drain-queued"}))
case("_hm_handle_running_session_message", RS_ARGS,
     state=dict(RS_STATE, _draining=True),
     io=dict(RS_IO, **{"self._queue_during_drain_enabled": False,
                       "self._status_action_gerund": "running",
                       "t": "drain-rejected"}))
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE,
     io=dict(RS_IO, **{"self._agent_has_active_subagents": True,
                       "self._queue_or_replace_pending_event": None}))
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE,
     io=dict(RS_IO, **{"self._session_has_compression_in_flight": True,
                       "self._queue_or_replace_pending_event": None}))
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE,
     io=dict(RS_IO, **{"self._hm_busy_telegram_grace_queue": True}))
case("_hm_handle_running_session_message",
     {"event": ev(internal=False), "source": SRC, "key": KEY},
     state=RS_STATE, io=RS_IO)   # internal falsy -> no get_command
case("_hm_handle_running_session_message", RS_ARGS, io={})   # fail-closed
case("_hm_handle_running_session_message", RS_ARGS,
     io={"self._hm_busy_slash_or_photo": rs(raise_=rais("SystemExit", "8"))})
case("_hm_handle_running_session_message", RS_ARGS,
     state={"session_states": {KEY: {}}},
     io=dict(RS_IO, **{"self._hm_busy_telegram_grace_queue": False}))   # turn miss
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE,
     io=dict(RS_IO, **{"self._hm_busy_interrupt": rs(raise_=rais("ValueError"))}))
case("_hm_handle_running_session_message", RS_ARGS, state=RS_STATE,
     io=dict(RS_IO, **{"self._clarify_takeover": True}))   # takeover -> interrupt mode

# ── _prefix_inbound_sender_context ──
PX_ARGS = {"event": EV, "source": SRC, "message_text": "body"}
case("_prefix_inbound_sender_context", PX_ARGS,
     io={"label_shared_sender": "labeled"})
case("_prefix_inbound_sender_context",
     {"event": ev(channel_context="ctx"), "source": SRC, "message_text": "body"},
     io={"label_shared_sender": "labeled"})
case("_prefix_inbound_sender_context", PX_ARGS,
     state={"config": {"group_sessions_per_user": False,
                       "thread_sessions_per_user": True}},
     io={"label_shared_sender": "labeled"})
case("_prefix_inbound_sender_context", PX_ARGS,
     io={"label_shared_sender": rs(raise_=rais("SystemExit", "9"))})
case("_prefix_inbound_sender_context", PX_ARGS, io={})   # fail-closed
case("_prefix_inbound_sender_context", PX_ARGS, state={"config": None},
     io={"label_shared_sender": "labeled"})

# ── _classify_inbound_media ──
CM_EV = ev(media_urls=["p1", "p2", "p3"],
           media_types=["image/png", "audio/ogg", "video/mp4"],
           message_type="photo")
CM_IO = {"event_media_is_image": [True, False, False],
         "event_media_is_audio": [False, True, False],
         "event_media_is_stt_input": [True]}
case("_classify_inbound_media", {"event": CM_EV}, io=CM_IO)
case("_classify_inbound_media",
     {"event": ev(media_urls=["p1"], media_types=["audio/ogg"], message_type="audio")},
     io={"event_media_is_image": [False], "event_media_is_audio": [True]})
case("_classify_inbound_media",
     {"event": ev(media_urls=["p1"], media_types=["audio/ogg"], message_type="document")},
     io={"event_media_is_image": [False], "event_media_is_audio": [True]})
case("_classify_inbound_media", {"event": CM_EV, "pending_stt_prepared": True},
     io={"event_media_is_image": [True, False, False],
         "event_media_is_audio": [False, True, False]})
case("_classify_inbound_media",
     {"event": ev(media_urls=["p1"], media_types=[""], message_type="video")},
     io={"event_media_is_image": [False], "event_media_is_audio": [False]})
case("_classify_inbound_media",
     {"event": ev(media_urls=["p1"], media_types=[None], message_type="text")},
     io={"event_media_is_image": [False], "event_media_is_audio": [False]})
case("_classify_inbound_media",
     {"event": ev(media_urls=["p1"], media_types=None, message_type="text")},
     io={})
case("_classify_inbound_media", {"event": CM_EV},
     io={"event_media_is_image": rs(raise_=rais("SystemExit", "10"))})
case("_classify_inbound_media",
     {"event": ev(media_urls=None, media_types=[], message_type="text")}, io={})
case("_classify_inbound_media", {"event": CM_EV}, io={})   # fail-closed
case("_classify_inbound_media",
     {"event": ev(media_urls=["p1", "p2"], media_types=["image/png"],
                  message_type="photo")},
     io={"event_media_is_image": [False, False],
         "event_media_is_audio": [False, False]})   # mtype absent -> ""

# ── _enrich_inbound_images ──
EN_ARGS = {"source": SRC, "session_key": KEY, "message_text": "body",
           "image_paths": ["i1", "i2"]}
EN_IO = {"self._decide_image_input_mode": "text",
         "self._resolve_session_agent_runtime": [["m1", {"base_url": "u"}]],
         "scoped_runtime_main": "cm1",
         "scoped_runtime_main.__enter__": None,
         "self._enrich_message_with_vision": "enriched",
         "scoped_runtime_main.__exit__": None}
case("_enrich_inbound_images", EN_ARGS,
     io={"self._decide_image_input_mode": "native"},
     state={"session_states": {KEY: {"persistent": {}}}})
case("_enrich_inbound_images", EN_ARGS, io=EN_IO)
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"self._resolve_session_agent_runtime": rs(raise_=rais("ValueError"))}))
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"self._resolve_session_agent_runtime": rs(raise_=rais("SystemExit", "11"))}))
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"self._enrich_message_with_vision": rs(raise_=rais("ValueError"))}))
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"self._enrich_message_with_vision": rs(raise_=rais("ValueError")),
                       "scoped_runtime_main.__exit__": True}))   # suppressed
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"scoped_runtime_main.__exit__": rs(raise_=rais("RuntimeError"))}))
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"scoped_runtime_main.__enter__": rs(raise_=rais("SystemExit", "12"))}))
case("_enrich_inbound_images", EN_ARGS,
     io={"self._decide_image_input_mode": "native"},
     state={"session_states": {KEY: {}}})   # persistent miss
case("_enrich_inbound_images", EN_ARGS, io={})   # fail-closed
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"self._resolve_session_agent_runtime": [["m1", None]]}))
case("_enrich_inbound_images", EN_ARGS,
     io=dict(EN_IO, **{"self._resolve_session_agent_runtime": [["m1", {}, "extra"]]}))  # unpack swallowed

# ── _echo_stt_transcripts ──
EC_ARGS = {"adapter": AD, "source": SRC, "transcripts": ["t1", "t2"],
           "metadata": {"k": "v"}}
EC_IO = {"automatic_notices_allowed": True,
         "t": "echo",
         "adapter.send": [{"success": True}, {"success": False, "error": "no"}]}
case("_echo_stt_transcripts", EC_ARGS, io=EC_IO)
case("_echo_stt_transcripts", EC_ARGS,
     io={"automatic_notices_allowed": False})
case("_echo_stt_transcripts", EC_ARGS,
     io=dict(EC_IO, **{"adapter.send": [rs(raise_=rais("ValueError")), {"success": True}]}))
case("_echo_stt_transcripts", EC_ARGS,
     io=dict(EC_IO, **{"adapter.send": rs(raise_=rais("SystemExit", "13"))}))
case("_echo_stt_transcripts", dict(EC_ARGS, adapter=None), io=EC_IO)
case("_echo_stt_transcripts", EC_ARGS,
     io=dict(EC_IO, **{"adapter.send": [None, None]}))
case("_echo_stt_transcripts", EC_ARGS, io={})   # fail-closed
case("_echo_stt_transcripts", dict(EC_ARGS, metadata=None), io=EC_IO)
case("_echo_stt_transcripts", EC_ARGS,
     io=dict(EC_IO, **{"t": rs(raise_=rais("RuntimeError"))}))
case("_echo_stt_transcripts", dict(EC_ARGS, transcripts=[]),
     io={"automatic_notices_allowed": True})

# ── _inbound_attachment_display_name ──
DN_IO = {"clean_attachment_filename": "report.pdf",
         "to_agent_visible_cache_path": "/agent/p"}
case("_inbound_attachment_display_name",
     {"path": "/cache/1_2_report.pdf", "original_name": "report.pdf"}, io=DN_IO)
case("_inbound_attachment_display_name", {"path": "/cache/1_2_photo.jpg"},
     io={"clean_attachment_filename": "", "to_agent_visible_cache_path": "/agent/p"})
case("_inbound_attachment_display_name", {"path": "/cache/relay_media_xyz"},
     io={"clean_attachment_filename": "", "to_agent_visible_cache_path": "/agent/p"})
case("_inbound_attachment_display_name", {"path": "/cache/plain"},
     io={"clean_attachment_filename": "", "to_agent_visible_cache_path": "/agent/p"})
case("_inbound_attachment_display_name", {"path": "/cache/x", "original_name": "bad/name?"},
     io={"clean_attachment_filename": "bad/name?", "to_agent_visible_cache_path": "/agent/p"})
case("_inbound_attachment_display_name", {"path": "/cache/x"},
     io={"clean_attachment_filename": rs(raise_=rais("SystemExit", "14"))})
case("_inbound_attachment_display_name", {"path": "/cache/x"},
     io={"clean_attachment_filename": "",
         "to_agent_visible_cache_path": rs(raise_=rais("ValueError"))})
case("_inbound_attachment_display_name", {"path": "/cache/x"}, io={})   # fail-closed
case("_inbound_attachment_display_name", {"path": "/cache/a_b"},
     io={"clean_attachment_filename": "", "to_agent_visible_cache_path": "/agent/p"})
case("_inbound_attachment_display_name", {"path": "/cache/x", "original_name": "my file-v2.txt"},
     io={"clean_attachment_filename": "my file-v2.txt",
         "to_agent_visible_cache_path": "/agent/p"})

# ── _prepend_inbound_media_file_notes ──
MN_IO = {"clean_attachment_filename": "", "to_agent_visible_cache_path": "/agent/p"}
case("_prepend_inbound_media_file_notes",
     {"message_text": "body", "audio_file_paths": ["/c/1_2_a.mp3"],
      "video_paths": ["/c/1_2_v.mp4"]}, io=MN_IO)
case("_prepend_inbound_media_file_notes",
     {"message_text": "body", "audio_file_paths": [], "video_paths": []}, io={})
case("_prepend_inbound_media_file_notes",
     {"message_text": "body", "audio_file_paths": ["/c/1_2_a.mp3"],
      "video_paths": [], "original_names": {"/c/1_2_a.mp3": "song.mp3"}},
     io={"clean_attachment_filename": "song.mp3",
         "to_agent_visible_cache_path": "/agent/p"})
case("_prepend_inbound_media_file_notes",
     {"message_text": "body", "audio_file_paths": ["/c/x"], "video_paths": []},
     io={"clean_attachment_filename": rs(raise_=rais("SystemExit", "15"))})
case("_prepend_inbound_media_file_notes",
     {"message_text": "body", "audio_file_paths": ["/c/x"], "video_paths": []},
     io={})   # fail-closed
case("_prepend_inbound_media_file_notes",
     {"message_text": "body", "audio_file_paths": ["/c/1_2_a.mp3", "/c/3_4_b.mp3"],
      "video_paths": []}, io=MN_IO)
case("_prepend_inbound_media_file_notes",
     {"message_text": "", "audio_file_paths": [], "video_paths": ["/c/1_2_v.mp4"]},
     io=MN_IO)

# ── _prepend_inbound_document_notes ──
DC_EV = ev(media_urls=["d1.pdf", "d2.txt"],
           media_types=["application/octet-stream", ""],
           media_filenames=["orig.pdf", ""],
           media_text_inlined=[True, None])
DC_IO = {"event_media_is_image": [False, False],
         "event_media_is_audio": [False, False],
         "event_media_is_video": [False, False],
         "mimetypes.guess_type": [["application/pdf", None]],
         "clean_attachment_filename": ["orig.pdf", ""],
         "to_agent_visible_cache_path": ["/agent/d1", "/agent/d2"],
         "build_document_context_note": ["note-pdf", "note-txt"]}
case("_prepend_inbound_document_notes", {"event": DC_EV, "message_text": "body"},
     io=DC_IO)
case("_prepend_inbound_document_notes",
     {"event": ev(media_urls=None, media_types=[]), "message_text": "body"}, io={})
case("_prepend_inbound_document_notes", {"event": DC_EV, "message_text": "body"},
     io=dict(DC_IO, **{"event_media_is_image": [True, False],
                       "mimetypes.guess_type": [],
                       "clean_attachment_filename": [""],
                       "to_agent_visible_cache_path": ["/agent/d2"],
                       "build_document_context_note": ["note-txt"]}))
case("_prepend_inbound_document_notes",
     {"event": ev(media_urls=["d1.pdf"], media_types=["application/pdf"],
                  media_filenames=[], media_text_inlined=[]),
      "message_text": "body"},
     io={"event_media_is_image": [False], "event_media_is_audio": [False],
         "event_media_is_video": [False],
         "clean_attachment_filename": [""],
         "to_agent_visible_cache_path": ["/agent/d1"],
         "build_document_context_note": ["note"]})
case("_prepend_inbound_document_notes",
     {"event": ev(media_urls=["d1.bin"], media_types=[None],
                  media_filenames=[], media_text_inlined=[]),
      "message_text": "body"},
     io={"event_media_is_image": [False], "event_media_is_audio": [False],
         "event_media_is_video": [False],
         "clean_attachment_filename": [""],
         "to_agent_visible_cache_path": ["/agent/d1"],
         "build_document_context_note": ["note"]})
case("_prepend_inbound_document_notes", {"event": DC_EV, "message_text": "body"},
     io=dict(DC_IO, **{"mimetypes.guess_type": rs(raise_=rais("SystemExit", "16"))}))
case("_prepend_inbound_document_notes",
     {"event": ev(media_urls=["d1.txt"], media_types=[""],
                  media_filenames=[], media_text_inlined=[1]),
      "message_text": "body"},
     io={"event_media_is_image": [False], "event_media_is_audio": [False],
         "event_media_is_video": [False],
         "clean_attachment_filename": [""],
         "to_agent_visible_cache_path": ["/agent/d1"],
         "build_document_context_note": ["note"]})   # 1 is not True
case("_prepend_inbound_document_notes", {"event": DC_EV, "message_text": "body"},
     io={})   # fail-closed
case("_prepend_inbound_document_notes",
     {"event": ev(media_urls=["d1.bin"], media_types=[""],
                  media_filenames=[], media_text_inlined=[]),
      "message_text": "body"},
     io={"event_media_is_image": [False], "event_media_is_audio": [False],
         "event_media_is_video": [False],
         "mimetypes.guess_type": [[None, None]],
         "clean_attachment_filename": [""],
         "to_agent_visible_cache_path": ["/agent/d1"],
         "build_document_context_note": ["note"]})
case("_prepend_inbound_document_notes",
     {"event": ev(media_urls=["d1.TXT"], media_types=[""],
                  media_filenames=[], media_text_inlined=[]),
      "message_text": "body"},
     io={"event_media_is_image": [False], "event_media_is_audio": [False],
         "event_media_is_video": [False],
         "clean_attachment_filename": [""],
         "to_agent_visible_cache_path": ["/agent/d1"],
         "build_document_context_note": ["note"]})

# ── _prepend_inbound_reply_context ──
RP_IO = {"reply_pointer": "pointer"}
case("_prepend_inbound_reply_context",
     {"event": ev(reply_to_text="quoted", reply_to_message_id="m0"),
      "source": SRC, "message_text": "body"}, io=RP_IO)
case("_prepend_inbound_reply_context",
     {"event": ev(message_id="m1"), "source": src(platform="discord"),
      "message_text": "body"},
     io={"_discord_tools_loaded": True, "discord_triggering_note": "note"})
case("_prepend_inbound_reply_context",
     {"event": ev(message_id="m1"), "source": src(platform="discord"),
      "message_text": "body"},
     io={"_discord_tools_loaded": False})
case("_prepend_inbound_reply_context",
     {"event": ev(reply_to_text="quoted", reply_to_message_id="m0",
                  reply_to_is_own_message=True),
      "source": SRC, "message_text": "body"}, io=RP_IO)
case("_prepend_inbound_reply_context",
     {"event": ev(reply_to_text=None), "source": SRC, "message_text": "body"},
     io={})
case("_prepend_inbound_reply_context",
     {"event": ev(reply_to_text="quoted", reply_to_message_id="m0"),
      "source": SRC, "message_text": "body"},
     io={"reply_pointer": rs(raise_=rais("SystemExit", "17"))})
case("_prepend_inbound_reply_context",
     {"event": ev(reply_to_text="quoted", reply_to_message_id="m0"),
      "source": SRC, "message_text": "body"}, io={})   # fail-closed
case("_prepend_inbound_reply_context",
     {"event": ev(reply_to_text="quoted", reply_to_message_id="m0"),
      "source": None, "message_text": "body"}, io=RP_IO)
case("_prepend_inbound_reply_context",
     {"event": ev(message_id=None), "source": src(platform="discord"),
      "message_text": "body"}, io={})
case("_prepend_inbound_reply_context",
     {"event": ev(reply_to_text="quoted", reply_to_message_id="m0", message_id="m1"),
      "source": src(platform="discord"), "message_text": "body"},
     io={"reply_pointer": "pointer", "_discord_tools_loaded": True,
         "discord_triggering_note": "note"})

# ── _inbound_model_context_length ──
MC_CFG = {"model": {"default": "m1", "context_length": 128000}}
MC_RT = ["m1", {"base_url": "u", "api_key": "k", "provider": "p"}]
MC_IO = {"_load_gateway_config": MC_CFG,
         "get_compatible_custom_providers": [],
         "self._resolve_session_agent_runtime": [MC_RT],
         "should_clear_context_pin_async": False,
         "get_model_context_length_async": 200000}
MC_ARGS = {"source": SRC, "session_key": KEY}
case("_inbound_model_context_length", MC_ARGS, io=MC_IO)
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"self._resolve_session_agent_runtime": [["m2", {"base_url": "u"}]]}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"should_clear_context_pin_async": True}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"should_clear_context_pin_async": rs(raise_=rais("ValueError"))}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"get_compatible_custom_providers": [{"base_url": "u"}],
                       "get_custom_provider_context_length": 50000}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"get_compatible_custom_providers": [{"base_url": "u"}],
                       "get_custom_provider_context_length": 0}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"_load_gateway_config": rs(raise_=rais("ValueError"))}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"_load_gateway_config": rs(raise_=rais("SystemExit", "18"))}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"_load_gateway_config": {"model": {"default": "m1", "context_length": "abc"}}}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"_load_gateway_config": {"model": {"default": "m1", "context_length": "  4096  "}}}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"get_compatible_custom_providers": rs(raise_=rais("RuntimeError")),
                       "_load_gateway_config": dict(MC_CFG, custom_providers=[{"base_url": "u"}]),
                       "get_custom_provider_context_length": 60000}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"self._resolve_session_agent_runtime": [[None, None]]}))
case("_inbound_model_context_length", MC_ARGS, io={})   # fail-closed chain
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"_load_gateway_config": {"model": "x"}}))
case("_inbound_model_context_length", MC_ARGS,
     io=dict(MC_IO, **{"_load_gateway_config": {"model": {"default": "m1", "context_length": 128000.9}}}))

# ── _expand_inbound_context_references ──
EX_ARGS = {"source": SRC, "session_key": KEY, "message_text": "body"}
case("_expand_inbound_context_references", EX_ARGS,
     io={"inbound_references": [["expanded", None]]})
case("_expand_inbound_context_references", EX_ARGS,
     io={"inbound_references": [["expanded", "refused"]],
         "automatic_notices_allowed": True,
         "self._delivery_adapter_for": AD,
         "adapter.send": None})
case("_expand_inbound_context_references", EX_ARGS,
     io={"inbound_references": [["expanded", "refused"]],
         "automatic_notices_allowed": False})
case("_expand_inbound_context_references", EX_ARGS,
     io={"inbound_references": [["expanded", "refused"]],
         "automatic_notices_allowed": True,
         "self._delivery_adapter_for": None})
case("_expand_inbound_context_references", EX_ARGS,
     io={"inbound_references": rs(raise_=rais("SystemExit", "19"))})
case("_expand_inbound_context_references", EX_ARGS,
     io={"inbound_references": [["a", "b", "c"]]})   # unpack ValueError
case("_expand_inbound_context_references", EX_ARGS, io={})   # fail-closed
case("_expand_inbound_context_references", EX_ARGS,
     io={"inbound_references": [["expanded", "refused"]],
         "automatic_notices_allowed": True,
         "self._delivery_adapter_for": AD,
         "adapter.send": rs(raise_=rais("ValueError"))})

# ── _prepare_clarify_reply_text ──
case("_prepare_clarify_reply_text",
     {"event": ev(_telegram_replied_voice_typed_text="typed")}, io={})
case("_prepare_clarify_reply_text", {"event": ev(text="  hi  ")},
     io={"self._pending_event_audio_paths": None})
case("_prepare_clarify_reply_text", {"event": EV},
     io={"self._pending_event_audio_paths": rs(ret=["p1"]),
         "self._transcribe_pending_audio_event_once": [[None, [" a ", "", "b"]]]})
case("_prepare_clarify_reply_text", {"event": EV},
     io={"self._pending_event_audio_paths": rs(ret=["p1"]),
         "self._transcribe_pending_audio_event_once": [[None, []]]})
case("_prepare_clarify_reply_text", {"event": ev(text=None)},
     io={"self._pending_event_audio_paths": None})
case("_prepare_clarify_reply_text", {"event": EV},
     io={"self._pending_event_audio_paths": rs(ret=["p1"]),
         "self._transcribe_pending_audio_event_once": rs(raise_=rais("SystemExit", "20"))})
case("_prepare_clarify_reply_text", {"event": EV}, io={})   # fail-closed
case("_prepare_clarify_reply_text", {"event": EV},
     io={"self._pending_event_audio_paths": rs(ret=["p1"]),
         "self._transcribe_pending_audio_event_once": [[None]]})   # unpack
case("_prepare_clarify_reply_text", {"event": ev(text="plain")},
     io={"self._pending_event_audio_paths": None})
case("_prepare_clarify_reply_text",
     {"event": ev(_telegram_replied_voice_typed_text="")},
     io={"self._pending_event_audio_paths": None})   # typed falsy -> fall through


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print(f"  {fn}: {counts[fn]}{flag}")
    assert all(v >= 5 for v in counts.values()), "every func needs >=5 cases"


if __name__ == "__main__":
    main()