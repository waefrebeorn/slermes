#!/usr/bin/env python3
"""Generate tests/auxiliary_client_flow_cases.jsonl — scripted-I/O cases for
the auxiliary_client FLOW oracle. Each line is {"func","in"} consumed by
t_port_auxiliary_client_flow.c (C seams) and oracle_auxiliary_client_flow.py
(live recording stubs fed the SAME scripts).

Run: python tests/gen_auxiliary_client_flow_cases.py > tests/auxiliary_client_flow_cases.jsonl

Script conventions (mirrored by the harness dispatcher and the oracle stubs):
  io.<seam> = plain value (reused for every call) | list (queue, call order)
  {"__script__": true, "ret": v} / {"__script__": true, "raise": exc_spec}
  perform results: raw response for call/retry/fallback steps;
    {"resp","failure","repairs"} for "attempt" steps.
  rate_limit_retry_rungs: {"steps":[...], "resp":..., "failure":...}.
"""
import json
import sys

CASES = []
_EID = [0]

def add(func, **inp):
    CASES.append({"func": func, "in": inp})

def E(msg, type="RuntimeError", status=None, classified=None, base_only=False):
    _EID[0] += 1
    d = {"type": type, "message": msg, "exc_id": f"e{_EID[0]}"}
    if status is not None:
        d["status_code"] = status
    if classified is not None:
        d["classified"] = classified
    if base_only:
        d["base_only"] = True
    return d

def V(reason=None, ctx=None, rotate=False, fallback=False, retryable=True, model=None):
    return {"reason": reason, "error_context": ctx or {}, "retryable": retryable,
            "should_rotate_credential": rotate, "should_fallback": fallback, "model": model}

def F(exc, verdict, rejects=False, client=None):
    return {"error": exc, "verdict": verdict, "rejects_parameter": rejects, "client": client}

def S(ret=None):
    d = {"__script__": True}
    if ret is not None:
        d["ret"] = ret
    return d

def SR(type="RuntimeError", message="boom", status=None):
    d = {"type": type, "message": message}
    if status is not None:
        d["status_code"] = status
    return {"__script__": True, "raise": d}

def CL(cid, base_url="https://api.example.com/v1", api_key="k1", backend=None, kind=None, effective=None):
    d = {"id": cid, "base_url": base_url, "api_key": api_key}
    if backend is not None:
        d["_hermes_aux_backend_provider"] = backend
    if kind is not None:
        d["kind"] = kind
    if effective is not None:
        d["_hermes_aux_effective_provider"] = effective
    return d

def ROUTE(client="c1", task="title_generation", resolved_provider="openrouter",
          final_model="m1", base_info="https://api.example.com/v1", **kw):
    r = {"client": client, "task": task, "tag": "", "async_mode": False,
         "base_info": base_info, "resolved_provider": resolved_provider,
         "resolved_model": kw.get("resolved_model", "m1"),
         "resolved_base_url": kw.get("resolved_base_url", base_info),
         "resolved_api_key": kw.get("resolved_api_key", "k1"),
         "resolved_api_mode": kw.get("resolved_api_mode", None),
         "final_model": final_model, "main_runtime": kw.get("main_runtime", None),
         "route_info": kw.get("route_info", None), "timeout": kw.get("timeout", 30.0)}
    return r

def DEST(provider="p2", base_url="https://p2.example/v1", api_mode=None, model="m2"):
    return {"provider": provider, "base_url": base_url, "api_mode": api_mode, "model": model}

# ── _field / _message_field / _contains_any ──────────────────────────────
add("_field", obj={"choices": [1]}, key="choices")
add("_field", obj={"a": 1}, key="b", default="dflt")
add("_field", obj={"__obj__": True, "data": {"x": 1}}, key="data")
add("_field", obj={"a": None}, key="a", default=7)
add("_message_field", msg={"message": {"role": "assistant"}}, name="message")
add("_message_field", msg={"role": "user"}, name="message")
add("_message_field", msg={"__obj__": True, "content": "hi"}, name="content")
add("_contains_any", text="unsupported parameter: temperature", needles=["temperature", "max_tokens"])
add("_contains_any", text="all good", needles=["bad", "worse"])
add("_contains_any", text="anything", needles=[])

# ── creds / timeout coercion ──────────────────────────────────────────────
add("_creds_pair", creds={"api_key": " sk-1 ", "base_url": " https://x.ai/v1/ "})
add("_creds_pair", creds={"api_key": "", "base_url": "https://x.ai"})
add("_creds_pair", creds={"api_key": "sk-1", "base_url": "   "})
add("_creds_pair", creds={"api_key": "sk-1", "base_url": "https://x.ai///"})
add("_creds_have_api_key", creds={"api_key": " sk "})
add("_creds_have_api_key", creds={"api_key": "   "})
add("_creds_have_api_key", creds={})
add("_coerce_positive_timeout", raw=30)
add("_coerce_positive_timeout", raw=0.5)
add("_coerce_positive_timeout", raw=0)
add("_coerce_positive_timeout", raw=-3)
add("_coerce_positive_timeout", raw=True)
add("_coerce_positive_timeout", raw="30")

# ── parameter strips ──────────────────────────────────────────────────────
add("_without_temperature", kwargs={"model": "m", "temperature": 0.7})
add("_without_temperature", kwargs={"model": "m"})
add("_without_temperature", kwargs={"temperature": None, "model": "m"})
add("_without_max_tokens", kwargs={"max_tokens": 100, "max_completion_tokens": 200, "model": "m"})
add("_without_max_tokens", kwargs={"model": "m"})
add("_without_max_tokens", kwargs={"max_completion_tokens": 5})
add("_without_reasoning_fields", kwargs={"reasoning_effort": "none", "model": "m"})
add("_without_reasoning_fields", kwargs={"_reasoning_config": {"enabled": False}})
add("_without_reasoning_fields", kwargs={"extra_body": {"reasoning": {"enabled": False}, "user": "u"}})
add("_without_reasoning_fields", kwargs={"extra_body": {"thinking": True}})
add("_without_reasoning_fields", kwargs={"model": "m"})
add("_without_reasoning_fields", kwargs={"reasoning_effort": None})

# ── budget from error / latency / route info ─────────────────────────────
add("_affordable_max_tokens_from_error", exc=E("credit limited, but can only afford 7117 tokens"))
add("_affordable_max_tokens_from_error", exc=E("can only afford 10,000"))
add("_affordable_max_tokens_from_error", exc=E("can only afford 500"))
add("_affordable_max_tokens_from_error", exc=E("no budget info here"))
add("_affordable_max_tokens_from_error", exc=E("CAN ONLY AFFORD  2000"))
add("_elapsed_ms", started_at=10.0, now=11.5)
add("_elapsed_ms", started_at=10.0, now=9.0)
add("_elapsed_ms", started_at=10.0, now=None, io={"time.monotonic": 10.25})
add("_stamp_latency_once", latency_info={}, key="first_token", started_at=5.0,
    io={"time.monotonic": 5.75})
add("_stamp_latency_once", latency_info={"first_token": 100}, key="first_token", started_at=5.0,
    io={"time.monotonic": 9.0})
add("_stamp_latency_once", latency_info=None, key="k", started_at=0.0)
add("_record_route_info", route_info={}, provider="nous", model="m9")
add("_record_route_info", route_info={"x": 1}, provider=None, model=None)
add("_record_route_info", route_info=None, provider="p", model="m")
add("_fallback_lane", client=CL("c1"), model="m1", label="fallback_chain[0](p2)")
add("_fallback_lane", client=None, model=None, label="main-agent(p3)")
add("_fallback_lane", client={"id": "c9", "base_url": None}, model="m", label="L")

# ── response parsing ──────────────────────────────────────────────────────
add("_coerce_llm_message", response=None)
add("_coerce_llm_message", response="plain text")
add("_coerce_llm_message", response={"id": "x"})
add("_coerce_llm_message", response={"choices": [{"message": {"content": "hi"}}]})
add("_coerce_llm_message", response={"__obj__": True, "id": "x"})
add("_coerce_llm_message", response={"choices": None})
add("_dedupe_tool_names", tools=[
    {"function": {"name": "a"}}, {"function": {"name": "a"}}, {"function": {"name": "b"}}],
    provider="p", model="m")
add("_dedupe_tool_names", tools=[{"function": None}, {}, {"function": {"name": ""}}],
    provider="p", model="m")
add("_dedupe_tool_names", tools=[], provider="p", model="m")
add("_tag_backend", client=CL("c1"), provider="nous")
add("_tag_backend", client=CL("c1"), provider="")
add("_tag_backend", client=None, provider="nous")
add("_provider_outcome", response={"ok": 1}, error=None, judged=[])
add("_provider_outcome", response=None, error=E("plain failure"), judged=[])
add("_provider_outcome", response=None,
    error=E("inner 402", type="_ClassifiedAttempt",
            classified=F(E("inner 402", status=402), V("billing", rotate=True))),
    judged=[])

# ── ladder step resolution / failure sender ──────────────────────────────
add("_ladder_step_call", step={"kind": "call", "args": ["c1", {"model": "m"}]},
    req={"resolved_provider": "nous", "resolved_api_mode": "responses"},
    retry_kwargs={}, candidate_kwargs={})
