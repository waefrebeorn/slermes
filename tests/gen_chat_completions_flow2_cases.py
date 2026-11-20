#!/usr/bin/env python3
"""Generate tests/fixtures/chat_completions_flow2.jsonl — scripted-seam FLOW cases
for the Chat Completions transport remainder port (>=5 per func, 16 funcs).

Each case is {"func", "in": {...args..., "io": {seam: script}}}. The C harness and
the live oracle consume the SAME scripts. Script shapes: MISSING -> fail-closed
RuntimeError; null -> None; {"$seq":[...]} per-call queue; {"__raise__":cls,"msg":m}
raises; any other value reused (bare array = list result). ASCII scope; integral
floats collapse to ints; int finish_reason rides {"__int__":n}; non-serializable
content rides {"__pyobj__":s}/{"__pyobj_falsy__":s}; K3 identity rides
"__KIMI_K3_EFFORTS__"; OMIT_TEMPERATURE rides "__OMIT_TEMPERATURE__" verbatim.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "chat_completions_flow2.jsonl"

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_spec(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


# ── api_mode / convert_tools (trivial leaves) ───────────────────────────────
for i in range(5):
    case("api_mode", tag=i)
for tools in ([{"type": "function", "function": {"name": "f"}}], None, [],
              [{"type": "function"}, {"type": "web_search"}], "notalist"):
    case("convert_tools", tools=tools)

# ── _is_gemini_openai_compat_base_url ───────────────────────────────────────
for bu in ("https://generativelanguage.googleapis.com/v1beta/openai",
           "https://generativelanguage.googleapis.com/v1beta/openai///",
           "HTTPS://GENERATIVELANGUAGE.GOOGLEAPIS.COM/OPENAI",
           "https://generativelanguage.googleapis.com/v1",
           "https://aiplatform.googleapis.com/v1/openai",
           "", None, "  https://generativelanguage.googleapis.com/openai  ",
           "https://generativelanguage.googleapis.com/openai/extra"):
    case("_is_gemini_openai_compat_base_url", base_url=bu)

# ── _is_openai_api_base_url (urlparse transcription) ────────────────────────
for bu in ("https://api.openai.com/v1", "https://api.openai.com",
           "  https://API.OPENAI.com:443/v1  ", "https://evil.com",
           "api.openai.com", "https://api.openai.com.evil.com",
           "https://user:pass@api.openai.com", "http://[::1]", "http://[abc",
           None, "https://api.openai.com\n.evil.com", "https://api.o\npenai.com",
           "https://api.openai.com:abc", "//api.openai.com", "://api.openai.com",
           "https://api.openai.com.", "http://api.openai.com\x00",
           "\x01https://api.openai.com", "https://***@evil.com"):
    case("_is_openai_api_base_url", base_url=bu)

# ── _snake_case_gemini_thinking_config ──────────────────────────────────────
for cfg in ({"includeThoughts": True, "thinkingLevel": " HIGH ", "thinkingBudget": 2.9},
            {}, None, {"includeThoughts": "yes"}, {"thinkingLevel": "  "},
            {"thinkingBudget": True}, {"thinkingBudget": -2.7},
            {"includeThoughts": False, "thinkingLevel": "Low"},
            "notdict", [1, 2], {"thinkingLevel": "Medium", "thinkingBudget": 0}):
    case("_snake_case_gemini_thinking_config", config=cfg)

# ── _build_gemini_thinking_config ───────────────────────────────────────────
case("_build_gemini_thinking_config", model="gemini-2.5-flash", reasoning_config={"effort": "high"})
case("_build_gemini_thinking_config", model="gemini-3-pro", reasoning_config={"effort": "high"},
     io={"gemini_uses_thinking_level": True})
case("_build_gemini_thinking_config", model="gemini-3-flash", reasoning_config={"effort": "minimal"},
     io={"gemini_uses_thinking_level": True})
case("_build_gemini_thinking_config", model="gemini-3-flash", reasoning_config={"effort": "medium"},
     io={"gemini_uses_thinking_level": True})
case("_build_gemini_thinking_config", model="gemini-3-flash", reasoning_config={"enabled": False},
     io={"gemini_uses_thinking_level": True})
case("_build_gemini_thinking_config", model="gemini-2.5-pro", reasoning_config={"enabled": False})
case("_build_gemini_thinking_config", model="gemini-4-argon", reasoning_config={"effort": "high"},
     io={"clamp_effort": "high"})
case("_build_gemini_thinking_config", model="gemini-4-argon", reasoning_config={"enabled": False})
case("_build_gemini_thinking_config", model="gemini-4-argon", reasoning_config={"effort": "ultra"},
     io={"clamp_effort": "medium"})
case("_build_gemini_thinking_config", model="gemini-4-argon", reasoning_config={"effort": "high"},
     io={"clamp_effort": "bogus"})
case("_build_gemini_thinking_config", model="gemma-3", reasoning_config={"effort": "high"})
case("_build_gemini_thinking_config", model="google/gemini-3-pro", reasoning_config={"effort": "low"},
     io={"gemini_uses_thinking_level": True})
case("_build_gemini_thinking_config", model="gemini-3-flash", reasoning_config={"effort": "high"},
     io={"gemini_uses_thinking_level": False})
case("_build_gemini_thinking_config", model="gemini-3-flash", reasoning_config=None)
case("_build_gemini_thinking_config", model="gemini-3-pro", reasoning_config={"effort": "high"})  # fail-closed
case("_build_gemini_thinking_config", model="gemini-3-pro", reasoning_config={"effort": "high"},
     io={"gemini_uses_thinking_level": raise_spec("KeyboardInterrupt", "ki")})
case("_build_gemini_thinking_config", model="  Google/Gemini-3-Flash  ", reasoning_config={"effort": "xhigh"},
     io={"gemini_uses_thinking_level": True})
case("_build_gemini_thinking_config", model="gemini-3-flash", reasoning_config={"effort": "weird"},
     io={"gemini_uses_thinking_level": True})

# ── _raise_gemini_thinking_max_tokens ───────────────────────────────────────
case("_raise_gemini_thinking_max_tokens", model="gemini-2.5-flash", reasoning_config={"effort": "high"},
     requested=100, io={"_effective_gemini_max_output_tokens": 65535})
case("_raise_gemini_thinking_max_tokens", model="gemma-3", reasoning_config={"effort": "high"}, requested=100)
case("_raise_gemini_thinking_max_tokens", model="gemini-3-pro", reasoning_config={"effort": "high"},
     requested=50, io={"gemini_uses_thinking_level": True, "_effective_gemini_max_output_tokens": 65535})
case("_raise_gemini_thinking_max_tokens", model="gemini-2.5-flash", reasoning_config=None, requested="abc")
case("_raise_gemini_thinking_max_tokens", model="gemini-2.5-flash", reasoning_config={"effort": "high"},
     requested=100)  # fail-closed
case("_raise_gemini_thinking_max_tokens", model="gemini-2.5-flash", reasoning_config={"effort": "high"},
     requested=100, io={"_effective_gemini_max_output_tokens": raise_spec("SystemExit", "se")})

# ── _model_consumes_thought_signature ───────────────────────────────────────
case("_model_consumes_thought_signature", model="gemini-3-pro")
case("_model_consumes_thought_signature", model="gemma-3")
case("_model_consumes_thought_signature", model="other", provider="gemini")
case("_model_consumes_thought_signature", model="other", provider="vertex")
case("_model_consumes_thought_signature", model="other", provider="x", base_url=None,
     io={"_profile_replays_tool_call_signatures": False, "base_url_host_matches": {"$seq": [False, False]}})
case("_model_consumes_thought_signature", model="other", provider="x",
     io={"_profile_replays_tool_call_signatures": True})
case("_model_consumes_thought_signature", model="other", provider="x",
     io={"_profile_replays_tool_call_signatures": "yes"})
case("_model_consumes_thought_signature", model="other", provider="x",
     io={"_profile_replays_tool_call_signatures": False, "base_url_host_matches": True})
case("_model_consumes_thought_signature", model=None, provider=None, base_url=None,
     io={"_profile_replays_tool_call_signatures": False, "base_url_host_matches": {"$seq": [False, False]}})
case("_model_consumes_thought_signature", model="other", provider="x")  # fail-closed
case("_model_consumes_thought_signature", model="other", provider="x",
     io={"_profile_replays_tool_call_signatures": raise_spec("GeneratorExit", "ge")})
case("_model_consumes_thought_signature", model="GEMINI-3", provider=None)

# ── _static_prompt_instructions ─────────────────────────────────────────────
for msgs in ([{"role": "system", "content": "hello"}],
             [{"role": "developer", "content": "dev"}],
             [{"role": "user", "content": "x"}],
             [], None,
             [{"role": "system", "content": {"b": 2, "a": 1}}],
             [{"role": "system", "content": [1, 2, 3]}],
             [{"role": "system", "content": None}],
             [{"role": "system", "content": True}],
             [{"role": "system", "content": {"__pyobj__": "OBJ"}}],
             [{"role": "system", "content": {"__pyobj_falsy__": "F"}}],
             [{"role": "system"}],
             [{"role": ["system"], "content": "x"}],
             [{"role": "system", "content": "  "}],
             "abc",
             [{"role": "system", "content": {"nested": {"z": 1, "a": 2}}}],
             [{"role": "system", "content": 'a"b\\c\nd'}],
             [{"role": "system", "content": 42}],
             [{"role": "system", "content": {"__pyobj__": ""}}]):
    case("_static_prompt_instructions", messages=msgs)

# ── _add_prompt_cache_key ───────────────────────────────────────────────────
SYS_MSG = [{"role": "system", "content": "hi"}]
case("_add_prompt_cache_key", api_kwargs={"model": "m"}, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=True, session_id="s1", cache_scope_id=None,
     io={"_cache_scope_from_session_id": "scope1", "_content_cache_key": "pck_abc"})
case("_add_prompt_cache_key", api_kwargs={"prompt_cache_key": "x" * 70}, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=True, session_id="s1",
     io={"_bound_prompt_cache_key_field": {"__update__": {"prompt_cache_key": "bounded"}}})
case("_add_prompt_cache_key", api_kwargs={"extra_body": {"prompt_cache_key": "long"}}, messages=SYS_MSG,
     tools=None, supports_prompt_cache_key=True, session_id="s1",
     io={"_bound_prompt_cache_key_field": {"__pop__": ["prompt_cache_key"]}})
case("_add_prompt_cache_key", api_kwargs={"model": "m"}, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=False, session_id="s1")
case("_add_prompt_cache_key", api_kwargs={"model": "m"}, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=True, session_id="s1",
     io={"_cache_scope_from_session_id": "s", "_content_cache_key": None})
case("_add_prompt_cache_key", api_kwargs={"model": "m"}, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=True, session_id="s1", cache_scope_id="root",
     io={"_cache_scope_from_session_id": "rootscope", "_content_cache_key": "pck_x"})
case("_add_prompt_cache_key", api_kwargs=None, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=True, session_id="s1")
case("_add_prompt_cache_key", api_kwargs={"model": "m"}, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=True, session_id="s1")  # fail-closed
case("_add_prompt_cache_key", api_kwargs={"model": "m"}, messages=SYS_MSG, tools=None,
     supports_prompt_cache_key=True, session_id="s1",
     io={"_cache_scope_from_session_id": raise_spec("KeyboardInterrupt", "k")})
case("_add_prompt_cache_key", api_kwargs={"prompt_cache_key": "a", "extra_body": {"prompt_cache_key": "b"}},
     messages=SYS_MSG, tools=None, supports_prompt_cache_key=True, session_id="s1",
     io={"_bound_prompt_cache_key_field": {"$seq": [{"__update__": {"prompt_cache_key": "A"}},
                                                  {"__update__": {"prompt_cache_key": "B"}}]}})

# ── convert_messages ────────────────────────────────────────────────────────
UM = [{"role": "user", "content": "hi"}]
case("convert_messages", messages=UM, kwargs={"model": "gemini-3"},
     io={"_wire_reasoning_carriers": [], "_route_requires_mistral_tool_ids": False, "_sanitize_message": None})
case("convert_messages", messages=UM, kwargs={"model": "gemini"},
     io={"_wire_reasoning_carriers": ["reasoning"], "_route_requires_mistral_tool_ids": False,
         "_sanitize_message": {"role": "user", "content": "hi"}})
case("convert_messages", messages=[], kwargs={"model": "gemini"},
     io={"_wire_reasoning_carriers": [], "_route_requires_mistral_tool_ids": False})
case("convert_messages", messages=None, kwargs={"model": "gemini"},
     io={"_wire_reasoning_carriers": [], "_route_requires_mistral_tool_ids": False})
case("convert_messages", messages=UM, kwargs={"model": "other", "provider": "x"},
     io={"_profile_replays_tool_call_signatures": False, "base_url_host_matches": {"$seq": [False, False]},
         "_wire_reasoning_carriers": [], "_route_requires_mistral_tool_ids": False, "_sanitize_message": None})
case("convert_messages", messages=UM,
     kwargs={"model": "gemini", "provider_profile": {"name": "gemini", "native_reasoning_details_type": "x.native"}},
     io={"_wire_reasoning_carriers": ["reasoning"], "_route_requires_mistral_tool_ids": False, "_sanitize_message": None})
case("convert_messages", messages=UM, kwargs={"model": "gemini"})  # fail-closed
case("convert_messages", messages=UM, kwargs={"model": "gemini"},
     io={"_wire_reasoning_carriers": raise_spec("SystemExit", "s")})
case("convert_messages", messages=UM, kwargs={"model": "gemini"},
     io={"_wire_reasoning_carriers": [], "_route_requires_mistral_tool_ids": True, "_sanitize_message": None})
case("convert_messages", messages=[{"role": "user"}, {"role": "assistant", "content": "a"}],
     kwargs={"model": "gemini"},
     io={"_wire_reasoning_carriers": [], "_route_requires_mistral_tool_ids": False,
         "_sanitize_message": {"$seq": [None, {"role": "assistant", "content": "a"}]}})

# ── build_kwargs (legacy path; model gemini avoids _model_consumes seams) ───
def legacy_io(**over):
    io = {
        "data_use_requirement": "allow",
        "_wire_reasoning_carriers": [],
        "_route_requires_mistral_tool_ids": False,
        "_sanitize_message": None,
        "_swap_developer_role": UM,
        "_base_kwargs": {"model": "gemini-3-flash", "messages": UM},
        "_reasoning_config_for_model": None,
        "_apply_max_tokens": None,
        "requested_effort": None,
        "_finish_kwargs": {"model": "gemini-3-flash", "messages": UM, "prompt_cache_key": "pck"},
    }
    io.update(over)
    return io


BASE_PARAMS = {"provider_name": "openai", "base_url": "https://api.openai.com",
               "supports_prompt_cache_key": True}
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None, params=dict(BASE_PARAMS), io=legacy_io())
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, is_kimi=True),
     io=legacy_io(kimi_supported_efforts="__KIMI_K3_EFFORTS__", requested_effort=None,
                  _finish_kwargs={"model": "gemini-3-flash", "messages": UM, "reasoning_effort": "high",
                                  "extra_body": {"thinking": {"type": "enabled"}}, "prompt_cache_key": "pck"}))
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, is_kimi=True),
     io=legacy_io(kimi_supported_efforts=["low", "medium", "high"], requested_effort="xhigh",
                  clamp_effort="high",
                  _finish_kwargs={"model": "gemini-3-flash", "messages": UM, "reasoning_effort": "high",
                                  "extra_body": {"thinking": {"type": "enabled"}}, "prompt_cache_key": "pck"}))
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, is_tokenhub=True),
     io=legacy_io(requested_effort="low", tokenhub_effort="low",
                  _finish_kwargs={"model": "gemini-3-flash", "messages": UM, "reasoning_effort": "low",
                                  "prompt_cache_key": "pck"}))
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, supports_reasoning=True),
     io=legacy_io(_reasoning_config_for_model={"effort": "high"},
                  _finish_kwargs={"model": "gemini-3-flash", "messages": UM,
                                  "extra_body": {"reasoning": {"enabled": True, "effort": "high"}},
                                  "prompt_cache_key": "pck"}))
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, is_openrouter=True, provider_preferences={"order": ["x"]}),
     io=legacy_io(_finish_kwargs={"model": "gemini-3-flash", "messages": UM,
                                  "extra_body": {"provider": {"order": ["x"]}}, "prompt_cache_key": "pck"}))
case("build_kwargs", model="openrouter/pareto-code", messages=UM, tools=None,
     params=dict(BASE_PARAMS, is_openrouter=True, openrouter_min_coding_score=0.5),
     io=legacy_io(_swap_developer_role=UM, _base_kwargs={"model": "openrouter/pareto-code", "messages": UM},
                  _pareto_score=0.5,
                  _finish_kwargs={"model": "openrouter/pareto-code", "messages": UM,
                                  "extra_body": {"plugins": [{"id": "pareto-router", "min_coding_score": 0.5}]},
                                  "prompt_cache_key": "pck"}))
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, provider_name="gemini"),
     io=legacy_io(gemini_uses_thinking_level=True,
                  _finish_kwargs={"model": "gemini-3-flash", "messages": UM,
                                  "extra_body": {"thinking_config": {"includeThoughts": True, "thinkingLevel": "medium"}},
                                  "prompt_cache_key": "pck"}))
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, provider_name="gemini", base_url="https://generativelanguage.googleapis.com/v1/openai"),
     io=legacy_io(gemini_uses_thinking_level=True,
                  _finish_kwargs={"model": "gemini-3-flash", "messages": UM,
                                  "extra_body": {"extra_body": {"google": {"thinking_config":
                                   {"include_thoughts": True, "thinking_level": "medium"}}}}, "prompt_cache_key": "pck"}))
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None, params=dict(BASE_PARAMS))  # fail-closed
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None, params=dict(BASE_PARAMS),
     io={"data_use_requirement": raise_spec("KeyboardInterrupt", "k")})
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, request_overrides={"temperature": 0.2}),
     io=legacy_io(_finish_kwargs={"model": "gemini-3-flash", "messages": UM, "temperature": 0.2,
                                  "prompt_cache_key": "pck"}))
# profile dispatch path
case("build_kwargs", model="gemini-3-flash", messages=UM, tools=None,
     params=dict(BASE_PARAMS, provider_profile={"name": "gemini", "supports_prompt_cache_key": True}),
     io={
         "data_use_requirement": "allow",
         "_wire_reasoning_carriers": [], "_route_requires_mistral_tool_ids": False, "_sanitize_message": None,
         "prepare_messages": UM, "_swap_developer_role": UM,
         "_base_kwargs": {"model": "gemini-3-flash", "messages": UM},
         "_reasoning_config_for_model": None, "get_max_tokens": 1000, "_apply_max_tokens": None,
         "build_api_kwargs_extras": [{}, {}], "build_extra_body": {},
         "_finish_kwargs": {"model": "gemini-3-flash", "messages": UM, "prompt_cache_key": "pck"},
     })

# ── _build_kwargs_from_profile ──────────────────────────────────────────────
def profile_io(**over):
    io = {
        "prepare_messages": UM,
        "_swap_developer_role": UM,
        "_base_kwargs": {"model": "gemini-3-flash", "messages": UM},
        "_reasoning_config_for_model": None,
        "get_max_tokens": 1000,
        "_apply_max_tokens": None,
        "build_api_kwargs_extras": [{}, {}],
        "build_extra_body": {},
        "_finish_kwargs": {"model": "gemini-3-flash", "messages": UM, "prompt_cache_key": "pck"},
    }
    io.update(over)
    return io


PROF = {"name": "gemini", "supports_prompt_cache_key": True, "fixed_temperature": 0.3}
PPARAMS = {"base_url": "https://api.openai.com", "session_id": "s1"}
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS), io=profile_io())
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS), io=profile_io(build_extra_body={"reasoning": {"enabled": True}},
                                         _finish_kwargs={"model": "gemini-3-flash", "messages": UM,
                                                         "extra_body": {"reasoning": {"enabled": True}},
                                                         "prompt_cache_key": "pck"}))
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS, base_url="https://generativelanguage.googleapis.com"),
     io=profile_io(build_extra_body={"reasoning": {"enabled": True}, "thinking_config": {"includeThoughts": True}},
                   is_native_gemini_base_url=True,
                   _finish_kwargs={"model": "gemini-3-flash", "messages": UM,
                                   "extra_body": {"thinking_config": {"includeThoughts": True}},
                                   "prompt_cache_key": "pck"}))
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS, base_url="https://generativelanguage.googleapis.com"),
     io=profile_io(build_extra_body={"reasoning": {"enabled": True}},
                   _finish_kwargs={"model": "gemini-3-flash", "messages": UM,
                                   "extra_body": {"reasoning": {"enabled": True}}, "prompt_cache_key": "pck"}))
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS, request_overrides={"extra_body": {"x": 1}, "temperature": 0.5}),
     io=profile_io(build_extra_body={"y": 2},
                   _finish_kwargs={"model": "gemini-3-flash", "messages": UM, "temperature": 0.5,
                                   "extra_body": {"y": 2, "x": 1}, "prompt_cache_key": "pck"}))
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS, lmstudio_reasoning_options={"a": 1}),
     io=profile_io(build_api_kwargs_extras=[{"z": 3}, {"top": 9}],
                   _finish_kwargs={"model": "gemini-3-flash", "messages": UM, "top": 9,
                                   "extra_body": {"z": 3}, "prompt_cache_key": "pck"}))
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS))  # fail-closed
case("_build_kwargs_from_profile", profile=dict(PROF), model="gemini-3-flash", sanitized=UM, tools=None,
     params=dict(PPARAMS), io={"prepare_messages": raise_spec("SystemExit", "s")})
case("_build_kwargs_from_profile", profile=dict(PROF, fixed_temperature="__OMIT_TEMPERATURE__"),
     model="gemini-3-flash", sanitized=UM, tools=None, params=dict(PPARAMS), io=profile_io())

# ── normalize_response ──────────────────────────────────────────────────────
def norm_io(**over):
    io = {
        "_normalize_finish_reason": "stop",
        "_attr_or_model_extra": None,
        "declared_carrier_keys": [[], []],
    }
    io.update(over)
    return io


def resp(content="hello", finish_reason="stop", tool_calls=None, usage=None, **msgextra):
    msg = {"content": content}
    msg.update(msgextra)
    ch = {"message": msg, "finish_reason": finish_reason}
    if tool_calls is not None:
        msg["tool_calls"] = tool_calls
    r = {"choices": [ch]}
    if usage is not None:
        r["usage"] = usage
    return r


TC = {"id": "c1", "name": "f", "arguments": "{}", "provider_data": None}
case("normalize_response", response=resp(), io=norm_io())
case("normalize_response", response=resp(content=None, finish_reason=None), io=norm_io())
case("normalize_response", response=resp(finish_reason={"__int__": 2}),
     io=norm_io(_normalize_finish_reason="stop"))
case("normalize_response", response=resp(finish_reason=True), io=norm_io(_normalize_finish_reason="stop"))
case("normalize_response", response=resp(tool_calls=[{"id": "c1", "function": {"name": "f"}}]),
     io=norm_io(_normalize_tool_call=TC))
case("normalize_response", response=resp(tool_calls=[{"id": "c1"}, {"id": "c2"}]),
     io=norm_io(_normalize_tool_call={"$seq": [TC, None]}))
case("normalize_response", response=resp(usage={"prompt_tokens": 5, "completion_tokens": 3, "total_tokens": 8}),
     io=norm_io(usage_from_openai={"prompt_tokens": 5, "completion_tokens": 3, "total_tokens": 8, "cached_tokens": 0}))
case("normalize_response", response=resp(reasoning_content="think"),
     io=norm_io(_attr_or_model_extra={"$seq": ["think", None, None, None]}))
case("normalize_response", response=resp(refusal="no"),
     io=norm_io(_attr_or_model_extra={"$seq": [None, None, None, "no"]}))
case("normalize_response", response=resp(content="  ", refusal="no", finish_reason="stop"),
     io=norm_io(_attr_or_model_extra={"$seq": [None, None, None, "no"]}))
case("normalize_response", response=resp(reasoning_details=[{"type": "x"}]),
     io=norm_io(declared_carrier_keys=[["extra_content"], ["reasoning_text"]],
                _attr_or_model_extra={"$seq": [None, None, None]}))
case("normalize_response", response=resp(reasoning="explicit"),
     io=norm_io(declared_carrier_keys=[[], ["reasoning_text"]]))
case("normalize_response", response={"choices": []}, io=norm_io())
case("normalize_response", response={"choices": None}, io=norm_io())
case("normalize_response", response={}, io=norm_io())
case("normalize_response", response=None, io=norm_io())
case("normalize_response", response=resp())  # fail-closed
case("normalize_response", response=resp(), io={"_normalize_finish_reason": raise_spec("KeyboardInterrupt", "k")})
case("normalize_response", response=resp(content=None, refusal="because", finish_reason="stop",
                                         tool_calls=[{"id": "c1"}]),
     io=norm_io(_normalize_tool_call=TC, _attr_or_model_extra={"$seq": [None, None, None, "because"]}))

# ── validate_response ───────────────────────────────────────────────────────
case("validate_response", response={"choices": [{"message": {"content": "hi"}}]},
     io={"is_router_timeout_shim": False})
case("validate_response", response={"choices": [{"message": {"content": "hi"}}]},
     io={"is_router_timeout_shim": True})
case("validate_response", response={"choices": []})
case("validate_response", response={"choices": None})
case("validate_response", response={})
case("validate_response", response=None)
case("validate_response", response=5)
case("validate_response", response={"choices": [{"message": {}}]})  # fail-closed
case("validate_response", response={"choices": [{"message": {}}]},
     io={"is_router_timeout_shim": raise_spec("SystemExit", "s")})

# ── extract_cache_stats ─────────────────────────────────────────────────────
case("extract_cache_stats", response={"usage": {"prompt_tokens_details": {"cached_tokens": 10, "cache_write_tokens": 4}}})
case("extract_cache_stats", response={"usage": {"prompt_tokens_details": {"cached_tokens": 0, "cache_write_tokens": 0}}})
case("extract_cache_stats", response={"usage": {"prompt_cache_hit_tokens": 7}})
case("extract_cache_stats", response={"usage": {"prompt_tokens_details": {"cached_tokens": 0}, "prompt_cache_hit_tokens": 7}})
case("extract_cache_stats", response={"usage": {}})
case("extract_cache_stats", response={"usage": None})
case("extract_cache_stats", response={})
case("extract_cache_stats", response=None)
case("extract_cache_stats", response={"usage": {"prompt_tokens_details": {"cached_tokens": 2.5}}})
case("extract_cache_stats", response={"usage": {"prompt_tokens_details": None, "prompt_cache_hit_tokens": 3}})
case("extract_cache_stats", response=5)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn, n in sorted(counts.items()):
        flag = "" if n >= 5 else "  (<5!)"
        print(f"  {fn}: {n}{flag}")
    assert all(n >= 5 for n in counts.values()), "every func needs >=5 cases"
    assert len(counts) == 16, f"expected 16 funcs, got {len(counts)}"


if __name__ == "__main__":
    main()