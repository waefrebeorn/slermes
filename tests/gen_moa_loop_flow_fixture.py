#!/usr/bin/env python3
"""Generate tests/fixtures/moa_loop_flow.jsonl — decision-sequence cases for
the moa_loop_flow port. Each line {"func","in"}; the oracle drives LIVE Python
and the C harness drives the port under identical io scripts. Run with the
quarry venv python; writes the fixture beside the tests dir.
"""
import json
import os

OUT = os.path.join(os.path.dirname(__file__), "fixtures", "moa_loop_flow.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def usage(i=10, o=5, cr=0, cw=0, r=0, rc=1):
    return {"_type": "CanonicalUsage", "input_tokens": i, "output_tokens": o,
            "cache_read_tokens": cr, "cache_write_tokens": cw,
            "reasoning_tokens": r, "request_count": rc}


def acct(u=None, cost=0.5, status="actual", source="none", output="x",
         model="m", provider="p", temp=None, messages=None):
    return {"_type": "_RefAccounting", "usage": u if u is not None else usage(),
            "cost_usd": cost, "cost_status": status, "cost_source": source,
            "messages": messages, "output": output, "model": model,
            "provider": provider, "temperature": temp}


def out(label="p:m", text="hello", a=None):
    return [label, text, a if a is not None else acct()]


def fanout_state(**kw):
    st = {"preset_name": "default", "_fanout_turn_sig": None,
          "_fanout_iteration_count": 0, "_fanout_last_state_sig": None,
          "_ref_cache_outputs": [], "_ref_cache_key": None}
    st.update(kw)
    return st


def run_state(**kw):
    st = {"preset_name": "default", "_privacy_mode": "", "_agent": None,
          "_ref_cache_key": None, "_ref_cache_outputs": [],
          "_pending_trace": None, "_last_reference_metrics": None,
          "_pending_reference_usage": usage(), "_pending_reference_cost": None}
    st.update(kw)
    return st


RAISE = {"__raise__": "RuntimeError", "msg": "boom"}
CANCEL = {"__raise__": "CancelledError", "msg": "cancelled"}

# ── _field ──
add("_field", obj={"a": 1}, name="a")
add("_field", obj={"a": 1}, name="b")
add("_field", obj=None, name="a")
add("_field", obj={"__attr__": {"a": 2}}, name="a")
add("_field", obj={"__attr__": {"a": 2}}, name="z")
add("_field", obj={"a": [1, 2]}, name="a")

# ── _slot_labels ──
add("_slot_labels", slots=[], io={})
add("_slot_labels", slots=[{"provider": "p", "model": "m"}], io={"_slot_label": "p:m"})
add("_slot_labels", slots=[{"provider": "p", "model": "m"}, {"provider": "q", "model": "n"}],
    io={"_slot_label": {"seq": ["p:m", "q:n"]}})
add("_slot_labels", slots=[{"provider": "p", "model": "m"}, {"provider": "q", "model": "n"},
                           {"provider": "r", "model": "s"}],
    io={"_slot_label": {"seq": ["p:m", "q:n", "r:s"]}})
add("_slot_labels", slots=[{"provider": "p", "model": "m", "reasoning_effort": "high"}],
    io={"_slot_label": "p:m[reasoning=high]"})
add("_slot_labels", slots=[{"provider": "p", "model": "m"}], io={"_slot_label": RAISE})

# ── _agent_cache_opts ──
add("_agent_cache_opts", agent=None)
add("_agent_cache_opts", agent={})
add("_agent_cache_opts", agent={"_cache_disabled": True})
add("_agent_cache_opts", agent={"_cache_ttl": "1h"})
add("_agent_cache_opts", agent={"_cache_ttl_auto": 1})
add("_agent_cache_opts", agent={"_cache_disabled": False, "_cache_ttl": "5m", "_cache_ttl_auto": True})

# ── _with_cache_disabled ──
add("_with_cache_disabled", runtime={"provider": "p"}, cache_disabled=None)
add("_with_cache_disabled", runtime={"provider": "p"}, cache_disabled=False)
add("_with_cache_disabled", runtime={"provider": "p"}, cache_disabled=True)
add("_with_cache_disabled", runtime={"provider": "p"}, cache_disabled="x")
add("_with_cache_disabled", runtime={"provider": "p", "_cache_disabled": False}, cache_disabled=True)
add("_with_cache_disabled", runtime={}, cache_disabled=True)

# ── _hash_messages ──
add("_hash_messages", msgs=[])
add("_hash_messages", msgs=[{"role": "user", "content": "hi"}])
add("_hash_messages", msgs=[{"role": "user", "content": "hi"}, {"role": "assistant", "content": "yo"}])
add("_hash_messages", msgs=[{"role": "user", "content": None}])
add("_hash_messages", msgs=[{"role": None, "content": "x"}])
add("_hash_messages", msgs=[{"role": "user", "content": 42}])

# ── _tool_activity_since_last_user ──
add("_tool_activity_since_last_user", messages=[])
add("_tool_activity_since_last_user", messages=[{"role": "user", "content": "hi"}])
add("_tool_activity_since_last_user", messages=[{"role": "tool", "content": "r"}])
add("_tool_activity_since_last_user", messages=[{"role": "assistant", "tool_calls": [{"id": 1}]}])
add("_tool_activity_since_last_user", messages=[{"role": "assistant", "content": "no calls"}])
add("_tool_activity_since_last_user", messages=[{"role": "user", "content": "hi"}, {"role": "tool", "content": "r"}])
add("_tool_activity_since_last_user", messages=[{"role": "assistant", "tool_calls": [{"id": 1}]}, {"role": "user", "content": "hi"}])

# ── _placeholder_output ──
add("_placeholder_output", slot={"provider": "p", "model": "m"}, note="[skipped: x]", io={"_slot_label": "p:m"})
add("_placeholder_output", slot={"provider": "p", "model": "m"}, note="[skipped: interrupted by user]", io={"_slot_label": "p:m"})
add("_placeholder_output", slot={"provider": "p", "model": "m", "reasoning_effort": "high"}, note="n", io={"_slot_label": "p:m[reasoning=high]"})
add("_placeholder_output", slot={}, note="n", io={"_slot_label": ":"})
add("_placeholder_output", slot={"provider": "p", "model": "m"}, note="n", io={"_slot_label": RAISE})

# ── _join_reference_outputs ──
add("_join_reference_outputs", outputs=[], degraded="")
add("_join_reference_outputs", outputs=[], degraded="[degraded]")
add("_join_reference_outputs", outputs=[out()], degraded="")
add("_join_reference_outputs", outputs=[out("p:m", "a"), out("q:n", "b")], degraded="")
add("_join_reference_outputs", outputs=[out()], degraded="[degraded]")
add("_join_reference_outputs", outputs=[out("p:m", "a"), out("q:n", "b")], degraded="[degraded]")
add("_join_reference_outputs", outputs=[out("p:m", "")], degraded="")

# ── _guidance_inputs ──
add("_guidance_inputs", reference_outputs=[out()], privacy_full=False, policy="loud",
    io={"_is_failed_reference": {"seq": [False, False]}, "_degraded_notice": ""})
add("_guidance_inputs", reference_outputs=[out(), out("q:n", "[failed: boom]")], privacy_full=False, policy="loud",
    io={"_is_failed_reference": {"seq": [False, True, False, True]}, "_degraded_notice": "[Reference models unavailable: q:n]"})
add("_guidance_inputs", reference_outputs=[out(), out("q:n", "[failed: boom]")], privacy_full=False, policy="silent",
    io={"_is_failed_reference": {"seq": [False, True, False, True]}, "_degraded_notice": ""})
add("_guidance_inputs", reference_outputs=[out("p:m", "[failed: a]"), out("q:n", "[skipped: b]")], privacy_full=False, policy="loud",
    io={"_is_failed_reference": {"seq": [True, True, True, True]}, "_degraded_notice": "[Reference models unavailable: p:m, q:n]"})
add("_guidance_inputs", reference_outputs=[out()], privacy_full=True, policy="loud",
    io={"_is_failed_reference": {"seq": [False, False]}, "_redact_reference_outputs": [out("p:m", "[redacted]")], "_degraded_notice": ""})
add("_guidance_inputs", reference_outputs=[], privacy_full=False, policy="loud",
    io={"_degraded_notice": ""})
add("_guidance_inputs", reference_outputs=[out()], privacy_full=False, policy="loud",
    io={"_is_failed_reference": RAISE})

# ── _sum_reference_accounting ──
add("_sum_reference_accounting", outputs=[])
add("_sum_reference_accounting", outputs=[out()])
add("_sum_reference_accounting", outputs=[out(), out("q:n", "y", acct(usage(20, 10, rc=2), cost=1.5))])
add("_sum_reference_accounting", outputs=[["p:m", "x", {"_type": "other"}]])
add("_sum_reference_accounting", outputs=[out(a=acct(u={"raw": 1}, cost=0.25))])
add("_sum_reference_accounting", outputs=[out(a=acct(cost=None))])
add("_sum_reference_accounting", outputs=[out(), ["q:n", "y", {"_type": "other"}], out("r:s", "z", acct(usage(1, 1, rc=1), cost=0.1))])

# ── _fold_pending_accounting ──
add("_fold_pending_accounting", usage=usage(), cost=0.5, state={"_pending_reference_usage": usage(), "_pending_reference_cost": None})
add("_fold_pending_accounting", usage=usage(), cost=0.5, state={"_pending_reference_usage": None, "_pending_reference_cost": None})
add("_fold_pending_accounting", usage=usage(), cost=None, state={"_pending_reference_usage": usage(), "_pending_reference_cost": None})
add("_fold_pending_accounting", usage=usage(), cost=0.5, state={"_pending_reference_usage": usage(), "_pending_reference_cost": 1.0})
add("_fold_pending_accounting", usage=usage(), cost=0.5, state={"_pending_reference_usage": usage(), "_pending_reference_cost": 0})
add("_fold_pending_accounting", usage=usage(1, 1, rc=3), cost=2.0, state={"_pending_reference_usage": usage(2, 2, rc=2), "_pending_reference_cost": 0.5})

# ── _reference_context_length ──
add("_reference_context_length", slot={"model": "m"}, runtime={"provider": "p"}, state={"ctx_cache": {}}, io={"get_model_context_length": 128000})
add("_reference_context_length", slot={"model": "m"}, runtime={"provider": "p"}, state={"ctx_cache": {"p\tm": 64000}}, io={})
add("_reference_context_length", slot={"model": "m"}, runtime={"provider": "p"}, state={"ctx_cache": {}}, io={"get_model_context_length": RAISE})
add("_reference_context_length", slot={"model": "m"}, runtime={"provider": "p"}, state={"ctx_cache": None}, io={"get_model_context_length": 32000})
add("_reference_context_length", slot={"model": "m", "provider": "sp"}, runtime={}, state={"ctx_cache": {}}, io={"get_model_context_length": 8000})
add("_reference_context_length", slot={"model": "m"}, runtime={"provider": "p"}, state={"ctx_cache": {"p\tm": None}}, io={})
add("_reference_context_length", slot={"model": "m"}, runtime={"provider": "p"}, state={"ctx_cache": {}}, io={"get_model_context_length": CANCEL})

# ── _price_reference_response ──
add("_price_reference_response", response_usage={"input_tokens": 3}, slot={"model": "m"}, runtime={"provider": "p", "api_mode": "chat"},
    io={"normalize_usage": usage(), "estimate_usage_cost": {"amount_usd": 0.1, "status": "estimated", "source": "none"}})
add("_price_reference_response", response_usage=None, slot={"model": "m"}, runtime={"provider": "p"},
    io={"estimate_usage_cost": {"amount_usd": 0.0, "status": "included", "source": "none"}})
add("_price_reference_response", response_usage={"input_tokens": 3}, slot={"model": "m"}, runtime={"provider": "p"},
    io={"normalize_usage": RAISE, "estimate_usage_cost": {"amount_usd": 0.2, "status": "actual", "source": "none"}})
add("_price_reference_response", response_usage={"input_tokens": 3}, slot={"model": "m"}, runtime={"provider": "p"},
    io={"normalize_usage": usage(), "estimate_usage_cost": RAISE})
add("_price_reference_response", response_usage={"input_tokens": 3}, slot={"model": "m"}, runtime={"provider": "p"},
    io={"normalize_usage": CANCEL})
add("_price_reference_response", response_usage={"input_tokens": 3}, slot={"model": "m"}, runtime={"provider": "p"},
    io={"normalize_usage": usage(), "estimate_usage_cost": CANCEL})
add("_price_reference_response", response_usage={}, slot={"model": "m"}, runtime={"provider": "p"},
    io={"estimate_usage_cost": {"amount_usd": 0.3, "status": "actual", "source": "none"}})
add("_price_reference_response", response_usage={"input_tokens": 3}, slot={}, runtime={},
    io={"normalize_usage": usage(), "estimate_usage_cost": {"amount_usd": 0.4, "status": "estimated", "source": "none"}})

# ── _record_late ──
add("_record_late", label="p:m", future_idx=0, io={"future.result": out(), "sink": None})
add("_record_late", label="p:m", future_idx=0, io={"future.result": RAISE})
add("_record_late", label="p:m", future_idx=0, io={"future.result": out(), "sink": RAISE})
add("_record_late", label="p:m", future_idx=0, io={"future.result": CANCEL})
add("_record_late", label="p:m", future_idx=0, io={"future.result": out(), "sink": CANCEL})
add("_record_late", label="q:n", future_idx=1, io={"future.result": out("q:n", "y"), "sink": "ok"})

# ── _settle_interrupted ──
add("_settle_interrupted", futures=[{"idx": 0}], reference_models=[{"provider": "p", "model": "m"}], sink_present=True,
    state={"results": [out()]}, io={})
add("_settle_interrupted", futures=[{"idx": 0}], reference_models=[{"provider": "p", "model": "m"}], sink_present=True,
    state={"results": [None]}, io={"future.cancel": True, "_placeholder_output": out("p:m", "[skipped: interrupted by user]")})
add("_settle_interrupted", futures=[{"idx": 0}], reference_models=[{"provider": "p", "model": "m"}], sink_present=True,
    state={"results": [None]}, io={"future.cancel": False, "future.done": True, "future.result": out()})
add("_settle_interrupted", futures=[{"idx": 0}], reference_models=[{"provider": "p", "model": "m"}], sink_present=True,
    state={"results": [None]}, io={"future.cancel": False, "future.done": False, "_placeholder_output": out("p:m", "[skipped: interrupted by user]")})
add("_settle_interrupted", futures=[{"idx": 0}], reference_models=[{"provider": "p", "model": "m"}], sink_present=False,
    state={"results": [None]}, io={"future.cancel": False, "future.done": False, "_placeholder_output": out("p:m", "[skipped: interrupted by user]")})
add("_settle_interrupted", futures=[{"idx": 0}, {"idx": 1}], reference_models=[{"provider": "p", "model": "m"}, {"provider": "q", "model": "n"}], sink_present=True,
    state={"results": [None, None]}, io={"future.cancel": {"seq": [False, True]}, "future.done": {"seq": [True]}, "future.result": out(), "_placeholder_output": out("q:n", "[skipped: interrupted by user]")})
add("_settle_interrupted", futures=[{"idx": 0}], reference_models=[{"provider": "p", "model": "m"}], sink_present=True,
    state={"results": [None]}, io={"future.cancel": RAISE})
add("_settle_interrupted", futures=[{"idx": 0}], reference_models=[{"provider": "p", "model": "m"}], sink_present=True,
    state={"results": [None]}, io={"future.cancel": False, "future.done": True, "future.result": RAISE})

# ── _fanout_cache_key ──
add("_fanout_cache_key", preset={"fanout": "user_turn"}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], state=fanout_state(), io={"_hash_messages": "H1", "_slot_label": "p:m"})
ADV = ("[The conversation above is the current state of the task. Give your "
       "most intelligent judgement: what is going on, what should happen next, "
       "what risks or mistakes you see, and how the acting agent should "
       "proceed.]")
