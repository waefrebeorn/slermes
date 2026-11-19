#!/usr/bin/env python3
"""Generate tests/fixtures/chat_completion_helpers_flow.jsonl for the cch2 port.

Each line: {"func": <live_name>, "in": {...inputs..., "io": {seam scripts}}}.
Script semantics (shared with the C harness dispatcher and the oracle Script):
  - absent key            -> fail-closed RuntimeError "no script for <name>"
  - non-list value        -> constant result reused every call
  - list value            -> per-call queue (exhausted -> fail-closed)
  - {"__raise__":K,"message":M} -> raised exception of kind K
  - {"__script__":true,"ret":X} -> list-valued result X (wrap to avoid queue ambiguity)
  - null                  -> None result
>=5 cases per function; covers branches, seam order, raises, BaseException
passthrough and fail-closed paths. ASCII inputs; str()-positions integral.
"""
import json
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "chat_completion_helpers_flow.jsonl"

CASES = []


def add(func, inp, io=None):
    inp = dict(inp)
    inp["io"] = io or {}
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def L(ret):
    return {"__script__": True, "ret": ret}


# 1. _is_provider_stream_empty_frame_error
F = "_is_provider_stream_empty_frame_error"
add(F, {"exc": {"body": {"error": {"code": "provider_stream_empty_frame"}}}})
add(F, {"exc": {"body": {"error": {"code": "other"}}}})
add(F, {"exc": {"body": {"error": "a string"}}})
add(F, {"exc": {"body": None}})
add(F, {"exc": {"body": {}}})
add(F, {"exc": {"body": {"error": {"code": "provider_stream_empty_frame", "x": 1}}}})
add(F, {"exc": {}})

# 2. _rejects_stream_options
F = "_rejects_stream_options"
add(F, {"exc": {"status_code": 400, "body": "stream_options extra field", "message": ""}})
add(F, {"exc": {"status_code": 422, "body": "", "message": "stream_options not supported"}})
add(F, {"exc": {"status_code": 400, "body": "stream_options", "message": ""}})
add(F, {"exc": {"status_code": 500, "body": "stream_options extra", "message": ""}})
add(F, {"exc": {"status_code": 400, "body": "other field extra", "message": ""}})
add(F, {"exc": {"status_code": 400, "body": None, "message": "Stream_Options Unexpected"}})
add(F, {"exc": {"status_code": 422, "body": "stream_options unknown", "message": ""}})

# 3. _is_sse_connection_error
F = "_is_sse_connection_error"
add(F, {"is_api_error": True, "exc": {"message": "Connection lost"}})
add(F, {"is_api_error": True, "exc": {"status_code": 500, "message": "connection lost"}})
add(F, {"is_api_error": False, "exc": {"message": "connection lost"}})
add(F, {"is_api_error": True, "exc": {"message": "peer closed connection"}})
add(F, {"is_api_error": True, "exc": {"message": "some other error"}})
add(F, {"is_api_error": True, "exc": {"message": "UPSTREAM CONNECT ERROR"}})
add(F, {"is_api_error": True, "exc": {"status_code": 0, "message": "broken pipe"}})

# 4. _relay_stream_identity
F = "_relay_stream_identity"
add(F, {"agent": {"session_id": "s1", "provider": "openai", "model": "gpt"}, "name_default": "d"})
add(F, {"agent": {"provider": "", "model": "m"}, "name_default": "def"})
add(F, {"agent": {}, "name_default": "d"})
add(F, {"agent": {"session_id": "s", "provider": "p", "model": ""}, "name_default": "d"})
add(F, {"agent": {"session_id": 0, "provider": "p", "model": "m"}, "name_default": "d"})
add(F, {"agent": {"provider": "p", "model": "m", "session_id": "s"}, "name_default": None})

