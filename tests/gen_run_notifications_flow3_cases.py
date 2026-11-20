#!/usr/bin/env python3
"""Generate tests/fixtures/run_notifications_flow3.jsonl for the
run_notifications_flow3 oracle (t_port_run_notifications_flow3.c harness).

Case shape: {"func": <live name minus leading underscore>, "in": {...}}.
"in"."io" carries the universal-seam scripts: a plain value (every call) or
{"__seq__": [...]} (one element per call, last repeats). {"__raise__": kind,
"msg": text} scripts a raised exception. Deterministic output; >=5 cases/func.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def seq(*vs):
    return {"__seq__": list(vs)}


def rs(kind, msg, **extra):
    d = {"__raise__": kind, "msg": msg}
    d.update(extra)
    return d


def case(func, **inp):
    return {"func": func, "in": inp}


def claim(**kw):
    d = {"delegation_id": "", "claim_id": "", "proceed": True, "early_result": None,
         "owner_session_id": "", "proven_session_id": "", "defer": False}
    d.update(kw)
    return d


CASES = []

# ── 1. send_queued_final_text ──
SQ_SRC = {"chat_id": "c1", "platform": "telegram"}
SQ_BASE = {"_mark_notify_metadata": {"n": 1}, "logger.warning": None}
CASES += [
    case("send_queued_final_text", self={}, adapter={"__base__": True, "send_final_ledgered": True},
         source=dict(SQ_SRC), text_content="answer", metadata={"m": 1}, event_message_id="e1",
         session_key="***", inbound_message_id="i1",
         io=dict(SQ_BASE, **{"adapter.send_final_ledgered": [{"success": True}, None]})),
    case("send_queued_final_text", self={}, adapter={"__base__": True, "send_final_ledgered": True},
         source=dict(SQ_SRC), text_content="answer", metadata=None, event_message_id=None,
         session_key="***", inbound_message_id=None,
         io=dict(SQ_BASE, **{"adapter.send_final_ledgered": [{"success": False, "error": "flood"}, None]})),
    case("send_queued_final_text", self={}, adapter={"__base__": True, "send_final_ledgered": True},
         source=dict(SQ_SRC), text_content="answer", metadata=None, event_message_id=None,
         session_key="***", inbound_message_id=None,
         io=dict(SQ_BASE, **{"adapter.send_final_ledgered": [{"success": False}, None]})),
    case("send_queued_final_text", self={}, adapter={"send": True}, source=dict(SQ_SRC),
         text_content="answer", metadata={"m": 1}, event_message_id=None, session_key=None,
         inbound_message_id=None, io={"adapter.send": {"success": True}}),
    case("send_queued_final_text", self={}, adapter={"send": True}, source={},
         text_content="answer", metadata=None, event_message_id=None, session_key=None,
         inbound_message_id=None, io={}),
    case("send_queued_final_text", self={}, adapter={"__base__": True, "send_final_ledgered": True},
         source=dict(SQ_SRC), text_content="answer", metadata=None, event_message_id=None,
         session_key="***", inbound_message_id=None,
         io=dict(SQ_BASE, **{"adapter.send_final_ledgered": [{"success": True}]})),
    case("send_queued_final_text", self={}, adapter={"__base__": True, "send_final_ledgered": True},
         source=dict(SQ_SRC), text_content="answer", metadata=None, event_message_id=None,
         session_key="***", inbound_message_id=None,
         io=dict(SQ_BASE, **{"adapter.send_final_ledgered": rs("SystemExit", "bye")})),
    case("send_queued_final_text", self={}, adapter={"send": True}, source=dict(SQ_SRC),
         text_content="answer", metadata=None, event_message_id=None, session_key="***",
         inbound_message_id=None, io={"adapter.send": {"success": True}}),
]

# ── 2. send_home_channel_message ──
HC_HOME = {"chat_id": "c1", "thread_id": "t1", "user_id": "u1", "scope_id": "s1"}
HC_TR = {"adapter": {"name": "a1", "send": True}, "is_relay": False, "send": True}
HC_IO = {"self._thread_metadata_for_target": {"m": 1}, "_non_conversational_metadata": {"nc": 1},
         "transport.send": {"success": True}, "adapter.send": {"success": True}, "logger.warning": None}
CASES += [
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s", io=dict(HC_IO)),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s",
         io=dict(HC_IO, _non_conversational_metadata=None)),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR, is_relay=True), message="hi", failure_fmt="fail %s:%s: %s",
         io=dict(HC_IO, **{"self._thread_metadata_for_target": None})),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s",
         io=dict(HC_IO, **{"transport.send": {"success": False, "error": "dead"}})),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s",
         io=dict(HC_IO, **{"transport.send": {"success": False}})),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s",
         io=dict(HC_IO, **{"transport.send": rs("RuntimeError", "boom")})),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s",
         io=dict(HC_IO, **{"transport.send": rs("KeyboardInterrupt", "ki")})),
    case("send_home_channel_message", self={}, platform="telegram", home={"thread_id": "t1"},
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s", io=dict(HC_IO)),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s",
         io=dict(HC_IO, **{"transport.send": {"success": 0}})),
]

# ── 3. replay_pending_planned_restart_notification ──
RP_CFG = [[None, "telegram", {"home_channel": {"chat_id": "c1", "thread_id": "t1"},
                              "gateway_restart_notification": True}]]
RP_IO = {"self._note_restart_notice_boot": None, "_planned_restart_notification_path": "/h/restart.json",
         "path.exists": True, "path.read_text": '{"delivered_targets":[]}',
         "self._served_home_channel_configs": RP_CFG, "_served_notice_target_key": ["telegram", "c1", "t1"],
         "self._send_home_channel_startup_notifications": [["telegram", "c1", "t1"]],
         "path.unlink": None, "atomic_json_write": None, "logger.warning": None}
CASES += [
    case("replay_pending_planned_restart_notification", self={"_planned_restart_notice_lock": None}, io=dict(RP_IO)),
    case("replay_pending_planned_restart_notification", self={"_planned_restart_notice_lock": None},
         io=dict(RP_IO, **{"self._send_home_channel_startup_notifications": []})),
    case("replay_pending_planned_restart_notification", self={}, io=dict(RP_IO, **{"path.exists": False})),
    case("replay_pending_planned_restart_notification", self={}, io=dict(RP_IO, **{"path.read_text": ""})),
    case("replay_pending_planned_restart_notification", self={},
         io=dict(RP_IO, **{"path.read_text": "\ufeff{\"delivered_targets\":[[\"telegram\",\"c1\",\"t1\"]]}",
                           "self._send_home_channel_startup_notifications": []})),
    case("replay_pending_planned_restart_notification", self={}, io=dict(RP_IO, **{"path.read_text": "[1]"})),
    case("replay_pending_planned_restart_notification", self={},
         io=dict(RP_IO, **{"self._note_restart_notice_boot": rs("RuntimeError", "boot")})),
    case("replay_pending_planned_restart_notification", self={},
         io=dict(RP_IO, **{"path.read_text": rs("SystemExit", "bye")})),
    case("replay_pending_planned_restart_notification", self={},
         io=dict(RP_IO, **{"self._served_home_channel_configs":
                           [[None, "telegram", {"home_channel": {"chat_id": "c1", "thread_id": "t1"},
                                                "gateway_restart_notification": False}]],
                           "self._send_home_channel_startup_notifications": []})),
]

# ── 4. send_session_db_warning_notifications ──
DB_TR = [["telegram", {}, {"chat_id": "c1", "thread_id": "t1", "user_id": None, "scope_id": None},
          {"adapter": {"name": "a1", "send": True}, "is_relay": False, "send": True}]]
DB_IO = {"classify_persistence_error": "locked", "profile_cli_selector": "--profile p2",
         "describe_storage_failure": {"action": "retry", "cause": "locked", "gloss": "the store is locked"},
         "t": "MSG", "logger.warning": None, "logger.info": None,
         "self._home_channel_transports": DB_TR, "warning_notifications_enabled": True,
         "self._thread_metadata_for_target": {"m": 1}, "_non_conversational_metadata": {"nc": 1},
         "transport.send": {"success": True}}
CASES += [
    case("send_session_db_warning_notifications", self={}, io={}),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "database is locked"},
         io=dict(DB_IO, classify_persistence_error="corrupt", profile_scope="scope",
                 store_location="/h/state.db", get_default_hermes_root="/h/root")),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "fts broken"},
         io=dict(DB_IO, classify_persistence_error="fts_index")),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "locked"},
         io=dict(DB_IO, **{"describe_storage_failure": {"action": "retry", "cause": "compression", "gloss": "g"}})),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "locked"},
         io=dict(DB_IO, t=seq("WRAPPED", "MSG"),
                 **{"describe_storage_failure": {"action": "retry", "cause": "other", "gloss": "g"}})),
    case("send_session_db_warning_notifications",
         self={"_session_db_init_error": "locked", "_session_db_handle_cache": {}},
         io=dict(DB_IO, **{"self._open_session_db_for_active_scope": None})),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "locked"},
         io=dict(DB_IO, warning_notifications_enabled=False)),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "locked"},
         io=dict(DB_IO, **{"describe_storage_failure": {"action": "a", "cause": ["x"], "gloss": "g"}})),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "locked"},
         io=dict(DB_IO, classify_persistence_error=rs("GeneratorExit", "ge"))),
]

# ── 5. self_post_api_server ──
SP_IO = {"self._served_api_server_wake_profile": None, "logger.info": None, "logger.warning": None,
         "_mark_internal_notification": "MARKED", "diagnostic_process_event": False,
         "deliver_wake": None, "persist_delegation_delivery": None, "scope_enter": None, "scope_exit": None,
         "self._resolve_profile_home_for_source": "/h/p2",
         "_async_profile_runtime_scope": {"scope": "profile", "home": "/h/p2"}}
CASES += [
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"}, io=dict(SP_IO)),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"}, io=dict(SP_IO, **{"self._served_api_server_wake_profile": "p2"})),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "async_delegation"}, io=dict(SP_IO)),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"},
         io=dict(SP_IO, **{"self._served_api_server_wake_profile": rs("LookupError", "not owned")})),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"},
         io=dict(SP_IO, **{"self._served_api_server_wake_profile": rs("KeyError", "raw1")})),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"},
         io=dict(SP_IO, **{"self._served_api_server_wake_profile": rs("RuntimeError", "boom")})),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"}, io=dict(SP_IO, deliver_wake=rs("RuntimeError", "wake"))),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"}, io=dict(SP_IO, diagnostic_process_event=True)),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"},
         io=dict(SP_IO, **{"self._served_api_server_wake_profile": "p2",
                           "scope_exit": rs("RuntimeError", "exit")})),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"}, io=dict(SP_IO, deliver_wake=rs("CancelledError", "c"))),
]

# ── 6. committed_relay_adapter ──
CR_IO = {"active_authority": {"auth": 1}, "session_store._generate_session_key": "agent:k",
         "session_store.lookup_by_session_key": {"session_id": "s1"},
         "committed_route_envelope": {"provenance": {"relay": True}, "text": "x", "media": "m"},
         "restore_native": {"source": {"chat_id": "c1", "platform": "relay"}},
         "self._adapter_for_source": {"name": "relay"}}
CASES += [
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"}, io=dict(CR_IO)),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, active_authority=None)),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, **{"session_store.lookup_by_session_key": None})),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, committed_route_envelope=None)),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, committed_route_envelope={"provenance": {"native": True}})),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, committed_route_envelope={"text": "x"})),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, restore_native=rs("AuthorityRefusal", "stale_generation"))),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, restore_native=rs("ValueError", "bad"))),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=dict(CR_IO, committed_route_envelope={"provenance": ["relay"]})),
    case("committed_relay_adapter", self={}, source={"chat_id": "c1"}, io=dict(CR_IO)),
]

# ── 7. completion_event_scope ──
CE_IO = {"self._build_process_event_source": None, "_raw_process_event_session_id": "raw1",
         "async_launch_profile_scope_if_multiplexed": {"scope": "launch"},
         "self._resolve_profile_home_for_source": "/h/p2", "get_hermes_home_override": "/h/other",
         "_async_profile_runtime_scope": {"scope": "profile", "home": "/h/p2"}}
CASES += [
    case("completion_event_scope", self={"config": {"multiplex_profiles": True}}, evt={}, io=dict(CE_IO)),
    case("completion_event_scope", self={"config": {"multiplex_profiles": True}}, evt={},
         io=dict(CE_IO, _raw_process_event_session_id="")),
    case("completion_event_scope", self={"config": {"multiplex_profiles": False}}, evt={}, io=dict(CE_IO)),
    case("completion_event_scope", self={"config": {"multiplex_profiles": True}}, evt={},
         io=dict(CE_IO, **{"self._build_process_event_source": {"profile": "p2"}})),
    case("completion_event_scope", self={"config": {"multiplex_profiles": True}}, evt={},
         io=dict(CE_IO, **{"self._build_process_event_source": {"profile": "p2"},
                           "get_hermes_home_override": "/h/p2"})),
    case("completion_event_scope", self={"config": {"multiplex_profiles": True}}, evt={},
         io=dict(CE_IO, **{"self._build_process_event_source": {"profile": ""}})),
    case("completion_event_scope", self={"config": {"multiplex_profiles": True}}, evt={},
         io=dict(CE_IO, **{"self._build_process_event_source": rs("RuntimeError", "src")})),
]

# ── 8. served_api_server_event_scope ──
SA_IO = {"self._served_api_server_wake_profile": None,
         "async_launch_profile_scope_if_multiplexed": {"scope": "launch"},
         "scope_enter": None, "scope_exit": None, "self._resolve_profile_home_for_source": "/h/p2",
         "_async_profile_runtime_scope": {"scope": "profile", "home": "/h/p2"}}
CASES += [
    case("served_api_server_event_scope", self={}, evt={}, raw_sid="raw1", io=dict(SA_IO)),
    case("served_api_server_event_scope", self={}, evt={}, raw_sid="raw1",
         io=dict(SA_IO, **{"self._served_api_server_wake_profile": "p2"})),
    case("served_api_server_event_scope", self={}, evt={}, raw_sid="raw1",
         io=dict(SA_IO, **{"async_launch_profile_scope_if_multiplexed": {"scope": "nullcontext"}})),
    case("served_api_server_event_scope", self={}, evt={}, raw_sid="raw1",
         io=dict(SA_IO, **{"self._served_api_server_wake_profile": rs("LookupError", "x")})),
    case("served_api_server_event_scope", self={}, evt={}, raw_sid="raw1",
         io=dict(SA_IO, scope_exit=rs("RuntimeError", "exit"))),
    case("served_api_server_event_scope", self={}, evt={}, raw_sid="raw1",
         io=dict(SA_IO, scope_enter=rs("RuntimeError", "enter"))),
]

# ── 9. deliver_completion_notification_scoped ──
DC_EVT = {"type": "completion", "session_key": "agent:k", "session_id": "s1"}
DC_SELF = {"_completion_delivery_lock": True, "_completion_deliveries_inflight": [["completion", "s1", 100]]}
DC_IO = {"should_surface_notification": True, "restore_completion_route": None,
         "follow_detached_pin": dict(DC_EVT), "self._completion_delivery_identity": ["completion", "s1", 100],
         "self._preflight_completion_delivery": claim(), "self._completion_identity_seen": False,
         "self._inject_watch_notification": True, "self._mark_completions_delivered_locked": None,
         "self._settle_durable_claim": None, "self._record_coalesced_completion_siblings": None,
         "completion_admission": None, "logger.warning": None}
CASES += [
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[], io=dict(DC_IO)),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[], io=dict(DC_IO, should_surface_notification=False)),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[],
         io=dict(DC_IO, **{"self._preflight_completion_delivery": claim(proceed=False, early_result=False)})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[], io=dict(DC_IO, **{"self._completion_identity_seen": True})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[], io=dict(DC_IO, **{"self._inject_watch_notification": False})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[],
         io=dict(DC_IO, **{"self._inject_watch_notification": rs("WakeNotAccepted", "refused", permanent=False)})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[],
         io=dict(DC_IO, **{"self._inject_watch_notification": rs("WakeNotAccepted", "refused", permanent=True)})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[[{"delegation_id": "d2"}, "c2"]],
         io=dict(DC_IO, **{"self._preflight_completion_delivery": claim(delegation_id="d1", claim_id="c1")})),
    case("deliver_completion_notification_scoped",
         self=dict(DC_SELF, session_authority={"a": 1}), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[], io=dict(DC_IO, completion_admission={"committed": True})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[],
         io=dict(DC_IO, **{"self._preflight_completion_delivery": rs("RuntimeError", "pf")})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[],
         io=dict(DC_IO, **{"self._inject_watch_notification": rs("SystemExit", "bye")})),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[],
         io=dict(DC_IO, **{"self._preflight_completion_delivery":
                           claim(owner_session_id="owner1", proven_session_id="prov1")})),
]

# ── 10. settle_batch_waiters ──
SB_F = {"id": "f1", "done": False, "result": None}
CASES += [
    case("settle_batch_waiters", entries=[["t1", {"session_id": "s1"}, dict(SB_F)]], result=False,
         io={"future.done": False, "future.set_result": None}),
    case("settle_batch_waiters", entries=[["t1", {"session_id": "s1"}, dict(SB_F)]], result=False,
         io={"future.done": True}),
    case("settle_batch_waiters",
         entries=[["t1", {}, dict(SB_F)], ["t2", {}, {"id": "f2", "done": False, "result": None}]],
         result=True, io={"future.done": seq(False, True), "future.set_result": None}),
    case("settle_batch_waiters", entries=[["t1", {}, dict(SB_F)]], result=None,
         io={"future.done": False, "future.set_result": None}),
    case("settle_batch_waiters", entries=[["t1", {}, dict(SB_F)]], result=False,
         io={"future.done": False, "future.set_result": rs("RuntimeError", "invalid")}),
    case("settle_batch_waiters", entries=[["t1", {"id": "f1"}]], result=False, io={}),
    case("settle_batch_waiters", entries=[], result=False, io={}),
]

# ── 11. deliver_async_delegation_group ──
DG_GROUP = [{"type": "async_delegation", "session_key": "agent:k"}]
DG_IO = {"completion_event_scope_or_refuse": {"scope": "profile", "home": "/h/p2"},
         "scope_enter": None, "scope_exit": None, "self._deliver_async_delegation_group_scoped": True}
CASES += [
    case("deliver_async_delegation_group", self={}, group=list(DG_GROUP), io=dict(DG_IO)),
    case("deliver_async_delegation_group", self={}, group=list(DG_GROUP),
         io=dict(DG_IO, completion_event_scope_or_refuse=None)),
    case("deliver_async_delegation_group", self={}, group=[], io={}),
    case("deliver_async_delegation_group", self={}, group=list(DG_GROUP),
         io=dict(DG_IO, **{"self._deliver_async_delegation_group_scoped": rs("RuntimeError", "scoped")})),
    case("deliver_async_delegation_group", self={}, group=list(DG_GROUP),
         io=dict(DG_IO, scope_exit=rs("RuntimeError", "exit"))),
    case("deliver_async_delegation_group", self={}, group=list(DG_GROUP),
         io=dict(DG_IO, completion_event_scope_or_refuse={"scope": "nullcontext"})),
    case("deliver_async_delegation_group", self={}, group=list(DG_GROUP),
         io=dict(DG_IO, **{"self._deliver_async_delegation_group_scoped": rs("KeyboardInterrupt", "ki")})),
]

# ── 12. deliver_async_delegation_groups ──
DS_PR = {"completion_queue": {"id": "completion_queue"}}
DS_IO = {"time.monotonic": 100.0, "completion_event_scope_or_refuse": {"scope": "nullcontext"},
         "self._deliver_async_delegation_group_scoped": True, "queue.put": None,
         "self._back_off_refused_completions": None, "logger.error": None}
CASES += [
    case("deliver_async_delegation_groups", self={}, groups={"k1": list(DG_GROUP)}, pr=dict(DS_PR), io=dict(DS_IO)),
    case("deliver_async_delegation_groups", self={}, groups={"k1": list(DG_GROUP)}, pr=dict(DS_PR),
         io=dict(DS_IO, **{"self._deliver_async_delegation_group_scoped": False})),
    case("deliver_async_delegation_groups", self={},
         groups={"k1": [{"type": "async_delegation", "_watch_next_retry_at": 200}]}, pr=dict(DS_PR), io=dict(DS_IO)),
    case("deliver_async_delegation_groups", self={}, groups={"k1": list(DG_GROUP)}, pr=dict(DS_PR),
         io=dict(DS_IO, **{"self._deliver_async_delegation_group_scoped": rs("RuntimeError", "inj")})),
    case("deliver_async_delegation_groups", self={}, groups={"k1": list(DG_GROUP)}, pr=dict(DS_PR),
         io=dict(DS_IO, **{"self._deliver_async_delegation_group_scoped": rs("SystemExit", "bye")})),
    case("deliver_async_delegation_groups", self={},
         groups={"k1": [{"type": "async_delegation", "_watch_next_retry_at": 200}], "k2": list(DG_GROUP)},
         pr=dict(DS_PR), io=dict(DS_IO)),
    case("deliver_async_delegation_groups", self={},
         groups={"k1": [{"type": "async_delegation", "_watch_next_retry_at": None}]}, pr=dict(DS_PR), io=dict(DS_IO)),
]

# ── 13. arm_process_watcher ──
AW_SELF = {"_running": True, "_gateway_loop": {"id": "loop1", "running": True}}
AW_IO = {"loop.is_running": True,
         "self._run_process_watcher": {"__coro__": "_run_process_watcher", "watcher": {"session_id": "s1"}},
         "safe_schedule_threadsafe": {"future": 1}}
CASES += [
    case("arm_process_watcher", self=dict(AW_SELF), watcher={"session_id": "s1"}, io=dict(AW_IO)),
    case("arm_process_watcher", self=dict(AW_SELF, _running=False), watcher={"session_id": "s1"}, io={}),
    case("arm_process_watcher", self=dict(AW_SELF, _gateway_loop=None), watcher={"session_id": "s1"}, io={}),
    case("arm_process_watcher", self=dict(AW_SELF), watcher={"session_id": "s1"},
         io=dict(AW_IO, **{"loop.is_running": False})),
    case("arm_process_watcher", self=dict(AW_SELF), watcher={"session_id": "s1"},
         io=dict(AW_IO, safe_schedule_threadsafe=None)),
    case("arm_process_watcher", self=dict(AW_SELF), watcher={"session_id": "s1"},
         io=dict(AW_IO, **{"self._run_process_watcher": rs("RuntimeError", "coro")})),
    case("arm_process_watcher", self={"_running": True}, watcher={"session_id": "s1"}, io={}),
]


# ── fail-closed: one case per flow reaches a seam with NO script ──
def fc(base, drop):
    io = dict(base)
    io.pop(drop, None)
    return io


CASES += [
    case("send_queued_final_text", self={}, adapter={"__base__": True, "send_final_ledgered": True},
         source=dict(SQ_SRC), text_content="answer", metadata=None, event_message_id=None,
         session_key="***", inbound_message_id=None,
         io=fc(dict(SQ_BASE, **{"adapter.send_final_ledgered": [{"success": True}, None]}), "_mark_notify_metadata")),
    case("send_home_channel_message", self={}, platform="telegram", home=dict(HC_HOME),
         transport=dict(HC_TR), message="hi", failure_fmt="fail %s:%s: %s",
         io=fc(HC_IO, "self._thread_metadata_for_target")),
    case("replay_pending_planned_restart_notification", self={}, io=fc(RP_IO, "path.exists")),
    case("send_session_db_warning_notifications", self={"_session_db_init_error": "locked"},
         io=fc(DB_IO, "classify_persistence_error")),
    case("self_post_api_server", self={}, adapter={"name": "api"}, synth_text="done", raw_sid="raw1",
         evt={"type": "watch_match"}, io=fc(SP_IO, "deliver_wake")),
    case("committed_relay_adapter", self={"session_store": {}}, source={"chat_id": "c1"},
         io=fc(CR_IO, "active_authority")),
    case("completion_event_scope", self={"config": {"multiplex_profiles": True}}, evt={},
         io=fc(CE_IO, "self._build_process_event_source")),
    case("served_api_server_event_scope", self={}, evt={}, raw_sid="raw1",
         io=fc(SA_IO, "self._served_api_server_wake_profile")),
    case("deliver_completion_notification_scoped", self=dict(DC_SELF), synth_text="done",
         evt=dict(DC_EVT), sibling_claims=[], io=fc(DC_IO, "should_surface_notification")),
    case("settle_batch_waiters", entries=[["t1", {}, dict(SB_F)]], result=False,
         io=fc({"future.done": False, "future.set_result": None}, "future.done")),
    case("deliver_async_delegation_group", self={}, group=list(DG_GROUP),
         io=fc(DG_IO, "completion_event_scope_or_refuse")),
    case("deliver_async_delegation_groups", self={}, groups={"k1": list(DG_GROUP)}, pr=dict(DS_PR),
         io=fc(DS_IO, "time.monotonic")),
    case("arm_process_watcher", self=dict(AW_SELF), watcher={"session_id": "s1"},
         io=fc(AW_IO, "loop.is_running")),
]


def main():
    out = os.path.join(HERE, "fixtures", "run_notifications_flow3.jsonl")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases -> %s" % (len(CASES), out))
    for k, v in sorted(counts.items()):
        print("  %-46s %d" % (k, v))
    assert all(v >= 5 for v in counts.values()), counts
    assert len(counts) == 13, counts


if __name__ == "__main__":
    main()