add("_fanout_cache_key", preset={"fanout": "user_turn"},
    ref_messages=[{"role": "user", "content": "hi"}, {"role": "assistant", "content": "a"}, {"role": "user", "content": ADV}],
    reference_models=[{"provider": "p", "model": "m"}], state=fanout_state(), io={"_hash_messages": "H2", "_slot_label": "p:m"})
add("_fanout_cache_key", preset={"fanout": "per_iteration"}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], state=fanout_state(), io={"_hash_messages": "H3", "_slot_label": "p:m"})
add("_fanout_cache_key", preset={"fanout": "every_n:2"}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], state=fanout_state(),
    io={"_hash_messages": {"seq": ["T1", "S1", "H4"]}, "_slot_label": "p:m"})
add("_fanout_cache_key", preset={"fanout": "every_n:2"}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}],
    state=fanout_state(_fanout_turn_sig="T1", _fanout_iteration_count=1, _fanout_last_state_sig="S0",
                       _ref_cache_outputs=[out()], _ref_cache_key=["default", "OLD", ["p:m"]]),
    io={"_hash_messages": {"seq": ["T1", "S2"]}})
add("_fanout_cache_key", preset={"fanout": "every_n:3"}, ref_messages=[{"role": "user", "content": "new turn"}],
    reference_models=[{"provider": "p", "model": "m"}],
    state=fanout_state(_fanout_turn_sig="OLDT", _fanout_iteration_count=5, _fanout_last_state_sig="OLDS"),
    io={"_hash_messages": {"seq": ["T2", "S2", "H5"]}, "_slot_label": "p:m"})