# 5. _relay_stream_metadata
F = "_relay_stream_metadata"
add(F, {"agent": {"is_subagent": True}, "api_mode": "chat"})
add(F, {"agent": {"_fallback_index": 2}, "api_mode": "chat"})
add(F, {"agent": {"_fallback_index": 0}, "api_mode": "chat"})
add(F, {"agent": {}, "api_mode": "chat"})
add(F, {"agent": {"_fallback_index": "3"}, "api_mode": "chat"})
add(F, {"agent": {"_fallback_index": "abc"}, "api_mode": "chat"})
add(F, {"agent": {"is_subagent": False, "_fallback_index": 1, "_current_api_request_id": "r1"}, "api_mode": "chat"})
add(F, {"agent": {"_fallback_index": None}, "api_mode": "chat"})

# 6. _stream_final_text
F = "_stream_final_text"
add(F, {"response": {"choices": [{"message": {"content": "hello"}}]}})
add(F, {"response": {"choices": [], "content": "world"}})
add(F, {"response": {"content": [{"text": "a"}, {"text": "b"}, {"notext": 1}]}})
add(F, {"response": {"content": "  x "}})
add(F, {"response": {}})
add(F, {"response": {"choices": [{"message": {"content": 123}}], "content": "fallback"}})
add(F, {"response": {"choices": "notlist", "content": "c"}})
add(F, {"response": {"content": [{"text": "a"}, {"text": 5}]}})

# 7. _high_effort_silence_floor
F = "_high_effort_silence_floor"
add(F, {"agent": {"reasoning_config": {"enabled": True, "effort": "high"}}})
add(F, {"agent": {"reasoning_config": {"effort": "xhigh"}}})
add(F, {"agent": {"reasoning_config": {"effort": "medium"}}})
add(F, {"agent": {"reasoning_config": {"enabled": False, "effort": "high"}}})
add(F, {"agent": {"reasoning_config": {"effort": "HIGH"}}})
add(F, {"agent": {"reasoning_config": {"effort": " high "}}})
add(F, {"agent": {"reasoning_config": None}})
add(F, {"agent": {"reasoning_config": {"effort": "ultra"}}})
add(F, {"agent": {"reasoning_config": {"effort": "none"}}})
add(F, {"agent": {"reasoning_config": {"enabled": True, "effort": "bogus"}}})

# 8. _consume_ephemeral_max_output
F = "_consume_ephemeral_max_output"
add(F, {"agent": {"_ephemeral_max_output_tokens": 1000}})
add(F, {"agent": {"_ephemeral_max_output_tokens": None}})
add(F, {"agent": {}})
add(F, {"agent": {"_ephemeral_max_output_tokens": 0}})
add(F, {"agent": {"_ephemeral_max_output_tokens": "x"}})

# 9. _model_dump_safe
F = "_model_dump_safe"
add(F, {}, {"model_dump": {"a": 1}})
add(F, {}, {"model_dump": [R("TypeError", "x"), {"a": 2}]})
add(F, {}, {"model_dump": R("ValueError", "v")})
add(F, {}, {"model_dump": R("SystemExit", "s")})
add(F, {}, {"model_dump": None})
add(F, {}, {})

# 10. _dump_if_model
F = "_dump_if_model"
add(F, {"has_model_dump": True, "value": {"v": 1}}, {"model_dump": {"d": 1}})
add(F, {"has_model_dump": False, "value": {"v": 1}})
add(F, {"has_model_dump": False, "value": "str"})
add(F, {"has_model_dump": True, "value": {}}, {"model_dump": [R("TypeError", "x"), [1, 2]]})
add(F, {"has_model_dump": True, "value": {}}, {"model_dump": L([1, 2])})
add(F, {"has_model_dump": False, "value": None})

# 11. _codex_silent_hang_hint
F = "_codex_silent_hang_hint"
add(F, {"hint_callable": True, "api_kwargs": {"model": "m"}}, {"hint_fn": "hint!"})
add(F, {"hint_callable": False, "api_kwargs": {"model": "m"}})
add(F, {"hint_callable": True, "api_kwargs": {}}, {"hint_fn": "h"})
add(F, {"hint_callable": True, "api_kwargs": {}}, {"hint_fn": R("ValueError", "v")})
add(F, {"hint_callable": True, "api_kwargs": {}}, {"hint_fn": R("SystemExit", "s")})
add(F, {"hint_callable": True, "api_kwargs": {}}, {})