add("_ladder_step_call", step={"kind": "retry_same_provider", "args": ["nous", "m2", ["without_temperature"]]},
    req={"resolved_provider": "auto", "resolved_api_mode": None},
    retry_kwargs={"task": "t", "max_tokens": 5}, candidate_kwargs={"task": "t"})
add("_ladder_step_call", step={"kind": "fallback", "args": ["c2", "m2", "L"]},
    req={"resolved_provider": "auto", "resolved_api_mode": None},
    retry_kwargs={}, candidate_kwargs={"task": "t"})
add("_failure_sender", route=ROUTE(), failure=F(E("x"), V(), client=None),
    clients={"c1": CL("c1")})
add("_failure_sender", route=ROUTE(), failure=F(E("x"), V(), client="c2"),
    clients={"c1": CL("c1"), "c2": CL("c2")})

# ── rejection detectors ───────────────────────────────────────────────────
add("_is_structured_output_rejection", exc=E("bad", status=500))
add("_is_structured_output_rejection", exc=E("guided_grammar compile failed", status=400))
add("_is_structured_output_rejection",
    exc=E("Extra inputs are not permitted: response_format", status=422))
add("_is_structured_output_rejection", exc=E("response_format.json_schema str type expected"))
add("_is_structured_output_rejection", exc=E("some other error"),
    io={"_is_unsupported_parameter_error": [True]})
add("_is_structured_output_rejection", exc=E("some other error"),
    io={"_is_unsupported_parameter_error": [False, False]})
add("_is_reasoning_field_rejection", exc=E("x", status=500))
add("_is_reasoning_field_rejection", exc=E("Unrecognized request argument supplied: reasoning_effort", status=400),
    io={"is_reasoning_field_rejection": True})
add("_is_reasoning_field_rejection", exc=E("x"), io={"is_reasoning_field_rejection": False})
add("_is_reasoning_required_rejection", exc=E("x", status=404))
add("_is_reasoning_required_rejection", exc=E("Reasoning is mandatory for this endpoint", status=400),
    io={"is_reasoning_required_rejection": True})
add("_is_reasoning_required_rejection", exc=E("x"), io={"is_reasoning_required_rejection": False})
add("_is_max_tokens_rejection", exc=E("max_tokens is not supported"), client=CL("c1"))
add("_is_max_tokens_rejection", exc=E("unsupported_parameter"), client=CL("c1"))
add("_is_max_tokens_rejection", exc=E("error code 1210"),
    client={"id": "cz", "base_url": "https://open.bigmodel.cn/api"})
add("_is_max_tokens_rejection", exc=E("mystery"), client=CL("c1"),
    io={"_is_unsupported_parameter_error": True})
add("_param_rung_accepts", failure=F(E("x"), V(), rejects=True))
add("_param_rung_accepts", failure=F(E("x"), V()), io={"ladder_accepts": True})
add("_param_rung_accepts", failure=F(E("x"), V()), io={"ladder_accepts": False})

# ── fast lane / selection ─────────────────────────────────────────────────
add("_task_prefers_fast_model", task="compression")
add("_task_prefers_fast_model", task="title_generation",
    io={"_get_auxiliary_task_config": {"prefer_fast_model": "yes"}})
add("_task_prefers_fast_model", task="title_generation",
    io={"_get_auxiliary_task_config": {"prefer_fast_model": "0"}})
add("_task_prefers_fast_model", task="title_generation",
    io={"_get_auxiliary_task_config": {}})
add("_fast_lane_config_fields", config={"provider": " Nous ", "model": " m1 ", "reasoning_effort": "none"},
    io={"parse_reasoning_effort": {"enabled": False}})
add("_fast_lane_config_fields", config={"provider": None, "model": None},
    io={"parse_reasoning_effort": None})
add("_fast_lane_config_fields", config={"provider": "p", "model": "m", "reasoning_effort": "high"},
    io={"parse_reasoning_effort": {"enabled": True, "effort": "high"}})
add("_compression_config_claims_fast_lane",
    config={"provider": "nous", "model": "m1", "reasoning_effort": "none"},
    io={"parse_reasoning_effort": {"enabled": False}})
add("_compression_config_claims_fast_lane",
    config={"provider": "auto", "model": "m1", "reasoning_effort": "none"},
    io={"parse_reasoning_effort": {"enabled": False}})
add("_compression_config_claims_fast_lane",
    config={"provider": "nous", "model": "auto", "reasoning_effort": "none"},
    io={"parse_reasoning_effort": {"enabled": False}})
add("_compression_config_claims_fast_lane",
    config={"provider": "nous", "model": "m1"}, io={"parse_reasoning_effort": None})
add("_compression_fast_lane_controls", task="vision", max_tokens=None, extra_body={"a": 1})
add("_compression_fast_lane_controls", task="compression", max_tokens=100, extra_body={"a": 1})
add("_compression_fast_lane_controls", task="compression", max_tokens=None,
    actual_provider="nous", actual_model="m1", requested_provider="nous", requested_model="m1",
    route_config={}, leak_guard_config={}, extra_body={"a": 1},
    io={"resolve_compression_fast_lane": {"certified_non_reasoning": True,
                                          "reasoning_config": {"enabled": False, "effort": "none"}}})
add("_compression_fast_lane_controls", task="compression", max_tokens=None,
    actual_provider="nous", actual_model="m1", route_config={}, leak_guard_config={},
    extra_body={"reasoning": {"effort": "high"}},
    io={"resolve_compression_fast_lane": {"certified_non_reasoning": True,
                                          "reasoning_config": {"enabled": False}}})
add("_compression_fast_lane_controls", task="compression", max_tokens=None,
    actual_provider="p", actual_model="m", route_config={},
    leak_guard_config={"provider": "nous", "model": "m1", "reasoning_effort": "none"},
    extra_body={"reasoning": {"effort": "high"}, "b": 2},
    io={"resolve_compression_fast_lane": {"certified_non_reasoning": False, "reasoning_config": None},
        "parse_reasoning_effort": {"enabled": False}})
add("_compression_fast_lane_controls", task="compression", max_tokens=None,
    actual_provider="p", actual_model="m", route_config={}, leak_guard_config={},
    extra_body={"reasoning": {"effort": "high"}},
    io={"resolve_compression_fast_lane": {"certified_non_reasoning": False, "reasoning_config": None},
        "parse_reasoning_effort": None})
add("_get_task_no_progress_timeout", task="")
add("_get_task_no_progress_timeout", task="compression", io={"_get_auxiliary_task_config": {}})
add("_get_task_no_progress_timeout", task="compression",
    io={"_get_auxiliary_task_config": {"no_progress_timeout": "30.5"}})
add("_get_task_no_progress_timeout", task="compression",
    io={"_get_auxiliary_task_config": {"no_progress_timeout": True}})
add("_get_task_no_progress_timeout", task="compression",
    io={"_get_auxiliary_task_config": {"no_progress_timeout": "abc"}})
add("_get_task_no_progress_timeout", task="compression",
    io={"_get_auxiliary_task_config": {"no_progress_timeout": 120}})
add("_unwrap_moa_provider", prov="nous", mdl="m1")
add("_unwrap_moa_provider", prov=" MOA ", mdl="preset1",
    io={"_resolve_moa_aggregator": {"provider": "openrouter", "model": "agg"}})
add("_unwrap_moa_provider", prov="moa", mdl=None,
    io={"_resolve_moa_aggregator": {"provider": None, "model": None}})
add("_preserve_provider_with_base_url", prov="")
add("_preserve_provider_with_base_url", prov="custom:foo")
add("_preserve_provider_with_base_url", prov="ollama")
add("_preserve_provider_with_base_url", prov="nous", io={"get_provider": {"name": "nous"}})
add("_preserve_provider_with_base_url", prov="myp",
    io={"get_provider": None, "_get_named_custom_provider": {"name": "myp"}})
add("_preserve_provider_with_base_url", prov="unknown",
    io={"get_provider": None, "_get_named_custom_provider": None})
add("_builtin_provider_present", name="nous", io={"get_provider": {"name": "nous"}})
add("_builtin_provider_present", name="foo", io={"get_provider": None})
add("_builtin_provider_present", name="anthropic", io={"get_provider": SR("ImportError", "no catalog")})
add("_builtin_provider_present", name="envproc",
    io={"get_provider": SR("ImportError", "no catalog"),
        "get_provider_profile": {"name": "envproc", "ambient_credentials": False,
                                 "auth_type": "external_process"}})
add("_profile_credentials_bypass_custom", name="amb",
    io={"get_provider_profile": {"name": "amb", "ambient_credentials": True, "auth_type": "api_key"}})
add("_profile_credentials_bypass_custom", name="ext",
    io={"get_provider_profile": {"name": "ext", "ambient_credentials": False,
                                 "auth_type": "external_process"}})
add("_profile_credentials_bypass_custom", name="other",
    io={"get_provider_profile": {"name": "different", "ambient_credentials": True, "auth_type": ""}})
