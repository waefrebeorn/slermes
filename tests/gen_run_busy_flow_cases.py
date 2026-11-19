#!/usr/bin/env python3
"""Generate tests/fixtures/run_busy_flow.jsonl for the run_busy_flow oracle.

Each case: {"func": <live name>, "in": {state, io, ...args}}. The state mirror
and io scripts follow the modeling contract documented in
src/gateway/run_busy_flow.c. Run: python tests/gen_run_busy_flow_cases.py
"""
import copy
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "run_busy_flow.jsonl")

S = {"platform": {"value": "telegram"}, "chat_id": "c1", "user_id": "u1",
     "chat_type": "dm", "thread_id": None}
MARKER = "[Continuing toward your standing goal]\nGoal: stay focused"


def state():
    return {
        "events": {
            "e1": {"id": "e1", "text": "hello", "internal": False,
                   "_admitted_before_drain": False, "user_id": "u1",
                   "user_name": "Alice", "source": copy.deepcopy(S)},
            "e2": {"id": "e2", "text": "second", "internal": False,
                   "_admitted_before_drain": False},
            "e3": {"id": "e3", "text": "wake", "internal": True,
                   "_admitted_before_drain": True},
            "e4": {"id": "e4", "text": "fourth", "internal": False,
                   "_admitted_before_drain": False},
        },
        "agents": {
            "a1": {"steer_callable": True, "interrupt_callable": True,
                   "_delegate_id": "d-a1",
                   "_active_children": {"kind": "list", "items": ["c1", "c2"]},
                   "_active_children_lock": False},
            "c1": {"steer_callable": True, "_delegate_id": "c1"},
            "c2": {"steer_callable": True, "_delegate_id": "c2"},
        },
        "sessions": {
            "k1": {"conversation": {"queued_events": ["e2"], "one_turn_restore": False},
                   "turn": {"busy_ack_ts": 0.0}},
        },
        "adapters": {"ad1": {"_pending_messages": {"k1": "e1"}}},
    }


def pin(prompt="prompt", src=None):
    return {"__script__": True, "ret": [prompt, copy.deepcopy(S if src is None else src)]}


def mevent(eid="e5", text="go"):
    return {"id": eid, "text": text, "source": copy.deepcopy(S),
            "channel_prompt": "prompt", "user_id": "u1", "user_name": "Alice"}


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(**kw):
    base = state()
    base.update(kw)
    return base


# ── _strip_slot ─────────────────────────────────────────────────────────────
add("_strip_slot", text="a:b:c", slot="a")
add("_strip_slot", text="a", slot="a")
add("_strip_slot", text="ab:x", slot="a")
add("_strip_slot", text="x", slot="y")
add("_strip_slot", text="é:tail", slot="é")
add("_strip_slot", text=":x", slot="")
add("_strip_slot", text="", slot="")

# ── _same_chat_key_slots ────────────────────────────────────────────────────
P = "agent:default:telegram:"
add("_same_chat_key_slots", key="other:ns", prefix=P, chat_id="c1", scope_id=None)
add("_same_chat_key_slots", key=P + "dm:c1", prefix=P, chat_id="c1", scope_id=None)
add("_same_chat_key_slots", key=P + "dm:c1:t1", prefix=P, chat_id="c1", scope_id=None)
add("_same_chat_key_slots", key=P + "group:ws1:c1", prefix=P, chat_id="c1", scope_id="ws1")
add("_same_chat_key_slots", key=P + "group:ws2:c1", prefix=P, chat_id="c1", scope_id="ws1")
add("_same_chat_key_slots", key=P + "dm:c1extra", prefix=P, chat_id="c1", scope_id=None)
add("_same_chat_key_slots", key=P + ":c1", prefix=P, chat_id="c1", scope_id=None)
PM = "agent:default:matrix:"
add("_same_chat_key_slots", key=PM + "group:!room:example.org", prefix=PM,
    chat_id="!room:example.org", scope_id=None)

# ── _queue_during_drain_enabled ─────────────────────────────────────────────
add("_queue_during_drain_enabled", self={"_busy_input_mode": "queue", "_restart_requested": True})
add("_queue_during_drain_enabled", self={"_busy_input_mode": "steer", "_restart_requested": True})
add("_queue_during_drain_enabled", self={"_busy_input_mode": "interrupt", "_restart_requested": True})
add("_queue_during_drain_enabled", self={"_busy_input_mode": "queue", "_restart_requested": False})
add("_queue_during_drain_enabled", busy_input_mode="steer",
    self={"_busy_input_mode": "interrupt", "_restart_requested": True})
