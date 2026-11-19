#!/usr/bin/env python3
"""Generate tests/fixtures/openai_transport_flow.jsonl for the OpenAI Chat
Completions transport UNCLAIMED-remainder FLOW port
(src/agent/transports/openai_transport_flow.c; live module
agent/transports/chat_completions.py + agent/transports/types.py leaves).

Case shape: {"func": <name>, "in": {...args..., "io": {seam: script}}}.
Scripts (shared contract with the C universal seam oai_call and the oracle):
  - MISSING key            -> fail-closed RuntimeError("no script for <name>")
  - null                   -> None result
  - {"$seq": [v1, v2, ...]}-> one result per call; exhaustion raises
                              RuntimeError("script exhausted: <name>")
  - {"__raise__": cls, "msg": m, ...} -> raises; trace ret is the WHOLE marker
  - any other value        -> reused on every call (a bare ARRAY is a LIST
                              RESULT, not a queue — wrap queues in $seq)

Seam names are the LIVE collaborator names (underscores kept). Claimed siblings:
_add_prompt_cache_key (void mutator; {"__update__":{...}} merges api_kwargs),
_raise_gemini_thinking_max_tokens. Foreign imports: _alias_reserved_tools,
get_provider_profile, route_reasoning_carriers, base_url_host_matches,
coalesce_tool_call_id, is_moonshot_model, sanitize_moonshot_tools. Callback:
max_tokens_param_fn (truthy {"fn":1} marker in params). Object method: model_dump
(has_model_dump flag attaches it to the extra double).

Fixture domain (documented in the port header):
  - ASCII strings; base_url / provider / model str-or-null.
  - str()-computed positions carry strings/integers only.
  - coalesce_tool_call_id scripts str-or-null; profile scripts carry name +
    replays_tool_call_signatures; max_tokens_param_fn scripts dicts;
    route_reasoning_carriers scripts string arrays.
  - _mistral_tool_id non-str keys: null/bool/list/dict/non-integral float only.
  - _normalize_tool_call tc.extra_content plain (no model_dump).
  - provider_data truthy non-dict cases: list/str/bool (no numbers).
  - profile.fixed_temperature numeric / null / "__OMIT_TEMPERATURE__" marker.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "openai_transport_flow.jsonl"


def seq(*vals):
    return {"$seq": list(vals)}


def rais(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []


def add(*cases):
    CASES.extend(cases)


# ── _rename_tool_search_bridge_for_xai ──────────────────────────────────────
TOOLS = [{"type": "function", "function": {"name": "tool_search", "parameters": {}}}]
add(
    case("_rename_tool_search_bridge_for_xai", tools=TOOLS,
         io={"_alias_reserved_tools": [[{"type": "function", "function": {"name": "hermes_tool_search"}}],
                                        {"hermes_tool_search": "tool_search"}]}),
    case("_rename_tool_search_bridge_for_xai", tools=[],
         io={"_alias_reserved_tools": [[], {}]}),
    case("_rename_tool_search_bridge_for_xai", tools=None,
         io={"_alias_reserved_tools": None}),
    case("_rename_tool_search_bridge_for_xai", tools=TOOLS,
         io={"_alias_reserved_tools": rais("ValueError", "alias clash")}),
    case("_rename_tool_search_bridge_for_xai", tools=TOOLS, io={}),   # fail-closed
    case("_rename_tool_search_bridge_for_xai", tools=TOOLS,
         io={"_alias_reserved_tools": rais("SystemExit", "bye")}),   # BaseException
)

# ── _profile_replays_tool_call_signatures ───────────────────────────────────
add(
    case("_profile_replays_tool_call_signatures", provider="gemini",
         io={"get_provider_profile": {"name": "gemini", "replays_tool_call_signatures": True}}),
    case("_profile_replays_tool_call_signatures", provider="  Gemini  ",
         io={"get_provider_profile": {"name": "gemini", "replays_tool_call_signatures": True}}),
    case("_profile_replays_tool_call_signatures", provider="vertex",
         io={"get_provider_profile": {"name": "other", "replays_tool_call_signatures": True}}),
    case("_profile_replays_tool_call_signatures", provider="gemini",
         io={"get_provider_profile": {"name": "gemini", "replays_tool_call_signatures": False}}),
    case("_profile_replays_tool_call_signatures", provider="gemini",
         io={"get_provider_profile": None}),
    case("_profile_replays_tool_call_signatures", provider=None, io={}),   # empty -> no seam
    case("_profile_replays_tool_call_signatures", provider="   ", io={}),
    case("_profile_replays_tool_call_signatures", provider="gemini", io={}),   # fail-closed
    case("_profile_replays_tool_call_signatures", provider="gemini",
         io={"get_provider_profile": rais("KeyboardInterrupt")}),
)

# ── _wire_reasoning_carriers ────────────────────────────────────────────────
add(
    case("_wire_reasoning_carriers", provider="openrouter", base_url="https://openrouter.ai/api/v1",
         io={"route_reasoning_carriers": ["reasoning", "reasoning_content", "reasoning_details"]}),
    case("_wire_reasoning_carriers", provider="groq", base_url=None,
         io={"route_reasoning_carriers": []}),
    case("_wire_reasoning_carriers", provider=None, base_url=None,
         io={"route_reasoning_carriers": ["reasoning_details"]}),
    case("_wire_reasoning_carriers", provider="mistral", base_url="https://api.mistral.ai/v1",
         io={"route_reasoning_carriers": rais("RuntimeError", "table gone")}),
    case("_wire_reasoning_carriers", provider="x", base_url=None, io={}),   # fail-closed
    case("_wire_reasoning_carriers", provider="x", base_url=None,
         io={"route_reasoning_carriers": rais("GeneratorExit")}),
    case("_wire_reasoning_carriers", provider="x", base_url=None,
         io={"route_reasoning_carriers": None}),
)

# ── _has_replayable_thought_signature ───────────────────────────────────────
add(
    case("_has_replayable_thought_signature", extra_content={"thought_signature": "sig123"}),
    case("_has_replayable_thought_signature", extra_content={"google": {"thought_signature": "gsig"}}),
    case("_has_replayable_thought_signature",
         extra_content={"thought_signature": None, "google": {"thought_signature": "gsig"}}),
    case("_has_replayable_thought_signature", extra_content={"thought_signature": "   "}),
    case("_has_replayable_thought_signature", extra_content={"thought_signature": "\t\n\x1c "}),
    case("_has_replayable_thought_signature", extra_content={"thought_signature": 5}),
    case("_has_replayable_thought_signature", extra_content={"thought_signature": ""}),
    case("_has_replayable_thought_signature", extra_content={"google": {"thought_signature": ""}}),
    case("_has_replayable_thought_signature", extra_content={"google": "notadict"}),
    case("_has_replayable_thought_signature", extra_content=None),
    case("_has_replayable_thought_signature", extra_content="sig"),
    case("_has_replayable_thought_signature", extra_content={}),
)

# ── _attr_or_model_extra ────────────────────────────────────────────────────
add(
    case("_attr_or_model_extra", obj={"reasoning_content": "think"}, name="reasoning_content"),
    case("_attr_or_model_extra", obj={"model_extra": {"refusal": "no"}}, name="refusal"),
    case("_attr_or_model_extra", obj={"refusal": None, "model_extra": {"refusal": "no"}}, name="refusal"),
    case("_attr_or_model_extra", obj={"model_extra": "notadict"}, name="refusal"),
    case("_attr_or_model_extra", obj={"model_extra": None}, name="refusal"),
    case("_attr_or_model_extra", obj={"other": 1}, name="refusal"),
    case("_attr_or_model_extra", obj=None, name="refusal"),
    case("_attr_or_model_extra", obj="plain", name="refusal"),
    case("_attr_or_model_extra", obj={"model_extra": {"x": {"nested": [1, 2]}}}, name="x"),
)

# ── _dump_extra_content ─────────────────────────────────────────────────────
add(
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=False),
    case("_dump_extra_content", extra=None, has_model_dump=False),
    case("_dump_extra_content", extra="plain", has_model_dump=True),   # no method on scalar
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": {"dumped": True}}),
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": seq(rais("TypeError", "warnings kwarg"), {"dumped": "second"})}),
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": seq(rais("TypeError", "w"), rais("TypeError", "nokw"))}),
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": rais("ValueError", "bad model")}),
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": seq(rais("RuntimeError", "first"))}),
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": rais("SystemExit", "propagate")}),
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": rais("KeyboardInterrupt")}),
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True, io={}),   # fail-closed
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": seq({"first": 1}, {"second": 2})}),   # first wins, one call
    case("_dump_extra_content", extra={"a": 1}, has_model_dump=True,
         io={"model_dump": None}),
)

# ── _pareto_score ───────────────────────────────────────────────────────────
add(
    case("_pareto_score", raw=None),
    case("_pareto_score", raw=""),
    case("_pareto_score", raw=0),
    case("_pareto_score", raw=1),
    case("_pareto_score", raw=0.5),
    case("_pareto_score", raw=-0.1),
    case("_pareto_score", raw=1.1),
    case("_pareto_score", raw="0.5"),
    case("_pareto_score", raw=" 0.5 "),
    case("_pareto_score", raw="0.5_0"),
    case("_pareto_score", raw="1_0"),          # 10.0 out of range
    case("_pareto_score", raw="abc"),
    case("_pareto_score", raw="1e"),
    case("_pareto_score", raw="0x10"),
    case("_pareto_score", raw=True),
    case("_pareto_score", raw=False),
    case("_pareto_score", raw=[0.5]),
    case("_pareto_score", raw={"x": 1}),
    case("_pareto_score", raw="nan"),
    case("_pareto_score", raw="inf"),
    case("_pareto_score", raw="-inf"),
    case("_pareto_score", raw="1e-1"),
    case("_pareto_score", raw=".5"),
    case("_pareto_score", raw="1."),
    case("_pareto_score", raw="+0.5"),
    case("_pareto_score", raw="1__0"),
    case("_pareto_score", raw="_0.5"),
    case("_pareto_score", raw="0.5_"),
    case("_pareto_score", raw="1_.5"),
    case("_pareto_score", raw="0.5\n"),
    case("_pareto_score", raw="0.5\x1c"),      # float() does NOT strip \x1c
    case("_pareto_score", raw="1e999"),        # overflow -> inf -> out of range
)

# ── _swap_developer_role ────────────────────────────────────────────────────
SYS = [{"role": "system", "content": "be nice"}, {"role": "user", "content": "hi"}]
add(
    case("_swap_developer_role", sanitized=SYS, model_lower="gpt-5.2"),
    case("_swap_developer_role", sanitized=SYS, model_lower="codex-mini"),
    case("_swap_developer_role", sanitized=SYS, model_lower="gpt-4o"),
    case("_swap_developer_role", sanitized=[{"role": "user", "content": "hi"}], model_lower="gpt-5"),
    case("_swap_developer_role", sanitized=[], model_lower="gpt-5"),
    case("_swap_developer_role", sanitized=None, model_lower="gpt-5"),
    case("_swap_developer_role", sanitized=["plain", {"role": "system"}], model_lower="gpt-5"),
    case("_swap_developer_role", sanitized=[{"role": "system", "content": "x", "z": 1}],
         model_lower="openai/codex-1"),
)

# ── _apply_max_tokens ───────────────────────────────────────────────────────
FN = {"fn": 1}
add(
    case("_apply_max_tokens", api_kwargs={"model": "m"}, model="gemini-2.5-pro",
         reasoning_config={"effort": "high"},
         params={"ephemeral_max_output_tokens": 500, "max_tokens": 100, "max_tokens_param_fn": FN},
         io={"_raise_gemini_thinking_max_tokens": 900, "max_tokens_param_fn": {"max_tokens": 900}}),
    case("_apply_max_tokens", api_kwargs={}, model="gpt-5", reasoning_config=None,
         params={"max_tokens": 100, "max_tokens_param_fn": FN},
         io={"_raise_gemini_thinking_max_tokens": 100, "max_tokens_param_fn": {"max_tokens": 100}}),
    case("_apply_max_tokens", api_kwargs={}, model="m", reasoning_config=None,
         params={"max_tokens_param_fn": FN}, profile_max=200,
         io={"_raise_gemini_thinking_max_tokens": 200, "max_tokens_param_fn": {"max_tokens": 200}}),
    case("_apply_max_tokens", api_kwargs={}, model="m", reasoning_config=None,
         params={"max_tokens_param_fn": FN}, profile_max=0, io={}),   # falsy profile_max, no seams
    case("_apply_max_tokens", api_kwargs={}, model="m", reasoning_config=None,
         params={"max_tokens": 100, "max_tokens_param_fn": {}}, io={}),   # falsy marker
    case("_apply_max_tokens", api_kwargs={}, model="m", reasoning_config=None,
         params={"max_tokens": 100}, io={}),   # no fn
    case("_apply_max_tokens", api_kwargs={"temperature": 0.5}, model="m", reasoning_config=None,
         params={"ephemeral_max_output_tokens": 0, "max_tokens_param_fn": FN},
         io={"_raise_gemini_thinking_max_tokens": 0, "max_tokens_param_fn": {"max_tokens": 0}}),
    case("_apply_max_tokens", api_kwargs={}, model="m", reasoning_config=None,
         params={"max_tokens": 100, "max_tokens_param_fn": FN},
         io={"_raise_gemini_thinking_max_tokens": rais("ValueError", "cap")}),
    case("_apply_max_tokens", api_kwargs={}, model="m", reasoning_config=None,
         params={"max_tokens": 100, "max_tokens_param_fn": FN},
         io={"_raise_gemini_thinking_max_tokens": 100,
             "max_tokens_param_fn": rais("SystemExit", "stop")}),
    case("_apply_max_tokens", api_kwargs={}, model="m", reasoning_config=None,
         params={"max_tokens": 100, "max_tokens_param_fn": FN}, io={}),   # fail-closed
    case("_apply_max_tokens", api_kwargs={}, model=None, reasoning_config=None,
         params={"max_tokens": None, "max_tokens_param_fn": FN}, profile_max=None, io={}),
)

# ── _base_kwargs ────────────────────────────────────────────────────────────
MSGS = [{"role": "user", "content": "hi"}]
TLIST = [{"type": "function", "function": {"name": "f"}}]
add(
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=None, params={}, profile=None, io={}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=None,
         params={"temperature": 0.7, "timeout": 30},
         profile={"fixed_temperature": 0.2}, io={}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=None,
         params={"temperature": 0.7}, profile={"fixed_temperature": "__OMIT_TEMPERATURE__"}, io={}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=None,
         params={"temperature": 0.7}, profile={"fixed_temperature": None}, io={}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=None, params={},
         profile={"fixed_temperature": None}, io={}),
    case("_base_kwargs", model="kimi-k3", sanitized=MSGS, tools=TLIST, params={}, profile=None,
         io={"is_moonshot_model": True, "sanitize_moonshot_tools": [{"type": "function", "function": {"name": "f", "strict": True}}]}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=TLIST, params={}, profile=None,
         io={"is_moonshot_model": False}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=[], params={}, profile=None, io={}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=TLIST, params={}, profile=None,
         io={"is_moonshot_model": rais("RuntimeError", "probe failed")}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=TLIST, params={}, profile=None,
         io={"is_moonshot_model": True, "sanitize_moonshot_tools": rais("ValueError", "schema")}),
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=None, params={},
         profile={}, io={}),   # missing fixed_temperature -> AttributeError
    case("_base_kwargs", model="gpt-5", sanitized=MSGS, tools=TLIST, params={}, profile=None, io={}),  # fail-closed
)

# ── _finish_kwargs ──────────────────────────────────────────────────────────
add(
    case("_finish_kwargs", api_kwargs={"model": "m"}, sanitized=MSGS, params={},
         supports_prompt_cache_key=True, io={"_add_prompt_cache_key": None}),
    case("_finish_kwargs", api_kwargs={"model": "m"}, sanitized=MSGS,
         params={"session_id": "s1", "cache_scope_id": "c1"}, supports_prompt_cache_key=True,
         io={"_add_prompt_cache_key": {"__update__": {"prompt_cache_key": "abc123"}}}),
    case("_finish_kwargs", api_kwargs={"model": "m", "tools": TLIST}, sanitized=MSGS,
         params={"session_id": "s1"}, supports_prompt_cache_key=False,
         io={"_add_prompt_cache_key": None}),
    case("_finish_kwargs", api_kwargs={"model": "m", "tools": None}, sanitized=MSGS, params={},
         supports_prompt_cache_key=True, io={"_add_prompt_cache_key": None}),
    case("_finish_kwargs", api_kwargs={"model": "m"}, sanitized=MSGS, params={},
         supports_prompt_cache_key=True,
         io={"_add_prompt_cache_key": rais("RuntimeError", "hash failed")}),
    case("_finish_kwargs", api_kwargs={"model": "m"}, sanitized=MSGS, params={},
         supports_prompt_cache_key=True, io={}),   # fail-closed
    case("_finish_kwargs", api_kwargs={"model": "m"}, sanitized=MSGS, params={},
         supports_prompt_cache_key=True,
         io={"_add_prompt_cache_key": rais("GeneratorExit")}),
    case("_finish_kwargs", api_kwargs={"prompt_cache_key": "x" * 70}, sanitized=MSGS, params={},
         supports_prompt_cache_key=True,
         io={"_add_prompt_cache_key": {"__update__": {"prompt_cache_key": "x" * 64}}}),
)

# ── _route_requires_mistral_tool_ids ────────────────────────────────────────
add(
    case("_route_requires_mistral_tool_ids", provider="mistral", model=None, base_url=None, io={}),
    case("_route_requires_mistral_tool_ids", provider="  Mistral  ", model=None, base_url=None, io={}),
    case("_route_requires_mistral_tool_ids", provider="other", model=None,
         base_url="https://api.mistral.ai/v1", io={"base_url_host_matches": True}),
    case("_route_requires_mistral_tool_ids", provider=None, model="mistral-large-latest",
         base_url=None, io={"base_url_host_matches": False}),
    case("_route_requires_mistral_tool_ids", provider=None, model="magistral-medium",
         base_url=None, io={"base_url_host_matches": False}),
    case("_route_requires_mistral_tool_ids", provider=None, model="MISTRAL-Small",
         base_url=None, io={"base_url_host_matches": False}),
    case("_route_requires_mistral_tool_ids", provider=None, model="gpt-5", base_url=None,
         io={"base_url_host_matches": False}),
    case("_route_requires_mistral_tool_ids", provider=None, model=None, base_url=None, io={}),  # fail-closed
    case("_route_requires_mistral_tool_ids", provider=None, model=None, base_url="https://x.ai",
         io={"base_url_host_matches": rais("OSError", "dns")}),
    case("_route_requires_mistral_tool_ids", provider=None, model="devstral-2", base_url=None,
         io={"base_url_host_matches": False}),
)

# ── _mistral_tool_id ────────────────────────────────────────────────────────
add(
    case("_mistral_tool_id", key="abc123XYZ"),       # 9 alnum -> passthrough
    case("_mistral_tool_id", key="AAAAAAAAA"),
    case("_mistral_tool_id", key="abc123XY"),        # 8 -> digest
    case("_mistral_tool_id", key="abc123XYZ0"),      # 10 -> digest
    case("_mistral_tool_id", key="abc-123XY"),       # 9 non-alnum -> digest
    case("_mistral_tool_id", key=""),
    case("_mistral_tool_id", key="call_0123456789abcdef"),
    case("_mistral_tool_id", key="call|item_42"),
    case("_mistral_tool_id", key=None),
    case("_mistral_tool_id", key=True),
    case("_mistral_tool_id", key=[]),
    case("_mistral_tool_id", key={}),
    case("_mistral_tool_id", key=5.5),
)

# ── _sanitize_message ───────────────────────────────────────────────────────
add(
    case("_sanitize_message", msg="notadict", strip_extra_content=False),
    case("_sanitize_message", msg=None, strip_extra_content=False),
    case("_sanitize_message", msg={"role": "user", "content": "hi"}, strip_extra_content=False),
    case("_sanitize_message", msg={"role": "user", "content": "hi", "timestamp": 5, "_x": 1,
                                   "message_uid": "u1", "data_use": ["a"]},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "content": "x", "reasoning": "r",
                                   "reasoning_content": "rc"},
         strip_extra_content=False, strip_reasoning=["reasoning", "reasoning_content"]),
    case("_sanitize_message", msg={"role": "assistant", "content": "x", "reasoning": "r"},
         strip_extra_content=False, strip_reasoning=["reasoning_details"]),
    case("_sanitize_message", msg={"role": "tool", "content": "res", "name": "f",
                                   "tool_call_id": "abc123XYZ"},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "content": "x", "tool_calls": []},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "content": "x", "tool_calls": None},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "content": "x",
                                   "tool_calls": [{"id": "call_1", "call_id": "c1",
                                                   "response_item_id": "r1",
                                                   "function": {"name": "f", "arguments": "{}"}}]},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "content": "x",
                                   "tool_calls": [{"id": "i1", "extra_content": {"thought_signature": "s"}}]},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "content": "x",
                                   "tool_calls": [{"id": "i1", "extra_content": {"thought_signature": "s"}}]},
         strip_extra_content=True),
    case("_sanitize_message", msg={"role": "assistant", "content": "x",
                                   "tool_calls": [{"id": "i1", "extra_content": {}}]},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "content": "x",
                                   "tool_calls": ["plain", {"id": "i2", "call_id": "c2"}]},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "reasoning_details": [
            {"type": "openai.native_assistant", "text": "a"},
            {"type": "reasoning.summary", "text": "b"}]},
         strip_extra_content=False, native_reasoning_details_type="other.native_assistant"),
    case("_sanitize_message", msg={"role": "assistant", "reasoning_details": [
            {"type": "openai.native_assistant", "text": "a"}]},
         strip_extra_content=False, native_reasoning_details_type="openai.native_assistant"),
    case("_sanitize_message", msg={"role": "assistant", "reasoning_details": [
            {"type": "openai.native_assistant"}, {"type": "x.native_assistant"}]},
         strip_extra_content=False, native_reasoning_details_type=None),
    case("_sanitize_message", msg={"role": "assistant", "reasoning_details": "notalist"},
         strip_extra_content=False),
    case("_sanitize_message", msg={"role": "assistant", "reasoning_details": [
            {"type": "openai.native_assistant"}], "reasoning_content": "keep"},
         strip_extra_content=False, strip_reasoning=["reasoning_details"]),
    # mistral remap: tool result
    case("_sanitize_message", msg={"role": "tool", "content": "res", "tool_call_id": "call_0123456789abcdef"},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": "call_0123456789abcdef"}),
    case("_sanitize_message", msg={"role": "tool", "content": "res", "tool_call_id": "abc123XYZ"},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": "abc123XYZ"}),
    case("_sanitize_message", msg={"role": "tool", "content": "res", "tool_call_id": "call_x"},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": ""}),
    case("_sanitize_message", msg={"role": "tool", "content": "res"},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": "call_x"}),   # KeyError 'tool_call_id'
    case("_sanitize_message", msg={"role": "tool", "content": "res", "tool_call_id": "call_x"},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": rais("RuntimeError", "coalesce down")}),
    # mistral remap: assistant tool_calls
    case("_sanitize_message", msg={"role": "assistant", "tool_calls": [
            {"id": "call_0123456789abcdef", "function": {"name": "f"}}]},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": "call_0123456789abcdef"}),
    case("_sanitize_message", msg={"role": "assistant", "tool_calls": [
            {"id": "abc123XYZ", "function": {"name": "f"}}]},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": "abc123XYZ"}),
    case("_sanitize_message", msg={"role": "assistant", "tool_calls": [
            {"function": {"name": "f"}, "call_id": "call_z"}]},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": "call_z"}),   # no id -> mapped appended
    case("_sanitize_message", msg={"role": "assistant", "tool_calls": [
            {"id": "i1", "function": {"name": "f"}}]},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": None}),
    case("_sanitize_message", msg={"role": "assistant", "tool_calls": [
            {"id": "i1", "call_id": "c1"}, {"id": "i2"}]},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": seq("c1", "")}),
    case("_sanitize_message", msg={"role": "assistant", "tool_calls": [
            {"id": "i1"}]},
         strip_extra_content=False, mistral_tool_ids=True, io={}),   # fail-closed
    case("_sanitize_message", msg={"role": "user", "content": "hi", "tool_calls": [{"id": "i1"}]},
         strip_extra_content=False, mistral_tool_ids=True,
         io={"coalesce_tool_call_id": "i1"}),   # non-assistant: tc loop still maps
)

# ── _normalize_tool_call ────────────────────────────────────────────────────
def tc(fn=None, **kw):
    d = dict(kw)
    if fn is not None:
        d["function"] = fn
    return d


add(
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "f", "arguments": "{\"a\":1}"}, id="i1")),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "hermes_tool_search", "arguments": None}, id="i2")),
    case("_normalize_tool_call", self={"_last_wire_aliases": {"hermes_tool_search": "tool_search"}},
         tc=tc(fn={"name": "hermes_tool_search", "arguments": "{}"}, id="i3")),
    case("_normalize_tool_call", self={"_last_wire_aliases": {"hermes_tool_search": "tool_search"}},
         tc=tc(fn={"name": "real_tool", "arguments": "{}"}, id="i4")),
    case("_normalize_tool_call", self={"_last_wire_aliases": {}},
         tc=tc(fn={"name": "hermes_tool_search", "arguments": "{}"}, id="i5")),
    case("_normalize_tool_call", self={"_last_wire_aliases": None}, tc=tc(id="i6")),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"arguments": "{}"}, id="i7")),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "f", "arguments": "{}", "thought_signature": "sig"}, id="i8",
               extra_content={"google": {"thought_signature": "gs"}})),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "f", "arguments": "{}", "model_extra": {"thought_signature": "msig"}},
               id="i9", model_extra={"extra_content": {"k": "v"}})),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "f", "arguments": "{}", "args_repaired": True}, id="i10")),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "f", "arguments": "{}", "args_repaired": 1}, id="i11")),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "f", "arguments": "{}", "args_repaired": False}, id="i12")),
    case("_normalize_tool_call", self={"_last_wire_aliases": None},
         tc=tc(fn={"name": "f", "arguments": None, "extra_content": None}, id=None)),
    case("_normalize_tool_call", self={},
         tc=tc(fn={"name": "f", "arguments": "{}"}, id="i14")),
)

# ── ToolCall._pd / NormalizedResponse._pd ───────────────────────────────────
for fname in ("ToolCall._pd", "NormalizedResponse._pd"):
    add(
        case(fname, provider_data={"call_id": "c1", "extra_content": {"g": 1}}, key="call_id"),
        case(fname, provider_data={"call_id": "c1"}, key="missing"),
        case(fname, provider_data=None, key="call_id"),
        case(fname, provider_data={}, key="call_id"),
        case(fname, provider_data=[1], key="call_id"),
        case(fname, provider_data="x", key="call_id"),
        case(fname, provider_data=True, key="call_id"),
        case(fname, provider_data={"nested": {"a": [1, 2]}}, key="nested"),
    )

# ── Usage.from_openai ───────────────────────────────────────────────────────
add(
    case("Usage.from_openai", u={"prompt_tokens": 10, "completion_tokens": 20, "total_tokens": 30}),
    case("Usage.from_openai", u={"prompt_tokens": 10}),
    case("Usage.from_openai", u={"prompt_tokens": None, "completion_tokens": 0, "total_tokens": False}),
    case("Usage.from_openai", u=None),
    case("Usage.from_openai", u={}),
    case("Usage.from_openai", u={"prompt_tokens": "5", "completion_tokens": 2.5, "total_tokens": 7}),
    case("Usage.from_openai", u={"prompt_tokens": 10, "extra_field": 99}),
    case("Usage.from_openai", u="plain"),
)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")
    if short:
        raise SystemExit(f"FUNCS WITH <5 CASES: {short}")


if __name__ == "__main__":
    main()