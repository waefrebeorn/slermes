#!/usr/bin/env python3
"""Generate tests/fixtures/run_notifications_flow.jsonl for the
run_notifications_flow oracle (t_port_run_notifications_flow.c harness).

Case shape: {"func": <live name minus leading underscore>, "in": {...}}.
"in"."io" carries the universal-seam scripts: {"const": v} (every call) or
{"queue": [..]} (one element per call). Deterministic output.
"""
import ast
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_notifications.py")

FOOTER = None
for node in ast.parse(open(SRC, encoding="utf-8").read()).body:
    if isinstance(node, ast.Assign) and any(
            getattr(t, "id", None) == "INTERNAL_NOTIFICATION_FOOTER" for t in node.targets):
        FOOTER = ast.literal_eval(node.value)
assert FOOTER


def const(v):
    return {"const": v}


def queue(*vs):
    return {"queue": list(vs)}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []

# ── 1. mark_internal_notification ──
CASES += [
    case("mark_internal_notification", text="Process finished."),
    case("mark_internal_notification", text="done  \n\t"),
    case("mark_internal_notification", text="x\n\n" + FOOTER),
    case("mark_internal_notification", text="x\n\n" + FOOTER + "  "),
    case("mark_internal_notification", text=""),
    case("mark_internal_notification", text="x\u00a0"),
]

# ── 2. raw_process_event_session_id ──
CASES += [
    case("raw_process_event_session_id",
         evt={"session_key": "agent:main:telegram:dm:1", "platform": "telegram"}),
    case("raw_process_event_session_id", evt={"platform": "telegram", "origin_session_id": "raw1"}),
    case("raw_process_event_session_id", evt={"platform": "", "chat_id": "c1", "origin_session_id": "raw1"}),
    case("raw_process_event_session_id", evt={"session_key": "  raw-sid  ", "platform": "api_server"}),
    case("raw_process_event_session_id", evt={"platform": "API_SERVER", "origin_session_id": " o1 "}),
    case("raw_process_event_session_id", evt={"session_key": 42, "platform": ""}),
    case("raw_process_event_session_id", evt={"session_key": "", "platform": "", "origin_session_id": 0}),
    case("raw_process_event_session_id", evt={"platform": "", "chat_type": "dm", "session_key": "s"}),
    case("raw_process_event_session_id", evt={"session_key": "s", "platform": "", "origin_session_id": "o"}),
]

# ── 3. event_route_key ──
CASES += [
    case("event_route_key",
         evt={"session_key": "agent:x", "platform": "telegram", "chat_id": "c",
              "thread_id": "t", "user_id": "u"},
         fields=["session_key", "platform", "chat_id"]),
    case("event_route_key", evt={"platform": "telegram"}, fields=["session_key", "thread_id"]),
    case("event_route_key", evt={"chat_id": 0, "user_id": False, "thread_id": None},
         fields=["chat_id", "user_id", "thread_id"]),
    case("event_route_key", evt={"platform": 7}, fields=["platform"]),
    case("event_route_key", evt={"platform": "telegram"}, fields=[]),
]

# ── 4. completion_delivery_identity ──
CASES += [
    case("completion_delivery_identity", evt={"type": "async_delegation", "delegation_id": "d1"}),
    case("completion_delivery_identity", evt={"type": "async_delegation"}),
    case("completion_delivery_identity",
         evt={"type": "async_delegation", "delegation_id": "d1",
              "task_failure_notice": True, "results": [{"task_index": 2}]}),
    case("completion_delivery_identity",
         evt={"type": "async_delegation", "delegation_id": "d1",
              "task_failure_notice": True, "results": []}),
    case("completion_delivery_identity",
         evt={"type": "async_delegation", "delegation_id": "d1",
              "task_failure_notice": True, "results": [0]}),
    case("completion_delivery_identity",
         evt={"type": "async_delegation", "delegation_id": "d1",
              "task_failure_notice": True, "results": ["x"]}),
    case("completion_delivery_identity",
         evt={"type": "watch_match", "session_id": "s1", "event_id": "e1"}),
    case("completion_delivery_identity", evt={"type": "heartbeat", "event_id": "e1"}),
    case("completion_delivery_identity",
         evt={"type": "completion", "session_id": "s1", "started_at": 123}),
    case("completion_delivery_identity", evt={"type": "completion", "session_id": "s1"}),
    case("completion_delivery_identity", evt={"type": "other"}),
]

