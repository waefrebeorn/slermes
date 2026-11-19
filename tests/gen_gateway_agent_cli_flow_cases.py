#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_agent_cli_flow.jsonl — {func,in} cases for
t_port_gateway_agent_cli_flow.c / oracle_gateway_agent_cli_flow.py. >=5 cases
per ported function. List-returning seams are wrapped ([[...]]) so the scripted
RESULT is the list; per-call sequences are plain arrays of results. Inputs are
consumed identically by the C harness and the oracle (which drives LIVE quarry
code). Integral values are written as ints (the C-serialize round-trip keeps
isinstance(int) checks faithful on the live side)."""
import json
from pathlib import Path

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(**kw):
    base = {
        "session_id": "s1", "connection_epoch": 1, "state": {"info": {}},
        "context_compressor": {"last_prompt_tokens": 0, "last_real_prompt_tokens": None,
                               "context_length": 0, "compression_count": 0,
                               "_config_context_length": None},
        "model": "m0", "provider": "p0",
        "_interrupt_requested": False, "_interrupt_sent_at": None, "_generation": None,
        "_turn_ask_index": None, "_turn": None, "_pending_cli_user_message": None,
        "suppress_status_output": False, "_print_fn": None,
        "_config_context_length": None, "_context_tokens": 0, "_turn_calls_base": 0,
        "_turn_base_usage_anchor": None, "_turn_api_calls": None,
        "_api_latency_history": [], "_api_output_history": [], "_rate_limit_state": None,
        "session_input_tokens": 0, "session_output_tokens": 0, "session_prompt_tokens": 0,
        "session_completion_tokens": 0, "session_total_tokens": 0,
        "session_cache_read_tokens": 0, "session_cache_write_tokens": 0,
        "session_reasoning_tokens": 0, "session_api_calls": 0,
    }
    base.update(kw)
    return base


# ── review_summary_line ──
add("review_summary_line", payload={"text": "hello"})
add("review_summary_line", payload={"text": "  hi  "})
add("review_summary_line", payload={"text": ""})
add("review_summary_line", payload={"text": None})
add("review_summary_line", payload={})
add("review_summary_line", payload="notdict")
add("review_summary_line", payload={"text": "   "})

# ── summarize_api_error ──
add("summarize_api_error", exc={"kind": "RuntimeError", "msg": "boom"})
add("summarize_api_error", exc={"kind": "ValueError", "msg": "x" * 400})
add("summarize_api_error", exc={"kind": "RuntimeError", "msg": ""})
add("summarize_api_error", exc={"kind": "GatewayClientError", "msg": "lost"})
add("summarize_api_error", exc={"kind": "RuntimeError", "msg": "a" * 300})

# ── rate_limit_state ──
add("rate_limit_state", data=None)
add("rate_limit_state", data={}, seams={"RateLimitBucket": [{"limit": 1}, {"limit": 2}, {"limit": 3}, {"limit": 4}],
                                        "RateLimitState": {"ok": True}})
add("rate_limit_state", data={"requests_min": {"limit": 5, "remaining": 2, "bogus": 9},
                              "captured_at": 1.5, "provider": "openai"},
    seams={"RateLimitBucket": [{"limit": 5}, {}, {}, {}], "RateLimitState": {"rebuilt": True}})
add("rate_limit_state", data={"requests_min": None, "tokens_hour": {"reset_seconds": 30}},
    seams={"RateLimitBucket": [{}, {}, {}, {"reset_seconds": 30}], "RateLimitState": {"ok": 1}})
add("rate_limit_state", data={"requests_min": {"limit": 1}},
    seams={"RateLimitBucket": {"__raise__": "RuntimeError", "msg": "b"}})
add("rate_limit_state", data={}, seams={"RateLimitBucket": [{}, {}, {}, {}],
                                        "RateLimitState": {"__raise__": "ValueError", "msg": "s"}})
add("rate_limit_state", data={}, seams={})  # fail-closed

# ── moa_handler ──
add("moa_handler", cb_present=False, kind="moa.reference", payload={"label": "L"})
add("moa_handler", cb_present=True, kind="moa.reference",
    payload={"label": "L", "text": "T", "index": 2, "count": 5, "extra1": "v"},
    seams={"tool_progress_callback": None})
add("moa_handler", cb_present=True, kind="moa.aggregating", payload={"name": "N", "preview": "P"},
    seams={"tool_progress_callback": None})
add("moa_handler", cb_present=True, kind="moa.reference",
    payload={"label": "", "name": "N", "text": "", "preview": "P", "index": None, "count": 0},
    seams={"tool_progress_callback": None})
add("moa_handler", cb_present=True, kind="moa.reference", payload={}, seams={"tool_progress_callback": None})
add("moa_handler", cb_present=True, kind="moa.reference", payload={"label": "L"},
    seams={"tool_progress_callback": {"__raise__": "RuntimeError", "msg": "x"}})
add("moa_handler", cb_present=True, kind="moa.reference", payload={"label": "L"}, seams={})  # fail-closed

# ── apply_info ──
add("apply_info", st=st(), info={"model": "m1", "provider": "p1", "context_length": 100})
add("apply_info", st=st(), info={"context_length": 12.7})
add("apply_info", st=st(), info={})
add("apply_info", st=st(), info=None)
add("apply_info", st=st(), info={"model": "", "provider": "p1"})
add("apply_info", st=st(), info={"context_length": 0})
add("apply_info", st=st(), info={"model": "m2"})

# ── tools ──
add("tools", st=st(state={"info": {"tools": {"a": ["x", "y"], "b": ["z"]}}}))
add("tools", st=st(state={"info": {"tools": {"a": []}}}))
add("tools", st=st(state={"info": {"tools": {}}}))
add("tools", st=st(state={"info": {}}))
add("tools", st=st(state={"info": {"tools": {"a": None, "b": ["z"]}}}))
add("tools", st=st(state={"info": {"tools": {"a": ["x"], "b": ["x"]}}}))

# ── valid_tool_names ──
add("valid_tool_names", st=st(state={"info": {"tools": {"a": ["x", "y"], "b": ["y", "z"]}}}))
add("valid_tool_names", st=st(state={"info": {"tools": {}}}))
add("valid_tool_names", st=st(state={"info": {"tools": {"a": ["b", "a"]}}}))
add("valid_tool_names", st=st(state={"info": {}}))
add("valid_tool_names", st=st(state={"info": {"tools": {"a": ["z"], "b": ["a", "m"]}}}))

# ── apply_session_usage ──
add("apply_session_usage", st=st(), usage="notdict")
add("apply_session_usage", st=st(), usage={"input_tokens": 10, "output_tokens": 20, "api_calls": 3})
add("apply_session_usage", st=st(), usage={"input_tokens": True})
add("apply_session_usage", st=st(), usage={"last_prompt_tokens": 50, "context_length": 200,
                                           "compression_count": 2, "config_context_length": 300})
add("apply_session_usage", st=st(), usage={"config_context_length": None})
add("apply_session_usage", st=st(), usage={"context_source": "provider_usage", "last_prompt_tokens": 10})
add("apply_session_usage", st=st(), usage={"context_source": "local_estimate", "last_prompt_tokens": 10})
add("apply_session_usage", st=st(), usage={"context_tokens": 500}, seams={"_anchor_display": None})
add("apply_session_usage", st=st(), usage={"avg_latency_s": 0.5, "tokens_per_s": 10})
add("apply_session_usage", st=st(), usage={"rate_limit": {"requests_min": {"limit": 1}}},
    seams={"_rate_limit_state": {"rebuilt": True}})
add("apply_session_usage", st=st(), usage={"model": "m9", "provider": "p9"})
add("apply_session_usage", st=st(), usage={"context_tokens": 5, "rate_limit": {}},
    seams={"_anchor_display": None, "_rate_limit_state": None})
add("apply_session_usage", st=st(), usage={"context_tokens": 5},
    seams={"_anchor_display": {"__raise__": "RuntimeError", "msg": "a"}})
add("apply_session_usage", st=st(), usage={"last_prompt_tokens": True})

# ── reset_session_figures ──
add("reset_session_figures", st=st(session_input_tokens=50, _context_tokens=99,
                                   _api_latency_history=[1, 2],
                                   context_compressor={"last_prompt_tokens": 99, "last_real_prompt_tokens": 5,
                                                       "context_length": 7, "compression_count": 3,
                                                       "_config_context_length": 8}))
add("reset_session_figures", st=st())
add("reset_session_figures", st=st(session_api_calls=12, _turn_api_calls=4))
add("reset_session_figures", st=st(_api_output_history=[3.5]))
add("reset_session_figures", st=st(session_total_tokens=1000, _turn_calls_base=7))

# ── reset_session_state ──
add("reset_session_state", st=st(state={}), seams={"_reset_session_figures": None})
add("reset_session_state", st=st(state={"messages": [{"role": "user"}], "usage": {}}),
    seams={"_reset_session_figures": None, "_refresh_stats": True})
add("reset_session_state", st=st(state={"usage": {"input_tokens": 1}}),
    seams={"_reset_session_figures": None, "_refresh_stats": False, "apply_session_usage": None})
add("reset_session_state", st=st(state={"messages": [], "usage": {}}), seams={"_reset_session_figures": None})
add("reset_session_state", st=st(state={"usage": {"a": 1}}),
    seams={"_reset_session_figures": None, "_refresh_stats": {"__raise__": "GatewayClientError", "msg": "x"}})
add("reset_session_state", st=st(state={"usage": {"a": 1}}), seams={})  # fail-closed

# ── interrupt ──
add("interrupt", st=st(_interrupt_sent_at=None, _generation=5), message="hi",
    seams={"monotonic": 100.0, "call": None})
add("interrupt", st=st(_interrupt_sent_at=50, _generation=None, state={"execution_generation": 7}),
    seams={"call": None})
add("interrupt", st=st(_generation=None, state={}), seams={"monotonic": 1.0, "call": None})
add("interrupt", st=st(_generation=5), seams={"monotonic": 1.0, "call": {"__raise__": "GatewayClientError", "msg": "e"}})
add("interrupt", st=st(_generation=5), seams={"monotonic": 1.0, "call": {"__raise__": "SystemExit", "msg": "bye"}})
add("interrupt", st=st(_generation=5), seams={"monotonic": 1.0})  # call fail-closed, swallowed
add("interrupt", st=st(_interrupt_sent_at=None), seams={"monotonic": {"__raise__": "RuntimeError", "msg": "m"}})

# ── hard_interrupt ──
add("hard_interrupt", st=st(), message="hi", seams={"interrupt": None})
add("hard_interrupt", st=st(), message=None, seams={"interrupt": None})
add("hard_interrupt", st=st(), message="x", seams={"interrupt": {"__raise__": "RuntimeError", "msg": "x"}})
add("hard_interrupt", st=st(), message="x", seams={})  # fail-closed
add("hard_interrupt", st=st(), message="", seams={"interrupt": None})

# ── steer ──
add("steer", st=st(), text="hi", seams={"_correct": True})
add("steer", st=st(), text="hi", seams={"_correct": False})
add("steer", st=st(), text="stop", seams={"_correct": "voice_stopped"})
add("steer", st=st(), text="hi", seams={"_correct": {"__raise__": "GatewayClientError", "msg": "e"}})
add("steer", st=st(), text="hi", seams={})  # fail-closed
add("steer", st=st(), text=None, seams={"_correct": False})

# ── redirect ──
add("redirect", st=st(_turn_ask_index=5), text="hi", seams={"_correct": True})
add("redirect", st=st(_turn_ask_index=5), text="hi", seams={"_correct": False})
add("redirect", st=st(_turn_ask_index=5), text="stop", seams={"_correct": "voice_stopped"})
add("redirect", st=st(_turn_ask_index=5), text="hi", seams={"_correct": {"__raise__": "RuntimeError", "msg": "e"}})
add("redirect", st=st(_turn_ask_index=5), text="hi", seams={})  # fail-closed
add("redirect", st=st(_turn_ask_index=None), text="hi", seams={"_correct": True})

# ── expired ──
add("expired", st=st(_interrupt_sent_at=None), seams={})
add("expired", st=st(_interrupt_sent_at=100), seams={"monotonic": 105})
add("expired", st=st(_interrupt_sent_at=100), seams={"monotonic": 111})
add("expired", st=st(_interrupt_sent_at=100), seams={"monotonic": 110})
add("expired", st=st(_interrupt_sent_at=100), seams={"monotonic": {"__raise__": "RuntimeError", "msg": "m"}})
add("expired", st=st(_interrupt_sent_at=100), seams={})  # fail-closed

# ── stage_content_parts ──
add("stage_content_parts", st=st(), parts=[{"type": "text", "text": "hello"}])
add("stage_content_parts", st=st(), parts=[{"type": "text", "text": "hi\n[Image attached at: x]\nbye"}])
add("stage_content_parts", st=st(), parts=[{"type": "image_url", "image_url": {"url": "data:image/png;base64,AAA"}}],
    seams={"call": {"path": "/p", "mime": "image/png"}})
add("stage_content_parts", st=st(), parts=[{"type": "text", "text": "a"}, {"type": "text", "text": "b"}])
add("stage_content_parts", st=st(), parts=[{"type": "text", "text": ""}, {"type": "text", "text": "b"}])
add("stage_content_parts", st=st(), parts=[{"type": "image_url", "image_url": {"url": "http://x"}}])
add("stage_content_parts", st=st(), parts=[{"type": "text", "text": "x"},
                                           {"type": "image_url", "image_url": {"url": "data:image/jpeg;base64,B"}}],
    seams={"call": {"path": "/q", "mime": "image/jpeg"}})
add("stage_content_parts", st=st(), parts=[{"type": "image_url", "image_url": {"url": "data:image/png;base64,AAA"}}],
    seams={"call": {"__raise__": "GatewayClientError", "msg": "e"}})
add("stage_content_parts", st=st(), parts=["str", {"type": "text", "text": "y"}])

# ── lost_turn_notice ──
add("lost_turn_notice", st=st(state={"pending": []}), seams={})
add("lost_turn_notice", st=st(state={"pending": [{"status": "queued"}]}), seams={})
add("lost_turn_notice", st=st(state={"pending": [{"status": "unknown"}]}), seams={"pending_lines": [["l1", "l2"]]})
add("lost_turn_notice", st=st(state={"pending": [{"status": "unknown"}, {"status": "queued"}]}),
    seams={"pending_lines": [["only"]]})
add("lost_turn_notice", st=st(state={"pending": [{"status": "unknown"}]}),
    seams={"pending_lines": {"__raise__": "RuntimeError", "msg": "x"}})
add("lost_turn_notice", st=st(state={"pending": [{"status": "unknown"}]}), seams={})  # fail-closed
add("lost_turn_notice", st=st(state={"pending": [{"status": "started"}]}), seams={})

# ── user_row ──
add("user_row", st=st(_pending_cli_user_message={"role": "user", "content": "staged", "display_kind": "card"}),
    user_message="ignored")
add("user_row", st=st(_pending_cli_user_message=None), user_message="hi")
add("user_row", st=st(), user_message=["parts"])
add("user_row", st=st(_pending_cli_user_message="notdict"), user_message="hi")
add("user_row", st=st(), user_message=None)

# ── failed ──
add("failed", st=st(), history=[], user_message="hi", error="boom", failure_reason=None)
add("failed", st=st(), history=[{"role": "user", "content": "old"}], user_message="new", error="e",
    failure_reason="gateway_connection", retryable=True)
add("failed", st=st(), history=[], user_message="x", error="busy", failure_reason="session_busy", retryable=True)
add("failed", st=st(), history=[], user_message="x", error=None)
add("failed", st=st(), history=[], user_message="x", error="e", failure_reason="")
add("failed", st=st(_pending_cli_user_message={"role": "user", "content": "staged"}), history=[],
    user_message="ignored", error="e")

# ── unadmitted ──
add("unadmitted", st=st(), history=[{"role": "user", "content": "old"}], user_message="new", error="not sent",
    seams={"_give_back_turn_route": None})
add("unadmitted", st=st(), history=[], user_message="x", error="e", seams={"_give_back_turn_route": None})
add("unadmitted", st=st(), history=[], user_message="x", error="e",
    seams={"_give_back_turn_route": {"__raise__": "RuntimeError", "msg": "x"}})
add("unadmitted", st=st(), history=[], user_message="x", error="e", seams={})  # fail-closed
add("unadmitted", st=st(), history=[], user_message="x", error=None, seams={"_give_back_turn_route": None})
add("unadmitted", st=st(_pending_cli_user_message={"role": "user", "content": "staged"}), history=[],
    user_message="ignored", error="e", seams={"_give_back_turn_route": None})

# ── emit_print ──
add("emit_print", st=st(suppress_status_output=False, _print_fn=None), text="hi", seams={"print": None})
add("emit_print", st=st(suppress_status_output=True), text="hi", seams={})
add("emit_print", st=st(_print_fn={"fn": 1}), text="hi", seams={"print_fn": None})
add("emit_print", st=st(), text="hi", seams={"print": {"__raise__": "RuntimeError", "msg": "x"}})
add("emit_print", st=st(), text="hi", seams={})  # fail-closed
add("emit_print", st=st(), text="", seams={"print": None})

# ── on_print ──
add("on_print", st=st(), payload={"text": "hello"}, seams={"print": None})
add("on_print", st=st(), payload={"text": ""}, seams={"print": None})
add("on_print", st=st(), payload={"text": None}, seams={"print": None})
add("on_print", st=st(), payload={}, seams={"print": None})
add("on_print", st=st(suppress_status_output=True), payload={"text": "x"}, seams={})

# ── on_review_summary ──
add("on_review_summary", st=st(), payload={"text": "review"}, seams={"print": None})
add("on_review_summary", st=st(), payload={"text": ""}, seams={})
add("on_review_summary", st=st(), payload={"text": "  x  "}, seams={"print": None})
add("on_review_summary", st=st(), payload={}, seams={})
add("on_review_summary", st=st(suppress_status_output=True), payload={"text": "x"}, seams={})
add("on_review_summary", st=st(), payload={"text": "x"}, seams={"print": {"__raise__": "RuntimeError", "msg": "x"}})

# ── owns ──
add("owns", st=st(_turn="t1", session_id="s1"), turn={"id": "t1", "session_id": "s1"})
add("owns", st=st(_turn="t2", session_id="s1"), turn={"id": "t1", "session_id": "s1"})
add("owns", st=st(_turn="t1", session_id="s1"), turn={"id": "t1", "session_id": "s2"})
add("owns", st=st(_turn=None, session_id="s1"), turn={"id": "t1", "session_id": "s1"})
add("owns", st=st(_turn="t1", session_id="s1"), turn={"id": "t1", "session_id": "s1"})
add("owns", st=st(_turn="t9", session_id="s9"), turn={"id": "t9", "session_id": "s9"})

out = Path(__file__).resolve().parent / "fixtures" / "gateway_agent_cli_flow.jsonl"
out.parent.mkdir(parents=True, exist_ok=True)
with open(out, "w", encoding="utf-8", newline="\n") as fh:
    for c in CASES:
        fh.write(json.dumps(c, ensure_ascii=False) + "\n")
# per-function coverage check
from collections import Counter
cnt = Counter(c["func"] for c in CASES)
low = {f: n for f, n in cnt.items() if n < 5}
print("wrote %d cases -> %s" % (len(CASES), out))
print("funcs: %d, min cases/func: %d" % (len(cnt), min(cnt.values())))
if low:
    print("WARN <5 cases: %s" % low)