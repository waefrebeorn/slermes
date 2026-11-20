#!/usr/bin/env python3
"""Generate tests/fixtures/chat_completion_helpers_flow2.jsonl.

Each case is {"func": <live_name>, "in": {...}}. The harness reads these and
emits {func,in,ret,trace,st}; the oracle drives the live defs with the same
`io` seam scripts and diffs. Every func has >= 5 cases; _quiet sweeps the full
builtin exception hierarchy for contextlib.suppress(Exception).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "chat_completion_helpers_flow2.jsonl"
cases = []


def add(func, inp):
    cases.append({"func": func, "in": inp})


# 1. _live_route -- tuple(str(getattr(agent, a, "") or "") for 4 route attrs)
for agent in [
    {"model": "gpt-5", "provider": "openai", "base_url": "https://api.openai.com", "api_mode": "chat_completions"},
    {"model": "m", "provider": "", "base_url": "https://x", "api_mode": "codex_responses"},
    {"model": "m2", "provider": "xai", "base_url": "https://api.x.ai"},
    {"model": "", "provider": None, "base_url": None, "api_mode": ""},
    {},
]:
    add("_live_route", {"self": {"agent": agent}, "io": {}})

# 2. _visible_text_delivered -- bool((getattr(agent, text, "") or "").strip())
for text in [
    {"_current_streamed_assistant_text": "hello"},
    {"_current_streamed_assistant_text": "   \t  "},
    {"_current_streamed_assistant_text": ""},
    {"_current_streamed_assistant_text": "\t ok \n"},
    {},
    {"_current_streamed_assistant_text": None},
]:
    add("_visible_text_delivered", {"self": {"agent": text}, "io": {}})

# 3. _route_switched_under_request -- logger.warning on switch
route_cases = [
    ({"model": "m", "provider": "p", "base_url": "u", "api_mode": "chat_completions"},
     ["m", "p", "u", "chat_completions"]),
    ({"model": "m2", "provider": "p", "base_url": "u", "api_mode": "chat_completions"},
     ["m", "p", "u", "chat_completions"]),
    ({"model": "m", "provider": "", "base_url": "https://new", "api_mode": "chat_completions"},
     ["m", "", "https://old", "chat_completions"]),
    ({"model": "m", "provider": "p", "base_url": "u", "api_mode": "codex_responses"},
     ["m", "p", "u", "chat_completions"]),
    ({"model": "m", "provider": "p", "base_url": "u", "api_mode": "chat_completions"},
     ["m", "p"]),
    ({"model": "m", "provider": None, "base_url": "https://new", "api_mode": "chat_completions"},
     ["m", None, "https://old", "chat_completions"]),
]
for agent, req in route_cases:
    add("_route_switched_under_request", {
        "self": {"agent": agent, "_request_route": req},
        "io": {"logger.warning": None},
    })

# 4. _set_managed_stream -- holder["stream"] = stream; return stream
for stream in [None, "stream-obj", 7, {"kind": "anthropic"}, [1, 2]]:
    add("_set_managed_stream", {
        "self": {"managed_stream_holder": {"stream": None}},
        "stream": stream,
        "io": {},
    })

# 5. _start_stream_attempt -- current += 1; reset per-attempt state
for cur in [0, 1, 2, 5, -1]:
    add("_start_stream_attempt", {
        "self": {
            "stream_attempt_state": {"current": cur, "cancelled": []},
            "provider_tool_in_flight": {"yes": True},
            "prompt_progress": {"progress": "reading prompt"},
            "result": {"partial_tool_names": ["old_tool"]},
        },
        "io": {},
    })

# 6. _cancel_current_stream_attempt -- add current to cancelled; logger.debug
cancel_cases = [
    (0, [], "stopped by user"),
    (3, [], "stale kill"),
    (3, [3], "duplicate cancel"),
    (2, [1, 5], "interrupt"),
    (-1, [], "negative attempt"),
    (1, [4, 9, 1], "already present"),
]
for cur, cancelled, reason in cancel_cases:
    io = {"logger.debug": None} if cur else {}
    add("_cancel_current_stream_attempt", {
        "self": {"stream_attempt_state": {"current": cur, "cancelled": cancelled}},
        "reason": reason,
        "io": io,
    })

# 7. _stream_attempt_is_active -- sid == current and sid not in cancelled
active_cases = [
    (2, 2, []), (2, 3, []), (2, 2, [2]), (2, 2, [1, 3]), (0, 0, []), (2, 2, [2, 5]),
]
for cur, sid, cancelled in active_cases:
    add("_stream_attempt_is_active", {
        "self": {"stream_attempt_state": {"current": cur, "cancelled": cancelled}},
        "stream_attempt_id": sid,
        "io": {},
    })

# 8. _stream_attempt_was_cancelled -- sid in cancelled
for cur, sid, cancelled in [
    (2, 2, [2]), (2, 3, [2]), (0, 5, [1, 5]), (2, 4, []), (2, 1, [1, 3, 5]),
]:
    add("_stream_attempt_was_cancelled", {
        "self": {"stream_attempt_state": {"current": cur, "cancelled": cancelled}},
        "stream_attempt_id": sid,
        "io": {},
    })

# 9. _mark_finish_seen -- static diag mutation
for diag, fr in [
    ({"chunks": 2}, "stop"),
    ({"chunks": 2, "finish_reason_seen": True}, "stop"),
    ({"chunks": 0}, None),
    ({}, "length"),
    ("not-a-dict", "stop"),
    (None, "stop"),
]:
    add("_mark_finish_seen", {"diag": diag, "finish_reason": fr, "io": {}})

# 10. _quiet -- full builtin exception hierarchy under suppress(Exception)
quiet_cases = [
    ([], {"fn": None}),
    (["a", 1, True], {"fn": {"__raise__": "ValueError", "message": "bad value"}}),
    ([], {"fn": {"__raise__": "TypeError", "message": "wrong type"}}),
    ([], {"fn": {"__raise__": "RuntimeError", "message": "boom"}}),
    ([], {"fn": {"__raise__": "AttributeError", "message": "no attr"}}),
    ([], {"fn": {"__raise__": "ZeroDivisionError", "message": "divide by zero"}}),
    ([], {"fn": {"__raise__": "KeyError", "message": "missing"}}),
    ([], {"fn": {"__raise__": "FileNotFoundError", "message": "no file"}}),
    ([], {"fn": {"__raise__": "SystemExit", "message": "bye"}}),
    ([], {"fn": {"__raise__": "KeyboardInterrupt", "message": "ctrl-c"}}),
    ([], {"fn": {"__raise__": "GeneratorExit", "message": "gen exit"}}),
    ([], {"fn": {"__raise__": "BaseException", "message": "base"}}),
    ([], {"fn": {"__raise__": "CancelledError", "message": "cancelled"}}),
]
for args, io in quiet_cases:
    add("_quiet", {"fn_args": args, "io": io})

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

from collections import Counter
counts = Counter(c["func"] for c in cases)
print("wrote %d cases to %s" % (len(cases), OUT))
print("per func:", dict(sorted(counts.items())))
assert all(n >= 5 for n in counts.values()), "every func needs >=5 cases"
assert len(counts) == 10, "expected 10 funcs"