# ── 5. served_notice_target_key ──
CASES += [
    case("served_notice_target_key", profile=None, platform_value="telegram",
         chat_id="c1", thread_id="t1",
         io={"_notice_target_key": const(["telegram", "c1", "t1"])}),
    case("served_notice_target_key", profile="p2", platform_value="telegram",
         chat_id="c1", thread_id=None,
         io={"_notice_target_key": const(["p2:telegram", "c1", None])}),
    case("served_notice_target_key", profile="", platform_value="slack",
         chat_id="c2", thread_id=None,
         io={"_notice_target_key": const([":slack", "c2", None])}),
    case("served_notice_target_key", profile=None, platform_value="telegram",
         chat_id=5, thread_id=None,
         io={"_notice_target_key": const(["telegram", "5", None])}),
    case("served_notice_target_key", profile="p2", platform_value="telegram",
         chat_id="c1", thread_id=None,
         io={"_notice_target_key": const({"__raise__": "RuntimeError", "message": "key boom"})}),
]

# ── 6. refusal_retry_at ──
CASES += [
    case("refusal_retry_at", step=1,
         io={"monotonic": const(100.0), "jittered_backoff": const(4.0)}),
    case("refusal_retry_at", step=0,
         io={"monotonic": const(100.0), "jittered_backoff": const(2.0)}),
    case("refusal_retry_at", step=2,
         io={"monotonic": const(100.5), "jittered_backoff": const(None)}),
    case("refusal_retry_at", step=1,
         io={"monotonic": const({"__raise__": "RuntimeError", "message": "clock down"}),
             "jittered_backoff": const(4.0)}),
    case("refusal_retry_at", step=1,
         io={"monotonic": const(100.0),
             "jittered_backoff": const({"__raise__": "ValueError", "message": "bad step"})}),
    case("refusal_retry_at", step=1,
         io={"monotonic": const(100.5), "jittered_backoff": const("4")}),
]

# ── 7. held_reason ──
CASES += [
    case("held_reason", source={"profile": "p2"}, platform="telegram", multiplexed=True,
         io={"identity_of": const({"multiplexed": True, "transport_inferred": False,
                                   "transport_profile": "bot1"})}),
    case("held_reason", source={"profile": "p2"}, platform="telegram", multiplexed=True,
         io={"identity_of": const({"multiplexed": True, "transport_inferred": True,
                                   "transport_profile": "bot1"})}),
    case("held_reason", source={}, platform="slack", multiplexed=True,
         io={"identity_of": const(None)}),
    case("held_reason", source={}, platform="slack", multiplexed=False,
         io={"identity_of": const(None)}),
    case("held_reason", source={"profile": ""}, platform="discord", multiplexed=False,
         io={"identity_of": const(None)}),
    case("held_reason", source={"profile": "p2"}, platform="telegram", multiplexed=True,
         io={"identity_of": const({"multiplexed": True, "transport_inferred": False,
                                   "transport_profile": "bo't"})}),
]