add("_fanout_cache_key", preset={"fanout": "every_n:1"}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], state=fanout_state(), io={"_hash_messages": "H6", "_slot_label": "p:m"})
add("_fanout_cache_key", preset={"fanout": "every_n:abc"}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], state=fanout_state(), io={"_hash_messages": "H7", "_slot_label": "p:m"})
add("_fanout_cache_key", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], state=fanout_state(), io={"_hash_messages": "H8", "_slot_label": "p:m"})
add("_fanout_cache_key", preset={"fanout": "every_n:2"}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}],
    state=fanout_state(_fanout_turn_sig="T1", _fanout_iteration_count=2, _fanout_last_state_sig="S1",
                       _ref_cache_outputs=[out()], _ref_cache_key=["default", "OLD", ["p:m"]]),
    io={"_hash_messages": {"seq": ["T1", "S1"]}})

# ── _build_guidance ──
BG_STATE = {"preset_name": "default", "_privacy_mode": ""}
add("_build_guidance", reference_outputs=[out("p:m", "[failed: a]")], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="loud", stale=False, state=BG_STATE,
    io={"_guidance_inputs": [[], "[Reference models unavailable: p:m]", True], "_slot_label": "a:g"})
add("_build_guidance", reference_outputs=[out("p:m", "[failed: a]")], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="silent", stale=False, state=BG_STATE,
    io={"_guidance_inputs": [[], "", True], "_slot_label": "a:g"})
