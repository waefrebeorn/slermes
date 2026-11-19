#!/usr/bin/env python3
"""Generate tests/fixtures/moa_loop_flow2.jsonl — scripted-seam cases for the
agent/moa_loop.py UNCLAIMED-remainder FLOW port (src/agent/moa_loop_flow2.c).

>= 5 cases per ported func (20 funcs). Each case is {"func", "in": {...}} with
the seam scripts nested at in.io (absent/null -> None; {"seq": [...]} per-call
queue, None when exhausted; any other value reused). Raise scripts are
{"__raise__": cls, "msg": m}; BaseException-only kinds (SystemExit /
KeyboardInterrupt / GeneratorExit / CancelledError) probe passthrough past
`except Exception` / suppress(Exception) sites. Every flow has a fail-closed
case (missing script -> None -> the live None-deref / unpack path).

Domain (ASCII scope): numbers are ints or non-integral floats (no integral
floats in str()-computed positions); str() operands are scalars; unpack
elements are lists; relay kwargs is always a mapping (live **kwargs binding);
preset-cache stamps are null|str|int; labels are str.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "moa_loop_flow2.jsonl")

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


RST = "redact_sensitive_text"

# ── 1. _redact_reference_text ──
case("_redact_reference_text", text="mail a.b+x@example.co.uk end",
     io={RST: "mail a.b+x@example.co.uk end"})
case("_redact_reference_text", text="call (555) 123-4567 now",
     io={RST: "call (555) 123-4567 now"})
case("_redact_reference_text",
     text="nums 555-123-4567 555.123.4567 +1 555-123-4567 1-555-123-4567",
     io={RST: "nums 555-123-4567 555.123.4567 +1 555-123-4567 1-555-123-4567"})
case("_redact_reference_text",
     text="keep 5551234567 2026-07-12 192.168.1.1 555-123-4567x x@y.z",
     io={RST: "keep 5551234567 2026-07-12 192.168.1.1 555-123-4567x x@y.z"})
case("_redact_reference_text", text=5)
case("_redact_reference_text", text="")
case("_redact_reference_text", text=None)
case("_redact_reference_text", text="secret", io={RST: raise_spec("RuntimeError", "boom")})
case("_redact_reference_text", text="secret", io={RST: raise_spec("SystemExit", "bye")})
case("_redact_reference_text", text="secret", io={RST: 5})
case("_redact_reference_text", text="secret")   # fail-closed: None -> TypeError
case("_redact_reference_text", text="a@b.com c@d.org",
     io={RST: {"seq": ["a@b.com c@d.org", "ignored"]}})
case("_redact_reference_text",
     text="fo@_bar.com foo@ba_r.com ab+c@example_co.uk a.b+c@e9xa_mple.co.uk",
     io={RST: "fo@_bar.com foo@ba_r.com ab+c@example_co.uk a.b+c@e9xa_mple.co.uk"})

# ── 2. _redact_message_content ──
case("_redact_message_content", content="hi a@b.com", io={RST: "hi a@b.com"})
case("_redact_message_content",
     content=[{"type": "text", "text": "a@b.com"}, {"type": "image", "url": "u"},
              {"type": "text", "text": 5}, "raw", {"type": "text"}],
     io={RST: "a@b.com"})
case("_redact_message_content", content={"k": 1})
case("_redact_message_content", content=None)
case("_redact_message_content", content=5)
case("_redact_message_content", content=[])
case("_redact_message_content", content=[{"text": "x"}],
     io={RST: raise_spec("ValueError", "bad")})

# ── 3. _redact_reference_outputs ──
case("_redact_reference_outputs",
     reference_outputs=[["p:m", "a@b.com", acct()], ["q:n", "plain", None]],
     io={RST: {"seq": ["a@b.com", "plain"]}})
case("_redact_reference_outputs", reference_outputs=[], io={})
case("_redact_reference_outputs", reference_outputs=[["l", "t"]])
case("_redact_reference_outputs", reference_outputs=[["l", "t", None, "x"]])
case("_redact_reference_outputs", reference_outputs=[5])
case("_redact_reference_outputs", reference_outputs=[None])
case("_redact_reference_outputs", reference_outputs=None)
case("_redact_reference_outputs", reference_outputs=[["l", 7, acct(output="o")]],
     io={})
case("_redact_reference_outputs",
     reference_outputs=[["l1", "t1", None], ["l2", "t2", None]],
     io={RST: {"seq": ["t1", raise_spec("RuntimeError", "r")]}})

# ── 4. _redact_trace_messages ──
case("_redact_trace_messages",
     messages=[{"role": "user", "content": "a@b.com"},
               {"role": "assistant", "content": [{"text": "c@d.com"}]},
               {"role": "tool"}, "raw", 9],
     io={RST: {"seq": ["a@b.com", "c@d.com"]}})
case("_redact_trace_messages", messages=None)
case("_redact_trace_messages", messages="x")
case("_redact_trace_messages", messages=5)
case("_redact_trace_messages", messages=[])
case("_redact_trace_messages", messages=[{"content": "x"}],
     io={RST: raise_spec("KeyboardInterrupt", "ki")})

# ── 5. _redact_trace_accounting ──
case("_redact_trace_accounting",
     acct=acct(messages=[{"role": "user", "content": "a@b.com"}], output="c@d.com",
               model="m", provider="p"),
     io={RST: {"seq": ["a@b.com", "c@d.com"]}})
case("_redact_trace_accounting", acct={"plain": 1})
case("_redact_trace_accounting", acct=None)
case("_redact_trace_accounting", acct=acct())
case("_redact_trace_accounting", acct=acct(output=5, messages=None))
case("_redact_trace_accounting", acct=acct(output="x"),
     io={RST: raise_spec("RuntimeError", "out")})
case("_redact_trace_accounting",
     acct=acct(messages=[{"content": "x"}]),
     io={RST: raise_spec("GeneratorExit", "ge")})

# ── 6. _moa_privacy_mode ──
CPF = "coerce_privacy_filter"
case("_moa_privacy_mode", moa_raw={"privacy_filter": "full"}, io={CPF: "full"})
case("_moa_privacy_mode", moa_raw={}, io={CPF: "display"})
case("_moa_privacy_mode", moa_raw=None, io={CPF: ""})
case("_moa_privacy_mode", moa_raw=5, io={CPF: "full"})
case("_moa_privacy_mode", moa_raw=[1], io={CPF: None})
case("_moa_privacy_mode", moa_raw={"privacy_filter": "x"},
     io={CPF: raise_spec("RuntimeError", "cp")})
case("_moa_privacy_mode", moa_raw={"privacy_filter": "x"})   # fail-closed

# ── 7. _slot_label ──
case("_slot_label", slot={"provider": "openai", "model": "gpt"})
case("_slot_label", slot={"provider": " p ", "model": " m "})
case("_slot_label", slot={"provider": "p", "model": "m", "reasoning_effort": "high"})
case("_slot_label", slot={"provider": "p", "model": "m", "reasoning_effort": "  "})
case("_slot_label", slot={"provider": "p", "model": "m", "reasoning_effort": 5})
case("_slot_label", slot={"provider": "p", "model": "m", "reasoning_effort": 0.5})
case("_slot_label", slot={"provider": None, "model": None})
case("_slot_label", slot={"provider": 5, "model": "m"})
case("_slot_label", slot={"provider": "p", "model": True})
case("_slot_label", slot=None)
case("_slot_label", slot={"provider": "p", "model": "m", "reasoning_effort": None})
case("_slot_label", slot={"provider": "\tp\n", "model": "m", "reasoning_effort": "\thigh\n"})

# ── 8. peel_reference_guidance ──
G = "guidance block"
case("peel_reference_guidance",
     messages=[{"role": "user", "content": "task"}, {"role": "user", "content": G}],
     guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": G}, {"role": "assistant", "content": "a"}],
     guidance=G)
case("peel_reference_guidance", messages=["raw"], guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": [{"type": "text", "text": G}]}],
     guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": [{"type": "image", "text": G}]}],
     guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": [{"text": G}]}], guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": [{"type": "text", "text": None}]}],
     guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": [{"text": G}, {"text": "x"}]}],
     guidance=G)
case("peel_reference_guidance", messages=[{"role": "user", "content": G}],
     guidance=None)
case("peel_reference_guidance", messages=[], guidance=G)
case("peel_reference_guidance", messages="abc", guidance=G)
case("peel_reference_guidance", messages=5, guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": "5"}], guidance=5)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": None}], guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": [{"type": None, "text": G}]}],
     guidance=G)
case("peel_reference_guidance",
     messages=[{"role": "user", "content": [{"type": "text", "text": 5}]}],
     guidance="5")
case("peel_reference_guidance", messages=[{"role": "system", "content": G}],
     guidance=G)

# ── 9. _attach_reference_guidance ──
case("_attach_reference_guidance", agg_messages=[], guidance=G)
case("_attach_reference_guidance",
     agg_messages=[{"role": "user", "content": "task"}], guidance=G)
case("_attach_reference_guidance", agg_messages=[], guidance=None)
case("_attach_reference_guidance", agg_messages=[], guidance=5)
case("_attach_reference_guidance", agg_messages=None, guidance=G)
case("_attach_reference_guidance", agg_messages="x", guidance=G)
case("_attach_reference_guidance", agg_messages=5, guidance=G)

# ── 10. _reference_liveness_hook ──
case("_reference_liveness_hook", agent=attr(_touch_activity=call("touch")),
     label="p:m")
case("_reference_liveness_hook", agent=attr(_touch_activity=None), label="p:m")
case("_reference_liveness_hook", agent=None, label="l")
case("_reference_liveness_hook", agent=attr(), label="l")
case("_reference_liveness_hook", agent=attr(_touch_activity=5), label="l")
case("_reference_liveness_hook", agent=attr(_touch_activity=call("touch")), label="")

# ── 11. _touch_on_progress ──
case("_touch_on_progress", touch=call("touch"), label="p:m",
     io={"touch": None})
case("_touch_on_progress", touch=None, label="p:m")
case("_touch_on_progress", touch=5, label="p:m")
case("_touch_on_progress", touch=call("touch"), label="p:m",
     io={"touch": raise_spec("RuntimeError", "t")})
case("_touch_on_progress", touch=call("touch"), label="p:m",
     io={"touch": raise_spec("SystemExit", "s")})
case("_touch_on_progress", touch=call("touch"), label="", io={"touch": None})
case("_touch_on_progress", touch=call("touch"), label="p:m")   # fail-closed script

# ── 12. _touch_fanout_progress ──
case("_touch_fanout_progress", agent=attr(_touch_activity=call("touch")),
     description="MoA: 1 of 2 references complete", io={"touch": None})
case("_touch_fanout_progress", agent=attr(_touch_activity=call("touch")),
     description=None, io={"touch": None})
case("_touch_fanout_progress", agent=attr(_touch_activity=call("touch")),
     description="d", io={"touch": raise_spec("RuntimeError", "swallow")})
case("_touch_fanout_progress", agent=attr(_touch_activity=call("touch")),
     description="d", io={"touch": raise_spec("SystemExit", "pass")})
case("_touch_fanout_progress", agent=attr(_touch_activity=call("touch")),
     description="d", io={"touch": raise_spec("KeyboardInterrupt", "pass")})
case("_touch_fanout_progress", agent=None, description="d")
case("_touch_fanout_progress", agent=attr(_touch_activity=5), description="d")
case("_touch_fanout_progress", agent=attr(_touch_activity=call("touch")),
     description="d")   # fail-closed script

# ── 13. _slot_request_headers ──
NAP = "_normalize_aux_provider"
GPP = "get_provider_profile"
RA = "route_answer"
case("_slot_request_headers", runtime={"provider": "openai", "model": "gpt"},
     agent=attr(x=1),
     io={NAP: "openai", GPP: attr(base_url="https://api.x"), RA: {"h": 1}})
case("_slot_request_headers", runtime={"provider": "p", "model": "m"},
     io={NAP: "", GPP: attr(base_url="u"), RA: {"h": 1}})
case("_slot_request_headers", runtime={"provider": "p"}, io={NAP: None})
case("_slot_request_headers", runtime={"provider": "p"},
     io={NAP: "p", GPP: None})
case("_slot_request_headers", runtime={"provider": "p"},
     io={NAP: "p", GPP: attr()})
case("_slot_request_headers", runtime={"provider": "p"},
     io={NAP: "p", GPP: {"base_url": "u"}})
case("_slot_request_headers", runtime={"provider": "p", "model": "m"},
     io={NAP: "p", GPP: attr(base_url=None), RA: {"h": 2}})
case("_slot_request_headers", runtime=None)
case("_slot_request_headers", runtime=5)
case("_slot_request_headers", runtime={"provider": "p"},
     io={NAP: raise_spec("RuntimeError", "n")})
case("_slot_request_headers", runtime={"provider": "p"},
     io={NAP: "p", GPP: attr(base_url="u"), RA: raise_spec("ValueError", "r")})
case("_slot_request_headers", runtime={"provider": 5, "model": None},
     io={NAP: "5", GPP: attr(base_url="u"), RA: None})
case("_slot_request_headers", runtime={"provider": "p"})   # fail-closed

# ── 14. _moa_reference_relay ──
TPC = "tool_progress_callback"
case("_moa_reference_relay", event="moa.reference",
     kwargs={"label": "p:m", "text": "advice", "index": 1, "count": 2},
     agent=attr(tool_progress_callback=call(TPC)), io={TPC: None})
case("_moa_reference_relay", event="moa.progress",
     kwargs={"label": "p:m", "refs_done": 1, "refs_total": 2},
     agent=attr(tool_progress_callback=call(TPC)), io={TPC: None})
case("_moa_reference_relay", event="moa.phase",
     kwargs={"aggregator": "a:b", "phase": "aggregator", "refs_done": 2,
             "refs_total": 2},
     agent=attr(tool_progress_callback=call(TPC)), io={TPC: None})
case("_moa_reference_relay", event="moa.aggregating",
     kwargs={"aggregator": "a:b", "ref_count": 2},
     agent=attr(tool_progress_callback=call(TPC)), io={TPC: None})
case("_moa_reference_relay", event="moa.unknown", kwargs={"label": "l"},
     agent=attr(tool_progress_callback=call(TPC)))
case("_moa_reference_relay", event="moa.reference", kwargs={"label": "l"},
     agent=attr(tool_progress_callback=None))
case("_moa_reference_relay", event="moa.reference", kwargs={"label": "l"},
     agent=attr(tool_progress_callback=5))
case("_moa_reference_relay", event="moa.reference", kwargs={"label": "l"},
     agent=attr(tool_progress_callback=call(TPC)),
     io={TPC: raise_spec("RuntimeError", "sw")})
case("_moa_reference_relay", event="moa.reference", kwargs={"label": "l"},
     agent=attr(tool_progress_callback=call(TPC)),
     io={TPC: raise_spec("SystemExit", "pass")})
case("_moa_reference_relay", event="moa.reference", kwargs={},
     agent=attr(tool_progress_callback=call(TPC)), io={TPC: None})
case("_moa_reference_relay", event="moa.reference", kwargs={"label": 5, "text": 0},
     agent=attr(tool_progress_callback=call(TPC)), io={TPC: None})
case("_moa_reference_relay", event="moa.reference", kwargs={"label": "l"},
     agent=None)
case("_moa_reference_relay", event="moa.progress",
     kwargs={"label": "l", "refs_done": None, "refs_total": 3},
     agent=attr(tool_progress_callback=call(TPC)), io={TPC: None})

# ── 15/16. MoAClient properties ──
def client_self(comp, prop, value):
    return attr(chat=attr(completions=comp))


for prop in ("last_aggregator_route", "last_aggregator_attempt"):
    case(prop, **{"self": client_self(attr(**{prop: {"provider": "p"}}), prop, None)})
    case(prop, **{"self": client_self(attr(), prop, None)})
    case(prop, **{"self": client_self(None, prop, None)})
    case(prop, **{"self": client_self({"k": 1}, prop, None)})
    case(prop, **{"self": attr(chat=None)})
    case(prop, **{"self": None})
    case(prop, **{"self": attr(chat=attr())})
    case(prop, **{"self": {"chat": 1}})

# ── 17. _resolve_preset_cached ──
WCS = "worker_config_snapshot"
GCP = "get_config_path"
CCV = "config_cache_version_or_none"
LC = "load_config"
RMP = "resolve_moa_preset"
PRESET = {"enabled": True, "reference_models": [{"provider": "p", "model": "m"}],
          "aggregator": {"provider": "a", "model": "b"}}
case("_resolve_preset_cached", preset_name="default",
     state={"preset_cache": {}},
     io={WCS: None, GCP: "/c/config.yaml", CCV: "v1",
         LC: {"moa": {"privacy_filter": "full"}}, RMP: PRESET})
case("_resolve_preset_cached", preset_name="default",
     state={"preset_cache": {json.dumps(["v1", "default"], separators=(",", ":")): PRESET}},
     io={WCS: None, GCP: "/c/config.yaml", CCV: "v1",
         LC: {"moa": {"privacy_filter": "full"}}})
case("_resolve_preset_cached", preset_name="p2",
     state={"preset_cache": {}},
     io={WCS: {"frozen": 1}, LC: {"moa": {}}, RMP: PRESET})
case("_resolve_preset_cached", preset_name="p2",
     state={"preset_cache": {}},
     io={WCS: None, GCP: "/c", CCV: None, LC: {"moa": {}}, RMP: PRESET})
case("_resolve_preset_cached", preset_name="d", state={}, io={WCS: None, GCP: "/c", CCV: "v", LC: None})
case("_resolve_preset_cached", preset_name="d", state={},
     io={WCS: None, GCP: "/c", CCV: "v", LC: raise_spec("RuntimeError", "lc")})
case("_resolve_preset_cached", preset_name="d", state={"preset_cache": {}},
     io={WCS: None, GCP: "/c", CCV: "v", LC: {"moa": {}},
         RMP: raise_spec("ValueError", "rp")})
case("_resolve_preset_cached", preset_name="d", state={"preset_cache": {}},
     io={WCS: None, GCP: "/c", CCV: "v", LC: {"moa": {}},
         RMP: raise_spec("CancelledError", "ce")})
case("_resolve_preset_cached", preset_name="d", state={},
     io={WCS: None, GCP: "/c", CCV: "v", LC: {"moa": None}, RMP: PRESET})
case("_resolve_preset_cached", preset_name="d", state={},
     io={WCS: None, GCP: "/c", CCV: "v", LC: {"moa": 5}, RMP: PRESET})
case("_resolve_preset_cached", preset_name="new",
     state={"preset_cache": {json.dumps(["old", "other"], separators=(",", ":")): {"x": 1}}},
     io={WCS: None, GCP: "/c", CCV: "v2", LC: {"moa": {}}, RMP: PRESET})
case("_resolve_preset_cached", preset_name="d",
     state={"preset_cache": {json.dumps(["v", "d"], separators=(",", ":")): None}},
     io={WCS: None, GCP: "/c", CCV: "v", LC: {"moa": {}}, RMP: PRESET})
case("_resolve_preset_cached", preset_name="d",
     state={"preset_cache": {json.dumps(["v", "d"], separators=(",", ":")): {}}},
     io={WCS: None, GCP: "/c", CCV: "v", LC: {"moa": {}}})
case("_resolve_preset_cached", preset_name="d", state={})   # fail-closed chain
case("_resolve_preset_cached", preset_name="d", state={},
     io={WCS: None, GCP: raise_spec("OSError", "path"), LC: {"moa": {}}})

# ── 18. collect ──
FR = "future.result"
PC = "progress_callback"
SLOTS = [{"provider": "p1", "model": "m1"}, {"provider": "p2", "model": "m2"}]
OUT0 = ["p1:m1", "advice one", acct()]
OUT1 = ["p2:m2", "advice two", acct()]
case("collect", done=[{"idx": 0}, {"idx": 1}], reference_models=SLOTS,
     agent=attr(_touch_activity=call("touch")), progress_callback=call(PC),
     total=2, state={"results": [None, None], "completed": 0},
     io={FR: {"seq": [OUT0, OUT1]}, "touch": None, PC: None})
case("collect", done=[{"idx": 0}], reference_models=SLOTS[:1],
     agent=attr(_touch_activity=call("touch")), progress_callback=None,
     total=1, state={"results": [None], "completed": 0},
     io={FR: OUT0, "touch": None})
case("collect", done=[{"idx": 0}, {"idx": 1}], reference_models=SLOTS,
     agent=None, progress_callback=call(PC), total=2,
     state={"results": [None, None], "completed": 0},
     io={FR: {"seq": [OUT0, OUT1]}, PC: raise_spec("RuntimeError", "cb")})
case("collect", done=[{"idx": 0}, {"idx": 1}], reference_models=SLOTS,
     agent=None, progress_callback=call(PC), total=2,
     state={"results": [None, None], "completed": 0},
     io={FR: {"seq": [OUT0, OUT1]}, PC: {"seq": [None, raise_spec("SystemExit", "stop")]}})
case("collect", done=[{"idx": 0}], reference_models=SLOTS[:1],
     agent=attr(_touch_activity=call("touch")), progress_callback=call(PC),
     total=1, state={"results": [None], "completed": 0},
     io={FR: OUT0, "touch": raise_spec("RuntimeError", "sw"), PC: None})
case("collect", done=[{"idx": 0}], reference_models=SLOTS[:1],
     agent=attr(_touch_activity=call("touch")), progress_callback=call(PC),
     total=1, state={"results": [None], "completed": 0},
     io={FR: OUT0, "touch": raise_spec("KeyboardInterrupt", "ki")})
case("collect", done=[{"idx": 0}], reference_models=SLOTS[:1],
     agent=None, progress_callback=None, total=1,
     state={"results": [None], "completed": 0},
     io={FR: raise_spec("TimeoutError", "to")})
case("collect", done=[{"idx": 2}], reference_models=SLOTS,
     agent=None, progress_callback=None, total=2,
     state={"results": [None, None], "completed": 0}, io={FR: OUT0})
case("collect", done=[{"idx": 1}], reference_models=SLOTS[:1],
     agent=None, progress_callback=call(PC), total=2,
     state={"results": [None, None], "completed": 0},
     io={FR: OUT1, PC: None})
case("collect", done=[{"idx": 0}], reference_models=[{"provider": 5, "model": "m"}],
     agent=None, progress_callback=call(PC), total=1,
     state={"results": [None], "completed": 0}, io={FR: OUT0, PC: None})
case("collect", done=[], reference_models=SLOTS, agent=None,
     progress_callback=call(PC), total=2,
     state={"results": [None, None], "completed": 0}, io={})
case("collect", done=[{"idx": 0}], reference_models=SLOTS[:1],
     agent=None, progress_callback=call(PC), total=3,
     state={"results": [None], "completed": 1},
     io={FR: OUT0, PC: None})
case("collect", done=None, reference_models=SLOTS, agent=None,
     progress_callback=None, total=2, state={"results": [], "completed": 0}, io={})
case("collect", done=[{"idx": 0}], reference_models=None,
     agent=None, progress_callback=call(PC), total=1,
     state={"results": [None], "completed": 0}, io={FR: OUT0, PC: None})
case("collect", done=[{"idx": 0}], reference_models=SLOTS[:1],
     agent=None, progress_callback=5, total=1,
     state={"results": [None], "completed": 0}, io={FR: OUT0})

# ── 19. _plan_aggregator_cache ──
ACO = "_agent_cache_opts"
PCS = "plan_cache_sections_for_destination"
MSGS = [{"role": "system", "content": "s"}, {"role": "user", "content": "task"}]
MSGS2 = [{"role": "system", "content": "s", "cache_control": {"type": "ephemeral"}},
         {"role": "user", "content": "task"}]
TOOLS = [{"name": "t"}]
TOOLS2 = [{"name": "t", "cache_control": True}]
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime={"provider": "a", "model": "m", "base_url": "u", "api_mode": "chat"},
     state={"_agent": None},
     io={ACO: [None, None, False], PCS: [MSGS2, TOOLS2]})
case("_plan_aggregator_cache",
     agg_messages=MSGS + [{"role": "user", "content": G}], tools=TOOLS,
     guidance=G, agg_runtime={"provider": "a"},
     state={"_agent": attr(_cache_disabled=True, _cached_system_prompt_static="sys")},
     io={ACO: [True, "1h", True], PCS: [MSGS2, TOOLS2]})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=G,
     agg_runtime={"provider": "a"}, state={"_agent": None},
     io={ACO: [None, "5m", False], PCS: [MSGS2, TOOLS2]})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime={}, state={}, io={ACO: None, PCS: [MSGS2, TOOLS2]})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime={}, state={}, io={ACO: [None, None], PCS: [MSGS2, TOOLS2]})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime={}, state={},
     io={ACO: [None, None, False], PCS: raise_spec("RuntimeError", "plan")})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime={}, state={},
     io={ACO: [None, None, False], PCS: raise_spec("SystemExit", "exit")})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime={}, state={}, io={ACO: [None, None, False], PCS: None})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=G,
     agg_runtime={}, state={},
     io={ACO: [None, None, False], PCS: [5, TOOLS2]})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime=None, state={}, io={ACO: [None, None, False], PCS: [MSGS2, TOOLS2]})
case("_plan_aggregator_cache", agg_messages=5, tools=TOOLS, guidance=G,
     agg_runtime={}, state={}, io={ACO: [None, None, False], PCS: [MSGS2, TOOLS2]})
case("_plan_aggregator_cache", agg_messages=MSGS, tools=TOOLS, guidance=None,
     agg_runtime={}, state={})   # fail-closed: opts None -> caught
case("_plan_aggregator_cache", agg_messages=MSGS, tools=None, guidance=None,
     agg_runtime={"provider": None, "model": 5}, state={"_agent": attr()},
     io={ACO: [False, None, False], PCS: [MSGS2, None]})

# ── 20. bind_moa_runtime ──
BMF = "build_moa_facade"
case("bind_moa_runtime", agent=attr(), preset_name="p", api_key="k",
     state={}, io={BMF: {"__facade__": "f1"}})
case("bind_moa_runtime", agent=attr(), preset_name=None, api_key=None,
     state={}, io={BMF: {"__facade__": "f2"}})
case("bind_moa_runtime", agent=attr(), preset_name=5, api_key="",
     state={}, io={BMF: None})
case("bind_moa_runtime", agent=None, preset_name="p", api_key="k", state={})
case("bind_moa_runtime", agent={"plain": 1}, preset_name="p", api_key="k", state={})
case("bind_moa_runtime", agent=attr(model="old", provider="openai"),
     preset_name="p", api_key="k", state={},
     io={BMF: raise_spec("RuntimeError", "bf")})
case("bind_moa_runtime", agent=attr(), preset_name="p", api_key="k", state={},
     io={BMF: raise_spec("SystemExit", "bf")})
case("bind_moa_runtime", agent=attr(model="old", extra="keep"),
     preset_name="p", api_key="k", state={}, io={BMF: {"__facade__": "f3"}})
case("bind_moa_runtime", agent=attr(), preset_name="p", api_key="k", state={})   # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        raise SystemExit("fewer than 5 cases: %s" % thin)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True, separators=(",", ":")) + "\n")
    print("wrote %s: %d cases across %d funcs" % (OUT, len(CASES), len(counts)))
    for fn in sorted(counts):
        print("  %-32s %d" % (fn, counts[fn]))


if __name__ == "__main__":
    main()