add("_queue_during_drain_enabled", busy_input_mode="",
    self={"_busy_input_mode": "queue", "_restart_requested": True})
add("_queue_during_drain_enabled", self={"_busy_input_mode": "queue", "_restart_requested": 0})
add("_queue_during_drain_enabled", self={"_busy_input_mode": "queue", "_restart_requested": ""})
add("_queue_during_drain_enabled", self={"_busy_input_mode": None, "_restart_requested": True})

# ── _overflow_queue ─────────────────────────────────────────────────────────
add("_overflow_queue", state=state(), session_key="k1")
add("_overflow_queue", state=state(), session_key="kx")
add("_overflow_queue", state=st(sessions={"k1": {"conversation": {"queued_events": []}, "turn": {}}}),
    session_key="k1")
add("_overflow_queue", state=st(sessions={"k1": {"conversation": {"queued_events": ["e2", "e4"]}, "turn": {}}}),
    session_key="k1")
add("_overflow_queue", state=st(sessions={}), session_key="k1")

# ── _enqueue_fifo ───────────────────────────────────────────────────────────
add("_enqueue_fifo", state=state(), session_key="k1", event_id="e4", adapter_id="ad1")
add("_enqueue_fifo", state=st(adapters={"ad1": {"_pending_messages": {}}}),
    session_key="k1", event_id="e4", adapter_id="ad1")
add("_enqueue_fifo", state=state(), session_key="k1", event_id="e4", adapter_id=None)
add("_enqueue_fifo", state=st(adapters={"ad1": {}}), session_key="k1", event_id="e4", adapter_id="ad1")
add("_enqueue_fifo", state=st(sessions={}), session_key="k1", event_id="e4", adapter_id="ad1")
add("_enqueue_fifo", state=st(adapters={"ad1": {"_pending_messages": None}}),
    session_key="k1", event_id="e4", adapter_id="ad1")

# ── _requeue_event_at_front ─────────────────────────────────────────────────
add("_requeue_event_at_front", state=state(), session_key="k1", event_id="e4", adapter_id="ad1")
add("_requeue_event_at_front", state=st(adapters={"ad1": {"_pending_messages": {}}}),
    session_key="k1", event_id="e4", adapter_id="ad1")
add("_requeue_event_at_front", state=state(), session_key="k1", event_id="e4", adapter_id=None)
add("_requeue_event_at_front", state=st(adapters={"ad1": {"_pending_messages": None}}),
    session_key="k1", event_id="e4", adapter_id="ad1")
add("_requeue_event_at_front", state=st(sessions={}), session_key="k1", event_id="e4", adapter_id="ad1")
add("_requeue_event_at_front",
    state=st(sessions={"k1": {"conversation": {"queued_events": []}, "turn": {}}}),
    session_key="k1", event_id="e4", adapter_id="ad1")

# ── _take_wake_admitted_before_drain ────────────────────────────────────────
add("_take_wake_admitted_before_drain",
    state=st(sessions={"k1": {"conversation": {"queued_events": []}, "turn": {}}},
             adapters={"ad1": {"_pending_messages": {"k1": "e3"}}}),
    session_key="k1", adapter_id="ad1")
add("_take_wake_admitted_before_drain",
    state=st(sessions={"k1": {"conversation": {"queued_events": ["e2", "e4"]}, "turn": {}}},
             adapters={"ad1": {"_pending_messages": {"k1": "e3"}}}),
    session_key="k1", adapter_id="ad1")
add("_take_wake_admitted_before_drain",
    state=st(sessions={"k1": {"conversation": {"queued_events": ["e2", "e3"]}, "turn": {}}}),
    session_key="k1", adapter_id="ad1")
add("_take_wake_admitted_before_drain", state=state(), session_key="k1", adapter_id="ad1")
add("_take_wake_admitted_before_drain",
    state=st(adapters={"ad1": {"_pending_messages": {}}},
             sessions={"k1": {"conversation": {"queued_events": ["e3"]}, "turn": {}}}),
    session_key="k1", adapter_id="ad1")
add("_take_wake_admitted_before_drain",
    state=st(events={**state()["events"],
                     "e3": {"id": "e3", "text": "wake", "_admitted_before_drain": 1}},
             adapters={"ad1": {"_pending_messages": {"k1": "e3"}}},
             sessions={"k1": {"conversation": {"queued_events": []}, "turn": {}}}),
    session_key="k1", adapter_id="ad1")