# 12. _stream_env_stale_base
F = "_stream_env_stale_base"
add(F, {}, {"env_float": 120.0, "env_has": True})
add(F, {}, {"env_float": 180.0, "env_has": False})
add(F, {}, {"env_float": R("ValueError", "v")})
add(F, {}, {"env_float": 100, "env_has": R("SystemExit", "s")})
add(F, {}, {"env_has": True})

# 13. _local_stream_stale_timeout_default
F = "_local_stream_stale_timeout_default"
add(F, {}, {"load_config_readonly": {"agent": {"local_stream_stale_timeout": 300}}, "env_float": 300.0})
add(F, {}, {"load_config_readonly": {"agent": {"local_stream_stale_timeout": True}}, "env_float": 1.0})
add(F, {}, {"load_config_readonly": None, "env_float": 900.0})
add(F, {}, {"load_config_readonly": {"agent": {}}, "env_float": 900.0})
add(F, {}, {"load_config_readonly": R("ValueError", "v"), "env_float": 900.0})
add(F, {}, {"load_config_readonly": R("SystemExit", "s")})
add(F, {}, {"load_config_readonly": {"agent": {"local_stream_stale_timeout": "x"}}, "env_float": 900.0})
add(F, {}, {"load_config_readonly": {"agent": {"local_stream_stale_timeout": 0}}, "env_float": 0.0})

# 14. cap_to_run_budget
F = "cap_to_run_budget"
add(F, {"agent": {"run_budget_seconds": 100, "_run_budget_started_at": 50}, "timeout": 200}, {"time.time": 60})
add(F, {"agent": {"run_budget_seconds": 100, "_run_budget_started_at": 50}, "timeout": 30}, {"time.time": 60})
add(F, {"agent": {}, "timeout": 200})
add(F, {"agent": {"run_budget_seconds": 0, "_run_budget_started_at": 50}, "timeout": 200})
add(F, {"agent": {"run_budget_seconds": "abc", "_run_budget_started_at": 50}, "timeout": 200})
add(F, {"agent": {"run_budget_seconds": 100, "_run_budget_started_at": "xyz"}, "timeout": 200}, {"time.time": 60})
add(F, {"agent": {"run_budget_seconds": 100, "_run_budget_started_at": 50}, "timeout": 200}, {"time.time": R("SystemExit", "s")})
add(F, {"agent": {"run_budget_seconds": "100", "_run_budget_started_at": "50"}, "timeout": 200}, {"time.time": 60})