# ── 8. safe_delivery_transport ──
CASES += [
    case("safe_delivery_transport", platform={"value": "telegram"}, config={"c": 1},
         adapters={"a": 1}, profile=None,
         io={"resolve_delivery_transport": const({"adapter": "a1"})}),
    case("safe_delivery_transport", platform={"value": "telegram"}, config=None,
         adapters=None, profile=None,
         io={"resolve_delivery_transport": const({"__raise__": "RuntimeError", "message": "down"})}),
    case("safe_delivery_transport", platform={"value": "slack"}, config=None,
         adapters=None, profile="p2",
         io={"resolve_delivery_transport": const({"__raise__": "RuntimeError", "message": "down"})}),
    case("safe_delivery_transport", platform={"value": "slack"}, config=None,
         adapters=None, profile="",
         io={"resolve_delivery_transport": const({"__raise__": "ValueError", "message": "bad"})}),
    case("safe_delivery_transport", platform="slack", config=None, adapters=None,
         profile=None,
         io={"resolve_delivery_transport": const({"__raise__": "RuntimeError", "message": "down"})}),
    case("safe_delivery_transport", platform={"value": "telegram"}, config=None,
         adapters=None, profile=None,
         io={"resolve_delivery_transport": const({"__raise__": "SystemExit", "message": "bye"})}),
]

# ── 9. completion_identity_seen ──
ID_A = ["watch_match", "s1", "e1"]
ID_B = ["watch_match", "s2", "e2"]
CASES += [
    case("completion_identity_seen", state={"inflight": [], "delivered": [], "retention": 5},
         identity=ID_A, claim=False),
    case("completion_identity_seen", state={"inflight": [], "delivered": [], "retention": 5},
         identity=ID_A, claim=True),
    case("completion_identity_seen", state={"inflight": [ID_A], "delivered": [], "retention": 5},
         identity=ID_A, claim=True),
    case("completion_identity_seen", state={"inflight": [], "delivered": [ID_A], "retention": 5},
         identity=ID_A, claim=False),
    case("completion_identity_seen", state={"inflight": [ID_B], "delivered": [ID_A], "retention": 5},
         identity=ID_A, claim=True),
    case("completion_identity_seen", state={"inflight": [], "delivered": [], "retention": 5},
         identity=[], claim=True),
]

# ── 10. mark_completions_delivered_locked ──
ID_C = ["completion", "s3", "t3"]
CASES += [
    case("mark_completions_delivered_locked",
         state={"inflight": [ID_A, ID_B], "delivered": [], "retention": 5},
         identities=[ID_A]),
    case("mark_completions_delivered_locked",
         state={"inflight": [], "delivered": [ID_A, ID_B, ID_C], "retention": 2},
         identities=[]),
    case("mark_completions_delivered_locked",
         state={"inflight": [], "delivered": [ID_A, ID_B], "retention": 5},
         identities=[ID_A]),
    case("mark_completions_delivered_locked",
         state={"inflight": [ID_A, ID_B, ID_C], "delivered": [], "retention": 5},
         identities=[ID_A, ID_B]),
    case("mark_completions_delivered_locked",
         state={"inflight": [], "delivered": [], "retention": 0},
         identities=[ID_A]),
    case("mark_completions_delivered_locked",
         state={"inflight": [], "delivered": [], "retention": 5},
         identities=[]),
]

# ── 11. record_coalesced_completion_siblings ──
EV_A = {"type": "watch_match", "session_id": "s1", "event_id": "e1"}
EV_B = {"type": "watch_match", "session_id": "s2", "event_id": "e2"}
CASES += [
    case("record_coalesced_completion_siblings",
         state={"inflight": [], "delivered": [], "retention": 5},
         events=[EV_A, EV_B, {"type": "other"}]),
    case("record_coalesced_completion_siblings",
         state={"inflight": [ID_A], "delivered": [], "retention": 5}, events=[]),
    case("record_coalesced_completion_siblings",
         state={"inflight": [], "delivered": [], "retention": 5},
         events=[{"type": "heartbeat", "event_id": "e9"}]),
    case("record_coalesced_completion_siblings",
         state={"inflight": [], "delivered": [], "retention": 1},
         events=[EV_A, EV_B]),
    case("record_coalesced_completion_siblings",
         state={"inflight": [], "delivered": [ID_C], "retention": 5},
         events=[{"type": "other"}, {"type": "completion", "session_id": "s9"}]),
]

