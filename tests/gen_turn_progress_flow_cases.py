#!/usr/bin/env python3
"""Generate tests/fixtures/turn_progress_flow.jsonl for the
gateway/run_turn_progress.py FLOW port (src/gateway/turn_progress_flow.c).

Each case: {"func": <live name>, "in": {state, io, ...args}}.
"io" scripts the universal seam: bare array = per-call queue, single value =
constant, {"__script__": true, "ret": v} = constant list/object return,
{"__raise__": kind, "message": m} = raised exception. NULL == None.

Run: python tests/gen_turn_progress_flow_cases.py  (rewrites the fixture).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "turn_progress_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(**over):
    base = {
        "adapter": "ad1", "progress_lines": [], "progress_msg_id": None,
        "can_edit": True, "_PROGRESS_TEXT_LIMIT": 100, "_edit_accepts_metadata": False,
        "flood_until": 0.0, "flood_pending": False, "owed_bubbles": [],
        "owed_fresh": False, "owed_send_failures": 0, "owed_tail": None,
        "owed_tail_failures": 0,
    }
    base.update(over)
    return base


def ctx(**over):
    base = {
        "_cleanup_progress": False, "_cleanup_msg_ids": [],
        "progress_queue": {"items": []}, "log_queue": None,
        "last_progress_msg": None, "repeat_count": 0, "last_tool": None,
        "last_was_terminal_block": False, "long_tool_hint_fired": False,
        "_thinking_enabled": False, "_native_slack_task_cards": False,
        "tool_progress_enabled": True, "progress_mode": "all",
        "progress_grouping": "edit",
        "source": {"platform": {"value": "telegram"}, "chat_id": "c1", "user_id": "u1"},
        "session_key": "telegram:c1", "run_generation": 1,
        "mute_notification_reply": False, "_status_adapter": "ad1",
        "_status_chat_id": "c1", "_status_thread_metadata": None,
        "_progress_metadata": None, "_progress_reply_to": None,
        "_loop_for_step": "loop1", "user_config": {},
        "stream_consumer_id": None, "event_message_id": "em1",
    }
    base.update(over)
    return base


def state(s=None, c=None, adapters=None, consumers=None):
    return {
        "st": st() if s is None else s,
        "ctx": ctx() if c is None else c,
        "adapters": {"ad1": {"name": "telegram", "supports_code_blocks": True,
                             "REQUIRES_EDIT_FINALIZE": False, "_resume_partial_send": False}}
        if adapters is None else adapters,
        "consumers": {"sc1": {"accepts_tool_progress": True}} if consumers is None else consumers,
    }


def ok(message_id=None):
    return {"success": True, "message_id": message_id}


def fail(error="", retry_after=None, retryable=False, raw=None):
    return {"success": False, "error": error, "retry_after": retry_after,
            "retryable": retryable, "raw_response": {} if raw is None else raw}


def raise_spec(kind, message=""):
    return {"__raise__": kind, "message": message}


CHUNKS = {"undelivered_chunks": ["tail-chunk"]}

# ── A. _append_progress_repeat_count ────────────────────────────────────────
add("_append_progress_repeat_count", message="hello", occurrences=2)
add("_append_progress_repeat_count", message="```", occurrences=3)
add("_append_progress_repeat_count", message="```\r\n", occurrences=2)
add("_append_progress_repeat_count", message="x\n```  ", occurrences=4)
add("_append_progress_repeat_count", message="x\n```  \n", occurrences=2)
add("_append_progress_repeat_count", message="```\nmore", occurrences=2)
add("_append_progress_repeat_count", message="  ```\t", occurrences=5)
add("_append_progress_repeat_count", message="x\n```x", occurrences=2)
add("_append_progress_repeat_count", message="", occurrences=1)
add("_append_progress_repeat_count", message="a\n\n```\r", occurrences=2)

# ── B. _note_flood ──────────────────────────────────────────────────────────
add("_note_flood", state=state(), result=ok(), io={"is_flood_error": [False]})
add("_note_flood", state=state(), result=fail("Flood control: retry after 5", retry_after=5),
    io={"is_flood_error": [True], "monotonic": [100.0]})
add("_note_flood", state=state(), result=fail("flood wait", retry_after=0),
    io={"is_flood_error": [False], "flood_wait_seconds": [12.0], "monotonic": [200.0]})
add("_note_flood", state=state(), result=fail("Retry After 30 seconds"),
    io={"is_flood_error": [False], "flood_wait_seconds": [30.0], "monotonic": [0.5]})
add("_note_flood", state=state(), result=fail("flood", retry_after="abc"),
    io={"is_flood_error": [False], "flood_wait_seconds": [7.5], "monotonic": [10.0]})
add("_note_flood", state=state(), result=fail("flood", retry_after=" 7 "),
    io={"is_flood_error": [False], "monotonic": [1.25]})
add("_note_flood", state=state(), result=fail("flood", retry_after=[]),
    io={"is_flood_error": [False], "flood_wait_seconds": [3.0], "monotonic": [2.0]})
add("_note_flood", state=state(), result=fail("flood", retry_after=True),
    io={"is_flood_error": [False], "monotonic": [50.0]})
add("_note_flood", state=state(), result=fail("flood"),
    io={"is_flood_error": [False], "flood_wait_seconds": [30.0], "monotonic": [None]})
add("_note_flood", state=state(), result=fail("flood"),
    io={"is_flood_error": [raise_spec("SystemExit", "bye")]})

# ── C. _in_flood_window (==, <, > at the threshold) ─────────────────────────
add("_in_flood_window", state=state(s=st(flood_until=100.0)), io={"monotonic": [99.5]})
add("_in_flood_window", state=state(s=st(flood_until=100.0)), io={"monotonic": [100.0]})
add("_in_flood_window", state=state(s=st(flood_until=100.0)), io={"monotonic": [100.5]})
add("_in_flood_window", state=state(s=st(flood_until=100.0)), io={"monotonic": [0.0]})
add("_in_flood_window", state=state(s=st(flood_until=0.0)), io={"monotonic": [0.0]})
add("_in_flood_window", state=state(s=st(flood_until=100.0)), io={"monotonic": [None]})

# ── D. _owed_send_failed (>= _OWED_SEND_ATTEMPTS boundary) ──────────────────
add("_owed_send_failed", state=state(s=st(owed_send_failures=0)))
add("_owed_send_failed", state=state(s=st(owed_send_failures=3)))
add("_owed_send_failed", state=state(s=st(owed_send_failures=4)))
add("_owed_send_failed", state=state(s=st(owed_send_failures=5)))
add("_owed_send_failed", state=state(s=st(owed_send_failures=9)))
add("_owed_send_failed", state=state(s=st(owed_send_failures=4, owed_fresh=True)))

# ── E. _settle_progress_send ────────────────────────────────────────────────
add("_settle_progress_send", state=state(s=st(flood_pending=True)), result=ok())
add("_settle_progress_send", state=state(), result=fail("flood", retry_after=5),
    io={"is_flood_error": [True], "monotonic": [10.0]})
add("_settle_progress_send", state=state(s=st(flood_pending=True)),
    result=fail("permission denied"), io={"is_flood_error": [False]})
add("_settle_progress_send", state=state(s=st(flood_pending=True)), result=fail("flood"),
    io={"is_flood_error": [False], "flood_wait_seconds": [2.0], "monotonic": [3.0]})
add("_settle_progress_send", state=state(), result=ok())
add("_settle_progress_send", state=state(), result=fail("flood"),
    io={"is_flood_error": [raise_spec("KeyboardInterrupt", "stop")]})

# ── F. _hold_partial_tail ───────────────────────────────────────────────────
add("_hold_partial_tail", state=state(), result=fail("x"), io={"is_partial_delivery": [False]})
add("_hold_partial_tail", state=state(), result=fail("x", raw=CHUNKS),
    io={"is_partial_delivery": [True]})
add("_hold_partial_tail", state=state(), result=fail("x"), io={"is_partial_delivery": [True]})
add("_hold_partial_tail", state=state(), result=fail("x", raw={"undelivered_chunks": []}),
    io={"is_partial_delivery": [True]})
add("_hold_partial_tail", state=state(s=st(owed_tail=fail("old", raw=CHUNKS))),
    result=fail("new", raw=CHUNKS), io={"is_partial_delivery": [True]})
add("_hold_partial_tail", state=state(), result=fail("x"),
    io={"is_partial_delivery": [raise_spec("SystemExit", "bye")]})

# ── G. _settle_partial_progress ─────────────────────────────────────────────
add("_settle_partial_progress", state=state(s=st(progress_msg_id="m1", progress_lines=["old"])),
    result=fail("x"), rest=["a"], io={"is_partial_delivery": [False]})
add("_settle_partial_progress", state=state(s=st(progress_msg_id="m1", progress_lines=["old"])),
    result=fail("x", raw=CHUNKS), rest=["a", "b"],
    io={"is_partial_delivery": [True], "is_flood_error": [False]})
add("_settle_partial_progress", state=state(s=st(progress_msg_id="m1")),
    result=fail("flood", retry_after=5, raw=CHUNKS), rest=[],
    io={"is_partial_delivery": [True], "is_flood_error": [True], "monotonic": [10.0]})
add("_settle_partial_progress", state=state(), result=fail("x"), rest=["x"],
    io={"is_partial_delivery": [True], "is_flood_error": [False]})
add("_settle_partial_progress", state=state(s=st(progress_msg_id="m1", progress_lines=["old"])),
    result=fail("x", raw=CHUNKS), rest=["new"],
    io={"is_partial_delivery": [True], "is_flood_error": [False]})
add("_settle_partial_progress", state=state(), result=fail("x"), rest=[],
    io={"is_partial_delivery": [raise_spec("GeneratorExit", "")]})

# ── H. _owed_tail_blocks ────────────────────────────────────────────────────
OT = fail("timeout", raw=CHUNKS)
AD_RESUME = {"ad1": {"name": "telegram", "supports_code_blocks": True,
                     "REQUIRES_EDIT_FINALIZE": False, "_resume_partial_send": True}}
add("_owed_tail_blocks", state=state(s=st(owed_tail=None)))
add("_owed_tail_blocks", state=state(s=st(owed_tail=OT, owed_tail_failures=2)))
add("_owed_tail_blocks",
    state=state(s=st(owed_tail=OT), c=ctx(_cleanup_progress=True), adapters=AD_RESUME),
    io={"resume_partial_send": [ok("m9")]})
add("_owed_tail_blocks",
    state=state(s=st(owed_tail=OT, owed_tail_failures=2), adapters=AD_RESUME),
    io={"resume_partial_send": [fail("flood", retry_after=4, raw=CHUNKS)],
        "is_flood_error": [True], "monotonic": [5.0]})
add("_owed_tail_blocks",
    state=state(s=st(owed_tail=OT, owed_tail_failures=3), adapters=AD_RESUME),
    io={"resume_partial_send": [fail("net", raw=CHUNKS)], "is_flood_error": [False]})
add("_owed_tail_blocks",
    state=state(s=st(owed_tail=OT, owed_tail_failures=4), adapters=AD_RESUME),
    io={"resume_partial_send": [fail("net", raw=CHUNKS)], "is_flood_error": [False]})
add("_owed_tail_blocks",
    state=state(s=st(owed_tail=OT), adapters=AD_RESUME),
    io={"resume_partial_send": [fail("net")], "is_flood_error": [False]})
add("_owed_tail_blocks",
    state=state(s=st(owed_tail=OT), adapters=AD_RESUME),
    io={"resume_partial_send": [None]})
add("_owed_tail_blocks",
    state=state(s=st(owed_tail=OT), adapters=AD_RESUME),
    io={"resume_partial_send": [fail("flood", retry_after=1)],
        "is_flood_error": [True], "monotonic": [2.0]})

# ── I. _progress_emit ───────────────────────────────────────────────────────
add("_progress_emit", state=state(), msg="a")
add("_progress_emit", state=state(c=ctx(stream_consumer_id="sc1")), msg="a",
    io={"on_tool_progress": [None]})
add("_progress_emit", state=state(c=ctx(last_progress_msg="a", repeat_count=1)), msg="a")
add("_progress_emit",
    state=state(c=ctx(last_progress_msg="a", repeat_count=2, stream_consumer_id="sc1")), msg="a",
    io={"on_tool_progress": [None]})
add("_progress_emit", state=state(c=ctx(last_progress_msg="a", repeat_count=1)), msg="b")
add("_progress_emit",
    state=state(c=ctx(last_progress_msg="```", repeat_count=0, stream_consumer_id="sc1")), msg="```",
    io={"on_tool_progress": [None]})
add("_progress_emit",
    state=state(c=ctx(stream_consumer_id="sc1"),
                consumers={"sc1": {"accepts_tool_progress": False}}), msg="a")

# ── J. _progress_absorb ─────────────────────────────────────────────────────
add("_progress_absorb", state=state(), raw="line1")
add("_progress_absorb", state=state(), raw=["__dedup__", "base", 2])
add("_progress_absorb", state=state(s=st(progress_lines=["x", "base"])),
    raw=["__dedup__", "base", 2])
add("_progress_absorb", state=state(s=st(progress_lines=["```"])), raw=["__dedup__", "```", 1])
add("_progress_absorb", state=state(), raw=["__dedup__", "m"])
add("_progress_absorb", state=state(), raw=["other", "m", 1])
add("_progress_absorb", state=state(s=st(progress_lines=["a"])), raw="x")

# ── K. _is_reset_marker ─────────────────────────────────────────────────────
add("_is_reset_marker", state=state(), raw=["__reset__"])
add("_is_reset_marker", state=state(), raw=["__reset__", "x"])
add("_is_reset_marker", state=state(), raw=[])
add("_is_reset_marker", state=state(), raw=["__dedup__", "m", 1])
add("_is_reset_marker", state=state(), raw="str")
add("_is_reset_marker", state=state(), raw=None)
add("_is_reset_marker", state=state(), raw={"type": "tool.started"})
add("_is_reset_marker", state=state(), raw=[5])

# ── L. _reset_progress_bubble ───────────────────────────────────────────────
add("_reset_progress_bubble",
    state=state(s=st(progress_lines=["a"], progress_msg_id="m1"),
                c=ctx(last_progress_msg="a", repeat_count=3)))
add("_reset_progress_bubble",
    state=state(s=st(flood_pending=True, progress_msg_id="m1", progress_lines=["a", "b"])),
    io={"len_fn": [1, 1, 3]})
add("_reset_progress_bubble",
    state=state(s=st(flood_pending=True, progress_msg_id="m1",
                     progress_lines=["aaa", "bbb", "ccc"], _PROGRESS_TEXT_LIMIT=5)),
    io={"len_fn": [3, 3, 7, 3, 7]})
add("_reset_progress_bubble",
    state=state(s=st(owed_fresh=True, progress_lines=["r1"])))
add("_reset_progress_bubble",
    state=state(s=st(owed_fresh=True, progress_msg_id="m1", progress_lines=["r1"])))
add("_reset_progress_bubble",
    state=state(s=st(flood_pending=True, can_edit=False, progress_msg_id="m1",
                     progress_lines=["a"])))
add("_reset_progress_bubble",
    state=state(s=st(flood_pending=True, progress_lines=["a"])))
add("_reset_progress_bubble",
    state=state(s=st(flood_pending=True, progress_msg_id="m1", progress_lines=["big"],
                     _PROGRESS_TEXT_LIMIT=5)),
    io={"len_fn": [10], "truncate_message": [[]]})

# ── M. _progress_send_or_edit ───────────────────────────────────────────────
add("_progress_send_or_edit",
    state=state(s=st(flood_until=100.0)), msg="tick", io={"monotonic": [50.0]})
add("_progress_send_or_edit",
    state=state(s=st(owed_tail=OT), adapters=AD_RESUME), msg="tick",
    io={"monotonic": [1.0, 5.0],
        "resume_partial_send": [fail("flood", retry_after=2, raw=CHUNKS)],
        "is_flood_error": [True]})
add("_progress_send_or_edit",
    state=state(s=st(progress_msg_id="m1", progress_lines=["a", "b"], flood_pending=True)),
    msg="tick", io={"monotonic": [1.0], "adapter_edit": [ok()]})
add("_progress_send_or_edit",
    state=state(s=st(progress_msg_id="m1", progress_lines=["a"])), msg="tick",
    io={"monotonic": [1.0], "adapter_edit": [fail("x", raw=CHUNKS)],
        "is_partial_delivery": [True], "is_flood_error": [False]})
add("_progress_send_or_edit",
    state=state(s=st(progress_msg_id="m1", progress_lines=["a"])), msg="tick",
    io={"monotonic": [1.0], "adapter_edit": [fail("net", retryable=True)],
        "is_partial_delivery": [False]})
add("_progress_send_or_edit",
    state=state(s=st(progress_msg_id="m1", progress_lines=["a"])), msg="tick",
    io={"monotonic": [1.0, 10.0], "adapter_edit": [fail("flood", retry_after=3)],
        "is_partial_delivery": [False], "is_flood_error": [True]})
add("_progress_send_or_edit",
    state=state(s=st(progress_msg_id="m1", progress_lines=["a"])), msg="tick",
    io={"monotonic": [1.0], "adapter_edit": [fail("not found")],
        "is_partial_delivery": [False, False], "is_flood_error": [False],
        "adapter_send": [ok("m2")]})
add("_progress_send_or_edit",
    state=state(s=st(progress_lines=["a", "b"])), msg="tick",
    io={"monotonic": [1.0], "adapter_send": [ok("m1")], "is_partial_delivery": [False]})
add("_progress_send_or_edit",
    state=state(s=st(can_edit=False, progress_lines=["a"])), msg="tick",
    io={"monotonic": [1.0], "adapter_send": [ok("m2")], "is_partial_delivery": [False]})
add("_progress_send_or_edit",
    state=state(s=st(owed_fresh=True, progress_lines=["r"])), msg="tick",
    io={"monotonic": [1.0], "adapter_send": [fail("permission")],
        "is_partial_delivery": [False], "is_flood_error": [False]})
add("_progress_send_or_edit",
    state=state(s=st(flood_pending=True, progress_lines=["a"])), msg="tick",
    io={"monotonic": [1.0, 8.0], "adapter_send": [fail("flood", retry_after=2)],
        "is_partial_delivery": [False], "is_flood_error": [True]})
add("_progress_send_or_edit",
    state=state(s=st(progress_msg_id="m1", progress_lines=["a"], owed_send_failures=3)),
    msg="tick", io={"monotonic": [1.0], "adapter_edit": [ok()]})

# ── N. _roll_progress_overflow_if_needed ────────────────────────────────────
add("_roll_progress_overflow_if_needed", state=state())
add("_roll_progress_overflow_if_needed", state=state(s=st(can_edit=False, progress_lines=["a"])))
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_lines=["a"], flood_until=100.0)), io={"monotonic": [50.0]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_lines=["a", "b"])), io={"monotonic": [1.0], "len_fn": [1, 1, 3]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_lines=["aaa", "bbb", "ccc"], _PROGRESS_TEXT_LIMIT=5, owed_tail=OT),
                adapters=AD_RESUME),
    io={"monotonic": [1.0, 5.0], "len_fn": [3, 3, 7, 3, 7],
        "resume_partial_send": [fail("flood", retry_after=2, raw=CHUNKS)],
        "is_flood_error": [True]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_msg_id="m1", progress_lines=["aaa", "bbb", "ccc"],
                     _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0], "len_fn": [3, 3, 7, 3, 7],
        "adapter_edit": [fail("x", raw=CHUNKS)],
        "is_partial_delivery": [True], "is_flood_error": [False]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_msg_id="m1", progress_lines=["aaa", "bbb"],
                     _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0], "len_fn": [3, 3, 7],
        "adapter_edit": [fail("net", retryable=True)], "is_partial_delivery": [False]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_msg_id="m1", progress_lines=["aaa", "bbb"],
                     _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0, 9.0], "len_fn": [3, 3, 7],
        "adapter_edit": [fail("flood", retry_after=2)],
        "is_partial_delivery": [False], "is_flood_error": [True]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_msg_id="m1", progress_lines=["aaa", "bbb"],
                     _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0], "len_fn": [3, 3, 7], "adapter_edit": [fail("gone")],
        "is_partial_delivery": [False], "is_flood_error": [False]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_msg_id="m1", progress_lines=["aaa", "bbb"],
                     _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0], "len_fn": [3, 3, 7], "adapter_edit": [ok()],
        "adapter_send": [ok("m2")], "is_partial_delivery": [False, False]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(progress_lines=["aaa", "bbb", "ccc"], _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0, 20.0], "len_fn": [3, 3, 7, 3, 7],
        "adapter_send": [ok("m1"), fail("flood", retry_after=2)],
        "is_partial_delivery": [False, False], "is_flood_error": [True]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(owed_fresh=True, progress_lines=["aaa", "bbb"], _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0], "len_fn": [3, 3, 7], "adapter_send": [fail("perm")],
        "is_partial_delivery": [False], "is_flood_error": [False]})
add("_roll_progress_overflow_if_needed",
    state=state(s=st(owed_fresh=True, owed_send_failures=3,
                     progress_lines=["aaa", "bbb"], _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0], "len_fn": [3, 3, 7],
        "adapter_send": [ok("m1"), ok("m2")], "is_partial_delivery": [False, False]})

# ── O. _deliver_flood_pending_progress ──────────────────────────────────────
add("_deliver_flood_pending_progress",
    state=state(s=st(flood_until=100.0)), io={"monotonic": [50.0]})
add("_deliver_flood_pending_progress", state=state(), io={"monotonic": [1.0]})
add("_deliver_flood_pending_progress",
    state=state(s=st(flood_pending=True, progress_lines=["a"])),
    io={"monotonic": [1.0], "run_still_current": [False]})
add("_deliver_flood_pending_progress",
    state=state(s=st(flood_pending=True, progress_lines=["a"])),
    io={"monotonic": [1.0], "run_still_current": [True], "agent_interrupted": [True]})
add("_deliver_flood_pending_progress",
    state=state(s=st(owed_bubbles=[["m1", "txt"]])),
    io={"monotonic": [1.0], "run_still_current": [True], "agent_interrupted": [False],
        "adapter_edit": [ok()], "is_partial_delivery": [False]})
add("_deliver_flood_pending_progress",
    state=state(s=st(owed_bubbles=[["m1", "txt"]])),
    io={"monotonic": [1.0], "run_still_current": [True], "agent_interrupted": [False],
        "adapter_edit": [fail("net", retryable=True)],
        "is_partial_delivery": [False], "is_flood_error": [False]})
add("_deliver_flood_pending_progress",
    state=state(s=st(owed_bubbles=[["m1", "txt"]])),
    io={"monotonic": [1.0, 6.0], "run_still_current": [True], "agent_interrupted": [False],
        "adapter_edit": [fail("flood", retry_after=2)],
        "is_partial_delivery": [False], "is_flood_error": [True]})
add("_deliver_flood_pending_progress",
    state=state(s=st(owed_bubbles=[["m1", "txt"]])),
    io={"monotonic": [1.0], "run_still_current": [True], "agent_interrupted": [False],
        "adapter_edit": [fail("x", raw=CHUNKS)],
        "is_partial_delivery": [True], "is_flood_error": [False]})
add("_deliver_flood_pending_progress",
    state=state(s=st(owed_tail=OT), adapters=AD_RESUME),
    io={"monotonic": [1.0, 5.0], "run_still_current": [True], "agent_interrupted": [False],
        "resume_partial_send": [fail("flood", retry_after=2, raw=CHUNKS)],
        "is_flood_error": [True]})
add("_deliver_flood_pending_progress",
    state=state(s=st(owed_tail=OT), adapters=AD_RESUME),
    io={"monotonic": [1.0], "run_still_current": [True], "agent_interrupted": [False],
        "resume_partial_send": [ok()]})
add("_deliver_flood_pending_progress",
    state=state(s=st(flood_pending=True, progress_lines=["a", "b"])),
    io={"monotonic": [1.0, 2.0, 3.0], "run_still_current": [True], "agent_interrupted": [False],
        "len_fn": [1, 1, 3], "adapter_send": [ok("m1")], "is_partial_delivery": [False]})
add("_deliver_flood_pending_progress",
    state=state(s=st(flood_pending=True, progress_lines=["aaa", "bbb", "ccc"],
                     _PROGRESS_TEXT_LIMIT=5)),
    io={"monotonic": [1.0, 2.0], "run_still_current": [True], "agent_interrupted": [False],
        "len_fn": [3, 3, 7, 3, 7],
        "adapter_send": [ok("m1"), ok("m2"), ok("m3")],
        "is_partial_delivery": [False, False, False]})

# ── P. progress_callback ────────────────────────────────────────────────────
add("progress_callback", state=state(), event_type="subagent.complete", preview="goal failed",
    kwargs={"status": "failed"}, io={"local_viewer_turn": [False], "subagent_notice": [None]})
add("progress_callback", state=state(), event_type="tool.started", tool_name="terminal",
    preview="p", kwargs={},
    io={"local_viewer_turn": [True], "publish_progress": [None], "live_status": [None],
        "run_still_current": [False]})
add("progress_callback", state=state(), event_type="tool.started", tool_name="terminal",
    preview="p", kwargs={},
    io={"local_viewer_turn": [True], "publish_progress": [raise_spec("RuntimeError", "boom")],
        "live_status": [None], "run_still_current": [False]})
add("progress_callback",
    state=state(c=ctx(log_queue={"items": []}, progress_queue=None)),
    event_type="tool.started", tool_name="terminal", preview="ls",
    io={"local_viewer_turn": [False], "live_status": [None],
        "now_str": ["2026-11-19 10:00:00"]})
add("progress_callback",
    state=state(c=ctx(log_queue={"items": []}, progress_queue=None)),
    event_type="tool.started", tool_name="terminal", preview=None,
    io={"local_viewer_turn": [False], "live_status": [None],
        "now_str": ["2026-11-19 10:00:00"]})
add("progress_callback", state=state(), event_type="tool.completed", tool_name="terminal",
    kwargs={"duration": 12},
    io={"local_viewer_turn": [False], "live_status": [None], "run_still_current": [True],
        "onboarding_hint": [None]})
add("progress_callback",
    state=state(c=ctx(_thinking_enabled=True)), event_type="_thinking", tool_name="scratch",
    preview="hmm", io={"local_viewer_turn": [False], "live_status": [None],
                       "run_still_current": [True]})
add("progress_callback",
    state=state(c=ctx(_thinking_enabled=True)), event_type="tool.started",
    tool_name="_thinking", preview="deep",
    io={"local_viewer_turn": [False], "live_status": [None], "run_still_current": [True]})
add("progress_callback", state=state(), event_type="_thinking", tool_name="scratch",
    preview="hmm", io={"local_viewer_turn": [False], "live_status": [None],
                       "run_still_current": [True]})
add("progress_callback",
    state=state(c=ctx(_native_slack_task_cards=True)), event_type="tool.started",
    tool_name="terminal", io={"local_viewer_turn": [False], "live_status": [None],
                              "run_still_current": [True]})
add("progress_callback",
    state=state(c=ctx(tool_progress_enabled=False)), event_type="tool.started",
    tool_name="terminal", io={"local_viewer_turn": [False], "live_status": [None],
                              "run_still_current": [True]})
add("progress_callback", state=state(), event_type="tool.started", tool_name="clarify",
    io={"local_viewer_turn": [False], "live_status": [None], "run_still_current": [True]})
add("progress_callback", state=state(), event_type="tool.started", tool_name="terminal",
    io={"local_viewer_turn": [False], "live_status": [None], "run_still_current": [True],
        "agent_interrupted": [True]})
add("progress_callback",
    state=state(c=ctx(progress_mode="new", last_tool="terminal")), event_type="tool.started",
    tool_name="terminal", io={"local_viewer_turn": [False], "live_status": [None],
                              "run_still_current": [True], "agent_interrupted": [False]})
add("progress_callback",
    state=state(c=ctx(progress_mode="new", last_tool="web")), event_type="tool.started",
    tool_name="terminal", preview="ls", args={"command": "ls"},
    io={"local_viewer_turn": [False], "live_status": [None], "run_still_current": [True],
        "agent_interrupted": [False], "get_tool_emoji": ["\U0001f5a5\ufe0f"],
        "delivery_adapter_for": ["ad1"], "preview_max_len": [40]})
add("progress_callback",
    state=state(c=ctx(progress_mode="verbose")), event_type="tool.started",
    tool_name="terminal", preview="ls", args={"command": "ls"},
    io={"local_viewer_turn": [False], "live_status": [None], "run_still_current": [True],
        "agent_interrupted": [False], "get_tool_emoji": ["\U0001f5a5\ufe0f"],
        "delivery_adapter_for": ["ad1"], "preview_max_len": [40]})

# ── Q. _progress_build_message ──────────────────────────────────────────────
EMOJI = "\U0001f5a5\ufe0f"
add("_progress_build_message",
    state=state(c=ctx(progress_mode="verbose")), tool_name="terminal", preview=None,
    args={"command": "ls -la\npwd"},
    io={"get_tool_emoji": [EMOJI], "delivery_adapter_for": ["ad1"], "preview_max_len": [40]})
add("_progress_build_message",
    state=state(c=ctx(progress_mode="verbose")), tool_name="terminal", preview=None,
    args={"command": "x" * 50},
    io={"get_tool_emoji": [EMOJI], "delivery_adapter_for": ["ad1"], "preview_max_len": [10]})
add("_progress_build_message",
    state=state(c=ctx(progress_mode="verbose")), tool_name="terminal", preview=None,
    args={"command": "abcde"},
    io={"get_tool_emoji": [EMOJI], "delivery_adapter_for": ["ad1"], "preview_max_len": [2]})
add("_progress_build_message",
    state=state(c=ctx(last_was_terminal_block=True)), tool_name="terminal", preview=None,
    args={"command": "ls"},
    io={"get_tool_emoji": [EMOJI], "delivery_adapter_for": ["ad1"], "preview_max_len": [40]})
add("_progress_build_message",
    state=state(c=ctx(progress_mode="verbose")), tool_name="web", preview=None,
    args={"query": "cats"},
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": ["ad1"],
        "preview_max_len": [0], "verbose_tool_args_text": ["query=cats"]})
add("_progress_build_message",
    state=state(c=ctx(progress_mode="verbose")), tool_name="web", preview="cats", args=None,
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": ["ad1"]})
add("_progress_build_message",
    state=state(c=ctx(progress_mode="verbose")), tool_name="web", preview=None, args=None,
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": ["ad1"]})
add("_progress_build_message", state=state(), tool_name="web", preview=None, args=None,
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": ["ad1"]})
add("_progress_build_message", state=state(), tool_name="web", preview="cats",
    args={"query": "cats"},
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": ["ad1"],
        "preview_max_len": [40], "prepare_tool_preview": [{"text": "prepared cats"}],
        "format_tool_preview": ["formatted cats"], "get_tool_verb": [None]})
add("_progress_build_message", state=state(), tool_name="web", preview="cats", args=None,
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": ["ad1"],
        "preview_max_len": [40], "prepare_tool_preview": [{"text": "prepared cats"}],
        "format_tool_preview": ["formatted cats"], "get_tool_verb": ["Searching"],
        "verb_drops_preview": [True]})
add("_progress_build_message", state=state(), tool_name="web", preview="cats", args=None,
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": ["ad1"],
        "preview_max_len": [40], "prepare_tool_preview": [{"text": "prepared cats"}],
        "format_tool_preview": ["formatted cats"], "get_tool_verb": ["Searching"],
        "verb_drops_preview": [False], "tool_verb_connector": [" the web for "]})
add("_progress_build_message", state=state(), tool_name="web", preview="cats", args=None,
    io={"get_tool_emoji": ["\U0001f50d"],
        "delivery_adapter_for": [raise_spec("RuntimeError", "no adapter")],
        "preview_max_len": [40], "prepare_tool_preview": [{"text": "p"}],
        "get_tool_verb": [None]})
add("_progress_build_message", state=state(), tool_name="web", preview="cats", args=None,
    io={"get_tool_emoji": ["\U0001f50d"], "delivery_adapter_for": [None],
        "preview_max_len": [40], "prepare_tool_preview": [{"text": "p"}],
        "get_tool_verb": [None]})

# ── R. _status_callback_sync ────────────────────────────────────────────────
add("_status_callback_sync", state=state(c=ctx(mute_notification_reply=True)),
    event_type="status", message="m")
add("_status_callback_sync", state=state(c=ctx(_status_adapter=None)),
    event_type="status", message="m")
add("_status_callback_sync", state=state(), event_type="status", message="m",
    io={"run_still_current": [False]})
add("_status_callback_sync", state=state(), event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [False]})
add("_status_callback_sync", state=state(), event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": [None],
        "redact_secrets": ["redacted"]})
add("_status_callback_sync", state=state(), event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": [None],
        "redact_secrets": ["r" * 200]})
add("_status_callback_sync", state=state(), event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": ["prepared text"],
        "is_warning_status": [False], "render_notification": [None],
        "local_viewer_turn": [True], "compaction_status_kind": ["compacting"],
        "publish_execution": [None]})
add("_status_callback_sync", state=state(c=ctx(_cleanup_progress=True)),
    event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": ["text"],
        "is_warning_status": [True], "render_notification": [None],
        "local_viewer_turn": [False], "send_or_update_status_coro": ["coro1"],
        "schedule": ["fut1"], "add_done_callback": [None]})
add("_status_callback_sync", state=state(c=ctx(run_generation=0, _cleanup_progress=True)),
    event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": ["text"],
        "is_warning_status": [False], "render_notification": [None],
        "local_viewer_turn": [False], "send_or_update_status_coro": ["coro1"],
        "schedule": ["fut1"], "add_done_callback": [None]})
add("_status_callback_sync", state=state(c=ctx(session_key=None)),
    event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": ["text"],
        "is_warning_status": [False], "render_notification": [None],
        "local_viewer_turn": [False], "send_or_update_status_coro": ["coro1"],
        "schedule": ["fut1"]})
add("_status_callback_sync", state=state(c=ctx(_cleanup_progress=True)),
    event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": ["text"],
        "is_warning_status": [False], "render_notification": [None],
        "local_viewer_turn": [False], "send_or_update_status_coro": ["coro1"],
        "schedule": [None]})
add("_status_callback_sync", state=state(), event_type="status", message="m",
    io={"run_still_current": [True], "automatic_notices_allowed": [True],
        "status_chat_type": ["private"], "prepare_status_message": ["text"],
        "is_warning_status": [False],
        "render_notification": [raise_spec("SystemExit", "x")]})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-40s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "every func needs >= 5 cases"


if __name__ == "__main__":
    main()
