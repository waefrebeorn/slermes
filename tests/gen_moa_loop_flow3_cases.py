#!/usr/bin/env python3
"""Generate tests/fixtures/moa_loop_flow3.jsonl — scripted-seam cases for the
agent/moa_loop.py REMAINING-unclaimed lifecycle FLOW port
(src/agent/moa_loop_flow3.c).

>= 5 cases per ported func (15 funcs). Each case is {"func", "in": {...}} with
the seam scripts nested at in.io (absent/null -> None; {"seq": [...]} per-call
queue, None when exhausted; any other value reused). Raise scripts are
{"__raise__": cls, "msg": m}; BaseException-only kinds (SystemExit /
KeyboardInterrupt / GeneratorExit / CancelledError) probe passthrough past
`except Exception` / suppress(Exception) sites. Every flow has a fail-closed
case (missing script -> None -> the live None-deref / unpack / binding path).

Domain (ASCII scope): str() operands are scalars; messages / agg_messages are
lists of dicts; reference_outputs rows are [label, text, acct] triples;
preset / agg_runtime / prepared / api_kwargs / pending are plain dicts or
null; _pending_reference_usage is null | CanonicalUsage tag | plain dict;
_ref_cache_key null | array; _ref_cache_outputs null | array;
_merge_same_role_destinations null | array of [route, model] arrays;
destination scripts are arrays (tuples live) or hashable scalars (a dict
script probes the py3.14 unhashable-set-element raise);
_sum_reference_accounting scripts are arrays | null | int (the * unpack
domain); agg_runtime keys stay disjoint from the partial's explicit kwargs
except the documented duplicate-key probes.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "moa_loop_flow3.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_spec(cls, msg):
    return {"__raise__": cls, "msg": msg}


def attr(**fields):
    return {"__attr__": fields}


def call(seam):
    return {"__call__": seam}


def acct(usage=None, cost_usd=None, cost_status=None, cost_source=None,
         messages=None, output=None, model=None, provider=None, temperature=None):
    return {"_type": "_RefAccounting", "usage": usage, "cost_usd": cost_usd,
            "cost_status": cost_status, "cost_source": cost_source,
            "messages": messages, "output": output, "model": model,
            "provider": provider, "temperature": temperature}


def cu(**fields):
    return {"_type": "CanonicalUsage", **fields}


MCC = "MoAChatCompletions."
CLI = "MoAClient."

# Shared script bundles -------------------------------------------------------
PRESET = {
    "enabled": True,
    "reference_models": [{"provider": "p1", "model": "m1"},
                         {"provider": "p2", "model": "m2", "enabled": False}],
    "aggregator": {"provider": "p", "model": "m"},
    "degraded_reference_policy": "loud",
}
RT = {"provider": "p", "model": "m", "base_url": "https://u",
      "api_mode": "chat_completions"}
MSGS = [{"role": "user", "content": "hi"}]
REF_OUT = [["p1:m1", "advice", acct(usage={"input_tokens": 3}, cost_usd=0.01)]]
KEY = ["default", "h", ["p1:m1"]]


def create_io(**over):
    io = {
        "_resolve_preset_cached": [PRESET, {"privacy_filter": "full"}],
        "_moa_privacy_mode": "full",
        "_preset_temperature": 0.5,
        "_reference_messages": MSGS,
        "_fanout_cache_key": KEY,
        "_run_fanout": REF_OUT,
        "_build_guidance": "GUIDANCE",
        "_attach_reference_guidance": None,
        "_slot_runtime": RT,
        "_plan_aggregator_cache": [MSGS, None],
        "_merge_slot_extra_body": None,
        "_aggregator_reasoning_config": None,
        "destination_key": ["https://u", "m"],
        "attempt_facts": {"clock": 1},
        "call_llm": "resp",
        "_slot_label": "p:m",
        "_extract_text": "resp",
    }
    io.update(over)
    return io


def create_state(**over):
    st = {"preset_name": "default", "_ref_cache_key": None,
          "_ref_cache_outputs": [], "_pending_trace": None}
    st.update(over)
    return st


def prepared(**over):
    p = {"aggregator": {"provider": "p", "model": "m"}, "messages": MSGS,
         "guidance": "G", "aggregator_temperature": 0.5}
    p.update(over)
    return p


def call_io(**over):
    io = {
        "_slot_runtime": RT,
        "_plan_aggregator_cache": [MSGS, None],
        "_merge_slot_extra_body": None,
        "_aggregator_reasoning_config": None,
        "destination_key": ["https://u", "m"],
        "attempt_facts": {"clock": 1},
        "call_llm": "resp",
        "_slot_label": "p:m",
        "_extract_text": "resp",
    }
    io.update(over)
    return io


# ── 1. MoAChatCompletions.__init__ ──
case(MCC + "__init__", preset_name="alpha", reference_callback=call("reference_callback"),
     agent=attr(provider="moa", model="alpha"))
case(MCC + "__init__", preset_name="", reference_callback=None, agent=None)
case(MCC + "__init__", preset_name=None)
case(MCC + "__init__", preset_name=0)
case(MCC + "__init__", preset_name=5)
case(MCC + "__init__", preset_name="x", reference_callback=None, agent=None)

# ── 2. MoAChatCompletions.consume_reference_usage ──
case(MCC + "consume_reference_usage",
     state={"_pending_reference_usage": cu(input_tokens=10, output_tokens=5),
            "_pending_reference_cost": 0.25})
case(MCC + "consume_reference_usage",
     state={"_pending_reference_usage": None, "_pending_reference_cost": None})
case(MCC + "consume_reference_usage",
     state={"_pending_reference_usage": cu(), "_pending_reference_cost": None})
case(MCC + "consume_reference_usage",
     state={"_pending_reference_usage": {}, "_pending_reference_cost": 0})
case(MCC + "consume_reference_usage",
     state={"_pending_reference_usage": {"input_tokens": 3},
            "_pending_reference_cost": None})
case(MCC + "consume_reference_usage", state={"_pending_reference_cost": 1})
case(MCC + "consume_reference_usage", state={"_pending_reference_usage": cu()})
case(MCC + "consume_reference_usage",
     state={"_pending_reference_usage": None, "_pending_reference_cost": 0})

# ── 3. MoAChatCompletions.last_reference_metrics ──
case(MCC + "last_reference_metrics",
     state={"_last_reference_metrics": [{"label": "p1:m1", "tokens": 3}]})
case(MCC + "last_reference_metrics", state={"_last_reference_metrics": None})
case(MCC + "last_reference_metrics", state={})
case(MCC + "last_reference_metrics", state={"_last_reference_metrics": "x"})
case(MCC + "last_reference_metrics", state={"_last_reference_metrics": []})

# ── 4. MoAChatCompletions._record_late_reference_accounting ──
case(MCC + "_record_late_reference_accounting", label="p1:m1",
     accounting=acct(usage={"input_tokens": 2}, cost_usd=0.02),
     io={"_sum_reference_accounting": [{"input_tokens": 2}, 0.02],
         "_fold_pending_accounting": None})
case(MCC + "_record_late_reference_accounting", label="x", accounting=None)
case(MCC + "_record_late_reference_accounting", label="x",
     accounting={"usage": 1})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(), io={"_sum_reference_accounting": None})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(), io={"_sum_reference_accounting": [1]})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(), io={"_sum_reference_accounting": []})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(), io={"_sum_reference_accounting": [1, 2, 3]})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(),
     io={"_sum_reference_accounting": raise_spec("RuntimeError", "sum boom")})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(),
     io={"_sum_reference_accounting": raise_spec("SystemExit", "bye")})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(),
     io={"_sum_reference_accounting": [None, None],
         "_fold_pending_accounting": raise_spec("RuntimeError", "fold boom")})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(), io={"_sum_reference_accounting": [None, None]})
case(MCC + "_record_late_reference_accounting", label="x",
     accounting=acct(), io={"_sum_reference_accounting": 5})

# ── 5. MoAChatCompletions.consume_and_save_trace ──
PENDING_FULL = {
    "aggregator_input_messages": MSGS,
    "aggregator_slot": {"provider": "p", "model": "m"},
    "aggregator_output": "txt", "preset": "alpha", "reference_outputs": [],
    "aggregator_label": "p:m", "aggregator_temperature": 0.5,
    "aggregator_streamed": True,
}
case(MCC + "consume_and_save_trace", session_id="s1",
     state={"_pending_trace": PENDING_FULL}, io={"save_moa_turn": None})
case(MCC + "consume_and_save_trace", state={"_pending_trace": None})
case(MCC + "consume_and_save_trace", state={"_pending_trace": {}})
case(MCC + "consume_and_save_trace",
     state={"_pending_trace": {"preset": "alpha"}})
case(MCC + "consume_and_save_trace",
     state={"_pending_trace": {"aggregator_input_messages": None}},
     io={"save_moa_turn": None})
case(MCC + "consume_and_save_trace", aggregator_output_fallback="fb",
     state={"_pending_trace": {"aggregator_input_messages": MSGS,
                               "aggregator_output": None}},
     io={"save_moa_turn": None})
case(MCC + "consume_and_save_trace", aggregator_output_fallback="",
     state={"_pending_trace": {"aggregator_input_messages": MSGS}},
     io={"save_moa_turn": None})
case(MCC + "consume_and_save_trace", aggregator_output_fallback="fb",
     state={"_pending_trace": {"aggregator_input_messages": MSGS,
                               "aggregator_output": "inline"}},
     io={"save_moa_turn": None})
case(MCC + "consume_and_save_trace",
     state={"_pending_trace": PENDING_FULL},
     io={"save_moa_turn": raise_spec("RuntimeError", "disk boom")})
case(MCC + "consume_and_save_trace",
     state={"_pending_trace": PENDING_FULL},
     io={"save_moa_turn": raise_spec("SystemExit", "bye")})
case(MCC + "consume_and_save_trace", state={"_pending_trace": 5})
case(MCC + "consume_and_save_trace",
     state={"_pending_trace": ["aggregator_input_messages"]})
case(MCC + "consume_and_save_trace", state={})
case(MCC + "consume_and_save_trace",
     state={"_pending_trace": {"aggregator_input_messages": MSGS,
                               "aggregator_slot": "x"}},
     io={"save_moa_turn": None})
case(MCC + "consume_and_save_trace",
     state={"_pending_trace": "aggregator_input_messages"})

# ── 6. MoAChatCompletions._emit ──
case(MCC + "_emit", event="moa.reference", kwargs={"label": "a", "text": "t"},
     state={"reference_callback": call("reference_callback")},
     io={"reference_callback": None})
case(MCC + "_emit", event="moa.phase", kwargs={},
     state={"reference_callback": None})
case(MCC + "_emit", event="x", state={})
case(MCC + "_emit", event="x", state={"reference_callback": "notcallable"})
case(MCC + "_emit", event="x",
     state={"reference_callback": call("reference_callback")},
     io={"reference_callback": raise_spec("RuntimeError", "cb boom")})
case(MCC + "_emit", event="x",
     state={"reference_callback": call("reference_callback")},
     io={"reference_callback": raise_spec("KeyboardInterrupt", "stop")})
case(MCC + "_emit", event="x",
     state={"reference_callback": call("reference_callback")},
     io={"reference_callback": raise_spec("SystemExit", "bye")})
case(MCC + "_emit", event="moa.progress", kwargs={"refs_done": 1},
     state={"reference_callback": call("reference_callback")},
     io={"reference_callback": None})

# ── 7. MoAChatCompletions.prepare ──
case(MCC + "prepare", messages=MSGS, state=create_state(), io=create_io())
case(MCC + "prepare", messages=MSGS, state=create_state(),
     io=create_io(_resolve_preset_cached=[
         {**PRESET, "enabled": False}, {}]))
case(MCC + "prepare", messages=MSGS,
     state=create_state(_ref_cache_key=KEY, _ref_cache_outputs=REF_OUT),
     io=create_io(_tool_activity_since_last_user=True))
case(MCC + "prepare", messages=MSGS, state=create_state(),
     io=create_io(_resolve_preset_cached=raise_spec("RuntimeError", "cfg boom")))
case(MCC + "prepare", messages=MSGS, state=create_state(),
     io=create_io(_resolve_preset_cached=None))
case(MCC + "prepare", messages=MSGS, state=create_state(),
     io=create_io(_build_guidance=None))
case(MCC + "prepare", messages=None, state=create_state(), io=create_io())
case(MCC + "prepare", messages=MSGS, state=create_state(),
     io=create_io(_resolve_preset_cached=raise_spec("SystemExit", "bye")))

# ── 8. MoAChatCompletions.rebase_prepared_request ──
case(MCC + "rebase_prepared_request", prepared={"guidance": "G", "x": 1},
     messages=MSGS, io={"_attach_reference_guidance": None})
case(MCC + "rebase_prepared_request", prepared={"guidance": None}, messages=MSGS)
case(MCC + "rebase_prepared_request", prepared={"guidance": ""}, messages=MSGS)
case(MCC + "rebase_prepared_request", prepared={"guidance": 5}, messages=MSGS,
     io={"_attach_reference_guidance": None})
case(MCC + "rebase_prepared_request", prepared=None, messages=MSGS)
case(MCC + "rebase_prepared_request", prepared={"guidance": "G"}, messages=None)
case(MCC + "rebase_prepared_request", prepared={"guidance": "G"}, messages=[5])
case(MCC + "rebase_prepared_request", prepared={"guidance": "G"}, messages=MSGS,
     io={"_attach_reference_guidance": raise_spec("RuntimeError", "attach boom")})
case(MCC + "rebase_prepared_request", prepared={"guidance": "G"}, messages=MSGS,
     io={"_attach_reference_guidance": raise_spec("SystemExit", "bye")})

# ── 9. MoAChatCompletions.create ──
case(MCC + "create", api_kwargs={"messages": MSGS}, state=create_state(),
     io=create_io())
case(MCC + "create", api_kwargs={"messages": MSGS,
                                 "_moa_prepared_request": prepared()},
     state=create_state(_pending_trace={}), io=call_io())
case(MCC + "create", api_kwargs={"messages": MSGS, "_moa_prepared_request": 5},
     state=create_state())
case(MCC + "create", api_kwargs={"messages": MSGS, "_moa_prepared_request": None},
     state=create_state(), io=create_io())
case(MCC + "create", api_kwargs={"messages": MSGS},
     state=create_state(_ref_cache_key=KEY, _ref_cache_outputs=REF_OUT),
     io=create_io(_tool_activity_since_last_user=False))
case(MCC + "create", api_kwargs={"messages": MSGS}, state=create_state(),
     io=create_io(_resolve_preset_cached=raise_spec("SystemExit", "bye")))
case(MCC + "create", api_kwargs={"messages": MSGS}, state=create_state(),
     io=create_io(_moa_privacy_mode=raise_spec("RuntimeError", "priv boom")))
case(MCC + "create", api_kwargs={"messages": MSGS, "temperature": 0.7},
     state=create_state(), io=create_io(_preset_temperature=None))
case(MCC + "create", api_kwargs={"messages": None}, state=create_state(),
     io=create_io())
case(MCC + "create", api_kwargs={"messages": MSGS}, state=create_state(),
     io=create_io(_build_guidance=raise_spec("RuntimeError", "guide boom")))
case(MCC + "create", api_kwargs={"messages": MSGS},
     state=create_state(_pending_trace={}), io=create_io())
case(MCC + "create", api_kwargs={"messages": MSGS}, state=create_state(),
     io=create_io(_resolve_preset_cached=[{"enabled": True,
                                          "reference_models": [5],
                                          "aggregator": {}}, {}]))

# ── 10. MoAChatCompletions._call_prepared_aggregator ──
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None}, io=call_io())
case(MCC + "_call_prepared_aggregator",
     prepared=prepared(aggregator={"provider": "moa", "model": "x"}),
     api_kwargs={}, state={"_pending_trace": None})
case(MCC + "_call_prepared_aggregator", prepared={"messages": MSGS},
     api_kwargs={}, state={"_pending_trace": None})
case(MCC + "_call_prepared_aggregator", prepared=None, api_kwargs={},
     state={"_pending_trace": None})
case(MCC + "_call_prepared_aggregator", prepared=prepared(aggregator=None),
     api_kwargs={}, state={"_pending_trace": None})
case(MCC + "_call_prepared_aggregator", prepared=prepared(),
     api_kwargs={"stream": True}, state={"_pending_trace": None},
     io=call_io(call_llm=attr(choices=[{"delta": "x"}]),
                _completed_response_as_stream_chunk={"delta": "x"}))
case(MCC + "_call_prepared_aggregator", prepared=prepared(),
     api_kwargs={"stream": True}, state={"_pending_trace": None},
     io=call_io(call_llm={"plain": "dict"}))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": {}, "_privacy_mode": "full"},
     io=call_io(_redact_trace_messages=[{"role": "user", "content": "[r]"}]))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None},
     io=call_io(call_llm={"seq": [raise_spec("RuntimeError", "alternation rejected"),
                                  "resp2"]},
                merge_same_role_messages=[{"role": "user", "content": "merged"}],
                is_role_alternation_rejection=True,
                attempt_facts={"seq": [{"clock": 1}, {"clock": 2}]}))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None},
     io=call_io(call_llm=raise_spec("RuntimeError", "alternation rejected"),
                merge_same_role_messages={"__echo__": "messages"}))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None},
     io=call_io(call_llm=raise_spec("RuntimeError", "alternation rejected")))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None},
     io=call_io(call_llm=raise_spec("RuntimeError", "alternation rejected"),
                merge_same_role_messages=[{"role": "user", "content": "merged"}],
                is_role_alternation_rejection=False))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None},
     io=call_io(call_llm=raise_spec("SystemExit", "bye")))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None,
            "_merge_same_role_destinations": [["https://u", "m"]]},
     io=call_io(call_llm=raise_spec("RuntimeError", "alternation rejected"),
                merge_same_role_messages=[{"role": "user", "content": "merged"}]))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": {}},
     io=call_io(_extract_text=raise_spec("RuntimeError", "extract boom")))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": {}},
     io=call_io(_extract_text=raise_spec("SystemExit", "bye")))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None}, io=call_io(destination_key={}))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None},
     io=call_io(_slot_runtime={**RT, "temperature": 0.9}))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": None}, io=call_io(_slot_runtime=None))
case(MCC + "_call_prepared_aggregator", prepared=prepared(), api_kwargs={},
     state={"_pending_trace": [1]}, io=call_io())
case(MCC + "_call_prepared_aggregator", prepared=prepared(),
     api_kwargs={"stream": True, "stream_options": {"include_usage": False},
                 "timeout": 30},
     state={"_pending_trace": None}, io=call_io(call_llm="streamresp"))
case(MCC + "_call_prepared_aggregator", prepared=prepared(guidance=None),
     api_kwargs={"tools": [{"type": "function"}], "extra_body": {"x": 1}},
     state={"_pending_trace": None},
     io=call_io(_merge_slot_extra_body={"x": 1, "y": 2}))

# ── 11. MoAClient.__init__ ──
case(CLI + "__init__", preset_name="p", reference_callback=call("reference_callback"),
     agent=attr(provider="moa"))
case(CLI + "__init__", preset_name=None)
case(CLI + "__init__", preset_name="")
case(CLI + "__init__", preset_name="x", reference_callback=None, agent=None)
case(CLI + "__init__", preset_name=0)

# ── 12. MoAClient.consume_reference_usage ──
case(CLI + "consume_reference_usage",
     state={"chat": {"completions": {
         "_pending_reference_usage": cu(input_tokens=4),
         "_pending_reference_cost": 0.1}}})
case(CLI + "consume_reference_usage",
     state={"chat": {"completions": {
         "_pending_reference_usage": None, "_pending_reference_cost": None}}})
case(CLI + "consume_reference_usage", state={"chat": {"completions": None}})
case(CLI + "consume_reference_usage", state={"chat": {}})
case(CLI + "consume_reference_usage", state={})
case(CLI + "consume_reference_usage", state={"chat": None})
case(CLI + "consume_reference_usage", state={"chat": {"completions": "x"}})

# ── 13. MoAClient.last_aggregator_slot ──
case(CLI + "last_aggregator_slot",
     state={"chat": {"completions": {
         "last_aggregator_slot": {"provider": "p", "model": "m"}}}})
case(CLI + "last_aggregator_slot", state={"chat": {"completions": {}}})
case(CLI + "last_aggregator_slot", state={"chat": {"completions": None}})
case(CLI + "last_aggregator_slot", state={"chat": {"completions": "x"}})
case(CLI + "last_aggregator_slot", state={})
case(CLI + "last_aggregator_slot", state={"chat": {}})
case(CLI + "last_aggregator_slot",
     state={"chat": {"completions": {"last_aggregator_slot": None}}})

# ── 14. MoAClient.consume_and_save_trace ──
case(CLI + "consume_and_save_trace", session_id="s1",
     state={"chat": {"completions": {"_pending_trace": PENDING_FULL}}},
     io={"save_moa_turn": None})
case(CLI + "consume_and_save_trace",
     state={"chat": {"completions": {"_pending_trace": None}}})
case(CLI + "consume_and_save_trace",
     state={"chat": {"completions": {"_pending_trace": PENDING_FULL}}},
     io={"save_moa_turn": raise_spec("RuntimeError", "disk boom")})
case(CLI + "consume_and_save_trace",
     state={"chat": {"completions": {"_pending_trace": PENDING_FULL}}},
     io={"save_moa_turn": raise_spec("SystemExit", "bye")})
case(CLI + "consume_and_save_trace", state={})
case(CLI + "consume_and_save_trace", state={"chat": {"completions": None}})
case(CLI + "consume_and_save_trace", aggregator_output_fallback="fb",
     state={"chat": {"completions": {"_pending_trace": {
         "aggregator_input_messages": MSGS, "aggregator_output": None}}}},
     io={"save_moa_turn": None})

# ── 15. MoAClient.last_reference_metrics ──
case(CLI + "last_reference_metrics",
     state={"chat": {"completions": {"_last_reference_metrics": [{"label": "a"}]}}})
case(CLI + "last_reference_metrics",
     state={"chat": {"completions": {"_last_reference_metrics": None}}})
case(CLI + "last_reference_metrics", state={"chat": {"completions": {}}})
case(CLI + "last_reference_metrics", state={"chat": {"completions": None}})
case(CLI + "last_reference_metrics", state={"chat": None})
case(CLI + "last_reference_metrics", state={})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    want = ({"MoAChatCompletions." + n for n in (
        "__init__", "consume_reference_usage", "last_reference_metrics",
        "_record_late_reference_accounting", "consume_and_save_trace", "_emit",
        "prepare", "rebase_prepared_request", "create",
        "_call_prepared_aggregator")}
        | {"MoAClient." + n for n in (
            "__init__", "consume_reference_usage", "last_aggregator_slot",
            "consume_and_save_trace", "last_reference_metrics")})
    assert set(counts) == want, (sorted(want - set(counts)),
                                 sorted(set(counts) - want))
    short = {f: n for f, n in counts.items() if n < 5}
    assert not short, "fewer than 5 cases: %s" % short
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %s: %d cases across %d funcs (min %d/func)"
          % (OUT, len(CASES), len(counts), min(counts.values())))


if __name__ == "__main__":
    main()