add("_take_wake_admitted_before_drain",
    state=st(sessions={"k1": {"conversation": {"queued_events": ["e3"]}, "turn": {}}}),
    session_key="k1", adapter_id=None)
add("_take_wake_admitted_before_drain", state=st(sessions={}), session_key="k1", adapter_id="ad1")

# ── _queue_depth ────────────────────────────────────────────────────────────
add("_queue_depth", state=state(), session_key="k1", adapter_id="ad1")
add("_queue_depth",
    state=st(sessions={"k1": {"conversation": {"queued_events": []}, "turn": {}}}),
    session_key="k1", adapter_id="ad1")
add("_queue_depth", state=state(), session_key="k1", adapter_id=None)
add("_queue_depth", state=st(adapters={"ad1": {}}), session_key="k1", adapter_id="ad1")
add("_queue_depth", state=st(adapters={"ad1": {"_pending_messages": {}}}),
    session_key="k1", adapter_id="ad1")
add("_queue_depth", state=st(sessions={}), session_key="k1", adapter_id="ad1")

# ── _merge_leftover_steer ───────────────────────────────────────────────────
add("_merge_leftover_steer", state=state(), result=None, adapter_id="ad1", source=S,
    session_key="k1", pending_event_id="e4", pending="txt")
add("_merge_leftover_steer", state=state(), result={}, adapter_id="ad1", source=S,
    session_key="k1", pending_event_id="e4", pending="txt")
add("_merge_leftover_steer", state=state(), result={"pending_steer": "   "}, adapter_id="ad1",
    source=S, session_key="k1", pending_event_id="e4", pending="txt")
add("_merge_leftover_steer", state=state(), result={"pending_steer": "go"}, adapter_id="ad1",
    source=S, session_key="k1", pending_event_id=None, pending=None)
add("_merge_leftover_steer", state=state(), result={"pending_steer": "go"}, adapter_id="ad1",
    source=S, session_key="k1", pending_event_id="e4", pending=None)
add("_merge_leftover_steer", state=st(sessions={}), result={"pending_steer": "go"},
    adapter_id="ad1", source=S, session_key="k1", pending_event_id="e4", pending=None)
add("_merge_leftover_steer", state=st(adapters={"ad1": {"_pending_messages": None}}),
    result={"pending_steer": "go"}, adapter_id="ad1", source=S, session_key="k1",
    pending_event_id="e4", pending=None)
add("_merge_leftover_steer", state=st(adapters={"ad1": {"_pending_messages": {}}}),
    result={"pending_steer": "go"}, adapter_id="ad1", source=S, session_key="k1",
    pending_event_id=None, pending="txt",
    io={"_pinned_channel_inputs": pin(), "running_event": "e1", "MessageEvent": mevent()})
add("_merge_leftover_steer", state=state(), result={"pending_steer": "go"}, adapter_id="ad1",
    source=S, session_key=None, pending_event_id=None, pending="txt")
add("_merge_leftover_steer", state=st(adapters={"ad1": {"_pending_messages": {}}}),
    result={"pending_steer": "go"}, adapter_id="ad1", source=S, session_key="k1",
    pending_event_id="e4", pending=None)
add("_merge_leftover_steer", state=state(), result={"pending_steer": "\u00a0go\u2003"},
    adapter_id="ad1", source=S, session_key="k1", pending_event_id=None, pending=None)

# ── _defer_past_exhaustion_reset ────────────────────────────────────────────
add("_defer_past_exhaustion_reset", state=state(), result={}, adapter_id="ad1", source=S,
    session_key="k1", pending_event_id="e4")
add("_defer_past_exhaustion_reset", state=state(), result={"pending_steer": "go"},
    adapter_id="ad1", source=S, session_key="k1", pending_event_id="e4",
    io={"_pinned_channel_inputs": pin(), "running_event": "e1", "MessageEvent": mevent()})
add("_defer_past_exhaustion_reset", state=state(), result={"pending_steer": None},
    adapter_id="ad1", source=S, session_key="k1", pending_event_id="e4")
add("_defer_past_exhaustion_reset", state=state(), result={"pending_steer": "  "},
    adapter_id="ad1", source=S, session_key="k1", pending_event_id="e4")
