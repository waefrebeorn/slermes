#!/usr/bin/env python3
"""Generate tests/fixtures/run_notifications_flow2.jsonl for the
run_notifications_flow2 oracle (t_port_run_notifications_flow2.c harness).

Case shape: {"func": <live name minus leading underscore>, "in": {...}}.
"in"."io" carries the universal-seam scripts: a plain value (every call) or
{"__seq__": [...]} (one element per call, last repeats). {"__raise__": kind,
"msg": text} scripts a raised exception. Deterministic output; >=5 cases/func.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def const(v):
    return v


def seq(*vs):
    return {"__seq__": list(vs)}


def rs(kind, msg):
    return {"__raise__": kind, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []

# ── 1. redacted_output_tail ──
RT_IO = {"strip_ansi": "clean", "transform_process_output": "transformed",
         "redact_terminal_output": "redacted", "_redact_gateway_user_facing_secrets": "final"}
CASES += [
    case("redacted_output_tail", self={}, session={"output_buffer": "hello", "command": "ls",
         "exit_code": 0, "task_id": "t1"}, limit=1000, io=dict(RT_IO)),
    case("redacted_output_tail", self={}, session={"output_buffer": "", "command": "ls",
         "exit_code": 0}, limit=1000, io={}),
    case("redacted_output_tail", self={}, session={"output_buffer": "abcdef", "command": "ls",
         "exit_code": 1, "task_id": "t"}, limit=3, io=dict(RT_IO)),
    case("redacted_output_tail", self={}, session={"output_buffer": "abcdef", "exit_code": 0},
         limit=0, io=dict(RT_IO)),
    case("redacted_output_tail", self={}, session={"output_buffer": "x", "exit_code": 0},
         limit=1000, io={"strip_ansi": ""}),
    case("redacted_output_tail", self={}, session={"output_buffer": "x", "command": "c",
         "exit_code": 0}, limit=1000,
         io={"strip_ansi": "clean", "transform_process_output": rs("RuntimeError", "boom")}),
    case("redacted_output_tail", self={}, session={"output_buffer": "x", "command": "c",
         "exit_code": 0}, limit=1000,
         io={"strip_ansi": "clean", "transform_process_output": "t",
             "redact_terminal_output": rs("ValueError", "bad")}),
    case("redacted_output_tail", self={}, session={"output_buffer": "x", "exit_code": None},
         limit=1000, io=dict(RT_IO)),
]

# ── 2. build_process_completion_event ──
BE_IO = {"strip_ansi": "clean", "transform_process_output": "transformed",
         "redact_terminal_output": "redacted", "_redact_gateway_user_facing_secrets": "R"}
LONG = ("line one\nline two\n" * 200)   # > 2000 chars, has newlines
LONG_NONL = "x" * 2500
WATCH = {"session_key": "agent:k", "platform": "telegram", "chat_type": "dm",
         "chat_id": "c1", "thread_id": "t1", "user_id": "u1", "user_name": "n",
         "message_id": " m1 ", "parent_session_id": "p1"}
SESS = {"output_buffer": "out", "command": "ls -la", "exit_code": 0, "task_id": "t1",
        "owner_task_id": "ot1", "started_at": 100, "completion_reason": "exited",
        "termination_source": "natural", "parent_session_id": "sp"}
CASES += [
    case("build_process_completion_event", watcher=dict(WATCH), session=dict(SESS),
         session_id="s1", io=dict(BE_IO)),
    case("build_process_completion_event", watcher={}, session={"output_buffer": "",
         "command": "", "exit_code": 0}, session_id="s2", io=dict(BE_IO)),
    case("build_process_completion_event", watcher=dict(WATCH),
         session=dict(SESS, output_buffer=LONG), session_id="s3",
         io=dict(BE_IO, redact_terminal_output=LONG)),
    case("build_process_completion_event", watcher=dict(WATCH),
         session=dict(SESS, output_buffer=LONG_NONL), session_id="s4",
         io=dict(BE_IO, redact_terminal_output=LONG_NONL)),
    case("build_process_completion_event", watcher=dict(WATCH),
         session=dict(SESS, completion_reason=None, termination_source=None, started_at=None),
         session_id="s5", io=dict(BE_IO)),
    case("build_process_completion_event", watcher={"message_id": "", "parent_session_id": ""},
         session={"output_buffer": "o", "exit_code": 2, "parent_session_id": "fallback"},
         session_id="s6", io=dict(BE_IO)),
    case("build_process_completion_event", watcher=dict(WATCH), session=dict(SESS),
         session_id="s7", io=dict(BE_IO, strip_ansi=rs("RuntimeError", "ansi"))),
    case("build_process_completion_event", watcher=dict(WATCH), session=dict(SESS),
         session_id="s8", io={"strip_ansi": "clean", "transform_process_output": "t",
         "redact_terminal_output": "r", "_redact_gateway_user_facing_secrets": seq(rs("RuntimeError", "cmd"))}),
]

# ── 3. format_process_final_message ──
FF_IO = {"strip_ansi": "clean", "transform_process_output": "out",
         "redact_terminal_output": "out", "_redact_gateway_user_facing_secrets": "cmd",
         "time.time": 200.0, "_format_concise_process_notification": "HEADER",
         "t": "TEXT"}
CASES += [
    case("format_process_final_message", self={}, session_id="s1",
         session=dict(SESS, started_at=100), notify_mode="concise", io=dict(FF_IO)),
    case("format_process_final_message", self={}, session_id="s1",
         session=dict(SESS, started_at=100), notify_mode="all", io=dict(FF_IO)),
    case("format_process_final_message", self={}, session_id="s1",
         session=dict(SESS, started_at=None), notify_mode="all", io=dict(FF_IO)),
    case("format_process_final_message", self={}, session_id="s1",
         session=dict(SESS, started_at=100), notify_mode="result",
         io=dict(FF_IO, strip_ansi="", transform_process_output="out")),
    case("format_process_final_message", self={}, session_id="s1",
         session=dict(SESS, started_at=True), notify_mode="concise", io=dict(FF_IO)),
    case("format_process_final_message", self={}, session_id="s1",
         session=dict(SESS, started_at=100), notify_mode="all",
         io=dict(FF_IO, **{"time.time": rs("RuntimeError", "clock")})),
    case("format_process_final_message", self={}, session_id="s1",
         session=dict(SESS, started_at="notnum"), notify_mode="concise", io=dict(FF_IO)),
]

# ── 4. format_process_running_message ──
FR_IO = {"strip_ansi": "clean", "transform_process_output": "out",
         "redact_terminal_output": "out", "_redact_gateway_user_facing_secrets": "cmd",
         "_shorten_command_for_display": "ls", "t": "HEAD"}
CASES += [
    case("format_process_running_message", self={}, session=dict(SESS), io=dict(FR_IO)),
    case("format_process_running_message", self={}, session=dict(SESS),
         io=dict(FR_IO, _shorten_command_for_display="")),
    case("format_process_running_message", self={}, session=dict(SESS, output_buffer=""),
         io=dict(FR_IO, strip_ansi="")),
    case("format_process_running_message", self={}, session=dict(SESS),
         io=dict(FR_IO, t=seq("HEAD", "RECENT"))),
    case("format_process_running_message", self={}, session=dict(SESS),
         io=dict(FR_IO, _shorten_command_for_display=rs("RuntimeError", "short"))),
    case("format_process_running_message", self={}, session=dict(SESS, command=""),
         io=dict(FR_IO)),
]

# ── 5. format_coalesced_process_completions ──
FC_IO = {"_redact_gateway_user_facing_secrets": "R"}
EV1 = {"session_id": "s1", "exit_code": 0, "completion_reason": "exited", "output": "o1"}
EV2 = {"session_id": "s2", "exit_code": 1, "completion_reason": "killed", "output": "o2"}
CASES += [
    case("format_coalesced_process_completions", entries=[["t1", EV1, None]], io=dict(FC_IO)),
    case("format_coalesced_process_completions", entries=[["t1", EV1, None], ["t2", EV2, None]],
         io=dict(FC_IO)),
    case("format_coalesced_process_completions", entries=[], io=dict(FC_IO)),
    case("format_coalesced_process_completions",
         entries=[["t", {"session_id": "", "exit_code": None, "output": ""}, None]], io=dict(FC_IO)),
    case("format_coalesced_process_completions",
         entries=[["t%d" % i, dict(EV1, session_id="s%d" % i), None] for i in range(12)],
         io=dict(FC_IO)),
    case("format_coalesced_process_completions",
         entries=[["t", dict(EV1, output="y" * 900), None]],
         io={"_redact_gateway_user_facing_secrets": "y" * 900}),
    case("format_coalesced_process_completions", entries=[["t1", EV1, None]],
         io={"_redact_gateway_user_facing_secrets": rs("RuntimeError", "redact")}),
]

# ── 6. launching_turn_active ──
LA_SRC = {"platform": "telegram", "profile": "p2"}
LA_IO = {"self._build_process_event_source": LA_SRC, "detached_route_of": False,
         "self._transport_owner": None, "identity_of": None,
         "self._adapters_for_profile": {"telegram": {"name": "a1", "_active_sessions": {"agent:k": True}}},
         "resolve_delivery_transport": {"adapter": {"name": "a1", "_active_sessions": {"agent:k": True}}},
         "self._routed_primary_adapter": None, "self._committed_relay_adapter": None}
CASES += [
    case("launching_turn_active", self={"config": {}, "adapters": {}}, platform_name="telegram",
         watcher={"session_key": "agent:k"}, io=dict(LA_IO)),
    case("launching_turn_active", self={"config": {}, "adapters": {}}, platform_name="telegram",
         watcher={"session_key": "  "}, io={}),
    case("launching_turn_active", self={"config": {}, "adapters": {}}, platform_name="telegram",
         watcher={"session_key": "agent:other"}, io=dict(LA_IO)),
    case("launching_turn_active", self={"config": {}, "adapters": {}}, platform_name="telegram",
         watcher={"session_key": "agent:k"},
         io=dict(LA_IO, resolve_delivery_transport=None,
                 **{"self._adapters_for_profile": {"telegram": {"name": "a1"}}})),
    case("launching_turn_active", self={"config": {}, "adapters": {}}, platform_name="telegram",
         watcher={"session_key": "agent:k"},
         io=dict(LA_IO, **{"self._build_process_event_source": rs("RuntimeError", "src")})),
]

# ── 7. send_watcher_message ──
SW_IO = {"self._build_process_event_source": LA_SRC, "detached_route_of": False,
         "self._transport_owner": None, "identity_of": None,
         "self._adapters_for_profile": {"telegram": {"name": "a1", "send": True}},
         "resolve_delivery_transport": {"adapter": {"name": "a1", "send": True}},
         "_non_conversational_metadata": {"m": 1}, "adapter.send": {"success": True},
         "logger.log": None, "self._routed_primary_adapter": None, "self._committed_relay_adapter": None}
CASES += [
    case("send_watcher_message", self={"config": {}, "adapters": {}}, platform_name="telegram",
         chat_id="c1", thread_id="t1", message_text="hi", watcher={"session_key": "k"}, io=dict(SW_IO)),
    case("send_watcher_message", self={"config": {}, "adapters": {}}, platform_name="telegram",
         chat_id="c1", thread_id=None, message_text="hi", watcher={}, io=dict(SW_IO)),
    case("send_watcher_message", self={"config": {}, "adapters": {}}, platform_name="telegram",
         chat_id="", thread_id="t1", message_text="hi", watcher={}, io=dict(SW_IO)),
    case("send_watcher_message", self={"config": {}, "adapters": {}}, platform_name="telegram",
         chat_id="c1", thread_id="t1", message_text="hi", watcher={},
         io=dict(SW_IO, **{"adapter.send": rs("RuntimeError", "send")}),
         ),
    case("send_watcher_message", self={"config": {}, "adapters": {}}, platform_name="telegram",
         chat_id="c1", thread_id="t1", message_text="hi", watcher={},
         io=dict(SW_IO, **{"adapter.send": rs("SystemExit", "bye")})),
    case("send_watcher_message", self={"config": {}, "adapters": {}}, platform_name="telegram",
         chat_id="c1", thread_id="t1", message_text="hi", watcher={},
         io=dict(SW_IO, resolve_delivery_transport=None,
                 **{"self._adapters_for_profile": {}})),
]
# the send-raise case needs a logger.log script for _log_suppressed
CASES[-3]["in"]["io"]["logger.log"] = None

# ── 8. each_secondary_ledger ──
ES_IO = {"profile_scope_enter": None, "profile_scope_exit": None, "ledger.fn": 3,
         "logger.info": None, "logger.warning": None}
HOMES = [["default", "/h/default"], ["p2", "/h/p2"], ["p3", "/h/p3"]]
CASES += [
    case("each_secondary_ledger", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, verb="Restored", io=dict(ES_IO)),
    case("each_secondary_ledger", self={}, profile_homes=HOMES, verb="V", io=dict(ES_IO)),
    case("each_secondary_ledger", self={"_primary_profile_name": "default"},
         profile_homes=[], verb="V", io=dict(ES_IO)),
    case("each_secondary_ledger", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, verb="V", io=dict(ES_IO, **{"ledger.fn": 0})),
    case("each_secondary_ledger", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, verb="V", io=dict(ES_IO, **{"ledger.fn": rs("RuntimeError", "fn")})),
    case("each_secondary_ledger", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, verb="V", io=dict(ES_IO, **{"profile_scope_enter": rs("RuntimeError", "enter")})),
    case("each_secondary_ledger", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, verb="V", io=dict(ES_IO, **{"profile_scope_exit": rs("RuntimeError", "exit")})),
    case("each_secondary_ledger", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, verb="V", io=dict(ES_IO, **{"ledger.fn": rs("SystemExit", "bye")})),
]

# ── 9. restore_secondary_completion_ledgers ──
RS_IO = {"profile_scope_enter": None, "profile_scope_exit": None,
         "restore_undelivered_completions": 2, "logger.info": None, "logger.warning": None}
CASES += [
    case("restore_secondary_completion_ledgers", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, io=dict(RS_IO)),
    case("restore_secondary_completion_ledgers", self={}, profile_homes=HOMES, io=dict(RS_IO)),
    case("restore_secondary_completion_ledgers", self={"_primary_profile_name": "default"},
         profile_homes=[], io=dict(RS_IO)),
    case("restore_secondary_completion_ledgers", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, io=dict(RS_IO, restore_undelivered_completions=0)),
    case("restore_secondary_completion_ledgers", self={"_primary_profile_name": "default"},
         profile_homes=HOMES, io=dict(RS_IO, restore_undelivered_completions=rs("RuntimeError", "db"))),
]

# ── 10. sweep_orphaned_completion_ledgers ──
SW2_IO = {"sweep_orphaned_completions": 2, "logger.info": None, "logger.log": None,
          "profile_scope_enter": None, "profile_scope_exit": None, "logger.warning": None}
CASES += [
    case("sweep_orphaned_completion_ledgers",
         self={"_primary_profile_name": "default", "_served_profile_homes": {"default": "/h/d", "p2": "/h/p2"}},
         io=dict(SW2_IO)),
    case("sweep_orphaned_completion_ledgers", self={}, io=dict(SW2_IO)),
    case("sweep_orphaned_completion_ledgers",
         self={"_served_profile_homes": {"p2": "/h/p2"}}, io=dict(SW2_IO, sweep_orphaned_completions=0)),
    case("sweep_orphaned_completion_ledgers", self={},
         io=dict(SW2_IO, sweep_orphaned_completions=rs("RuntimeError", "sweep"))),
    case("sweep_orphaned_completion_ledgers", self={},
         io=dict(SW2_IO, sweep_orphaned_completions=rs("SystemExit", "bye"))),
    case("sweep_orphaned_completion_ledgers",
         self={"_primary_profile_name": "default", "_served_profile_homes": {"p2": "/h/p2"}},
         io=dict(SW2_IO, sweep_orphaned_completions=seq(2, 5))),
]

# ── 11/12. classify_completion_target / classify_completion_owner ──
CL_IO = {"self._lookup_completion_owner": ["deliver", "parent1", None],
         "self._closed_local_target": None, "logger.debug": None}
SELF_CL = {"persistence": {"aio": "aio-handle"}}
CASES += [
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO)),
    case("classify_completion_owner", self={}, parent_session_id="p", session_key="", io={}),
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="agent:k",
         io=dict(CL_IO, **{"session_store.ensure_loaded": None}),
         ),
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._lookup_completion_owner": ["terminal", "parent1", None]})),
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._lookup_completion_owner": ["retry", "parent1", None]})),
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._closed_local_target": "tip1"})),
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._lookup_completion_owner": ["deliver", "parent1",
                                            {"ended_at": 5, "end_reason": "user_exit"}]})),
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._lookup_completion_owner": ["deliver", "parent1",
                                            {"ended_at": 5, "end_reason": "idle"}]})),
    case("classify_completion_owner", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._lookup_completion_owner": ["deliver", "parent1",
                                            {"ended_at": 5, "end_reason": "compression"}],
                                            "self._resolve_compression_lineage_target": ["deliver", "tip2"]})),
]
# session_key case needs session_store with _entries
for c in CASES:
    if c["func"] == "classify_completion_owner" and c["in"].get("session_key") == "agent:k":
        c["in"]["self"] = dict(SELF_CL, session_store={"_entries": {"agent:k": {"session_id": "sid1"}}})
CASES += [
    case("classify_completion_target", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO)),
    case("classify_completion_target", self={}, parent_session_id="p", session_key="", io={}),
    case("classify_completion_target", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._lookup_completion_owner": ["terminal", "p", None]})),
    case("classify_completion_target", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._lookup_completion_owner": ["retry", "p", None]})),
    case("classify_completion_target", self=dict(SELF_CL), parent_session_id="parent1",
         session_key="", io=dict(CL_IO, **{"self._closed_local_target": "tip"})),
]

# ── 13. completion_delivery_ready ──
RD_SRC = {"platform": "telegram", "profile": "p2"}
RD_AD = {"name": "a1", "send": True}
RD_IO = {"self._build_process_event_source": RD_SRC, "detached_route_of": False,
         "self._transport_owner": None, "identity_of": None,
         "self._adapters_for_profile": {"telegram": RD_AD},
         "resolve_delivery_transport": {"adapter": RD_AD},
         "adapter_supports_push": True, "_held_reason": "held", "logger.warning": None,
         "self._routed_primary_adapter": None, "self._committed_relay_adapter": None}
CASES += [
    case("completion_delivery_ready", self={"config": {}, "adapters": {}},
         evt={"type": "completion"}, io=dict(RD_IO)),
    case("completion_delivery_ready", self={"config": {}, "adapters": {}},
         evt={"type": "completion", "parent_session_id": "parent1"},
         io=dict(RD_IO, **{"self._lookup_completion_owner": ["deliver", "parent1", None],
                           "self._closed_local_target": None})),
    case("completion_delivery_ready", self={"config": {}, "adapters": {}},
         evt={"type": "completion", "parent_session_id": "parent1"},
         io=dict(RD_IO, **{"self._lookup_completion_owner": ["terminal", "parent1", None],
                           "self._closed_local_target": None})),
    case("completion_delivery_ready", self={"config": {}, "adapters": {}},
         evt={"type": "completion", "parent_session_id": "parent1"},
         io=dict(RD_IO, **{"self._lookup_completion_owner": ["retry", "parent1", None],
                           "self._closed_local_target": None})),
    case("completion_delivery_ready", self={"config": {}, "adapters": {"api_server": {"name": "api", "send": True}}},
         evt={"type": "completion", "platform": "api_server", "origin_session_id": "raw1"},
         io={"self._build_process_event_source": None, "_raw_process_event_session_id": "raw1",
             "adapter_supports_push": True}),
    case("completion_delivery_ready", self={"config": {}, "adapters": {}},
         evt={"type": "completion"},
         io=dict(RD_IO, resolve_delivery_transport=None, **{"self._adapters_for_profile": {}})),
    case("completion_delivery_ready", self={"config": {}, "adapters": {}},
         evt={"type": "completion"},
         io=dict(RD_IO, adapter_supports_push=False, **{"adapter.ensure_store": None})),
    case("completion_delivery_ready", self={"config": {}, "adapters": {}},
         evt={"type": "completion"},
         io=dict(RD_IO, adapter_supports_push=False,
                 **{"adapter.ensure_store": rs("RuntimeError", "db"), "logger.debug": None})),
]
# the no-transport case needs _held_reason + logger.warning (present in RD_IO)

# ── 14. preflight_completion_delivery ──
PF_AD = {"name": "a1", "send": True}
PF_READY_IO = {"self._build_process_event_source": RD_SRC, "detached_route_of": False,
               "self._transport_owner": None, "identity_of": None,
               "self._adapters_for_profile": {"telegram": PF_AD},
               "resolve_delivery_transport": {"adapter": PF_AD},
               "adapter_supports_push": True,
               "self._routed_primary_adapter": None, "self._committed_relay_adapter": None}
PF_IO = dict(PF_READY_IO, self_id=7, uuid4_hex="abc123", claim_completion_delivery=True,
             **{"self._lookup_completion_owner": ["deliver", "parent1", None],
                "self._closed_local_target": None, "self._settle_durable_claim": None,
                "logger.warning": None})
CASES += [
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "completion"}, io=dict(PF_IO)),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "async_delegation", "delegation_id": "d1"}, io=dict(PF_IO)),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "async_delegation", "delegation_id": "d1", "task_failure_notice": True},
         io=dict(PF_IO)),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "watch_match"}, io=dict(PF_IO)),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "async_delegation", "delegation_id": "d1"},
         io=dict(PF_IO, claim_completion_delivery=False)),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "async_delegation", "delegation_id": "d1"},
         io=dict(PF_IO, claim_completion_delivery=rs("RuntimeError", "claim"))),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "async_delegation", "delegation_id": "d1", "parent_session_id": "parent1"},
         io=dict(PF_IO, **{"self._lookup_completion_owner": ["terminal", "parent1", None]})),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "completion", "parent_session_id": "parent1", "session_id": "s1"},
         io=dict(PF_IO, **{"self._lookup_completion_owner": ["terminal", "parent1", None]})),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "completion", "parent_session_id": "parent1"},
         io=dict(PF_IO, **{"self._lookup_completion_owner": ["retry", "parent1", None]})),
    case("preflight_completion_delivery", self={"config": {}, "adapters": {}},
         evt={"type": "async_delegation", "delegation_id": "d1"},
         io=dict(PF_READY_IO, adapter_supports_push=False, **{"adapter.ensure_store": None})),
]

# ── 15. resolve_injection_adapter ──
RI_SRC = {"platform": "telegram", "profile": "p2"}
RI_IO = {"detached_route_of": False, "self._transport_owner": None, "identity_of": None,
         "self._adapters_for_profile": {"telegram": {"name": "a1"}},
         "resolve_delivery_transport": {"adapter": {"name": "a1"}},
         "self._routed_primary_adapter": None, "self._committed_relay_adapter": None}
CASES += [
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC, io=dict(RI_IO)),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC,
         io=dict(RI_IO, detached_route_of=True, **{"self._delivery_adapter_for": {"name": "det"}})),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC,
         io=dict(RI_IO, **{"self._transport_owner": [{"name": "owner"}]})),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {"relay": {"name": "relay"}}},
         platform_name="telegram", source=dict(RI_SRC, delivered_via_upstream_relay=True), io=dict(RI_IO)),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC,
         io=dict(RI_IO, identity_of={"multiplexed": True, "transport_inferred": False, "transport_profile": "bot1"},
                 **{"self._restored_transport_trusted": True,
                    "self._adapters_for_profile": {"telegram": {"name": "pinned"}}})),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC,
         io=dict(RI_IO, resolve_delivery_transport=None,
                 **{"self._routed_primary_adapter": {"name": "borrowed"}})),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC,
         io=dict(RI_IO, resolve_delivery_transport=None,
                 **{"self._committed_relay_adapter": {"name": "relay"}})),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC,
         io=dict(RI_IO, resolve_delivery_transport=None)),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="irc", source=RI_SRC, io=dict(RI_IO)),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=None, io=dict(RI_IO)),
    case("resolve_injection_adapter", self={"config": {}, "adapters": {}},
         platform_name="telegram", source=RI_SRC,
         io=dict(RI_IO, resolve_delivery_transport=rs("SystemExit", "bye"))),
]

# ── 16. served_api_server_wake_profile ──
WK_IO = {"self._build_process_event_source": None, "_multiplex_profile_homes": [["default", "/h/d"], ["p2", "/h/p2"]],
         "session_owned_by_profile": False}
CASES += [
    case("served_api_server_wake_profile", self={"config": {"multiplex_profiles": True}},
         evt={}, raw_sid="raw1", io=dict(WK_IO)),
    case("served_api_server_wake_profile", self={"config": {"multiplex_profiles": False}},
         evt={}, raw_sid="raw1", io={}),
    case("served_api_server_wake_profile", self={"config": {"multiplex_profiles": True}},
         evt={}, raw_sid="raw1", io=dict(WK_IO, session_owned_by_profile=True)),
    case("served_api_server_wake_profile",
         self={"config": {"multiplex_profiles": True}, "_primary_profile_name": "default"},
         evt={}, raw_sid="raw1",
         io=dict(WK_IO, **{"self._build_process_event_source": {"profile": "p2"},
                           "session_owned_by_profile": True})),
    case("served_api_server_wake_profile",
         self={"config": {"multiplex_profiles": True}, "_primary_profile_name": "default"},
         evt={}, raw_sid="raw1",
         io=dict(WK_IO, **{"self._build_process_event_source": {"profile": "p2"},
                           "session_owned_by_profile": False})),
    case("served_api_server_wake_profile",
         self={"config": {"multiplex_profiles": True}, "_primary_profile_name": "default"},
         evt={}, raw_sid="raw1",
         io=dict(WK_IO, **{"self._build_process_event_source": {"profile": "default"}})),
]

# ── 17. adapter_by_platform_value ──
CASES += [
    case("adapter_by_platform_value", self={"adapters": {"telegram": {"name": "a1"}, "slack": {"name": "a2"}}},
         platform_name="telegram"),
    case("adapter_by_platform_value", self={"adapters": {"telegram": {"name": "a1"}}},
         platform_name="slack"),
    case("adapter_by_platform_value", self={"adapters": {}}, platform_name="telegram"),
    case("adapter_by_platform_value", self={"adapters": {"telegram": {"name": "a1"}}},
         platform_name=""),
    case("adapter_by_platform_value", self={"adapters": {"relay": {"name": "r"}}},
         platform_name="relay"),
]

# ── 18. home_channel_transports ──
HC_CFG = {"platforms": {
    "telegram": {"home_channel": {"chat_id": "c1", "thread_id": "t1"}, "gateway_restart_notification": True},
    "slack": {"home_channel": {"chat_id": "", "thread_id": None}},
    "discord": {"home_channel": None}}}
HC_IO = {"_safe_delivery_transport": {"adapter": {"name": "a1"}}}
CASES += [
    case("home_channel_transports", self={"config": HC_CFG, "adapters": {}}, io=dict(HC_IO)),
    case("home_channel_transports", self={"config": {"platforms": {}}, "adapters": {}}, io={}),
    case("home_channel_transports", self={"config": HC_CFG, "adapters": {}},
         io={"_safe_delivery_transport": None}),
    case("home_channel_transports", self={"config": HC_CFG, "adapters": {}},
         io={"_safe_delivery_transport": rs("RuntimeError", "transport")}),
    case("home_channel_transports",
         self={"config": {"platforms": {"telegram": {"home_channel": {"chat_id": "c1"}}}}, "adapters": {}},
         io=dict(HC_IO)),
]

# ── 19. served_home_channel_configs ──
PC = {"p2": {"platforms": {"telegram": {"home_channel": {"chat_id": "c2"}}}}}
CASES += [
    case("served_home_channel_configs", self={"config": HC_CFG}),
    case("served_home_channel_configs", self={"config": HC_CFG, "_profile_configs": PC}),
    case("served_home_channel_configs", self={"config": {"platforms": {}}}),
    case("served_home_channel_configs", self={"config": {"platforms": {}}, "_profile_configs": {}}),
    case("served_home_channel_configs", self={"config": {"platforms": {}}, "_profile_configs": None}),
    case("served_home_channel_configs", self={"config": HC_CFG, "_profile_configs": PC,
                                              "_primary_profile_name": "default"}),
]

# ── 20. served_home_channel_transports ──
ST_IO = {"_safe_delivery_transport": {"adapter": {"name": "a1"}}}
CASES += [
    case("served_home_channel_transports", self={"config": HC_CFG, "adapters": {}}, io=dict(ST_IO)),
    case("served_home_channel_transports", self={"config": HC_CFG, "adapters": {},
         "_profile_configs": PC, "_profile_adapters": {"p2": {"telegram": {"name": "a2"}}}}, io=dict(ST_IO)),
    case("served_home_channel_transports", self={"config": {"platforms": {}}, "adapters": {}}, io={}),
    case("served_home_channel_transports", self={"config": HC_CFG, "adapters": {}},
         io={"_safe_delivery_transport": None}),
    case("served_home_channel_transports", self={"config": HC_CFG, "adapters": {},
         "_profile_configs": PC}, io=dict(ST_IO)),
    case("served_home_channel_transports", self={"config": HC_CFG, "adapters": {},
         "_profile_configs": PC, "_profile_adapters": {"p2": {"telegram": {"name": "a2"}}}},
         io={"_safe_delivery_transport": seq({"adapter": {"name": "a1"}}, {"adapter": {"name": "a2"}})}),
]


def main():
    out = os.path.join(HERE, "fixtures", "run_notifications_flow2.jsonl")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases -> %s" % (len(CASES), out))
    for k, v in sorted(counts.items()):
        print("  %-42s %d" % (k, v))
    assert all(v >= 5 for v in counts.values()), counts
    assert len(counts) == 20, counts


if __name__ == "__main__":
    main()