add("_profile_credentials_bypass_custom", name="x", io={"get_provider_profile": SR("RuntimeError", "no reg")})
add("_named_custom_provider_present", name="myp", io={"_get_named_custom_provider": {"name": "myp"}})
add("_named_custom_provider_present", name="myp", io={"_get_named_custom_provider": None})
add("_named_custom_provider_present", name="myp", io={"_get_named_custom_provider": SR("RuntimeError", "no cfg")})
add("_routes_to_custom_endpoint", provider_norm="custom", io={"_normalize_aux_provider": "custom"})
add("_routes_to_custom_endpoint", provider_norm="myp",
    io={"_normalize_aux_provider": "myp", "_get_named_custom_provider": {"name": "myp"}})
add("_routes_to_custom_endpoint", provider_norm="nous",
    io={"_normalize_aux_provider": "nous", "_get_named_custom_provider": None})
add("_classification_provider", client=CL("c1", backend="nous"), fallback="auto")
add("_classification_provider", client=CL("c1"), fallback="openrouter",
    io={"_effective_provider_for_client": "openrouter",
        "_fallback_provider_from_label": "openrouter",
        "_normalize_aux_provider": "openrouter"})
add("_classification_provider", client=CL("c1"), fallback="auto",
    io={"_effective_provider_for_client": "auto", "_fallback_provider_from_label": "auto",
        "_normalize_aux_provider": "auto", "_normalize_main_runtime": {},
        "_recoverable_pool_provider": "nous"})
add("_classification_provider", client=CL("c1"), fallback="custom:x",
    io={"_effective_provider_for_client": "custom:x",
        "_fallback_provider_from_label": "custom:x",
        "_normalize_aux_provider": "custom", "_normalize_main_runtime": {},
        "_recoverable_pool_provider": None})
add("_provider_for_host", base_url="https://api.moonshot.ai/v1",
    table=[["api.moonshot.ai", "moonshot"], ["openrouter.ai", "openrouter"]],
    io={"base_url_host_matches": [True]})
add("_provider_for_host", base_url="https://other.example/v1",
    table=[["api.moonshot.ai", "moonshot"]],
    io={"base_url_host_matches": [False], "profile_for_host": {"name": "prof"}})
add("_provider_for_host", base_url="https://other.example/v1", table=[],
    io={"profile_for_host": None})
add("_relay_path", base_url="https://x.ai/v1",
    io={"_to_openai_base_url": "https://x.ai/v1", "base_url_path": "/v1"})
add("_relay_path", base_url="https://gw.example/anthropic",
    io={"_to_openai_base_url": "https://gw.example/anthropic", "base_url_path": "/anthropic"})
add("_relay_path", base_url="https://gw.example/v1/",
    io={"_to_openai_base_url": "https://gw.example/v1/", "base_url_path": "/v1"})
add("_context_too_small", entry={}, provider="p", model="m", min_ctx=None, task="t", label="L")
add("_context_too_small", entry={"base_url": "https://x"}, provider="p", model="m", min_ctx=1000,
    task="t", label="L", io={"_fallback_entry_api_key": "k", "_candidate_context_window": 2000})
add("_context_too_small", entry={}, provider="p", model="m", min_ctx=1000, task="t", label="L",
    io={"_fallback_entry_api_key": None, "_candidate_context_window": 500})
add("_context_too_small", entry={}, provider="p", model="m", min_ctx=1000, task="t", label="L",
    io={"_fallback_entry_api_key": None, "_candidate_context_window": None})
add("_nous_on_messages_wire", provider_norm="openrouter", model="m")
add("_nous_on_messages_wire", provider_norm="nous", model="claude",
    io={"nous_api_mode": "anthropic_messages"})
add("_nous_on_messages_wire", provider_norm="nous-portal", model="gpt",
    io={"nous_api_mode": "chat_completions"})
add("_is_gemini_native_route", provider_norm="gemini", effective_base="")
add("_is_gemini_native_route", provider_norm="other", effective_base="")
add("_is_gemini_native_route", provider_norm="other", effective_base="https://generativelanguage.googleapis.com",
    io={"is_native_gemini_base_url": True})
add("_is_gemini_native_route", provider_norm="other", effective_base="https://x",
    io={"is_native_gemini_base_url": SR("ImportError", "no adapter")})
add("_forwards_max_tokens", provider="anthropic", provider_norm="anthropic", model="m",
    effective_base="https://api.anthropic.com", task="t",
    io={"_is_anthropic_compat_endpoint": True})
add("_forwards_max_tokens", provider="nous", provider_norm="nous", model="claude",
    effective_base="https://inference-api.nousresearch.com/v1", task="t",
    io={"_is_anthropic_compat_endpoint": False, "nous_api_mode": "anthropic_messages"})
add("_forwards_max_tokens", provider="nim", provider_norm="nim", model="m",
    effective_base="https://x", task="t", io={"_is_anthropic_compat_endpoint": False})
add("_forwards_max_tokens", provider="p", provider_norm="p", model="m",
    effective_base="https://x", task="moa_reference",
    io={"_is_anthropic_compat_endpoint": False, "base_url_host_matches": False})
add("_forwards_max_tokens", provider="or", provider_norm="openrouter", model="m",
    effective_base="https://x", task="t",
    io={"_is_anthropic_compat_endpoint": False, "base_url_host_matches": False})
add("_forwards_max_tokens", provider="p", provider_norm="p", model="m",
    effective_base="https://x", task="t",
    io={"_is_anthropic_compat_endpoint": False, "base_url_host_matches": False,
        "_is_managed_local_endpoint": False, "profile_forwards_max_tokens": False})
add("_stream_request_plan", kwargs={"model": "m", "timeout": 30}, client=CL("c1"),
    io={"_is_managed_local_endpoint": False, "_aux_stream_total_ceiling": 600.0})
add("_stream_request_plan", kwargs={"model": "m", "timeout": 30},
    client={"id": "cl", "base_url": "http://127.0.0.1:8080"},
    io={"_is_managed_local_endpoint": True,
        "with_prompt_progress": {"model": "m", "timeout": 30, "stream": True,
                                 "stream_options": {"include_usage": True}, "prompt_progress": True},
        "_aux_stream_total_ceiling": 600.0})
add("_stream_request_plan", kwargs={"timeout": None}, client=None,
    io={"_is_managed_local_endpoint": False, "_aux_stream_total_ceiling": 600.0})
add("_with_custom_endpoint_extra_body", extra_body={"a": 1}, provider="p", model="m", base_url="")
add("_with_custom_endpoint_extra_body", extra_body={"user": "caller"}, provider="p", model="m",
    base_url="https://proxy.example/v1",
    io={"load_config_readonly": {}, "get_compatible_custom_providers": [],
        "_custom_provider_extra_body_for_agent": {"user": "entry", "org": "o"}})
add("_with_custom_endpoint_extra_body", extra_body=None, provider="p", model="m",
    base_url="https://proxy.example/v1",
    io={"load_config_readonly": {}, "get_compatible_custom_providers": [],
        "_custom_provider_extra_body_for_agent": {}})
add("_with_custom_endpoint_extra_body", extra_body={"a": 1}, provider="p", model="m",
    base_url="https://proxy.example/v1",
    io={"load_config_readonly": SR("RuntimeError", "no config")})
add("_merge_aux_extra_body", extra_body={"reasoning_effort": "none", "user": "u"},
    projection={"body": {"model": "pm"}, "reasoning_extra": {"reasoning": {"effort": "low"}},
                "handles_reasoning": True},
    reasoning_config=None, provider_norm="openrouter",
    io={"clamp_reasoning_config": {"effort": "low"}})
add("_merge_aux_extra_body", extra_body={"reasoning": {"effort": "high"}},
    projection={"body": {}, "reasoning_extra": {}, "handles_reasoning": True},
    reasoning_config={"enabled": False}, provider_norm="p")
add("_merge_aux_extra_body", extra_body={},
    projection={"body": {}, "reasoning_extra": {}, "handles_reasoning": False},
    reasoning_config={"enabled": True, "effort": "medium"}, provider_norm="p",
    io={"generic_nested_reasoning": {"reasoning": {"effort": "medium"}}})
add("_merge_aux_extra_body", extra_body={"reasoning": {"effort": "ultra"}},
    projection={"body": {}, "reasoning_extra": {}, "handles_reasoning": True},
    reasoning_config=None, provider_norm="p",
    io={"clamp_reasoning_config": {"effort": "high"}})
add("_merge_aux_extra_body", extra_body={},
    projection={"body": {}, "reasoning_extra": {}, "handles_reasoning": True},
    reasoning_config=None, provider_norm="nous",
    io={"_nous_portal_tags": ["aux"], "get_conversation_context": "sess-1"})
add("_unwrap_data_envelope", response={"choices": [{"message": {"content": "ok"}}]}, task="t",
    io={"model_validate": SR("AttributeError", "no method")})
add("_unwrap_data_envelope", response={"data": "notdict"}, task="t",
    io={"model_validate": SR("AttributeError", "no method")})
add("_unwrap_data_envelope", response={"success": False, "data": {"error": {"message": "quota"}}},
    task="compression", io={"model_validate": SR("AttributeError", "no method")})