add("_defer_past_exhaustion_reset",
    state=st(sessions={"k1": {"conversation": {"queued_events": []}, "turn": {}}}),
    result={"pending_steer": "go"}, adapter_id="ad1", source=S, session_key="k1",
    pending_event_id="e4",
    io={"_pinned_channel_inputs": pin(), "running_event": "e1", "MessageEvent": mevent()})

# ── _requeue_at_depth_cap ───────────────────────────────────────────────────
add("_requeue_at_depth_cap", state=state(), session_key="k1", source=S,
    pending_event_id="e4", pending=None, io={"_delivery_adapter_for": None})
add("_requeue_at_depth_cap", state=state(), session_key=None, source=S,
    pending_event_id="e4", pending=None, io={"_delivery_adapter_for": "ad1"})
add("_requeue_at_depth_cap", state=state(), session_key="k1", source=S,
    pending_event_id=None, pending=None, io={"_delivery_adapter_for": "ad1"})
add("_requeue_at_depth_cap", state=state(), session_key="k1", source=S,
    pending_event_id=None, pending="txt",
    io={"_delivery_adapter_for": "ad1", "_pinned_channel_inputs": pin(),
        "running_event": "e1", "MessageEvent": mevent()})
add("_requeue_at_depth_cap", state=state(), session_key="k1", source=S,
    pending_event_id="e4", pending=None, io={"_delivery_adapter_for": "ad1"})
add("_requeue_at_depth_cap", state=state(), session_key="k1", source=S,
    pending_event_id=None, pending="", io={"_delivery_adapter_for": "ad1"})
add("_requeue_at_depth_cap", state=state(), session_key="k1", source=S,
    pending_event_id="e4", pending=None,
    io={"_delivery_adapter_for": {"__raise__": "RuntimeError", "message":"no adapter"}})

# ── _carry_over_event ───────────────────────────────────────────────────────
add("_carry_over_event", state=state(), session_key="k1", source=S, text="go",
    io={"_pinned_channel_inputs": pin(), "running_event": "e1", "MessageEvent": mevent()})
add("_carry_over_event",
    state=st(events={**state()["events"],
                     "e2": {"id": "e2", "text": "x", "user_id": None,
                            "source": {"user_id": "u9", "user_name": "Bob"}}}),
    session_key="k1", source=S, text="go",
    io={"_pinned_channel_inputs": pin(), "running_event": "e2",
        "MessageEvent": {"id": "e5", "text": "go", "user_id": "u9", "user_name": "Bob"}})
add("_carry_over_event", state=state(), session_key="k1", source=S, text="go",
    io={"_pinned_channel_inputs": pin(), "running_event": None,
        "MessageEvent": {"id": "e5", "text": "go"}})
add("_carry_over_event", state=state(), session_key="k1", source=S, text="go",
    io={"_pinned_channel_inputs": pin(prompt=None), "running_event": "e1",
        "MessageEvent": {"id": "e5", "text": "go", "user_id": "u1"}})
add("_carry_over_event", state=state(), session_key="k1", source=S, text="go",
    io={"_pinned_channel_inputs": pin(src={"platform": {"value": "slack"}, "chat_id": "c9"}),
        "running_event": "e1", "MessageEvent": mevent()})
add("_carry_over_event", state=state(), session_key="k1", source=S, text="go",
    io={"_pinned_channel_inputs": pin(), "running_event": "e1",
        "MessageEvent": {"__raise__": "ValueError", "message":"bad event"}})

# ── _is_goal_continuation_event ─────────────────────────────────────────────
add("_is_goal_continuation_event", event_or_text=MARKER)
add("_is_goal_continuation_event", event_or_text={"text": MARKER})
add("_is_goal_continuation_event", event_or_text={"text": None})
add("_is_goal_continuation_event", event_or_text="hello")
add("_is_goal_continuation_event", event_or_text={"text": 0})
add("_is_goal_continuation_event", event_or_text={"text": "0"})
add("_is_goal_continuation_event", event_or_text="")
add("_is_goal_continuation_event", event_or_text={"text": 5})
add("_is_goal_continuation_event", event_or_text="[Continuing toward your standing goal]")
add("_is_goal_continuation_event", event_or_text="[Continuing toward your standing goal]\nGoalX")
add("_is_goal_continuation_event", event_or_text="[Continuing toward your standing goal]\nGoal")
add("_is_goal_continuation_event", event_or_text=MARKER + " extra")