add("_build_guidance", reference_outputs=[out()], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="loud", stale=False, state=BG_STATE,
    io={"_guidance_inputs": [[out()], "", False], "_slot_label": "a:g", "_join_reference_outputs": "Reference 1 — p:m:\nhello"})
add("_build_guidance", reference_outputs=[out("p:m", "[failed: a]")], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="loud", stale=False, state=BG_STATE,
    io={"_guidance_inputs": [[], "[Reference models unavailable: p:m]", False], "_slot_label": "a:g", "_join_reference_outputs": "[Reference models unavailable: p:m]"})
add("_build_guidance", reference_outputs=[], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="loud", stale=False, state=BG_STATE,
    io={"_guidance_inputs": [[], "", False], "_slot_label": "a:g"})
add("_build_guidance", reference_outputs=[out()], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="loud", stale=True, state=BG_STATE,
    io={"_guidance_inputs": [[out()], "", False], "_slot_label": "a:g", "_join_reference_outputs": "Reference 1 — p:m:\nhello"})
add("_build_guidance", reference_outputs=[out()], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="loud", stale=False, state={"preset_name": "default", "_privacy_mode": "full"},
    io={"_guidance_inputs": [[out("p:m", "[redacted]")], "", False], "_slot_label": "a:g", "_join_reference_outputs": "Reference 1 — p:m:\n[redacted]"})
