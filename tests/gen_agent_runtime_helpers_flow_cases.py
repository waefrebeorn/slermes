#!/usr/bin/env python3
"""Generate tests/fixtures/agent_runtime_helpers_flow.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Representation
scope: ASCII text, scalar str() values, no integral-float JSON literals in
trajectory arguments (documented in the oracle/header).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "agent_runtime_helpers_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


# ── _with_think_block ─────────────────────────────────────────────────────
add("_with_think_block", content="hello")
add("_with_think_block", content="x\ny")
add("_with_think_block", content="")
add("_with_think_block", content="prefix  suffix")
add("_with_think_block", content="no tags here")

# ── _flatten_content_text ─────────────────────────────────────────────────
add("_flatten_content_text", content="plain")
add("_flatten_content_text", content=[
    "a", {"type": "text", "text": "b"},
    {"type": "thinking", "text": "secret"},
    {"type": "reasoning", "text": "s"},
    {"type": "redacted_thinking", "text": "s"},
    {"type": "text", "text": ""}, {"type": "text"}, 42, None,
])
add("_flatten_content_text", content=[
    {"type": " THINKING ", "text": "x"}, {"type": "Text", "text": "ok"},
])
add("_flatten_content_text", content={"text": "t", "content": "c"})
add("_flatten_content_text", content={"text": "", "content": "c"})
add("_flatten_content_text", content={"text": 0, "content": False})
add("_flatten_content_text", content={})
add("_flatten_content_text", content=None)
add("_flatten_content_text", content=42)
add("_flatten_content_text", content=True)
add("_flatten_content_text", content={"text": 123})

# ── _merge_user_content ───────────────────────────────────────────────────
add("_merge_user_content", prev="a", cur="b")
add("_merge_user_content", prev="", cur="b")
add("_merge_user_content", prev="a", cur="")
add("_merge_user_content", prev="", cur="")
add("_merge_user_content", prev=[1], cur=[2])
add("_merge_user_content", prev=[{"type": "text", "text": "x"}], cur="y")
add("_merge_user_content", prev=[1], cur="")
add("_merge_user_content", prev="a", cur=[{"type": "text", "text": "b"}])
add("_merge_user_content", prev="", cur=[1])
add("_merge_user_content", prev=None, cur="x")
add("_merge_user_content", prev=5, cur=6)

# ── _trajectory_gpt_prefix ────────────────────────────────────────────────
add("_trajectory_gpt_prefix", msg={"reasoning": "plan"})
add("_trajectory_gpt_prefix", msg={"reasoning": "  "})
add("_trajectory_gpt_prefix", msg={"reasoning": ""})
add("_trajectory_gpt_prefix", msg={})
add("_trajectory_gpt_prefix", msg={"reasoning": None})
add("_trajectory_gpt_prefix", msg={"reasoning": "a\nb"})

# ── _trajectory_tool_call_turn ────────────────────────────────────────────
add("_trajectory_tool_call_turn", msg={"tool_calls": []}, io={})
add("_trajectory_tool_call_turn", msg={
    "reasoning": "r",
    "tool_calls": [{"function": {"name": "f", "arguments": "{\"x\": 1}"}}],
}, io={})
add("_trajectory_tool_call_turn", msg={
    "content": "<REASONING_SCRATCHPAD>x</REASONING_SCRATCHPAD>",
    "tool_calls": [],
}, io={"convert_scratchpad_to_think": "converted"})
add("_trajectory_tool_call_turn", msg={"content": "   ", "tool_calls": []}, io={})
add("_trajectory_tool_call_turn", msg={
    "tool_calls": [{"function": {"name": "f", "arguments": "{bad"}}],
}, io={"logger.warning": script(None)})
add("_trajectory_tool_call_turn", msg={
    "tool_calls": [None, 5, {"function": {"name": "g", "arguments": {"k": [1, 2]}}}],
}, io={})
add("_trajectory_tool_call_turn", msg={
    "tool_calls": [{"function": {"name": "h", "arguments": 42}}],
}, io={})
add("_trajectory_tool_call_turn", msg={}, io={})  # KeyError 'tool_calls'
add("_trajectory_tool_call_turn", msg={
    "tool_calls": [{"function": {"name": "u", "arguments": "{\"t\": \"héllo →\"}"}}],
}, io={})
add("_trajectory_tool_call_turn", msg={
    "reasoning": "think", "content": "scratch",
    "tool_calls": [
        {"function": {"name": "a", "arguments": "{\"n\": 1}"}},
        {"function": {"name": "b", "arguments": "nope"}},
    ],
}, io={"convert_scratchpad_to_think": "THINK", "logger.warning": script(None)})

# ── _trajectory_tool_responses ────────────────────────────────────────────
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[{"role": "tool", "content": "plain", "tool_call_id": "c1"}], start=0)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[{"role": "tool", "content": "{\"a\": 1}", "tool_call_id": "c1"}], start=0)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[{"role": "tool", "content": "{bad", "tool_call_id": "c1"}], start=0)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[{"role": "tool", "content": "[1, 2]", "tool_call_id": "c1"}], start=0)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[
        {"role": "tool", "content": "one", "tool_call_id": "c1"},
        {"role": "tool", "content": "two", "tool_call_id": "c2"},
    ], start=0)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[{"role": "tool", "content": {"already": "dict"}, "tool_call_id": "c1"}], start=0)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[
        {"role": "tool", "content": "one", "tool_call_id": "c1"},
        {"role": "user", "content": "stop"},
    ], start=0)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[
        {"role": "user", "content": "skip"},
        {"role": "tool", "content": "one"},
    ], start=1)
add("_trajectory_tool_responses",
    msg={"tool_calls": [{"function": {"name": "f"}}]},
    messages=[{"role": "tool", "content": "  {\"a\": 1}  ", "tool_call_id": "c1"}], start=0)

# ── _duration_string_seconds ──────────────────────────────────────────────
for text in ["30", "1h30m", "500ms", "2h", "1m30s", "", "abc", "1x", "1e3",
             "+2.5", "inf", "nan", "1_000", "1h30", "0.5s", "  45  ", ".5",
             "5.", "1H30M", "-inf"]:
    add("_duration_string_seconds", text=text)

# ── _set_reset_from_retry_after ───────────────────────────────────────────
add("_set_reset_from_retry_after", context={}, retry_after="30",
    io={"parse_retry_after_seconds": 30.0, "time.time": 1000.0})
add("_set_reset_from_retry_after", context={"reset_at": 5}, retry_after="30", io={})
add("_set_reset_from_retry_after", context={}, retry_after="x",
    io={"parse_retry_after_seconds": None})
add("_set_reset_from_retry_after", context={}, retry_after="0",
    io={"parse_retry_after_seconds": 0.0, "time.time": 1000.0})
add("_set_reset_from_retry_after", context={}, retry_after="x",
    io={"parse_retry_after_seconds": script(raise_spec={"type": "ValueError", "message": "bad"})})
add("_set_reset_from_retry_after", context={}, retry_after=None,
    io={"parse_retry_after_seconds": 12.5, "time.time": 1000.0})

# ── _set_reset_from_vendor_headers ────────────────────────────────────────
add("_set_reset_from_vendor_headers", context={},
    headers={"x-ratelimit-reset-requests": "30"}, io={"time.time": 1000.0})
add("_set_reset_from_vendor_headers", context={},
    headers={"x-ratelimit-reset-requests": "  ", "x-ratelimit-reset-tokens": "1m"},
    io={"time.time": 1000.0})
add("_set_reset_from_vendor_headers", context={},
    headers={"x-ratelimit-reset-requests": "2026-01-01T00:00:00Z"},
    io={"_parse_absolute_timestamp": 2000.0, "time.time": [1000.0, 1500.0]})
add("_set_reset_from_vendor_headers", context={},
    headers={"x-ratelimit-reset-requests": "when"},
    io={"_parse_absolute_timestamp": None})
add("_set_reset_from_vendor_headers", context={},
    headers={"x-ratelimit-reset-requests": "when"},
    io={"_parse_absolute_timestamp": 500.0, "time.time": 1000.0})
add("_set_reset_from_vendor_headers", context={}, headers={}, io={})
add("_set_reset_from_vendor_headers", context={},
    headers={"x-ratelimit-reset-requests": "0s", "x-ratelimit-reset-tokens": "5s"},
    io={"time.time": 1000.0})
add("_set_reset_from_vendor_headers", context={}, headers=None, io={})
add("_set_reset_from_vendor_headers", context={},
    headers={"anthropic-ratelimit-tokens-reset": "2m"}, io={"time.time": 1000.0})
add("_set_reset_from_vendor_headers", context={},
    headers={"x-ratelimit-reset-requests": 5, "anthropic-ratelimit-requests-reset": "10s"},
    io={"time.time": 1000.0})
add("_set_reset_from_vendor_headers", context={"reset_at": 1},
    headers={"x-ratelimit-reset-requests": "30"}, io={"time.time": 1000.0})

# ── _is_entitlement_403 ───────────────────────────────────────────────────
add("_is_entitlement_403", agent={}, status_code=500, error_context={},
    io={"agent._is_entitlement_failure": True})
add("_is_entitlement_403", agent={}, status_code=401, error_context={},
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={}, status_code=403,
    error_context={"message": "OAuth authentication is currently not allowed for this organization"},
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "anthropic", "api_mode": "anthropic_messages"},
    status_code=403, error_context={}, io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "anthropic", "api_mode": "chat_completions"},
    status_code=403, error_context={}, io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "xai-oauth"}, status_code=403,
    error_context={}, io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "xai-oauth"}, status_code=403,
    error_context={"message": "[WKE=unauthenticated:xyz]"},
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "xai-oauth"}, status_code=403,
    error_context={"reason": "OAuth2 access token could not be validated"},
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "xai-oauth"}, status_code=403,
    error_context={"message": "something else"}, io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={}, status_code=403, error_context=None,
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "openai"}, status_code=403,
    error_context={"message": 0, "reason": False, "code": "X", "error": None},
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={}, status_code=403, error_context={},
    io={"agent._is_entitlement_failure": script(raise_spec={"type": "RuntimeError", "message": "probe boom"})})
add("_is_entitlement_403", agent={"provider": None}, status_code=403, error_context={},
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={}, status_code=403, error_context="text",
    io={"agent._is_entitlement_failure": False})
add("_is_entitlement_403", agent={"provider": "anthropic"}, status_code=403,
    error_context={}, io={"agent._is_entitlement_failure": False})  # api_mode absent


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for f, n in sorted(counts.items()):
        print(f"  {f}: {n}")
    assert all(n >= 4 for n in counts.values()), "every port needs >=4 cases"


if __name__ == "__main__":
    main()