# ── 12. requeue_refused_watch_event ──
RQ_IO = {"_refusal_retry_at": const(123.5), "queue_put": const(None)}
CASES += [
    case("requeue_refused_watch_event", evt={"type": "watch_match"}, io=RQ_IO),
    case("requeue_refused_watch_event",
         evt={"type": "watch_match", "_refusal_permanent": True, "_watch_injection_attempts": 2},
         io=RQ_IO),
    case("requeue_refused_watch_event",
         evt={"type": "watch_match", "_refusal_permanent": True, "_watch_injection_attempts": 7,
              "platform": "telegram", "chat_id": "c1", "session_id": "s1"},
         io=RQ_IO),
    case("requeue_refused_watch_event", evt={"_watch_retry_step": "2"}, io=RQ_IO),
    case("requeue_refused_watch_event", evt={"_watch_retry_step": 2.9}, io=RQ_IO),
    case("requeue_refused_watch_event", evt={"_refusal_permanent": 0}, io=RQ_IO),
    case("requeue_refused_watch_event", evt={"type": "watch_match"},
         io={"_refusal_retry_at": const({"__raise__": "RuntimeError", "message": "no clock"}),
             "queue_put": const(None)}),
    case("requeue_refused_watch_event",
         evt={"_refusal_permanent": True, "_watch_injection_attempts": 7},
         io=RQ_IO),
]

# ── 13. back_off_refused_completions ──
CASES += [
    case("back_off_refused_completions",
         group=[{"_watch_retry_step": 1}, {"_watch_retry_step": 2}],
         io={"_refusal_retry_at": queue(10.5, 20.5)}),
    case("back_off_refused_completions", group=[{}],
         io={"_refusal_retry_at": const(10.5)}),
    case("back_off_refused_completions", group=[],
         io={"_refusal_retry_at": const(10.5)}),
    case("back_off_refused_completions", group=[{}, {}],
         io={"_refusal_retry_at": queue(10.5)}),
    case("back_off_refused_completions", group=[{"_watch_retry_step": "4"}],
         io={"_refusal_retry_at": const(10.5)}),
]

# ── 14. settle_durable_claim ──
CASES += [
    case("settle_durable_claim", kind="drop", delegation_id="d1", claim_id="c1",
         io={"drop_completion_delivery": const(None)}),
    case("settle_durable_claim", kind="complete", delegation_id="d1", claim_id="c1",
         io={"complete_completion_delivery": const({"__raise__": "RuntimeError", "message": "db"})}),
    case("settle_durable_claim", kind="release", delegation_id="d1", claim_id="c1",
         io={"release_completion_delivery": const({"__raise__": "RuntimeError", "message": "db"})}),
    case("settle_durable_claim", kind="defer", delegation_id="d1", claim_id="c1",
         io={"defer_completion_delivery": const(None)}),
    case("settle_durable_claim", kind="bogus", delegation_id="d1", claim_id="c1", io={}),
    case("settle_durable_claim", kind="complete", delegation_id="d1", claim_id="c1",
         io={"complete_completion_delivery": const(None)}),
    case("settle_durable_claim", kind="drop", delegation_id="d1", claim_id="c1",
         io={"drop_completion_delivery": const({"__raise__": "SystemExit", "message": "bye"})}),
]

# ── 15. watch_notification_metadata ──
MD_BASE = {"diagnostic_process_event": const(False)}
MD_LOCAL = {"diagnostic_process_event": const(False),
            "_local_completion_card": const({"local_card": True}),
            "delegation_wakes_parent": const(True),
            "completion_resumes_parent": const(True)}