add("_build_guidance", reference_outputs=[out(), out("q:n", "y")], aggregator={"provider": "a", "model": "g"},
    degraded_reference_policy="loud", stale=False, state=BG_STATE,
    io={"_guidance_inputs": [[out(), out("q:n", "y")], "", False], "_slot_label": "a:g", "_join_reference_outputs": "Reference 1 — p:m:\nhello\n\nReference 2 — q:n:\ny"})

# ── _run_fanout ──
add("_run_fanout", preset={"reference_timeout": 30}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=0.7, cache_key=["default", "H", ["p:m"]], state=run_state(),
    io={"_preset_temperature": 0.5, "_run_references_parallel": {"outputs": [out()], "progress": [{"refs_done": 1, "refs_total": 1, "label": "p:m"}]},
        "_sum_reference_accounting": [usage(), 0.5], "_fold_pending_accounting": None, "slot_metrics": {"model": "m"}, "_slot_label": "a:g"})
add("_run_fanout", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}, {"provider": "q", "model": "n"}], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=None, cache_key=["default", "H", ["p:m", "q:n"]], state=run_state(),
    io={"_preset_temperature": None, "_run_references_parallel": {"outputs": [out(), out("q:n", "y")], "progress": [{"refs_done": 1, "refs_total": 2, "label": "p:m"}, {"refs_done": 2, "refs_total": 2, "label": "q:n"}]},
        "_sum_reference_accounting": [usage(), 1.0], "_fold_pending_accounting": None, "slot_metrics": {"seq": [{"model": "m"}, {"model": "n"}]}, "_slot_label": "a:g"})
