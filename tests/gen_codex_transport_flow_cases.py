#!/usr/bin/env python3
"""Generate tests/fixtures/codex_transport_flow.jsonl for the
agent/transports/codex.py UNCLAIMED-remainder FLOW port
(src/agent/transports/codex_transport_flow.c).

Case shape: {"func": <name>, "in": {...args..., "io": {seam: script}}}.
Scripts (shared contract with the C universal seam cxt_call and the oracle):
  - MISSING key            -> fail-closed RuntimeError("no script for <name>")
  - null                   -> None result
  - {"$seq": [v1, v2, ...]}-> one result per call; exhaustion raises
                              RuntimeError("script exhausted: <name>")
  - {"__raise__": cls, "msg": m, ...} -> raises; trace ret is the WHOLE marker
  - any other value        -> reused on every call (a bare ARRAY is a LIST
                              RESULT, not a queue — wrap queues in $seq)

Seam names are the LIVE collaborator names (underscores kept). Claimed siblings:
_bounded_prompt_cache_key, _xai_prefers_native_web_search, _is_opencode_responses_backend,
_alias_reserved_tools, _profile_declared_efforts. Foreign imports: base_url_hostname,
base_url_host_matches, is_grok_46_family, openai_model_rejects_reasoning,
grok_supports_reasoning_effort, is_official_codex_base_url, openai_reasoning_family,
strip_unverified_reasoning_items, get_active_search_provider, _get_search_backend,
clamp_effort, is_astra_model, codex_supported_efforts. Logger: logger.warning.

Fixture domain (documented in the port header):
  - ASCII strings; base_url / provider / model str-or-null.
  - str()-computed positions carry strings/integers only (no non-integral float repr).
  - include items / declared effort levels / header values are strings or integers.
  - error (reject_all_turns) is str / null / exception-marker {__raise__,msg,status_code}.
  - _all_turns_rejected an array of strings; _last_issuer_model a string on the mutate
    path; message_lists an array; _alias_reserved_tools results 2-element arrays.
  - integral floats avoided in str() positions; _coerce_timeout uses small numbers.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "codex_transport_flow.jsonl"

ASTRA = ["low", "medium", "high", "xhigh", "max"]
LEGACY = ["none", "low", "medium", "high", "xhigh"]
GROK46 = ["low", "medium", "high", "xhigh"]
XLEG = ["low", "medium", "high"]


def seq(*vals):
    return {"$seq": list(vals)}


def rais(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []


def add(*cases):
    CASES.extend(cases)


# ── _coerce_timeout ─────────────────────────────────────────────────────────
add(
    case("_coerce_timeout", timeout=5),
    case("_coerce_timeout", timeout=5.5),
    case("_coerce_timeout", timeout=0.1),
    case("_coerce_timeout", timeout=100),
    case("_coerce_timeout", timeout=0),
    case("_coerce_timeout", timeout=-3),
    case("_coerce_timeout", timeout=None),
    case("_coerce_timeout", timeout=True),
    case("_coerce_timeout", timeout=False),
    case("_coerce_timeout", timeout="5"),
    case("_coerce_timeout", timeout=[5]),
)

# ── _is_openai_api_origin ───────────────────────────────────────────────────
add(
    case("_is_openai_api_origin", base_url="https://api.openai.com/v1",
         io={"base_url_hostname": "api.openai.com"}),
    case("_is_openai_api_origin", base_url="https://API.OPENAI.COM",
         io={"base_url_hostname": "API.OPENAI.COM"}),
    case("_is_openai_api_origin", base_url="https://proxy.example.com",
         io={"base_url_hostname": "proxy.example.com"}),
    case("_is_openai_api_origin", base_url=None, io={"base_url_hostname": ""}),
    case("_is_openai_api_origin", base_url="", io={"base_url_hostname": ""}),
    case("_is_openai_api_origin", base_url="https://chatgpt.com",
         io={"base_url_hostname": "chatgpt.com"}),
    case("_is_openai_api_origin", base_url="https://x", io={"base_url_hostname": rais("ValueError")}),
    case("_is_openai_api_origin", base_url="https://x", io={"base_url_hostname": rais("KeyboardInterrupt")}),
    case("_is_openai_api_origin", base_url="https://x", io={}),
)

# ── _is_perplexity_responses_backend ────────────────────────────────────────
add(
    case("_is_perplexity_responses_backend", params={"base_url": "https://api.perplexity.ai"},
         io={"base_url_hostname": "api.perplexity.ai"}),
    case("_is_perplexity_responses_backend", params={"base_url": "https://x"},
         io={"base_url_hostname": "API.PERPLEXITY.AI"}),
    case("_is_perplexity_responses_backend", params={"base_url": "https://x"},
         io={"base_url_hostname": "other.com"}),
    case("_is_perplexity_responses_backend", params={}, io={"base_url_hostname": ""}),
    case("_is_perplexity_responses_backend", params={"base_url": "https://x"},
         io={"base_url_hostname": rais("ValueError")}),
    case("_is_perplexity_responses_backend", params={"base_url": "https://x"},
         io={"base_url_hostname": rais("RuntimeError")}),
    case("_is_perplexity_responses_backend", params={"base_url": "https://x"},
         io={"base_url_hostname": rais("KeyboardInterrupt")}),
    case("_is_perplexity_responses_backend", params={"base_url": "https://x"}, io={}),
)

# ── _is_azure_responses ─────────────────────────────────────────────────────
add(
    case("_is_azure_responses", params={"provider": "azure-foundry"}, io={}),
    case("_is_azure_responses", params={"provider": " Azure-Foundry "}, io={}),
    case("_is_azure_responses", params={"provider": "other", "base_url": "https://x.openai.azure.com"},
         io={"base_url_host_matches": True}),
    case("_is_azure_responses", params={"provider": "other", "base_url": "https://x.services.ai.azure.com"},
         io={"base_url_host_matches": seq(False, True)}),
    case("_is_azure_responses", params={"provider": "other", "base_url": "https://x.com"},
         io={"base_url_host_matches": seq(False, False)}),
    case("_is_azure_responses", params={}, io={"base_url_host_matches": seq(False, False)}),
    case("_is_azure_responses", params={"base_url": "https://x"},
         io={"base_url_host_matches": rais("ValueError")}),
    case("_is_azure_responses", params={"base_url": "https://x"},
         io={"base_url_host_matches": rais("KeyboardInterrupt")}),
    case("_is_azure_responses", params={"base_url": "https://x"}, io={}),
)

# ── _reserves_tool_search ───────────────────────────────────────────────────
add(
    case("_reserves_tool_search", params={}, is_xai_responses=True, io={}),
    case("_reserves_tool_search", params={"is_codex_backend": True}, is_xai_responses=False, io={}),
    case("_reserves_tool_search", params={"is_codex_backend": 1, "base_url": "https://api.openai.com"},
         is_xai_responses=False, io={"base_url_hostname": "api.openai.com"}),
    case("_reserves_tool_search", params={"base_url": "https://api.openai.com"},
         is_xai_responses=False, io={"base_url_hostname": "api.openai.com"}),
    case("_reserves_tool_search", params={"base_url": "https://chatgpt.com"},
         is_xai_responses=False, io={"base_url_hostname": "chatgpt.com"}),
    case("_reserves_tool_search", params={"base_url": "https://x"},
         is_xai_responses=False, io={"base_url_hostname": "other.com"}),
    case("_reserves_tool_search", params={"base_url": "https://x"},
         is_xai_responses=False, io={"base_url_hostname": rais("ValueError")}),
    case("_reserves_tool_search", params={"base_url": "https://x"},
         is_xai_responses=False, io={"base_url_hostname": rais("KeyboardInterrupt")}),
    case("_reserves_tool_search", params={"base_url": "https://x"}, is_xai_responses=False, io={}),
)

# ── _openai_prefers_native_web_search ───────────────────────────────────────
add(
    case("_openai_prefers_native_web_search", io={"get_active_search_provider": {"name": "openai-native"}}),
    case("_openai_prefers_native_web_search", io={"get_active_search_provider": {"name": "xai"}}),
    case("_openai_prefers_native_web_search", io={"get_active_search_provider": {}}),
    case("_openai_prefers_native_web_search",
         io={"get_active_search_provider": None, "_get_search_backend": "openai-native"}),
    case("_openai_prefers_native_web_search",
         io={"get_active_search_provider": None, "_get_search_backend": " OpenAI-Native "}),
    case("_openai_prefers_native_web_search",
         io={"get_active_search_provider": None, "_get_search_backend": "xai"}),
    case("_openai_prefers_native_web_search",
         io={"get_active_search_provider": None, "_get_search_backend": None}),
    case("_openai_prefers_native_web_search", io={"get_active_search_provider": rais("ValueError")}),
    case("_openai_prefers_native_web_search",
         io={"get_active_search_provider": None, "_get_search_backend": rais("RuntimeError")}),
    case("_openai_prefers_native_web_search", io={"get_active_search_provider": rais("KeyboardInterrupt")}),
    case("_openai_prefers_native_web_search", io={}),
    case("_openai_prefers_native_web_search", io={"get_active_search_provider": None}),
)

# ── _is_official_openai_responses_route ─────────────────────────────────────
add(
    case("_is_official_openai_responses_route", model="gpt-6.2-astra", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com"}),
    case("_is_official_openai_responses_route", model="vendor/gpt-6.2-sol", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com"}),
    case("_is_official_openai_responses_route", model="gpt-6.2-astra", base_url="https://proxy.com",
         io={"base_url_hostname": "proxy.com", "is_official_codex_base_url": True}),
    case("_is_official_openai_responses_route", model="gpt-6.2-astra", base_url="https://proxy.com",
         io={"base_url_hostname": seq("proxy.com", "us.api.openai.com"), "is_official_codex_base_url": False}),
    case("_is_official_openai_responses_route", model="gpt-6.2-astra", base_url="https://proxy.com",
         io={"base_url_hostname": seq("proxy.com", "eu.api.openai.com"), "is_official_codex_base_url": False}),
    case("_is_official_openai_responses_route", model="gpt-6.2-astra", base_url="https://proxy.com",
         io={"base_url_hostname": seq("proxy.com", "US.API.OPENAI.COM"), "is_official_codex_base_url": False}),
    case("_is_official_openai_responses_route", model="gpt-6.2-astra", base_url="https://proxy.com",
         io={"base_url_hostname": seq("proxy.com", "other.com"), "is_official_codex_base_url": False}),
    case("_is_official_openai_responses_route", model="gpt-6-astra", base_url="https://api.openai.com",
         io={"is_astra_model": True, "base_url_hostname": "api.openai.com"}),
    case("_is_official_openai_responses_route", model="gpt-6-astra", base_url="https://x",
         io={"is_astra_model": False}),
    case("_is_official_openai_responses_route", model="gpt-6-astra", base_url="https://proxy.com",
         io={"is_astra_model": True, "base_url_hostname": "proxy.com"}),
    case("_is_official_openai_responses_route", model=None, base_url="https://x",
         io={"is_astra_model": False}),
    case("_is_official_openai_responses_route", model="gpt-6.2-astra", base_url="https://x", io={}),
    case("_is_official_openai_responses_route", model="gpt-6-astra", base_url="https://x",
         io={"is_astra_model": rais("ValueError")}),
)

# ── _codex_efforts_for_route ────────────────────────────────────────────────
add(
    case("_codex_efforts_for_route", model="gpt-4o", base_url="https://api.openai.com", is_codex_backend=False,
         io={"base_url_hostname": "api.openai.com", "openai_model_rejects_reasoning": True}),
    case("_codex_efforts_for_route", model="gpt-4o", base_url="https://api.openai.com", is_codex_backend=False,
         io={"base_url_hostname": "api.openai.com", "openai_model_rejects_reasoning": False,
             "is_astra_model": False, "codex_supported_efforts": LEGACY}),
    case("_codex_efforts_for_route", model="gpt-5", base_url="https://x", is_codex_backend=True,
         io={"is_astra_model": False, "codex_supported_efforts": LEGACY}),
    case("_codex_efforts_for_route", model="gpt-6-astra", base_url="https://x", is_codex_backend=True,
         io={"is_astra_model": True, "codex_supported_efforts": ASTRA}),
    case("_codex_efforts_for_route", model="gpt-6-astra", base_url="https://proxy.com", is_codex_backend=False,
         io={"base_url_hostname": seq("proxy.com", "proxy.com"), "is_astra_model": seq(True, True)}),
    case("_codex_efforts_for_route", model="x", base_url="https://proxy.com", is_codex_backend=False,
         io={"base_url_hostname": "proxy.com", "is_astra_model": False, "codex_supported_efforts": []}),
    case("_codex_efforts_for_route", model="gpt-4o", base_url="https://api.openai.com", is_codex_backend=False,
         io={"base_url_hostname": "api.openai.com", "openai_model_rejects_reasoning": rais("ValueError")}),
    case("_codex_efforts_for_route", model="gpt-5", base_url="https://x", is_codex_backend=True, io={}),
)

# ── _sends_all_turns ────────────────────────────────────────────────────────
add(
    case("_sends_all_turns", model="gpt-5.4", params={"is_codex_backend": True}, io={}),
    case("_sends_all_turns", model="gpt-5.4", params={"is_codex_backend": 1, "base_url": "https://api.openai.com"},
         io={"base_url_hostname": "api.openai.com", "openai_reasoning_family": "gpt-6"}),
    case("_sends_all_turns", model="gpt-5.4", params={"base_url": "https://api.openai.com"},
         io={"base_url_hostname": "api.openai.com", "openai_reasoning_family": "gpt-6"}),
    case("_sends_all_turns", model="gpt-5.4", params={"base_url": "https://api.openai.com"},
         io={"base_url_hostname": "api.openai.com", "openai_reasoning_family": None}),
    case("_sends_all_turns", model="gpt-5.4", params={"base_url": "https://proxy.com"},
         io={"base_url_hostname": "proxy.com"}),
    case("_sends_all_turns", model="gpt-5.4", params={}, io={"base_url_hostname": ""}),
    case("_sends_all_turns", model="gpt-5.4", params={"base_url": "https://api.openai.com"},
         io={"base_url_hostname": "api.openai.com", "openai_reasoning_family": rais("ValueError")}),
    case("_sends_all_turns", model="gpt-5.4", params={"base_url": "https://x"}, io={}),
)

# ── _reasoning_fields ───────────────────────────────────────────────────────
add(
    case("_reasoning_fields", model="grok-4.6", params={}, effort="high", enabled=True,
         replay_encrypted_reasoning=True, is_xai_responses=True, profile_reasoning=False,
         io={"grok_supports_reasoning_effort": True}),
    case("_reasoning_fields", model="grok-4.6", params={}, effort="high", enabled=True,
         replay_encrypted_reasoning=True, is_xai_responses=True, profile_reasoning=False,
         io={"grok_supports_reasoning_effort": False}),
    case("_reasoning_fields", model="grok-4.6", params={}, effort="low", enabled=True,
         replay_encrypted_reasoning=False, is_xai_responses=True, profile_reasoning=False,
         io={"grok_supports_reasoning_effort": True}),
    case("_reasoning_fields", model="gpt-5", params={}, effort="medium", enabled=True,
         replay_encrypted_reasoning=True, is_xai_responses=False, profile_reasoning=False, all_turns=False, io={}),
    case("_reasoning_fields", model="gpt-5", params={}, effort="medium", enabled=True,
         replay_encrypted_reasoning=False, is_xai_responses=False, profile_reasoning=False, all_turns=True, io={}),
    case("_reasoning_fields", model="gpt-5", params={"profile_reasoning": {"effort": "high"}}, effort="medium",
         enabled=True, replay_encrypted_reasoning=True, is_xai_responses=False, profile_reasoning=True, io={}),
    case("_reasoning_fields", model="gpt-5", params={}, effort="medium", enabled=True,
         replay_encrypted_reasoning=True, is_xai_responses=False, profile_reasoning=True, io={}),
    case("_reasoning_fields", model="gpt-5", params={}, effort="none", enabled=False,
         replay_encrypted_reasoning=True, is_xai_responses=False, profile_reasoning=False, io={}),
    case("_reasoning_fields", model="gpt-5", params={}, effort="medium", enabled=False,
         replay_encrypted_reasoning=True, is_xai_responses=False, profile_reasoning=False, io={}),
    case("_reasoning_fields", model="gpt-5", params={}, effort="none", enabled=False,
         replay_encrypted_reasoning=True, is_xai_responses=False, profile_reasoning=True, io={}),
    case("_reasoning_fields", model="grok", params={}, effort="none", enabled=False,
         replay_encrypted_reasoning=True, is_xai_responses=True, profile_reasoning=False, io={}),
    case("_reasoning_fields", model="grok", params={}, effort="high", enabled=True,
         replay_encrypted_reasoning=True, is_xai_responses=True, profile_reasoning=False,
         io={"grok_supports_reasoning_effort": rais("ValueError")}),
)

# ── _bound_prompt_cache_key_field ───────────────────────────────────────────
add(
    case("_bound_prompt_cache_key_field", container={"prompt_cache_key": "short"},
         io={"_bounded_prompt_cache_key": "short"}),
    case("_bound_prompt_cache_key_field", container={"prompt_cache_key": "x" * 80},
         io={"_bounded_prompt_cache_key": "pck_abc123"}),
    case("_bound_prompt_cache_key_field", container={"prompt_cache_key": ""},
         io={"_bounded_prompt_cache_key": None}),
    case("_bound_prompt_cache_key_field", container={"prompt_cache_key": None},
         io={"_bounded_prompt_cache_key": None}),
    case("_bound_prompt_cache_key_field", container={"other": 1}, io={}),
    case("_bound_prompt_cache_key_field", container=None, io={}),
    case("_bound_prompt_cache_key_field", container={}, io={}),
    case("_bound_prompt_cache_key_field", container={"prompt_cache_key": "x"},
         io={"_bounded_prompt_cache_key": rais("ValueError")}),
    case("_bound_prompt_cache_key_field", container={"prompt_cache_key": "x"}, io={}),
    case("_bound_prompt_cache_key_field", container={"prompt_cache_key": "x", "other": 2},
         io={"_bounded_prompt_cache_key": None}),
)

# ── _merge_extra_headers ────────────────────────────────────────────────────
add(
    case("_merge_extra_headers", kwargs={"extra_headers": {"a": "1", "b": "2"}}, headers={"c": "3"}),
    case("_merge_extra_headers", kwargs={}, headers={"x": "y"}),
    case("_merge_extra_headers", kwargs={"extra_headers": {"a": "1", "": "empty", "b": None}}, headers={}),
    case("_merge_extra_headers", kwargs={"extra_headers": "notdict"}, headers={"x": "y"}),
    case("_merge_extra_headers", kwargs={"extra_headers": {"a": 5}}, headers={}),
    case("_merge_extra_headers", kwargs={"extra_headers": {"a": "1"}}, headers={"a": "override"}),
    case("_merge_extra_headers", kwargs={"extra_headers": None}, headers={"x": "y"}),
    case("_merge_extra_headers", kwargs={"extra_headers": {"a": "1", "b": 0}}, headers={"c": "3"}),
)

# ── _newest_reasoning_only ──────────────────────────────────────────────────
R1 = {"type": "reasoning", "id": "r1"}
C1 = {"type": "compaction", "id": "c1"}
add(
    case("_newest_reasoning_only", messages=[]),
    case("_newest_reasoning_only", messages=[{"role": "user", "content": "hi"}]),
    case("_newest_reasoning_only", messages=[{"role": "assistant", "codex_reasoning_items": [R1]}]),
    case("_newest_reasoning_only", messages=[
        {"role": "assistant", "codex_reasoning_items": [R1]},
        {"role": "user", "content": "x"},
        {"role": "assistant", "codex_reasoning_items": [{"type": "reasoning", "id": "r2"}]}]),
    case("_newest_reasoning_only", messages=[
        {"role": "assistant", "codex_reasoning_items": [R1, C1]},
        {"role": "assistant", "codex_reasoning_items": [{"type": "reasoning", "id": "r2"}]}]),
    case("_newest_reasoning_only", messages=[
        {"role": "assistant", "codex_reasoning_items": [C1]},
        {"role": "assistant", "codex_reasoning_items": [R1]}]),
    case("_newest_reasoning_only", messages=[5, "x", {"role": "assistant", "codex_reasoning_items": [R1]}, None]),
    case("_newest_reasoning_only", messages=[
        {"role": "assistant", "codex_reasoning_items": [R1]},
        {"role": "assistant", "codex_reasoning_items": [R1]},
        {"role": "assistant", "codex_reasoning_items": [R1]}]),
    case("_newest_reasoning_only", messages=[{"role": "assistant", "codex_reasoning_items": "notlist"}]),
)

# ── _sanitize_astra_request_kwargs ──────────────────────────────────────────
add(
    case("_sanitize_astra_request_kwargs",
         kwargs={"reasoning": {"effort": "HIGH"}, "temperature": 0.5,
                 "include": ["reasoning.encrypted_content", "reasoning.logprobs"]},
         model="gpt-6.2-astra", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com", "clamp_effort": "high"}),
    case("_sanitize_astra_request_kwargs", kwargs={"reasoning": {"effort": "high"}, "top_p": 0.9},
         model="gpt-4o", base_url="https://proxy.com",
         io={"is_astra_model": False, "base_url_hostname": "proxy.com"}),
    case("_sanitize_astra_request_kwargs", kwargs={"reasoning": {"effort": ""}},
         model="gpt-6.2-astra", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com"}),
    case("_sanitize_astra_request_kwargs", kwargs={"reasoning": {"effort": None}, "logprobs": True},
         model="gpt-6.2-astra", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com"}),
    case("_sanitize_astra_request_kwargs", kwargs={"temperature": 0.5, "include": ["a", "logprob_x"]},
         model="gpt-6.2-astra", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com"}),
    case("_sanitize_astra_request_kwargs", kwargs={"include": "notlist"},
         model="gpt-6.2-astra", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com"}),
    case("_sanitize_astra_request_kwargs", kwargs={"reasoning": {"effort": "max"}},
         model="gpt-6.2-astra", base_url="https://api.openai.com",
         io={"base_url_hostname": "api.openai.com", "clamp_effort": rais("ValueError")}),
    case("_sanitize_astra_request_kwargs", kwargs={"reasoning": {"effort": "high"}},
         model="gpt-6.2-astra", base_url="https://x", io={}),
)

# ── _resolve_reasoning ──────────────────────────────────────────────────────
add(
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"effort": "high"}},
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": ["low", "medium", "high"],
             "clamp_effort": "high"}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"enabled": False}},
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": LEGACY}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"enabled": False}}, warned=[],
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": ["low", "medium", "high"],
             "logger.warning": None}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"enabled": False}}, warned=["gpt-5"],
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": ["low", "medium", "high"]}),
    case("_resolve_reasoning", model="grok-4.6", params={"is_xai_responses": True},
         io={"is_grok_46_family": True, "clamp_effort": "medium"}),
    case("_resolve_reasoning", model="grok-4", params={"is_xai_responses": True},
         io={"is_grok_46_family": False, "clamp_effort": "medium"}),
    case("_resolve_reasoning", model="grok-4.6", params={"is_xai_responses": True, "reasoning_config": {"enabled": False}},
         warned=[], io={"is_grok_46_family": True, "logger.warning": None}),
    case("_resolve_reasoning", model="x", params={},
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": None,
             "is_astra_model": False, "codex_supported_efforts": []}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"effort": ""}},
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": ["low", "medium", "high"],
             "clamp_effort": "medium"}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"enabled": 0, "effort": "high"}},
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": ["low", "medium", "high"],
             "clamp_effort": "high"}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"effort": "high"}},
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": ["low", "medium", "high"],
             "clamp_effort": rais("ValueError")}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"enabled": False}}, warned=[],
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": ["low", "medium", "high"]}),
    case("_resolve_reasoning", model="gpt-5", params={"reasoning_config": {"effort": "high"}},
         io={"base_url_hostname": "proxy.com", "_profile_declared_efforts": rais("ValueError")}),
    case("_resolve_reasoning", model="gpt-5", params={"is_codex_backend": True, "reasoning_config": {"effort": "high"}},
         io={"is_astra_model": False, "codex_supported_efforts": LEGACY, "clamp_effort": "high"}),
)

# ── _alias_wire_tools ───────────────────────────────────────────────────────
WS = {"type": "function", "name": "web_search"}
SF = {"type": "function", "name": "search_files"}
FOO = {"type": "function", "name": "foo"}
add(
    case("_alias_wire_tools", response_tools=[WS], params={"base_url": "https://x"}, is_xai_responses=True,
         io={"_xai_prefers_native_web_search": True, "_is_opencode_responses_backend": False,
             "base_url_hostname": "other.com", "_alias_reserved_tools": [[{"type": "web_search"}], {}]}),
    case("_alias_wire_tools", response_tools=[WS], params={"base_url": "https://x"}, is_xai_responses=True,
         io={"_xai_prefers_native_web_search": False, "_is_opencode_responses_backend": False,
             "base_url_hostname": "other.com",
             "_alias_reserved_tools": [[{"type": "function", "name": "hermes_web_search"}], {}]}),
    case("_alias_wire_tools", response_tools=[SF], params={"base_url": "https://opencode.ai"}, is_xai_responses=False,
         io={"_is_opencode_responses_backend": True,
             "_alias_reserved_tools": [[{"type": "function", "name": "hermes_search_files"}],
                                       {"hermes_search_files": "search_files"}],
             "base_url_hostname": "opencode.ai"}),
    case("_alias_wire_tools", response_tools=[WS], params={"base_url": "https://x"}, is_xai_responses=False,
         is_codex_backend=True,
         io={"get_active_search_provider": {"name": "openai-native"}, "_is_opencode_responses_backend": False,
             "base_url_hostname": "other.com", "_alias_reserved_tools": [[{"type": "web_search"}], {}]}),
    case("_alias_wire_tools", response_tools=[], params={}, is_xai_responses=True, io={}),
    case("_alias_wire_tools", response_tools=None, params={}, is_xai_responses=True, io={}),
    case("_alias_wire_tools", response_tools=[FOO], params={"base_url": "https://x"}, is_xai_responses=True,
         io={"_is_opencode_responses_backend": False, "base_url_hostname": "other.com"}),
    case("_alias_wire_tools", response_tools=[WS], params={"base_url": "https://x"}, is_xai_responses=True,
         io={"_xai_prefers_native_web_search": rais("ValueError")}),
    case("_alias_wire_tools", response_tools=[WS], params={"base_url": "https://x"}, is_xai_responses=True, io={}),
    case("_alias_wire_tools", response_tools=[WS, FOO], params={"base_url": "https://api.perplexity.ai"},
         is_xai_responses=False,
         io={"_is_opencode_responses_backend": False, "base_url_hostname": "api.perplexity.ai",
             "_alias_reserved_tools": [[{"type": "function", "name": "hermes_web_search"}, FOO],
                                       {"hermes_web_search": "web_search"}]}),
)

# ── reject_all_turns ────────────────────────────────────────────────────────
SELF = {"_last_sent_all_turns": True, "_all_turns_rejected": [], "_last_issuer_model": "gpt-5"}
add(
    case("reject_all_turns", self=dict(SELF),
         error={"__raise__": "BadRequestError", "msg": "reasoning.context rejected", "status_code": 400},
         io={"logger.warning": None}),
    case("reject_all_turns", self=dict(SELF),
         error={"__raise__": "BadRequestError", "msg": "all_turns not supported", "status_code": None},
         io={"logger.warning": None}),
    case("reject_all_turns", self=dict(SELF),
         error={"__raise__": "BadRequestError", "msg": "reasoning.context bad", "status_code": 401}, io={}),
    case("reject_all_turns", self=dict(SELF),
         error={"__raise__": "BadRequestError", "msg": "some other error", "status_code": 400}, io={}),
    case("reject_all_turns", self={**SELF, "_last_sent_all_turns": False},
         error={"__raise__": "BadRequestError", "msg": "reasoning.context", "status_code": 400}, io={}),
    case("reject_all_turns", self=dict(SELF), error="reasoning.context bad", io={"logger.warning": None}),
    case("reject_all_turns", self=dict(SELF), error=None, io={}),
    case("reject_all_turns", self=dict(SELF),
         error={"__raise__": "BadRequestError", "msg": "ALL_TURNS", "status_code": 400}, io={}),
    case("reject_all_turns", self={**SELF, "_all_turns_rejected": ["gpt-5"]},
         error={"__raise__": "BadRequestError", "msg": "reasoning.context", "status_code": 400},
         io={"logger.warning": None}),
    case("reject_all_turns", self=dict(SELF),
         error={"__raise__": "BadRequestError", "msg": "reasoning.context", "status_code": 400},
         io={"logger.warning": rais("RuntimeError")}),
)

# ── drop_unverified_replay ──────────────────────────────────────────────────
DSELF = {"_last_issuer_kind": "openai", "_last_issuer_model": "gpt-5"}
M1 = [{"role": "assistant", "codex_reasoning_items": [{"type": "reasoning"}]}]
M2 = [{"role": "user", "content": "x"}]
add(
    case("drop_unverified_replay", self=dict(DSELF), message_lists=[M1, M2],
         io={"strip_unverified_reasoning_items": seq(2, 3)}),
    case("drop_unverified_replay", self={**DSELF, "_last_issuer_kind": None}, message_lists=[M1], io={}),
    case("drop_unverified_replay", self={**DSELF, "_last_issuer_model": None}, message_lists=[M1], io={}),
    case("drop_unverified_replay", self=dict(DSELF), message_lists=[], io={}),
    case("drop_unverified_replay", self=dict(DSELF), message_lists=[M1],
         io={"strip_unverified_reasoning_items": 0}),
    case("drop_unverified_replay", self=dict(DSELF), message_lists=[M1],
         io={"strip_unverified_reasoning_items": rais("ValueError")}),
    case("drop_unverified_replay", self=dict(DSELF), message_lists=[M1], io={}),
    case("drop_unverified_replay", self=dict(DSELF), message_lists=[None],
         io={"strip_unverified_reasoning_items": 5}),
    case("drop_unverified_replay", self=dict(DSELF), message_lists=[M1, M2],
         io={"strip_unverified_reasoning_items": seq(1, 0)}),
)


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        raise SystemExit(f"generator: funcs below 5 cases: {thin}")
    expected = {
        "_bound_prompt_cache_key_field", "_merge_extra_headers", "_is_perplexity_responses_backend",
        "_reserves_tool_search", "_openai_prefers_native_web_search", "_alias_wire_tools",
        "_resolve_reasoning", "_is_openai_api_origin", "_is_official_openai_responses_route",
        "_codex_efforts_for_route", "_sanitize_astra_request_kwargs", "_is_azure_responses",
        "_newest_reasoning_only", "_coerce_timeout", "_sends_all_turns", "_reasoning_fields",
        "reject_all_turns", "drop_unverified_replay",
    }
    missing = expected - set(counts)
    if missing:
        raise SystemExit(f"generator: missing funcs: {sorted(missing)}")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"gen_codex_transport_flow_cases: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()