# ── _steer_active_subagents ─────────────────────────────────────────────────
add("_steer_active_subagents", state=state(), running_agent=None, text="go")
add("_steer_active_subagents", state=st(agents={"a1": {}}), running_agent="a1", text="go")
add("_steer_active_subagents",
    state=st(agents={"a1": {"_active_children": {"kind": "dict", "items": ["c1"]}}}),
    running_agent="a1", text="go")
add("_steer_active_subagents",
    state=st(agents={"a1": {"_active_children": {"kind": "list", "items": []}}}),
    running_agent="a1", text="go")
add("_steer_active_subagents", state=state(), running_agent="a1", text="go",
    io={"child_steer": [True, False]})
add("_steer_active_subagents", state=state(), running_agent="a1", text="go",
    io={"child_steer": [True, {"__raise__": "RuntimeError", "message":"boom"}]})
add("_steer_active_subagents",
    state=st(agents={**state()["agents"],
                     "a1": {**state()["agents"]["a1"], "_active_children_lock": True}}),
    running_agent="a1", text="go", io={"child_steer": [True, True]})
add("_steer_active_subagents",
    state=st(agents={**state()["agents"],
                     "a1": {**state()["agents"]["a1"], "_active_children_lock": True}}),
    running_agent="a1", text="go",
    io={"lock_enter": {"__raise__": "RuntimeError", "message":"lock"}, "child_steer": [True]})
add("_steer_active_subagents",
    state=st(agents={**state()["agents"], "c2": {"steer_callable": False, "_delegate_id": "c2"}}),
    running_agent="a1", text="go", io={"child_steer": [True]})
add("_steer_active_subagents", state=state(), running_agent="a1", text="go",
    io={"child_steer": [{"__raise__": "CancelledError", "message":"x"}]})
add("_steer_active_subagents",
    state=st(agents={"a1": {"_active_children": {"kind": "set", "items": ["c1"]}},
                     "c1": {"steer_callable": True, "_delegate_id": "c1"}}),
    running_agent="a1", text="go", io={"child_steer": [True]})

# ── _try_agent_verb ─────────────────────────────────────────────────────────
add("_try_agent_verb", state=state(), running_agent="a1", verb="steer", text="go",
    session_key="k1", event_id=None, io={"agent_steer": True, "child_steer": [False, False]})
add("_try_agent_verb", state=state(), running_agent="a1", verb="steer", text="go",
    session_key="k1", event_id=None,
    io={"agent_steer": {"__raise__": "RuntimeError", "message":"no"}})
add("_try_agent_verb", state=state(), running_agent="a1", verb="steer", text="go",
    session_key="k1", event_id=None,
    io={"agent_steer": {"__raise__": "SystemExit", "message":"bye"}})
add("_try_agent_verb", state=state(), running_agent="a1", verb="redirect", text="go",
    session_key="k1", event_id=None, io={"agent_verb": True})
add("_try_agent_verb", state=state(), running_agent="a1", verb="redirect", text="go",
    session_key="k1", event_id=None,
    io={"agent_verb": {"__raise__": "AttributeError",
                       "message":"'AgentDouble' object has no attribute 'redirect'"}})
add("_try_agent_verb", state=state(), running_agent="a1", verb="steer", text="go",
    session_key="k1", event_id="e1",
    io={"steer_text_with_origin": "ORIGIN:go", "agent_steer": True, "child_steer": [False, False]})
add("_try_agent_verb", state=state(), running_agent="a1", verb="steer", text="go",
    session_key="k1", event_id="e1",
    io={"steer_text_with_origin": {"__raise__": "ValueError", "message":"bad"}})
add("_try_agent_verb", state=state(), running_agent="a1", verb="steer", text="go",
    session_key="k1", event_id=None, io={"agent_steer": False, "child_steer": [True, False]})
add("_try_agent_verb", state=state(), running_agent="a1", verb="steer", text="go",
    session_key="k1", event_id=None,
    io={"agent_steer": False, "child_steer": [{"__raise__": "KeyboardInterrupt", "message":"ki"}]})

# ── _busy_slash_route ───────────────────────────────────────────────────────
add("_busy_slash_route", cmd_def={"name": "start", "busy_handler": "start"})
add("_busy_slash_route", cmd_def={"name": "moa", "busy_handler": "moa"})
add("_busy_slash_route", cmd_def={"name": "model", "busy_handler": "model"})
add("_busy_slash_route", cmd_def={"name": "x", "busy_handler": "unknown"})
add("_busy_slash_route", cmd_def={"name": "status", "busy_policy": "dispatch"},
    io={"gateway_plain_command_handlers": {"status": "h_status"}})