add("_unwrap_data_envelope",
    response={"data": {"id": "d1", "model": "m", "choices": [{"message": {"content": "hi"}, "finish_reason": None}]}},
    task=None, io={"model_validate": SR("AttributeError", "no method")})
add("_unwrap_data_envelope",
    response={"data": {"choices": [{"message": {"content": "hi"}}]}},
    task="t", io={"model_validate": S({"validated": True})})
add("_note_relay_auxiliary_error", exc=E("x"), failure=None, context=None)
_same_exc = E("same", type="RuntimeError")
add("_note_relay_auxiliary_error", exc=_same_exc,
    failure=None,
    context={"provider": "p", "model": "m", "attempt_count": 2,
             "attempt": {"error": _same_exc, "provider": "p",
                         "model": "m", "failure": None, "attempt": 2}})
add("_note_relay_auxiliary_error", exc=E("new"), failure=F(E("new"), V("auth", rotate=True)),
    context={"provider": "p", "model": "m", "attempt_count": 1, "attempt": None})
add("_note_relay_auxiliary_error", exc=E("cancel", type="BaseException", base_only=True),
    failure=None, context={"provider": "p", "model": "m", "attempt_count": 0, "attempt": None})
add("_should_skip_same_provider_retry", task="compression",
    failure=F(E("request timed out after 30s"), V("timeout", ctx={"transport": "timeout"})))
add("_should_skip_same_provider_retry", task="compression",
    failure=F(E("no-progress timeout after 60s"), V("timeout", ctx={"transport": "timeout"})))
add("_should_skip_same_provider_retry", task="chat",
    failure=F(E("timed out"), V("timeout", ctx={"transport": "timeout"})))
add("_should_skip_same_provider_retry", task="vision",
    failure=F(E("conn reset"), V("timeout", ctx={"transport": "connection"})))
add("_should_retry_same_provider", task="chat", failure=F(E("x"), V("overloaded")), tag="",
    io={"same_provider_retry": False})
add("_should_retry_same_provider", task="compression",
    failure=F(E("timed out"), V("timeout", ctx={"transport": "timeout"})), tag="",
    io={"same_provider_retry": True})
add("_should_retry_same_provider", task="chat", failure=F(E("x"), V("overloaded")), tag="",
    io={"same_provider_retry": True})
add("_outcome", thunk="call_api", args={"x": 1}, io={"call_api": S({"ok": 1})})
add("_outcome", thunk="call_api", args={}, io={"call_api": SR("TimeoutError", "slow")})
add("_aoutcome", thunk="call_api", args={"x": 1}, io={"call_api": S({"ok": 1})})
add("_aoutcome", thunk="call_api", args={}, io={"call_api": SR("TimeoutError", "slow")})
# ── ladder: classification / failure / rung ──────────────────────────────
LADDER_CLIENTS = {"c1": CL("c1", backend="openrouter"),
                  "c2": CL("c2", base_url="https://p2.example/v1", api_key="***", backend="p2"),
                  "c3": CL("c3", base_url="https://p3.example/v1", api_key="***", backend="p3"),
                  "cn": CL("cn", base_url="https://inference-api.nousresearch.com/v1",
                           api_key="***", backend="nous")}
MATCH_FALSE = {"_is_unsupported_parameter_error": [False, False, False, False],
               "is_reasoning_required_rejection": False,
               "is_reasoning_field_rejection": False}

def merge_io(*ds):
    out = {}
    for d in ds:
        out.update(d)
    return out

add("_parameter_rungs", client="c1", max_tokens=100, clients=LADDER_CLIENTS)
add("_parameter_rungs", client="c1", max_tokens=None, clients=LADDER_CLIENTS)
add("_parameter_rungs", client=None, max_tokens=0, clients=LADDER_CLIENTS)

add("_classify_failure", exc=E("inner", type="_ClassifiedAttempt",
                               classified=F(E("inner"), V("billing", rotate=True), client="c1")),
    provider="openrouter", client="c1", model="m1", base_url="https://api.example.com/v1",
    clients=LADDER_CLIENTS)
add("_classify_failure", exc=E("Unsupported parameter: temperature", status=400),
    provider="openrouter", client="c1", model="m1", base_url="https://api.example.com/v1",
    clients=LADDER_CLIENTS,
    io={"classify_attempt": V("format_error"), "_is_unsupported_parameter_error": True})
add("_classify_failure", exc=E("mystery"), provider="openrouter", client="c1", model="m1",
    base_url="https://api.example.com/v1", clients=LADDER_CLIENTS,
    io=merge_io({"classify_attempt": V("unknown")}, MATCH_FALSE))
add("_classify_failure", exc=E("x"), provider="auto", client="c1", model="m1",
    base_url="https://api.example.com/v1", clients=LADDER_CLIENTS,
    relay_context={"provider": "p", "model": "m", "attempt_count": 1, "attempt": None},
    io=merge_io({"classify_attempt": V("auth", rotate=True)}, MATCH_FALSE))

add("_failure", route=ROUTE(), exc=F(E("x"), V("unknown")), clients=LADDER_CLIENTS)
add("_failure", route=ROUTE(), exc=E("x"), step={"kind": "call", "args": ["c2", {"model": "m2"}]},
    clients=LADDER_CLIENTS,
    io=merge_io({"classify_attempt": V("unknown")}, MATCH_FALSE))
add("_failure", route=ROUTE(), exc=E("x"), clients=LADDER_CLIENTS,
    io=merge_io({"classify_attempt": V("unknown")}, MATCH_FALSE))

add("_rung", step={"kind": "call", "args": ["c1", {"model": "m"}]}, accept="always",
    route=ROUTE(), clients=LADDER_CLIENTS, io={"perform": [{"ok": 1}]})
add("_rung", step={"kind": "call", "args": ["c1", {"model": "m"}]}, accept="ladder_unspent",
    route=ROUTE(), clients=LADDER_CLIENTS,
    io=merge_io({"perform": [SR("RuntimeError", "rate limited")],
                 "classify_attempt": V("rate_limit"), "ladder_accepts": True}, MATCH_FALSE))
add("_rung", step={"kind": "call", "args": ["c1", {"model": "m"}]}, accept="ladder_unspent",
    route=ROUTE(), clients=LADDER_CLIENTS,
    io=merge_io({"perform": [SR("ValueError", "weird")],
                 "classify_attempt": V("unknown", retryable=False), "ladder_accepts": False},
                MATCH_FALSE))
add("_rung", step={"kind": "call", "args": ["c1", {"model": "m"}]}, accept="ladder_unspent",
    route=ROUTE(), clients=LADDER_CLIENTS,
    io={"perform": [{"__script__": True,
                     "raise": E("inner", type="_ClassifiedAttempt",
                                classified=F(E("inner"), V("unknown", retryable=False)))}],
        "ladder_accepts": False})

# ── ladder: parameter rungs ───────────────────────────────────────────────
add("_ladder_parameter_rungs", first_err=F(E("mystery"), V("unknown")), route=ROUTE(),
    kwargs={"model": "m"}, max_tokens=None, clients=LADDER_CLIENTS,
    io=merge_io({"resend_cannot_help": False,
                 "_is_unsupported_parameter_error": [False, False, False]},
                {"is_reasoning_required_rejection": False,
                 "is_reasoning_field_rejection": False}))
add("_ladder_parameter_rungs",
    first_err=F(E("Unsupported parameter: temperature", status=400), V("format_error")),
    route=ROUTE(), kwargs={"model": "m", "temperature": 0.7}, max_tokens=100,
    clients=LADDER_CLIENTS,
    io={"resend_cannot_help": False, "_is_unsupported_parameter_error": True,
        "perform": [{"ok": 1}], "remember_temperature_rejection": S()})
add("_ladder_parameter_rungs",
    first_err=F(E("Unsupported parameter: temperature", status=400), V("format_error")),
    route=ROUTE(), kwargs={"model": "m", "temperature": 0.7, "max_tokens": 100}, max_tokens=100,
    clients=LADDER_CLIENTS,
    io={"resend_cannot_help": [False, False],
        "_is_unsupported_parameter_error": [True, False, False, False, False, False],
        "is_reasoning_required_rejection": [False, False],
        "is_reasoning_field_rejection": [False, False],
        "_effective_provider_for_client": "openrouter",
        "_fallback_provider_from_label": "openrouter",
        "_normalize_aux_provider": "openrouter",
        "classify_attempt": V("format_error"),
        "perform": [SR("RuntimeError", "max_tokens is not supported"), {"ok": 1}]})
add("_ladder_parameter_rungs", first_err=F(E("policy refusal"), V("content_policy_blocked")),
    route=ROUTE(), kwargs={"model": "m"}, max_tokens=None, clients=LADDER_CLIENTS,
    io={"resend_cannot_help": True})
add("_ladder_parameter_rungs",
    first_err=F(E("response_format unavailable"), V("format_error")),
    route=ROUTE(), kwargs={"model": "m", "reasoning_effort": "none"}, max_tokens=None,
    clients=LADDER_CLIENTS,
    io={"resend_cannot_help": False, "_is_unsupported_parameter_error": [False],
        "_without_structured_output_format": [S()],
        "is_reasoning_required_rejection": True,
        "with_reasoning_floor": {"model": "m", "reasoning_effort": "minimal"},
        "perform": [{"ok": 1}], "remember_reasoning_floor": S()})