# 15. _cloud_stale_timeout
F = "_cloud_stale_timeout"
add(F, {"base": 180, "api_kwargs": {"model": "gpt"}},
    {"estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180.0, "get_reasoning_stale_timeout_floor": None})
add(F, {"base": 180, "api_kwargs": {"model": "reasoner"}},
    {"estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": 300})
add(F, {"base": 180, "api_kwargs": {"modelId": "us.anthropic.x"}},
    {"estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": None, "bedrock_reasoning_stale_floor": 240})
add(F, {"base": 180, "api_kwargs": {"model": "m", "modelId": "id"}},
    {"estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": None, "bedrock_reasoning_stale_floor": 240})
add(F, {"base": 180, "api_kwargs": {}},
    {"estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": None})
add(F, {"base": 180, "api_kwargs": {"model": "m"}}, {"estimate_request_context_tokens": R("ValueError", "v")})
add(F, {"base": 180, "api_kwargs": {"model": "m"}},
    {"estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": R("SystemExit", "s")})
add(F, {"base": 50, "api_kwargs": {"model": "m"}},
    {"estimate_request_context_tokens": 200000, "scale_stale_timeout_for_context": 300, "get_reasoning_stale_timeout_floor": None})

# 16. _stream_stale_timeout_is_explicit
F = "_stream_stale_timeout_is_explicit"
add(F, {"agent": {"provider": "p", "model": "m"}}, {"get_provider_stale_timeout": 120})
add(F, {"agent": {"provider": "p", "model": "m"}}, {"get_provider_stale_timeout": None, "env_float": 180, "env_has": True})
add(F, {"agent": {"provider": "p", "model": "m"}}, {"get_provider_stale_timeout": None, "env_float": 180, "env_has": False})
add(F, {"agent": {}})
add(F, {"agent": {"provider": "p"}})
add(F, {"agent": {"provider": "p", "model": "m"}}, {"get_provider_stale_timeout": R("SystemExit", "s")})
add(F, {"agent": {"provider": "p", "model": "m"}}, {"get_provider_stale_timeout": 0})

# 17. _cloud_stale_timeout_for
F = "_cloud_stale_timeout_for"
add(F, {"agent": {"provider": "p", "model": "m", "run_budget_seconds": 100, "_run_budget_started_at": 50}, "api_kwargs": {"model": "gpt"}},
    {"get_provider_stale_timeout": None, "env_float": 180, "env_has": False, "estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": None, "time.time": 60})
add(F, {"agent": {"provider": "p", "model": "m"}, "api_kwargs": {"model": "gpt"}}, {"get_provider_stale_timeout": 250})
add(F, {"agent": {"provider": "p", "model": "m"}, "api_kwargs": {"model": "gpt"}},
    {"get_provider_stale_timeout": None, "env_float": 180, "env_has": True, "estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": None})
add(F, {"agent": {"provider": "p", "model": "m"}, "api_kwargs": {"model": "r"}},
    {"get_provider_stale_timeout": None, "env_float": 180, "env_has": False, "estimate_request_context_tokens": 1000, "scale_stale_timeout_for_context": 180, "get_reasoning_stale_timeout_floor": 300})
add(F, {"agent": {}, "api_kwargs": {}})

# 18. _fallback_chain_exhausted
F = "_fallback_chain_exhausted"
add(F, {"agent": {"_fallback_chain": [{"provider": "p"}], "_rate_limited_until": 0}, "reason": "timeout"}, {"time.monotonic": 100})
add(F, {"agent": {"_fallback_chain": [{"provider": "p"}], "_rate_limited_until": 0}, "reason": "rate_limit"})
add(F, {"agent": {"_fallback_chain": [{"provider": "p"}]}, "reason": "billing"})
add(F, {"agent": {"_fallback_chain": []}, "reason": "timeout"})
add(F, {"agent": {"_fallback_chain": [{}], "_rate_limited_until": 200}, "reason": "timeout"}, {"time.monotonic": 100})
add(F, {"agent": {"_fallback_chain": [{}]}, "reason": None}, {"time.monotonic": 10})
add(F, {"agent": {}})
add(F, {"agent": {"_fallback_chain": [{}]}, "reason": "timeout"}, {"time.monotonic": R("SystemExit", "s")})

# 19. _log_fallback_activated
F = "_log_fallback_activated"
add(F, {"reason": "timeout", "old_model": "m1", "old_provider": "p1", "fb_model": "m2", "fb_provider": "p2"}, {"logger.info": None})
add(F, {"reason": "billing", "old_model": "m1", "old_provider": "p1", "fb_model": "m2", "fb_provider": "p2"},
    {"get_hermes_home": "/h", "profile_name_for_home": "default", "logger.warning": None})
add(F, {"reason": "billing", "old_model": "m1", "old_provider": "p1", "fb_model": "m2", "fb_provider": "p2"},
    {"get_hermes_home": "/h", "profile_name_for_home": "prof", "logger.warning": None})
add(F, {"reason": "billing", "old_model": "m1", "old_provider": "p1", "fb_model": "m2", "fb_provider": "p2"},
    {"get_hermes_home": "/h", "profile_name_for_home": None, "logger.warning": None})
add(F, {"reason": None, "old_model": "m1", "old_provider": "p1", "fb_model": "m2", "fb_provider": "p2"}, {"logger.info": None})
add(F, {"reason": "billing", "old_model": "m1", "old_provider": "p1", "fb_model": "m2", "fb_provider": "p2"},
    {"get_hermes_home": R("SystemExit", "s")})
add(F, {"reason": "billing", "old_model": "m1", "old_provider": "p1", "fb_model": "m2", "fb_provider": "p2"},
    {"get_hermes_home": "/h", "profile_name_for_home": "default", "logger.warning": R("ValueError", "v")})

# 20. _rebind_fallback_credential_pool
F = "_rebind_fallback_credential_pool"
add(F, {"agent": {"_credential_pool": {"provider": "openai"}, "_credential_pool_entry_id": "e1"}, "fb_provider": "anthropic", "fb_model": "m", "prefetched_pool": None},
    {"logger.info": None, "load_route_pool": {"provider": "anthropic"}, "pool_has_credentials": True})
add(F, {"agent": {"_credential_pool": {"provider": "openai"}}, "fb_provider": "anthropic", "fb_model": "m", "prefetched_pool": {"provider": "anthropic"}},
    {"pool_has_credentials": True, "logger.info": None})
add(F, {"agent": {"_credential_pool": {"provider": "openai"}, "_credential_pool_entry_id": "e1"}, "fb_provider": "openai", "fb_model": "m", "prefetched_pool": None})
add(F, {"agent": {"_credential_pool": None}, "fb_provider": "p", "fb_model": "m", "prefetched_pool": None}, {"load_route_pool": None})
add(F, {"agent": {"_credential_pool": None}, "fb_provider": "p", "fb_model": "m", "prefetched_pool": None},
    {"load_route_pool": R("ValueError", "v"), "logger.debug": None})
add(F, {"agent": {"_credential_pool": None}, "fb_provider": "p", "fb_model": "m", "prefetched_pool": None},
    {"load_route_pool": R("SystemExit", "s")})
add(F, {"agent": {"_credential_pool": None}, "fb_provider": "p", "fb_model": "m", "prefetched_pool": None},
    {"load_route_pool": {"provider": "p"}, "pool_has_credentials": False})
add(F, {"agent": {"_credential_pool": {"provider": "openai"}}, "fb_provider": "anthropic", "fb_model": "m", "prefetched_pool": None},
    {"logger.info": R("ValueError", "v")})

# 21. _candidate_pool_exhausted
F = "_candidate_pool_exhausted"
add(F, {"agent": {"_credential_pool": {"provider": "p"}}, "fb_provider": "p", "fb_model": "m"},
    {"pool_has_credentials": True, "pool_has_available": False, "pool_next_available_at": 1000, "time.time": 100})
add(F, {"agent": {"_credential_pool": {"provider": "p"}}, "fb_provider": "p", "fb_model": "m"},
    {"pool_has_credentials": True, "pool_has_available": True})
add(F, {"agent": {"_credential_pool": {"provider": "p"}}, "fb_provider": "p", "fb_model": "m"}, {"pool_has_credentials": False})
add(F, {"agent": {"_credential_pool": None}, "fb_provider": "p", "fb_model": "m"},
    {"load_pool": {"provider": "p"}, "pool_has_credentials": True, "pool_has_available": False, "pool_next_available_at": None})
add(F, {"agent": {"_credential_pool": {"provider": "other"}}, "fb_provider": "p", "fb_model": "m"},
    {"load_pool": {"provider": "p"}, "pool_has_credentials": True, "pool_has_available": False, "pool_next_available_at": 500, "time.time": 100})
add(F, {"agent": {"_credential_pool": None}, "fb_provider": "p", "fb_model": "m"}, {"load_pool": R("ValueError", "v")})
add(F, {"agent": {"_credential_pool": None}, "fb_provider": "p", "fb_model": "m"}, {"load_pool": R("SystemExit", "s")})
add(F, {"agent": {"_credential_pool": {"provider": "p"}}, "fb_provider": "p", "fb_model": "m"},
    {"pool_has_credentials": True, "pool_has_available": False, "pool_next_available_at": 700, "time.time": 100})

# 22. _update_fallback_context_compressor
F = "_update_fallback_context_compressor"
add(F, {"agent": {"context_compressor": {}, "model": "m", "base_url": "u", "api_key": "k", "provider": "p", "api_mode": "chat"}},
    {"get_model_context_length": 100000, "compressor.update_model": None})
add(F, {"agent": {"context_compressor": None}})
add(F, {"agent": {"context_compressor": {}, "model": "m", "base_url": "u", "api_key": "k", "provider": "p", "api_mode": "chat", "_compression_feasibility_checked": True}},
    {"get_model_context_length": 1000, "compressor.update_model": None, "revalidate_compression_feasibility": None})
add(F, {"agent": {"context_compressor": {}, "model": "m", "base_url": "u", "api_key": 123, "provider": "p", "api_mode": "chat"}},
    {"get_model_context_length": 1000, "compressor.update_model": None})
add(F, {"agent": {"context_compressor": {}}})
add(F, {"agent": {"context_compressor": {}, "model": "m", "base_url": "u", "api_key": "k", "provider": "p", "api_mode": "chat"}},
    {"get_model_context_length": R("SystemExit", "s")})
add(F, {"agent": {"context_compressor": {}, "model": "m", "base_url": "u", "api_key": "k", "provider": "p", "api_mode": "chat", "_compression_feasibility_checked": False}},
    {"get_model_context_length": 1000, "compressor.update_model": None})

# 23. _reresolve_fallback_reasoning_config
F = "_reresolve_fallback_reasoning_config"
add(F, {"agent": {"model": "m", "reasoning_config": {"old": 1}}},
    {"load_config": {"cfg": 1}, "resolve_reasoning_config": {"new": 2}, "logger.info": None})
add(F, {"agent": {"model": "m", "reasoning_config": {"old": 1}}},
    {"load_config": None, "resolve_reasoning_config": {"r": 1}, "logger.info": None})
add(F, {"agent": {"model": "m", "reasoning_config": {"old": 1}}},
    {"load_config": R("ValueError", "v"), "logger.debug": None})
add(F, {"agent": {"model": "m", "reasoning_config": {"old": 1}}}, {"load_config": R("SystemExit", "s")})
add(F, {"agent": {"model": "m", "reasoning_config": {"old": 1}}},
    {"load_config": {}, "resolve_reasoning_config": R("ValueError", "v"), "logger.debug": None})
add(F, {"agent": {"model": "m", "reasoning_config": {"old": 1}}},
    {"load_config": {}, "resolve_reasoning_config": {"r": 1}, "logger.info": R("ValueError", "v"), "logger.debug": None})

# 24. _rescope_fallback_extra_body
F = "_rescope_fallback_extra_body"
add(F, {"agent": {"model": "m", "request_overrides": {"extra_body": {"a": 1, "b": 2}}, "_custom_providers": []}, "old_provider": "op", "old_model": "om", "old_base_url": "ou"},
    {"custom_provider_extra_body_for_agent": {"a": 1}, "merge_custom_provider_extra_body": None, "logger.info": None})
add(F, {"agent": {"model": "m", "request_overrides": {"extra_body": {"a": 1, "b": 2}}, "_custom_providers": []}, "old_provider": "op", "old_model": "om", "old_base_url": "ou"},
    {"custom_provider_extra_body_for_agent": {"a": 1, "b": 2}, "merge_custom_provider_extra_body": None, "logger.info": None})
add(F, {"agent": {"model": "m", "request_overrides": {"extra_body": {"a": 1, "b": 2}}, "_custom_providers": []}, "old_provider": "op", "old_model": "om", "old_base_url": "ou"},
    {"custom_provider_extra_body_for_agent": None, "merge_custom_provider_extra_body": None, "logger.info": None})
add(F, {"agent": {"model": "m", "request_overrides": {"extra_body": "notdict"}}, "old_provider": "op", "old_model": "om", "old_base_url": "ou"},
    {"custom_provider_extra_body_for_agent": {"a": 1}, "merge_custom_provider_extra_body": None, "logger.info": None})
add(F, {"agent": {"model": "m", "request_overrides": {"extra_body": {"a": 1}}}, "old_provider": "op", "old_model": "om", "old_base_url": "ou"},
    {"custom_provider_extra_body_for_agent": R("ValueError", "v"), "logger.debug": None})
add(F, {"agent": {"model": "m", "request_overrides": {"extra_body": {"a": 1}}}, "old_provider": "op", "old_model": "om", "old_base_url": "ou"},
    {"custom_provider_extra_body_for_agent": {"a": 9}, "merge_custom_provider_extra_body": None, "logger.info": None})
add(F, {"agent": {"model": "m", "request_overrides": {"extra_body": {"a": 1}}}, "old_provider": "op", "old_model": "om", "old_base_url": "ou"},
    {"custom_provider_extra_body_for_agent": {"a": 1}, "merge_custom_provider_extra_body": R("SystemExit", "s")})

# 25. _fallback_api_mode_hint
F = "_fallback_api_mode_hint"
add(F, {"fb": {"api_mode": "anthropic_messages"}, "fb_provider": "p", "fb_base_url_hint": None}, {"parse_api_mode": "anthropic_messages"})
add(F, {"fb": {"transport": "codex_responses"}, "fb_provider": "p", "fb_base_url_hint": None}, {"parse_api_mode": "codex_responses"})
add(F, {"fb": {}, "fb_provider": "named", "fb_base_url_hint": None}, {"parse_api_mode": None, "get_named_custom_provider": {"api_mode": "anthropic_messages"}})
add(F, {"fb": {}, "fb_provider": "anthropic", "fb_base_url_hint": None}, {"parse_api_mode": None})
add(F, {"fb": {}, "fb_provider": "custom", "fb_base_url_hint": "https://api.anthropic.com"}, {"parse_api_mode": None, "host_mandated_api_mode": "anthropic_messages"})
add(F, {"fb": {}, "fb_provider": "p", "fb_base_url_hint": None}, {"parse_api_mode": None, "get_named_custom_provider": None})
add(F, {"fb": {}, "fb_provider": "p", "fb_base_url_hint": "u"}, {"parse_api_mode": None, "get_named_custom_provider": {}, "host_mandated_api_mode": "chat_completions"})
add(F, {"fb": {}, "fb_provider": "p", "fb_base_url_hint": None}, {"parse_api_mode": R("SystemExit", "s")})

# 26. _is_anthropic_wire_url
F = "_is_anthropic_wire_url"
add(F, {"url": "u"}, {"host_mandated_api_mode": "anthropic_messages"})
add(F, {"url": "u"}, {"host_mandated_api_mode": "chat_completions"})
add(F, {"url": "u"}, {"host_mandated_api_mode": None})
add(F, {"url": "u"}, {"host_mandated_api_mode": R("SystemExit", "s")})
add(F, {"url": "u"}, {})

# 27. _managed_summary_call
F = "_managed_summary_call"
add(F, {"agent": {"provider": "p", "model": "m", "api_mode": "chat"}, "api_request_id": "r1", "request": {"k": 1}, "retry_count": 0, "callback": "agent._interruptible_api_call"},
    {"relay_llm.execute_current": {"resp": 1}})
add(F, {"agent": {}, "api_request_id": "r2", "request": {}, "retry_count": 0, "callback": "agent._interruptible_api_call"},
    {"relay_llm.execute_current": {"resp": 2}})
add(F, {"agent": {"provider": "p", "model": "m"}, "api_request_id": "r3", "request": {}, "retry_count": 1, "callback": "agent._interruptible_api_call"},
    {"relay_llm.execute_current": R("SystemExit", "s")})
add(F, {"agent": {"provider": "p", "model": "m", "api_mode": "codex_responses"}, "api_request_id": "r4", "request": {}, "retry_count": 2, "callback": "agent._interruptible_api_call"},
    {"relay_llm.execute_current": {"resp": 4}})
add(F, {"agent": {"provider": "p", "model": "m"}, "api_request_id": "r5", "request": {}, "retry_count": 0, "callback": "agent._interruptible_api_call"},
    {"relay_llm.execute_current": None})

# 28. _summary_text
F = "_summary_text"
add(F, {"agent": {}, "response": {"r": 1}, "normalize_kwargs": {}},
    {"is_router_timeout_shim": False, "transport.normalize_response": {"tool_calls": None, "content": "  hello "}})
add(F, {"agent": {}, "response": {"r": 1}, "normalize_kwargs": {}}, {"is_router_timeout_shim": True, "logger.warning": None})
add(F, {"agent": {}, "response": {"r": 1}, "normalize_kwargs": {}},
    {"is_router_timeout_shim": False, "transport.normalize_response": {"tool_calls": [1], "content": "x"}, "logger.warning": None})
add(F, {"agent": {}, "response": {"r": 1}, "normalize_kwargs": {}},
    {"is_router_timeout_shim": False, "transport.normalize_response": {"content": None}})
add(F, {"agent": {}, "response": {"r": 1}, "normalize_kwargs": {"strip_tool_prefix": True}},
    {"is_router_timeout_shim": False, "transport.normalize_response": {"content": "y"}})
add(F, {"agent": {}, "response": {"r": 1}, "normalize_kwargs": {}}, {"is_router_timeout_shim": R("SystemExit", "s")})
add(F, {"agent": {}, "response": {"r": 1}, "normalize_kwargs": {}},
    {"is_router_timeout_shim": False, "transport.normalize_response": R("ValueError", "v")})

# 29. _with_stream_emitters
F = "_with_stream_emitters"
add(F, {"agent": {"_emit_stream_start": True, "_emit_stream_end": True}},
    {"emit_stream_start": None, "run": {"choices": [{"message": {"content": "hi"}}]}, "emit_stream_end": None})
add(F, {"agent": {}}, {"run": {"content": "x"}})
add(F, {"agent": {"_emit_stream_end": True}}, {"run": R("ValueError", "v"), "emit_stream_end": None})
add(F, {"agent": {"_emit_stream_start": True, "_emit_stream_end": True}}, {"emit_stream_start": None, "run": R("SystemExit", "s")})
add(F, {"agent": {"_emit_stream_end": True}}, {"run": {"content": "x"}, "emit_stream_end": R("ValueError", "e")})
add(F, {"agent": {"_emit_stream_end": True}}, {"run": {"choices": [{"message": {"content": "a"}}]}, "emit_stream_end": None})
add(F, {"agent": {"_emit_stream_start": True}}, {"emit_stream_start": R("SystemExit", "s")})

# 30. _finalize_bedrock_relay_events
F = "_finalize_bedrock_relay_events"
add(F, {"events": [1, 2]}, {"stream_converse_with_callbacks": {"result": 1}})
add(F, {"events": [1]}, {"stream_converse_with_callbacks": R("EmptyStreamError", "empty")})
add(F, {"events": [1]}, {"stream_converse_with_callbacks": R("ValueError", "v")})
add(F, {"events": [1]}, {"stream_converse_with_callbacks": R("SystemExit", "s")})
add(F, {"events": []}, {"stream_converse_with_callbacks": None})
add(F, {"events": [1]}, {})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=True, separators=(",", ":")) + "\n")
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    missing = set(counts) ^ set(FUNCS_EXPECTED)
    if missing:
        raise SystemExit("func set drift: %s" % sorted(missing))


FUNCS_EXPECTED = [
    "_is_provider_stream_empty_frame_error", "_rejects_stream_options",
    "_is_sse_connection_error", "_relay_stream_identity", "_relay_stream_metadata",
    "_stream_final_text", "_high_effort_silence_floor", "_consume_ephemeral_max_output",
    "_model_dump_safe", "_dump_if_model", "_codex_silent_hang_hint",
    "_stream_env_stale_base", "_local_stream_stale_timeout_default", "cap_to_run_budget",
    "_cloud_stale_timeout", "_stream_stale_timeout_is_explicit", "_cloud_stale_timeout_for",
    "_fallback_chain_exhausted", "_log_fallback_activated", "_rebind_fallback_credential_pool",
    "_candidate_pool_exhausted", "_update_fallback_context_compressor",
    "_reresolve_fallback_reasoning_config", "_rescope_fallback_extra_body",
    "_fallback_api_mode_hint", "_is_anthropic_wire_url", "_managed_summary_call",
    "_summary_text", "_with_stream_emitters", "_finalize_bedrock_relay_events",
]

if __name__ == "__main__":
    main()