add("_run_fanout", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=0.7, cache_key=["default", "H", ["p:m"]], state=run_state(_privacy_mode="display"),
    io={"_preset_temperature": 0.5, "_run_references_parallel": {"outputs": [out()], "progress": []},
        "_sum_reference_accounting": [usage(), 0.5], "_fold_pending_accounting": None,
        "_redact_reference_text": {"seq": ["[redacted trace]", "[redacted emit]"]}, "_redact_trace_accounting": acct(output="[redacted trace]"),
        "slot_metrics": {"model": "m"}, "_slot_label": "a:g"})
add("_run_fanout", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=0.7, cache_key=["default", "H", ["p:m"]], state=run_state(),
    io={"_preset_temperature": 0.5, "_run_references_parallel": {"outputs": [out("p:m", "[skipped: interrupted by user]")], "progress": []},
        "_sum_reference_accounting": [usage(), 0.0], "_fold_pending_accounting": None, "slot_metrics": {"model": "m"}, "_slot_label": "a:g"})
add("_run_fanout", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=0.7, cache_key=["default", "H", ["p:m"]], state=run_state(),
    io={"_preset_temperature": 0.5, "_run_references_parallel": {"outputs": [out()], "progress": []},
        "_sum_reference_accounting": [usage(), 0.5], "_fold_pending_accounting": None, "slot_metrics": RAISE, "_slot_label": "a:g"})
add("_run_fanout", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=0.7, cache_key=["default", "H", []], state=run_state(),
    io={"_preset_temperature": 0.5, "_run_references_parallel": {"outputs": [], "progress": []},
        "_sum_reference_accounting": [usage(), None], "_fold_pending_accounting": None})
add("_run_fanout", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=0.7, cache_key=["default", "H", ["p:m"]], state=run_state(),
    io={"_preset_temperature": RAISE})
add("_run_fanout", preset={}, ref_messages=[{"role": "user", "content": "hi"}],
    reference_models=[{"provider": "p", "model": "m"}, {"provider": "q", "model": "n"}], aggregator={"provider": "a", "model": "g"},
    aggregator_temperature=0.7, cache_key=["default", "H", ["p:m", "q:n"]], state=run_state(_privacy_mode="full"),
    io={"_preset_temperature": 0.5, "_run_references_parallel": {"outputs": [out(), out("q:n", "y")], "progress": []},
        "_sum_reference_accounting": [usage(), 1.0], "_fold_pending_accounting": None,
        "_redact_reference_text": {"seq": ["rt0", "rt1", "re0", "re1"]},
        "_redact_trace_accounting": {"seq": [acct(output="rt0"), acct(output="rt1")]},
        "slot_metrics": {"seq": [{"model": "m"}, {"model": "n"}]}, "_slot_label": "a:g"})

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print("total cases:", len(CASES))
for k in sorted(cnt):
    print("  %-32s %d" % (k, cnt[k]))
print("funcs:", len(cnt), "min cases:", min(cnt.values()))