# ── ladder: Nous rungs ────────────────────────────────────────────────────
add("_refreshed_nous_step", route=ROUTE(resolved_provider="nous"), kwargs={"model": "old"},
    message="msg", clients=LADDER_CLIENTS,
    io={"_refresh_nous_auxiliary_client": {"client": None, "model": None}})
add("_refreshed_nous_step", route=ROUTE(resolved_provider="nous"), kwargs={"model": "old"},
    message="msg", clients=LADDER_CLIENTS,
    io={"_refresh_nous_auxiliary_client": {"client": "cn", "model": "new"}})
add("_refreshed_nous_step", route=ROUTE(resolved_provider="nous"), kwargs={"model": "same"},
    message="msg", clients=LADDER_CLIENTS,
    io={"_refresh_nous_auxiliary_client": {"client": "cn", "model": "same"}})

add("_ladder_nous_rungs", first_err=F(E("model not found"), V("model_not_found")),
    route=ROUTE(), kwargs={"model": "stale"}, client_is_nous=True, repairs=[],
    clients=LADDER_CLIENTS,
    io={"_refresh_nous_recommended_model": "fresh", "perform": [{"ok": 1}]})
add("_ladder_nous_rungs", first_err=F(E("model not found"), V("model_not_found")),
    route=ROUTE(), kwargs={"model": "stale"}, client_is_nous=True, repairs=[],
    clients=LADDER_CLIENTS,
    io=merge_io({"_refresh_nous_recommended_model": "fresh",
                 "perform": [SR("RuntimeError", "still bad"), {"ok": 1}],
                 "classify_attempt": V("auth", rotate=True),
                 "benches_provider": False, "auth_refresh": True,
                 "_refresh_nous_auxiliary_client": {"client": "cn", "model": "m1"}},
                MATCH_FALSE))
add("_ladder_nous_rungs", first_err=F(E("quota"), V("billing", ctx={"quota_exhausted": True})),
    route=ROUTE(), kwargs={"model": "m"}, client_is_nous=True, repairs=[],
    clients=LADDER_CLIENTS,
    io={"benches_provider": True, "_nous_portal_account_has_fresh_paid_access": True,
        "_refresh_nous_auxiliary_client": {"client": "cn", "model": None},
        "perform": [{"ok": 1}]})
add("_ladder_nous_rungs", first_err=F(E("401"), V("auth", rotate=True)),
    route=ROUTE(), kwargs={"model": "m"}, client_is_nous=False, repairs=[],
    clients=LADDER_CLIENTS, io={"benches_provider": False, "auth_refresh": True})
add("_ladder_nous_rungs", first_err=F(E("401"), V("auth", rotate=True)),
    route=ROUTE(), kwargs={"model": "m"}, client_is_nous=True, repairs=["without_temperature"],
    clients=LADDER_CLIENTS,
    io={"benches_provider": False, "auth_refresh": True,
        "_refresh_nous_auxiliary_client": {"client": None, "model": None}})

# ── ladder: failure pool / recovery ───────────────────────────────────────
add("_failure_pool", route=ROUTE(), failure=F(E("x"), V("auth", rotate=True)),
    clients=LADDER_CLIENTS, io={"_recoverable_pool_provider": "openrouter"})
add("_failure_pool", route=ROUTE(), failure=F(E("x"), V("auth", rotate=True), client="c2"),
    clients=LADDER_CLIENTS, io={"_recoverable_pool_provider": ["openrouter", "openrouter"]})
add("_failure_pool", route=ROUTE(), failure=F(E("x"), V("auth", rotate=True), client="c2"),
    clients=LADDER_CLIENTS, io={"_recoverable_pool_provider": ["openrouter", "nous"]})
add("_recover_failure_pool", route=ROUTE(), failure=F(E("x"), V("auth", rotate=True)),
    clients=LADDER_CLIENTS, io={"_recoverable_pool_provider": None})
add("_recover_failure_pool", route=ROUTE(), failure=F(E("x"), V("auth", rotate=True)),
    clients=LADDER_CLIENTS,
    io={"_recoverable_pool_provider": "openrouter", "_recover_provider_pool": True})
add("_recover_failure_pool", route=ROUTE(), failure=F(E("x"), V("auth", rotate=True)),
    clients=LADDER_CLIENTS,
    io={"_recoverable_pool_provider": "openrouter", "_recover_provider_pool": False})

# ── ladder: credential rungs ──────────────────────────────────────────────
CRED_BASE = {"_effective_provider_for_client": "auto",
             "_auth_refresh_provider_for_route": "nous"}
add("_ladder_credential_rungs", first_err=F(E("401"), V("auth", rotate=True)),
    route=ROUTE(resolved_provider="auto"), kwargs={"model": "m"}, client_is_nous=False,
    repairs=[], clients=LADDER_CLIENTS,
    io=merge_io(CRED_BASE, {"auth_refresh": True, "_refresh_provider_credentials": True,
                            "_normalize_aux_provider": "auto", "_evict_cached_clients": S(),
                            "perform": [{"ok": 1}]}))
add("_ladder_credential_rungs", first_err=F(E("401"), V("auth", rotate=True)),
    route=ROUTE(resolved_provider="auto"), kwargs={"model": "m"}, client_is_nous=False,
    repairs=[], clients=LADDER_CLIENTS,
    io=merge_io(CRED_BASE, MATCH_FALSE,
                {"auth_refresh": True, "_refresh_provider_credentials": True,
                 "_normalize_aux_provider": "auto", "_evict_cached_clients": S(),
                 "perform": [SR("RuntimeError", "401 again"), {"ok": 1}],
                 "classify_attempt": V("auth", rotate=True), "ladder_accepts": True,
                 "_recoverable_pool_provider": ["openrouter", "openrouter"],
                 "same_key_retry": False, "_recover_provider_pool": True}))
add("_ladder_credential_rungs", first_err=F(E("429"), V("rate_limit", rotate=True)),
    route=ROUTE(resolved_provider="auto"), kwargs={"model": "m"}, client_is_nous=False,
    repairs=[], clients=LADDER_CLIENTS,
    io=merge_io(CRED_BASE, {"auth_refresh": False,
                            "_recoverable_pool_provider": "openrouter",
                            "same_key_retry": True,
                            "rate_limit_retry_rungs": {"steps": [{"kind": "wait", "args": [0.5]}],
                                                       "resp": {"ok": 1}, "failure": None},
                            "time.sleep": S()}))
add("_ladder_credential_rungs", first_err=F(E("500"), V("server_error")),
    route=ROUTE(resolved_provider="auto"), kwargs={"model": "m"}, client_is_nous=False,
    repairs=[], clients=LADDER_CLIENTS,
    io=merge_io(CRED_BASE, {"auth_refresh": False, "_recoverable_pool_provider": "openrouter"}))
add("_ladder_credential_rungs", first_err=F(E("401"), V("auth", rotate=True)),
    route=ROUTE(resolved_provider="auto"), kwargs={"model": "m"}, client_is_nous=False,
    repairs=[], clients=LADDER_CLIENTS,
    io=merge_io(CRED_BASE, MATCH_FALSE,
                {"auth_refresh": True, "_refresh_provider_credentials": True,
                 "_normalize_aux_provider": "auto", "_evict_cached_clients": S(),
                 "perform": [SR("RuntimeError", "401 again"), SR("RuntimeError", "401 rotated")],
                 "classify_attempt": [V("auth", rotate=True), V("auth", rotate=True)],
                 "ladder_accepts": [True, True],
                 "_recoverable_pool_provider": ["openrouter", "openrouter", "openrouter"],
                 "same_key_retry": False, "_recover_provider_pool": [True, True]}))
add("_ladder_credential_rungs", first_err=F(E("401"), V("auth", rotate=True)),
    route=ROUTE(resolved_provider="anthropic"), kwargs={"model": "m"}, client_is_nous=False,
    repairs=[], clients=LADDER_CLIENTS,
    io=merge_io({"_effective_provider_for_client": "anthropic",
                 "_auth_refresh_provider_for_route": "anthropic"},
                {"auth_refresh": True, "_refresh_provider_credentials": True,
                 "_normalize_aux_provider": "anthropic", "perform": [{"ok": 1}]}))
# ── ladder: next fallback / provider fallback walk ───────────────────────
add("_next_fallback_after_quarantine", task="t", resolved_provider="auto", is_auto=True,
    route=ROUTE(), failed_model="m1", failure_scope=None, task_chain_only=False, skip_lanes=[],
    clients=LADDER_CLIENTS,
    io={"_try_configured_fallback_chain": {"client": "c2", "model": "m2", "label": "L1"}})
add("_next_fallback_after_quarantine", task="t", resolved_provider="nous", is_auto=False,
    route=ROUTE(), failed_model=None, failure_scope=None, task_chain_only=True, skip_lanes=[],
    clients=LADDER_CLIENTS,
    io={"_try_configured_fallback_chain": {"client": None, "model": None, "label": ""}})