add("_busy_slash_route", cmd_def={"name": "model", "busy_policy": "dispatch"},
    io={"gateway_plain_command_handlers": {}})
add("_busy_slash_route", cmd_def={"name": "approve", "busy_policy": "interrupt_then_dispatch"},
    io={"gateway_plain_command_handlers": {"approve": "h_approve"}})
add("_busy_slash_route", cmd_def={"name": "x"})
add("_busy_slash_route", cmd_def={"name": "start", "busy_handler": ""})
add("_busy_slash_route", cmd_def={"name": "goal", "busy_handler": "goal", "busy_policy": "dispatch"})

# ── _busy_interrupt_and_ack ─────────────────────────────────────────────────
ACK_IO = {"env": "true", "time": 1000.0, "busy_interrupt_text": "interrupted",
          "agent_interrupt": None, "busy_steer_ack_enabled": True,
          "compose_busy_ack_message": "ACK", "send_busy_ack_reply": None}


def steer(mode, redirected=False, dfs=False, dfc=False):
    return {"effective_mode": mode, "redirected": redirected,
            "demoted_for_subagents": dfs, "demoted_for_compression": dfc, "steered": False}


add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("interrupt"), io=ACK_IO)
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="__sentinel__",
    steer=steer("interrupt"), io=ACK_IO)
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1",
    steer=steer("interrupt", redirected=True), io=ACK_IO)
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("interrupt"),
    io={**ACK_IO, "env": "false"})
add("_busy_interrupt_and_ack",
    state=st(sessions={"k1": {"conversation": {"queued_events": []}, "turn": {"busy_ack_ts": 990.0}}}),
    event_id="e1", session_key="k1", adapter_id="ad1", busy_state_key="k1",
    running_agent="a1", steer=steer("queue"), io=ACK_IO)
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("steer"),
    io={**ACK_IO, "busy_steer_ack_enabled": False})
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("steer"), io=ACK_IO)
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("queue"), io=ACK_IO)
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key=None, running_agent="a1", steer=steer("queue"), io=ACK_IO)
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("interrupt"),
    io={**ACK_IO, "agent_interrupt": {"__raise__": "RuntimeError", "message":"fail"}})
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("interrupt"),
    io={**ACK_IO, "busy_interrupt_text": {"__raise__": "SystemExit", "message":"sx"}})
add("_busy_interrupt_and_ack", state=state(), event_id="e1", session_key="k1",
    adapter_id="ad1", busy_state_key="k1", running_agent="a1", steer=steer("queue"),
    io={**ACK_IO, "time": 20.0})

# ── _command_turn_admitted ──────────────────────────────────────────────────
ADM_IO = {"active_authority": {"id": "auth"}, "executing_admission_get": False}
add("_command_turn_admitted", state=state(), event_id="e1", session_key="k1",
    io={**ADM_IO, "active_authority": None})
add("_command_turn_admitted", state=state(), event_id="e1", session_key="k1",
    io={**ADM_IO, "executing_admission_get": True})
add("_command_turn_admitted",
    state=st(events={**state()["events"], "e1": {**state()["events"]["e1"], "internal": True}}),
    event_id="e1", session_key="k1", io=ADM_IO)
add("_command_turn_admitted",
    state=st(events={**state()["events"], "e1": {**state()["events"]["e1"], "command": "queue"}}),
    event_id="e1", session_key="k1", io=ADM_IO)
add("_command_turn_admitted", state=st(sessions={}), event_id="e1", session_key="k1", io=ADM_IO)
add("_command_turn_admitted",
    state=st(sessions={"k1": {"conversation": {"queued_events": [], "one_turn_restore": True}, "turn": {}}}),
    event_id="e1", session_key="k1", io=ADM_IO)
add("_command_turn_admitted", state=state(), event_id="e1", session_key="k1", io=ADM_IO)
add("_command_turn_admitted",
    state=st(sessions={"k1": {"conversation": {"queued_events": [], "one_turn_restore": 0}, "turn": {}}}),
    event_id="e1", session_key="k1", io=ADM_IO)
add("_command_turn_admitted", state=state(), event_id="e1", session_key="k1",
    io={**ADM_IO, "active_authority": {"__raise__": "RuntimeError", "message":"auth"}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-36s %d" % (k, counts[k]))
    thin = [k for k, v in counts.items() if v < 5]
    assert not thin, "funcs with <5 cases: %s" % thin


if __name__ == "__main__":
    main()