CASES += [
    case("watch_notification_metadata", evt={}, source={"platform": "telegram"}, io=MD_BASE),
    case("watch_notification_metadata", evt={}, source={"platform": "telegram"},
         io={"diagnostic_process_event": const(True)}),
    case("watch_notification_metadata",
         evt={"session_key": " agent:k ", "parent_session_id": " p1 ",
              "proven_session_id": " pv ", "user_id": " u1 "},
         source={"platform": "telegram"}, io=MD_BASE),
    case("watch_notification_metadata", evt={}, source={"platform": "local"}, io=MD_LOCAL),
    case("watch_notification_metadata", evt={}, source={"platform": "local"},
         io={"diagnostic_process_event": const(True),
             "_local_completion_card": const({}),
             "delegation_wakes_parent": const(False),
             "completion_resumes_parent": const(True)}),
    case("watch_notification_metadata", evt={"_automation_identities": ["x"]},
         source={"platform": "telegram"}, io=MD_BASE),
    case("watch_notification_metadata", evt={"completion_ids": ["c1", "c2"]},
         source={"platform": "telegram"}, io=MD_BASE),
    case("watch_notification_metadata", evt={"completion_ids": "ab"},
         source={"platform": "telegram"}, io=MD_BASE),
    case("watch_notification_metadata", evt={"completion_ids": []},
         source={"platform": "local"},
         io={"diagnostic_process_event": const(False),
             "_local_completion_card": const({}),
             "delegation_wakes_parent": const(False),
             "completion_resumes_parent": const(False)}),
    case("watch_notification_metadata", evt={}, source={"platform": "local"},
         io={"diagnostic_process_event": const(False),
             "_local_completion_card": const({"__raise__": "RuntimeError", "message": "card"}),
             "delegation_wakes_parent": const(False),
             "completion_resumes_parent": const(False)}),
]

# ── 16. drain_watch_notifications ──
def drain_io(events, scope="s1", surface=True, mode="all", inject=True,
             heartbeat=None, synth="synth", monotonic=10.0, retry_at=123.5,
             inject_raise=None, scope_raise=None):
    io = {
        "drain_gateway_watch_events": const(events),
        "monotonic": const(monotonic),
        "queue_put": const(None),
        "completion_event_scope_or_refuse": const(scope_raise if scope_raise else scope),
        "scope_enter": const(None),
        "scope_exit": const(None),
        "should_surface_notification": const(surface),
        "load_background_notifications_mode": const(mode),
        "format_gateway_process_notification": const(synth),
        "inject_watch_notification": const(inject_raise if inject_raise else inject),
        "_refusal_retry_at": const(retry_at),
    }
    if heartbeat is not None:
        io["deliver_heartbeat"] = const(heartbeat)
    return io


WM = {"type": "watch_match", "session_id": "s1"}
CASES += [
    case("drain_watch_notifications",
         io=drain_io([{"type": "watch_match", "_watch_next_retry_at": 50}], monotonic=10.0)),
    case("drain_watch_notifications",
         io=drain_io([{"type": "watch_match", "_watch_next_retry_at": 5}], monotonic=10.0)),
    case("drain_watch_notifications", io=drain_io([dict(WM)], scope=None)),
    case("drain_watch_notifications", io=drain_io([dict(WM)], surface=False)),
    case("drain_watch_notifications", io=drain_io([dict(WM)], mode="off")),
    case("drain_watch_notifications",
         io=drain_io([{"type": "heartbeat"}], heartbeat=True)),
    case("drain_watch_notifications",
         io=drain_io([{"type": "heartbeat"}], heartbeat=False)),
    case("drain_watch_notifications", io=drain_io([dict(WM)], synth="")),
    case("drain_watch_notifications",
         io=drain_io([dict(WM)], inject_raise={"__raise__": "RuntimeError", "message": "inject"})),
    case("drain_watch_notifications", io=drain_io([dict(WM)], inject=False)),
    case("drain_watch_notifications", io=drain_io([dict(WM)], inject=None)),
    case("drain_watch_notifications",
         io=drain_io([dict(WM)], inject_raise={"__raise__": "SystemExit", "message": "bye"})),
    case("drain_watch_notifications",
         io=dict(drain_io([dict(WM), dict(WM, session_id="s2")]),
                 **{"completion_event_scope_or_refuse": queue(None, "s1")})),
    case("drain_watch_notifications",
         io=drain_io([{"type": "watch_match", "_watch_next_retry_at": 50}],
                     monotonic={"__raise__": "RuntimeError", "message": "clock"})),
]


def main():
    out = os.path.join(HERE, "fixtures", "run_notifications_flow.jsonl")
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


if __name__ == "__main__":
    main()