add("_next_fallback_after_quarantine", task="t", resolved_provider="auto", is_auto=True,
    route=ROUTE(), failed_model="m1", failure_scope=None, task_chain_only=False,
    skip_lanes=[["L0", "m0", "https://x"]], clients=LADDER_CLIENTS,
    io={"_try_configured_fallback_chain": {"client": None, "model": None, "label": ""},
        "_try_main_fallback_chain": {"client": "c2", "model": "m2", "label": "L2"}})
add("_next_fallback_after_quarantine", task="t", resolved_provider="auto", is_auto=True,
    route=ROUTE(), failed_model="m1", failure_scope="endpoint", task_chain_only=False,
    skip_lanes=[], clients=LADDER_CLIENTS,
    io={"_try_configured_fallback_chain": {"client": None, "model": None, "label": ""},
        "_try_main_fallback_chain": {"client": None, "model": None, "label": ""},
        "_try_payment_fallback": {"client": "c3", "model": "m3", "label": "L3"}})

add("_ladder_provider_fallback", failure=F(E("x"), V("unknown", retryable=False)),
    route=ROUTE(), rung_spent=False, clients=LADDER_CLIENTS,
    io={"fallback_label": None})
add("_ladder_provider_fallback", failure=F(E("bad key"), V("auth", rotate=True)),
    route=ROUTE(resolved_provider="nous"), rung_spent=False, clients=LADDER_CLIENTS,
    io={"fallback_label": "auth error", "_get_auxiliary_task_config": {"fallback_chain": []}})
add("_ladder_provider_fallback", failure=F(E("bad key"), V("auth", rotate=True)),
    route=ROUTE(resolved_provider="nous", route_info={}), rung_spent=False,
    clients=LADDER_CLIENTS,
    io={"fallback_label": "auth error",
        "_get_auxiliary_task_config": {"fallback_chain": [{"provider": "p2"}]},
        "_try_configured_fallback_chain": {"client": "c2", "model": "m2",
                                            "label": "fallback_chain[0](p2)"},
        "_fallback_provider_from_label": "p2", "perform": [{"ok": 1}]})
add("_ladder_provider_fallback", failure=F(E("402 credits"), V("billing", rotate=True)),
    route=ROUTE(route_info={}), rung_spent=False, clients=LADDER_CLIENTS,
    io={"fallback_label": "payment error", "_get_auxiliary_task_config": {},
        "_recoverable_pool_provider": "openrouter", "_mark_provider_unhealthy": S(),
        "_custom_health_base_url": "",
        "_try_configured_fallback_chain": [{"client": "c2", "model": "m2", "label": "L1"},
                                            {"client": None, "model": None, "label": ""}],
        "_try_main_fallback_chain": {"client": "c3", "model": "m3", "label": "L2"},
        "_fallback_provider_from_label": ["p2", "p3"],
        "perform": [None, {"ok": 1}]})
add("_ladder_provider_fallback", failure=F(E("conn reset"), V("timeout", ctx={"transport": "connection"})),
    route=ROUTE(route_info={"provider": "orig", "model": "orig"}), rung_spent=False,
    clients=LADDER_CLIENTS,
    io={"fallback_label": "connection error", "_get_auxiliary_task_config": {},
        "_try_configured_fallback_chain": [{"client": "c2", "model": "m2", "label": "L1"},
                                            {"client": "c2", "model": "m2", "label": "L1"}],
        "_fallback_provider_from_label": "p2", "perform": [None]})
add("_ladder_provider_fallback", failure=F(E("slow"), V("timeout")),
    route=ROUTE(route_info={"provider": "orig", "model": "orig"}), rung_spent=True,
    clients=LADDER_CLIENTS,
    io={"fallback_label": "request timed out", "_get_auxiliary_task_config": {},
        "_try_configured_fallback_chain": {"client": None, "model": None, "label": ""},
        "_try_main_fallback_chain": {"client": None, "model": None, "label": ""},
        "_try_payment_fallback": {"client": None, "model": None, "label": ""}})

# ── ladder: full orchestration ────────────────────────────────────────────
add("_aux_recovery_ladder", first_err=F(E("Unsupported parameter: temperature", status=400),
                                        V("format_error")),
    client="c1", kwargs={"model": "m", "temperature": 0.7, "timeout": 30}, task="title_generation",
    async_mode=False, base_info="https://api.example.com/v1", resolved_provider="openrouter",
    resolved_model="m1", resolved_base_url="https://api.example.com/v1", resolved_api_key="***",
    resolved_api_mode=None, final_model="m1", max_tokens=100, main_runtime=None, route_info=None,
    clients=LADDER_CLIENTS,
    io={"resend_cannot_help": False, "_is_unsupported_parameter_error": True,
        "perform": [{"ok": 1}], "remember_temperature_rejection": S()})
add("_aux_recovery_ladder", first_err=F(E("connection reset"),
                                        V("timeout", ctx={"transport": "connection"}, retryable=False)),
    client="c1", kwargs={"model": "m", "timeout": 30}, task="title_generation",
    async_mode=False, base_info="https://api.example.com/v1", resolved_provider="auto",
    resolved_model="m1", resolved_base_url="https://api.example.com/v1", resolved_api_key="***",
    resolved_api_mode=None, final_model="m1", max_tokens=None, main_runtime=None, route_info=None,
    clients=LADDER_CLIENTS,
    relay_context={"provider": "p", "model": "m", "attempt_count": 1, "attempt": None},
    io=merge_io({"resend_cannot_help": False,
                 "_is_unsupported_parameter_error": [False, False, False],
                 "is_reasoning_required_rejection": False,
                 "is_reasoning_field_rejection": False,
                 "base_url_host_matches": False, "benches_provider": False,
                 "auth_refresh": False, "_effective_provider_for_client": "auto",
                 "_auth_refresh_provider_for_route": "nous",
                 "_recoverable_pool_provider": None, "fallback_label": None,
                 "_evict_cached_client_instance": S()}))
add("_aux_recovery_ladder", first_err=F(E("500"), V("server_error", fallback=True)),
    client="c1", kwargs={"model": "m", "timeout": 30}, task="title_generation",
    async_mode=False, base_info="https://api.example.com/v1", resolved_provider="auto",
    resolved_model="m1", resolved_base_url="https://api.example.com/v1", resolved_api_key="***",
    resolved_api_mode=None, final_model="m1", max_tokens=None, main_runtime=None, route_info={},
    clients=LADDER_CLIENTS,
    io=merge_io({"resend_cannot_help": False,
                 "_is_unsupported_parameter_error": [False, False, False],
                 "is_reasoning_required_rejection": False,
                 "is_reasoning_field_rejection": False,
                 "base_url_host_matches": False, "benches_provider": False,
                 "auth_refresh": False, "_effective_provider_for_client": "auto",
                 "_auth_refresh_provider_for_route": "nous",
                 "_recoverable_pool_provider": "openrouter",
                 "fallback_label": "server error", "_get_auxiliary_task_config": {},
                 "_try_configured_fallback_chain": {"client": "c2", "model": "m2", "label": "L"},
                 "_fallback_provider_from_label": "p2", "perform": [{"ok": 1}]}))
add("_aux_recovery_ladder", first_err=F(E("model not found"), V("model_not_found")),
    client="c1", kwargs={"model": "stale", "timeout": 30}, task="title_generation",
    async_mode=False, base_info="https://inference-api.nousresearch.com/v1",
    resolved_provider="nous", resolved_model="stale",
    resolved_base_url="https://inference-api.nousresearch.com/v1", resolved_api_key="***",
    resolved_api_mode=None, final_model="stale", max_tokens=None, main_runtime=None,
    route_info=None, clients=LADDER_CLIENTS,
    io=merge_io({"resend_cannot_help": False,
                 "_is_unsupported_parameter_error": [False, False, False],
                 "is_reasoning_required_rejection": False,
                 "is_reasoning_field_rejection": False,
                 "_refresh_nous_recommended_model": "fresh", "perform": [{"ok": 1}]}))
add("_aux_recovery_ladder", first_err=F(E("401"), V("auth", rotate=True)),
    client="c1", kwargs={"model": "m", "timeout": 30}, task="title_generation",
    async_mode=True, base_info="https://api.example.com/v1", resolved_provider="auto",
    resolved_model="m1", resolved_base_url="https://api.example.com/v1", resolved_api_key="***",
    resolved_api_mode=None, final_model="m1", max_tokens=None, main_runtime=None, route_info=None,
    clients=LADDER_CLIENTS,
    io=merge_io(CRED_BASE,
                {"resend_cannot_help": False,
                 "_is_unsupported_parameter_error": [False, False, False],
                 "is_reasoning_required_rejection": False,
                 "is_reasoning_field_rejection": False,
                 "base_url_host_matches": False, "benches_provider": False,
                 "auth_refresh": [True, True], "_refresh_provider_credentials": True,
                 "_normalize_aux_provider": "auto", "_evict_cached_clients": S(),
                 "perform": [{"ok": 1}]}))

# ── ladder: driver / primary / start ──────────────────────────────────────
add("_drive_ladder", gen={"steps": [{"kind": "call", "args": ["c1", {"model": "m"}]},
                                     {"kind": "call", "args": ["c1", {"model": "m"}]}],
                          "final": {"done": 1}, "on_throw": "propagate"},
    clients=LADDER_CLIENTS, io={"perform": [{"ok": 1}, {"ok": 2}]})
add("_drive_ladder", gen={"steps": [{"kind": "wait", "args": [0.5]},
                                     {"kind": "call", "args": ["c1", {"model": "m"}]}],
                          "final": {"done": 1}},
    clients=LADDER_CLIENTS, io={"perform": [{"ok": 1}], "time.sleep": S()})
add("_drive_ladder", gen={"steps": [{"kind": "call", "args": ["c1", {"model": "m"}]}],
                          "final": {"done": 1}, "on_throw": "propagate"},
    clients=LADDER_CLIENTS, io={"perform": [SR("TimeoutError", "slow")]})
add("_drive_ladder", gen={"steps": [{"kind": "call", "args": ["c1", {"model": "m"}]},
                                     {"kind": "call", "args": ["c1", {"model": "m"}]}],
                          "final": {"done": 1}, "on_throw": "swallow"},
    clients=LADDER_CLIENTS, io={"perform": [SR("TimeoutError", "slow"), {"ok": 2}]})

PREP_REQ = {"client": "c1", "final_model": "m1", "kwargs": {"model": "m1"},
            "resolved_provider": "openrouter", "request_provider": "openrouter",
            "resolved_model": "m1", "resolved_base_url": "https://api.example.com/v1",
            "resolved_api_key": "***", "resolved_api_mode": None,
            "effective_timeout": 30.0, "effective_extra_body": {},
            "base_info": "https://api.example.com/v1"}
add("_primary_failure", req=PREP_REQ, exc=E("x"), clients=LADDER_CLIENTS,
    io=merge_io({"classify_attempt": V("unknown")}, MATCH_FALSE))
add("_primary_failure", req={**PREP_REQ, "base_info": ""}, exc=E("x"), clients=LADDER_CLIENTS,
    io=merge_io({"classify_attempt": V("unknown")}, MATCH_FALSE))
add("_start_recovery_ladder", first_err=F(E("x"), V("unknown")), req=PREP_REQ,
    retry_kwargs={"max_tokens": 100, "main_runtime": {"provider": "nous"}},
    task="title_generation", async_mode=False, route_info={}, clients=LADDER_CLIENTS)
add("_start_recovery_ladder", first_err=F(E("x"), V("unknown")), req=PREP_REQ,
    retry_kwargs={"max_tokens": None, "main_runtime": None},
    task="compression", async_mode=True, route_info=None, clients=LADDER_CLIENTS)

# ── request preparation head ──────────────────────────────────────────────
PREP_IO = {"_resolve_task_provider_model": {"resolved_provider": "auto", "resolved_model": "m1",
                                             "resolved_base_url": "https://api.example.com/v1",
                                             "resolved_api_key": "***", "resolved_api_mode": None},
           "_get_task_extra_body": {"task_key": 1},
           "_resolve_call_client": {"client": "c1", "final_model": "m1",
                                     "resolved_provider": "auto", "effective_provider": "openrouter"},
           "_effective_aux_timeout": 30.0,
           "_set_relay_auxiliary_route": S(),
           "_fallback_provider_from_label": "openrouter",
           "_build_call_kwargs": {"model": "m1", "messages": [{"role": "user", "content": "hi"}],
                                   "timeout": 30.0},
           "_is_anthropic_compat_endpoint": False}
add("_prepare_aux_request", task="title_generation", provider=None, model=None, base_url=None,
    api_key=None, main_runtime=None, messages=[{"role": "user", "content": "hi"}],
    temperature=None, max_tokens=None, tools=None, timeout=None, extra_body={"caller": 1},
    reasoning_config=None, extra_headers=None, api_mode=None, route_info={}, async_mode=False,
    clients=LADDER_CLIENTS, io=PREP_IO)
add("_prepare_aux_request", task="title_generation", provider=None, model=None, base_url=None,
    api_key=None, main_runtime=None, messages=[], temperature=None, max_tokens=None, tools=None,
    timeout=None, extra_body=None, reasoning_config=None, extra_headers={"X-H": "v"},
    api_mode="responses", route_info=None, async_mode=True,
    clients=LADDER_CLIENTS, io=PREP_IO)
add("_prepare_aux_request", task="compression", provider="nous", model="m1", base_url=None,
    api_key=None, main_runtime=None, messages=[], temperature=None, max_tokens=100, tools=None,
    timeout=None, extra_body=None, reasoning_config=None, extra_headers=None, api_mode=None,
    route_info=None, async_mode=False,
    clients={"c1": CL("c1", backend="openrouter", kind="codex")},
    io=merge_io(PREP_IO, {"_get_auxiliary_task_config": {"no_progress_timeout": 90}}))
add("_prepare_aux_request", task="title_generation", provider=None, model=None, base_url=None,
    api_key=None, main_runtime=None, messages=[{"role": "user", "content": "hi"}],
    temperature=None, max_tokens=None, tools=None, timeout=None, extra_body=None,
    reasoning_config=None, extra_headers=None, api_mode=None, route_info=None, async_mode=False,
    clients=LADDER_CLIENTS,
    io=merge_io(PREP_IO, {"_is_anthropic_compat_endpoint": True,
                          "_convert_openai_images_to_anthropic": [{"role": "user", "content": "converted"}]}))
add("_plan_aux_call", task="title_generation", provider=None, model=None, base_url=None,
    api_key=None, main_runtime={"provider": " Nous "}, messages=[], temperature=None,
    max_tokens=None, tools=None, timeout=None, extra_body=None, reasoning_config=None,
    extra_headers=None, api_mode=None, route_info={}, async_mode=False,
    clients=LADDER_CLIENTS, io=merge_io(PREP_IO, {"_normalize_main_runtime": {"provider": "nous"}}))
add("_plan_aux_call", task="compression", provider=None, model=None, base_url=None,
    api_key=None, main_runtime=None, messages=[], temperature=None, max_tokens=50, tools=None,
    timeout=None, extra_body=None, reasoning_config=None, extra_headers=None, api_mode=None,
    route_info=None, async_mode=True,
    clients=LADDER_CLIENTS, io=merge_io(PREP_IO, {"_normalize_main_runtime": {}}))

# ── fallback candidates ───────────────────────────────────────────────────
FB_REQ = {"messages": [{"role": "user", "content": "x"}], "tools": None, "temperature": None,
          "max_tokens": None, "effective_extra_body": {}, "reasoning_config": None}
add("_fallback_request_kwargs", destination=DEST(), task="chat", effective_timeout=30.0,
    fallback_entry={}, task_config={}, apply_fast_lane=False,
    clients=LADDER_CLIENTS,
    io={"_replan_synchronous_cache_sections": {"messages": [{"role": "user", "content": "x"}], "tools": None},
        "_build_call_kwargs": {"model": "m2"}}, **FB_REQ)
add("_fallback_request_kwargs", destination=DEST(), task="compression", effective_timeout=30.0,
    effective_extra_body={"a": 1}, fallback_entry={"provider": "p2", "model": "m2"},
    task_config={}, apply_fast_lane=True, clients=LADDER_CLIENTS,
    io={"resolve_compression_fast_lane": {"certified_non_reasoning": True,
                                           "reasoning_config": {"enabled": False}},
        "_replan_synchronous_cache_sections": {"messages": [], "tools": None},
        "_build_call_kwargs": {"model": "m2"}},
    messages=[], tools=None, temperature=None, max_tokens=None, extra_body={"a": 1},
    reasoning_config=None)
add("_fallback_request_kwargs", destination=DEST(), task="compression", effective_timeout=30.0,
    effective_extra_body={}, fallback_entry={}, task_config={}, apply_fast_lane=True,
    clients=LADDER_CLIENTS,
    io={"_replan_synchronous_cache_sections": {"messages": [], "tools": None},
        "_build_call_kwargs": {"model": "m2"}},
    messages=[], tools=None, temperature=None, max_tokens=100, extra_body={}, reasoning_config=None)
add("_plan_fallback_candidate", fb_client="c2", fb_model="m2", fb_label="fallback_chain[0](p2)",
    task="chat", effective_timeout=30.0, apply_fast_lane=False, request=FB_REQ,
    clients=LADDER_CLIENTS,
    io={"_fallback_entry_timeout": 60.0, "_fallback_destination": DEST(),
        "_fallback_chain_entry": {"provider": "p2", "model": "m2"},
        "_replan_synchronous_cache_sections": {"messages": [], "tools": None},
        "_build_call_kwargs": {"model": "m2"}})
add("_plan_fallback_candidate", fb_client="c2", fb_model="m2", fb_label="L",
    task="chat", effective_timeout=30.0, apply_fast_lane=False, request=FB_REQ,
    rebuild_call={"provider": "p3", "client": "c2", "model": "m3"},
    clients=LADDER_CLIENTS,
    io={"_fallback_entry_timeout": None, "_fallback_destination": DEST(),
        "_fallback_chain_entry": None,
        "_replan_synchronous_cache_sections": [{"messages": [], "tools": None},
                                                {"messages": [], "tools": None}],
        "_build_call_kwargs": [{"model": "m2"}, {"model": "m3"}]})
add("_plan_fallback_candidate", fb_client="c2", fb_model="m2", fb_label="L",
    task="compression", effective_timeout=30.0, apply_fast_lane=False, request=FB_REQ,
    clients=LADDER_CLIENTS,
    io={"_fallback_entry_timeout": None, "_fallback_destination": DEST(),
        "_get_auxiliary_task_config": {"prefer_fast_model": True},
        "_fallback_chain_entry": None,
        "_replan_synchronous_cache_sections": {"messages": [], "tools": None},
        "_build_call_kwargs": {"model": "m2"}})
add("_quarantine_fallback_candidate", task="t", fb_label="L", fb_provider="p2",
    fb_err=E("404 model"), base_url="https://p2.example/v1", tag="",
    reason="model not found", model="m2",
    io={"fallback_candidate_quarantine_ttl": 300.0, "_mark_provider_unhealthy": S()})
add("_quarantine_fallback_candidate", task="t", fb_label="L", fb_provider="",
    fb_err=E("dead token"), base_url="", tag=" (async)", reason=None, model=None,
    io={"fallback_candidate_quarantine_ttl": 3600.0, "_mark_provider_unhealthy": S()})
add("_quarantine_fallback_candidate", task="t", fb_label="L", fb_provider="p2",
    fb_err=E("402"), base_url="https://p2.example/v1", tag="", reason="payment error",
    model="m2", io={"fallback_candidate_quarantine_ttl": 3600.0, "_mark_provider_unhealthy": S()})
add("_plan_fallback_auth_retry", destination=DEST(), async_mode=False, failed_api_key="k2",
    clients=LADDER_CLIENTS,
    io={"_auth_refresh_provider_for_route": "nous", "_refresh_provider_credentials": True,
        "_get_cached_client": {"client": "c2", "model": "m2"},
        "rebuild": {"destination": DEST("nous", "https://p2.example/v1", None, "m2"),
                    "kwargs": {"model": "m2"}}})
add("_plan_fallback_auth_retry", destination=DEST(), async_mode=False, failed_api_key="",
    clients=LADDER_CLIENTS,
    io={"_auth_refresh_provider_for_route": "nous", "_refresh_provider_credentials": False})
add("_plan_fallback_auth_retry", destination=DEST(), async_mode=True, failed_api_key="",
    clients=LADDER_CLIENTS,
    io={"_auth_refresh_provider_for_route": "nous", "_refresh_provider_credentials": True,
        "_get_cached_client": {"client": None, "model": None}})
add("_plan_fallback_auth_retry", destination=DEST(), async_mode=False, failed_api_key="",
    clients=LADDER_CLIENTS, io={"_auth_refresh_provider_for_route": "auto"})
add("_plan_fallback_auth_retry", destination=DEST(provider="anthropic"), async_mode=False,
    failed_api_key="bad-key", clients=LADDER_CLIENTS,
    io={"_auth_refresh_provider_for_route": "anthropic", "_refresh_provider_credentials": True,
        "_get_cached_client": {"client": "c2", "model": "m2"},
        "rebuild": {"destination": DEST("anthropic", "https://p2.example/v1", None, "m2"),
                    "kwargs": {"model": "m2"}}})
add("_fallback_candidate_ladder", fb_client="c2", fb_kwargs={"model": "m2"}, destination=DEST(),
    task="t", fb_label="L", tag="", async_mode=False, clients=LADDER_CLIENTS,
    io={"perform": [{"resp": {"ok": 1}, "failure": None, "repairs": []}]})
add("_fallback_candidate_ladder", fb_client="c2", fb_kwargs={"model": "m2"}, destination=DEST(),
    task="t", fb_label="L", tag="", async_mode=False, clients=LADDER_CLIENTS,
    io={"perform": [{"resp": None, "failure": F(E("401"), V("auth", rotate=True)),
                     "repairs": ["without_temperature"]},
                    {"resp": {"ok": 1}, "failure": None, "repairs": []}],
        "auth_refresh": True, "_auth_refresh_provider_for_route": "nous",
        "_refresh_provider_credentials": True,
        "_get_cached_client": {"client": "c2", "model": "m2"},
        "rebuild": {"destination": DEST("nous", "https://p2.example/v1", None, "m2"),
                    "kwargs": {"model": "m2"}},
        "reapply_repairs": {"model": "m2"}})
add("_fallback_candidate_ladder", fb_client="c2", fb_kwargs={"model": "m2"}, destination=DEST(),
    task="t", fb_label="L", tag="", async_mode=False, clients=LADDER_CLIENTS,
    io={"perform": [{"resp": None, "failure": F(E("401"), V("auth", rotate=True)), "repairs": []},
                    {"resp": None, "failure": F(E("401 again"), V("auth", rotate=True)), "repairs": []}],
        "auth_refresh": [True, True], "_auth_refresh_provider_for_route": "nous",
        "_refresh_provider_credentials": True,
        "_get_cached_client": {"client": "c2", "model": "m2"},
        "rebuild": {"destination": DEST("nous", "https://p2.example/v1", None, "m2"),
                    "kwargs": {"model": "m2"}},
        "reapply_repairs": {"model": "m2"},
        "fallback_candidate_quarantine_ttl": 3600.0, "_mark_provider_unhealthy": S()})
add("_fallback_candidate_ladder", fb_client="c2", fb_kwargs={"model": "m2"}, destination=DEST(),
    task="t", fb_label="L", tag="", async_mode=False, clients=LADDER_CLIENTS,
    io={"perform": [{"resp": None, "failure": F(E("429"), V("rate_limit", fallback=True),
                                                client=None), "repairs": []}],
        "auth_refresh": False, "fallback_label": "rate limit",
        "fallback_candidate_quarantine_ttl": 300.0, "_mark_provider_unhealthy": S()})
add("_fallback_candidate_ladder", fb_client="c2", fb_kwargs={"model": "m2"}, destination=DEST(),
    task="t", fb_label="L", tag="", async_mode=False, clients=LADDER_CLIENTS,
    io={"perform": [{"resp": None, "failure": F(E("weird"), V("unknown", retryable=False)),
                     "repairs": []}],
        "auth_refresh": False, "fallback_label": None})
add("_candidate_classification_provider", fb_client="c1", fb_model="m1", fb_label="L",
    task="t", clients=LADDER_CLIENTS,
    io={"_fallback_destination": DEST(provider="p2")})
add("_candidate_classification_provider", fb_client="c2", fb_model="m2", fb_label="L",
    task="t", clients={"c2": CL("c2", base_url="https://p2.example/v1")},
    io={"_fallback_destination": DEST(provider="p2"),
        "_effective_provider_for_client": "p2", "_fallback_provider_from_label": "p2",
        "_normalize_aux_provider": "p2"})


# ── top-up: ensure >=3 cases per function ─────────────────────────────────
add("_outcome", thunk="call_api", args={"nested": {"a": [1, 2]}},
    io={"call_api": S([1, 2, 3])})
add("_aoutcome", thunk="call_api", args={}, io={"call_api": S(None)})
add("_primary_failure", req={**PREP_REQ, "base_info": None}, exc=E("x", status=400),
    clients=LADDER_CLIENTS,
    io=merge_io({"classify_attempt": V("format_error")}, MATCH_FALSE))
add("_start_recovery_ladder", first_err=F(E("x"), V("auth", rotate=True)), req=PREP_REQ,
    retry_kwargs={"max_tokens": 0, "main_runtime": {}},
    task=None, async_mode=False, route_info={"provider": "p"}, clients=LADDER_CLIENTS)
add("_plan_aux_call", task=None, provider="nous", model="m9", base_url="https://x",
    api_key="***", main_runtime=None, messages=[], temperature=0.5, max_tokens=None, tools=None,
    timeout=10, extra_body=None, reasoning_config=None, extra_headers=None, api_mode=None,
    route_info=None, async_mode=False,
    clients=LADDER_CLIENTS, io=merge_io(PREP_IO, {"_normalize_main_runtime": {}}))
add("_candidate_classification_provider", fb_client="c1", fb_model=None, fb_label="main-agent(nous)",
    task="vision", clients=LADDER_CLIENTS,
    io={"_fallback_destination": DEST(provider="nous")})
add("_failure_sender", route=ROUTE(client="c2"), failure=F(E("x"), V(), client="c2"),
    clients=LADDER_CLIENTS)

# ── emit ──────────────────────────────────────────────────────────────────
def main():
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    seen = {}
    for c in CASES:
        seen[c["func"]] = seen.get(c["func"], 0) + 1
    missing = [f for f in seen if seen[f] < 2]
    if missing:
        print(f"WARNING: funcs with <2 cases: {missing}", file=sys.stderr)
    for c in CASES:
        print(json.dumps(c, separators=(",", ":"), ensure_ascii=True))
    print(f"# {len(CASES)} cases, {len(seen)} funcs", file=sys.stderr)

if __name__ == "__main__":
    main()
