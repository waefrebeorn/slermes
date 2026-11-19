#!/usr/bin/env python3
"""Generate tests/fixtures/agent_init_flow.jsonl for the agent/agent_init.py
UNCLAIMED-remainder FLOW port. >=5 cases per ported function, exercising happy
paths, branch ladders, scripted raises, BaseException propagation, and the
fail-closed missing-script path. Each case carries the seam scripts (io) the C
harness and the live oracle both consume; set-valued scripts are SORTED arrays,
multi-call seams use {"$seq": [...]}. ASCII fixture domain (see the port header).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "agent_init_flow.jsonl"

CASES = []


def case(func, inp=None, io=None, methods=None, env=None):
    body = dict(inp or {})
    body["io"] = io or {}
    if methods:
        body["methods"] = methods
    if env:
        body["env"] = env
    CASES.append({"func": func, "in": body})


def raise_spec(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


# ── _parse_config_int ───────────────────────────────────────────────────────
for raw, default, in [
    (5, 0), (True, 7), (5.0, 0), (5.5, 9), (" 42 ", 0), ("abc", 3),
    (None, 8), ("1_0", 0), ("-3", 0), ("", 4),
]:
    case("_parse_config_int", {"raw": raw, "default": default})

# ── _cfg_flag ───────────────────────────────────────────────────────────────
for cfg, key, default in [
    ({"enabled": "yes"}, "enabled", False), ({"enabled": "TRUE"}, "enabled", False),
    ({"enabled": "1"}, "enabled", False), ({"enabled": "no"}, "enabled", True),
    ({}, "enabled", True), ({}, "enabled", False), ({"enabled": None}, "enabled", True),
    ({"enabled": True}, "enabled", False), ({"enabled": "off"}, "enabled", True),
]:
    case("_cfg_flag", {"cfg": cfg, "key": key, "default": default})

# ── _cfg_dict ───────────────────────────────────────────────────────────────
for cfg, key in [
    ({"compression": {"a": 1}}, "compression"), ({"compression": "x"}, "compression"),
    ({"compression": None}, "compression"), ({}, "compression"),
    ({"compression": [1]}, "compression"), ({"compression": {"b": 2}}, "compression"),
]:
    case("_cfg_dict", {"cfg": cfg, "key": key})

# ── _positive_int ───────────────────────────────────────────────────────────
for raw, reject in [
    (5, []), (0, []), (-3, []), ("7", []), ("abc", []), (None, []),
    (True, ["bool"]), (5, ["bool", "float"]), ("5", ["bool", "float"]), (True, []),
]:
    case("_positive_int", {"raw": raw, "reject": reject})

# ── _set_defaults ───────────────────────────────────────────────────────────
case("_set_defaults", {"agent": {}, "table": {"a": 1, "b": "x"}})
case("_set_defaults", {"agent": {}, "table": {"_last_activity_ts": {"__factory__": True}}},
     io={"factory._last_activity_ts": 1000})
case("_set_defaults", {"agent": {"z": 9}, "table": {"x": {"__factory__": True}, "y": 2}},
     io={"factory.x": {"k": 1}})
case("_set_defaults", {"agent": {}, "table": {"x": {"__factory__": True}}},
     io={"factory.x": raise_spec("ValueError", "bad")})
case("_set_defaults", {"agent": {}, "table": {"x": {"__factory__": True}}},
     io={"factory.x": raise_spec("SystemExit", "1")})
case("_set_defaults", {"agent": {}, "table": {"x": {"__factory__": True}}}, io={})

# ── _init_turn_state ────────────────────────────────────────────────────────
TURN_IO = {"factory._last_activity_ts": 1000, "normalize_run_budget_seconds": 30.0,
           "new_credits_latch": {"latched": False}}
case("_init_turn_state", {"agent": {}, "run_budget_seconds": 30}, io=dict(TURN_IO))
case("_init_turn_state", {"agent": {}, "run_budget_seconds": None},
     io={**TURN_IO, "normalize_run_budget_seconds": None})
case("_init_turn_state", {"agent": {}, "run_budget_seconds": "x"},
     io={**TURN_IO, "normalize_run_budget_seconds": raise_spec("ValueError", "bad")})
case("_init_turn_state", {"agent": {}, "run_budget_seconds": 1},
     io={**TURN_IO, "new_credits_latch": raise_spec("RuntimeError", "boom")})
case("_init_turn_state", {"agent": {}, "run_budget_seconds": 1},
     io={**TURN_IO, "normalize_run_budget_seconds": raise_spec("SystemExit", "2")})
case("_init_turn_state", {"agent": {}, "run_budget_seconds": 1},
     io={"factory._last_activity_ts": 1000, "new_credits_latch": {}})

# ── _init_usage_state ───────────────────────────────────────────────────────
USAGE_IO = {"SubdirectoryHintTracker": {"working_dir": "/w"},
            "factory._api_latency_history": [], "factory._api_output_history": []}
case("_init_usage_state", {"agent": {"skip_context_files": False, "session_cwd": "/w"}},
     io=dict(USAGE_IO))
case("_init_usage_state", {"agent": {"skip_context_files": False, "session_cwd": None}},
     io={**USAGE_IO, "scope_terminal_cwd": "/fallback"})
case("_init_usage_state", {"agent": {"skip_context_files": False, "session_cwd": None}},
     io={**USAGE_IO, "scope_terminal_cwd": None, "SubdirectoryHintTracker": {}})
case("_init_usage_state", {"agent": {"skip_context_files": True, "session_cwd": "/w"}},
     io=dict(USAGE_IO))
case("_init_usage_state", {"agent": {"skip_context_files": False, "session_cwd": "/w"}},
     io={**USAGE_IO, "SubdirectoryHintTracker": raise_spec("ValueError", "bad")})
case("_init_usage_state", {"agent": {"skip_context_files": False, "session_cwd": "/w"}},
     io={"SubdirectoryHintTracker": {}, "factory._api_latency_history": []})

# ── _setup_logging ──────────────────────────────────────────────────────────
case("_setup_logging", {"agent": {"verbose_logging": False}},
     io={"get_hermes_home": "/h/.hermes", "setup_logging": None})
case("_setup_logging", {"agent": {"verbose_logging": True}},
     io={"get_hermes_home": "/h", "setup_logging": None, "setup_verbose_logging": None,
         "ra.logger.info": None})
case("_setup_logging", {"agent": {"verbose_logging": False}},
     io={"get_hermes_home": raise_spec("ValueError", "bad"), "setup_logging": None})
case("_setup_logging", {"agent": {"verbose_logging": False}},
     io={"get_hermes_home": "/h", "setup_logging": raise_spec("RuntimeError", "x")})
case("_setup_logging", {"agent": {"verbose_logging": True}},
     io={"get_hermes_home": "/h", "setup_logging": None, "setup_verbose_logging": None,
         "ra.logger.info": raise_spec("KeyboardInterrupt", "")})
case("_setup_logging", {"agent": {"verbose_logging": False}},
     io={"get_hermes_home": "/h"})

# ── _init_prompt_cache_config ───────────────────────────────────────────────
PC_M = ["_anthropic_prompt_cache_policy"]
case("_init_prompt_cache_config", {"agent": {}}, methods=PC_M,
     io={"agent._anthropic_prompt_cache_policy": [True, True],
         "load_config_readonly": {"prompt_caching": {"cache_ttl": "1h"}}})
case("_init_prompt_cache_config", {"agent": {}}, methods=PC_M,
     io={"agent._anthropic_prompt_cache_policy": [True, False],
         "load_config_readonly": {}, "auto_start_cache_ttl": "5m"})
case("_init_prompt_cache_config", {"agent": {"side_agent": True}}, methods=PC_M,
     io={"agent._anthropic_prompt_cache_policy": [True, True], "load_config_readonly": {}})
case("_init_prompt_cache_config", {"agent": {}}, methods=PC_M,
     io={"agent._anthropic_prompt_cache_policy": [True, True],
         "load_config_readonly": {"prompt_caching": {"cache_ttl": "off"}},
         "cache_ttl_means_disabled": True})
case("_init_prompt_cache_config", {"agent": {}}, methods=PC_M,
     io={"agent._anthropic_prompt_cache_policy": [True, True],
         "load_config_readonly": raise_spec("ValueError", "bad"),
         "logger.warning": None, "auto_start_cache_ttl": "5m"})
case("_init_prompt_cache_config", {"agent": {}}, methods=PC_M,
     io={"agent._anthropic_prompt_cache_policy": [True, True],
         "load_config_readonly": None, "logger.warning": None, "auto_start_cache_ttl": "1h"})
case("_init_prompt_cache_config", {"agent": {}}, methods=PC_M,
     io={"agent._anthropic_prompt_cache_policy": raise_spec("SystemExit", "1")})
case("_init_prompt_cache_config", {"agent": {}}, methods=PC_M, io={})

# ── _resolve_api_mode ───────────────────────────────────────────────────────
def ram(agent, api_mode=None, provider_name=None, base_url=None, io=None):
    case("_resolve_api_mode",
         {"agent": agent, "api_mode": api_mode, "provider_name": provider_name,
          "base_url": base_url}, io=io)

A = {"_base_url_hostname": "api.openai.com", "_base_url_lower": "https://api.openai.com/v1",
     "provider": "openai", "model": "gpt-4"}
ram(A, base_url="https://api.openai.com/v1", io={"is_actual_route": True})
ram(A, api_mode="anthropic_messages", io={"is_actual_route": False})
ram(A, api_mode="custom_dialect", io={"is_actual_route": False,
                                      "registered_api_modes": ["custom_dialect"]})
ram({**A, "provider": "xai"}, io={"is_actual_route": False})
ram({**A, "_base_url_hostname": "chatgpt.com",
     "_base_url_lower": "https://chatgpt.com/backend-api/codex", "provider": ""},
    provider_name=None, base_url="https://chatgpt.com/backend-api/codex",
    io={"is_actual_route": False})
ram({**A, "provider": "anthropic"}, io={"is_actual_route": False})
ram({**A, "provider": "custom", "_base_url_hostname": "x.com",
     "_base_url_lower": "https://x.com/anthropic"}, io={"is_actual_route": False})
ram({**A, "provider": "bedrock"}, io={"is_actual_route": False})
ram({**A, "provider": "custom", "_base_url_hostname": "bedrock-runtime.us-east-1.amazonaws.com",
     "_base_url_lower": "https://bedrock-runtime.us-east-1.amazonaws.com"},
    base_url="https://bedrock-runtime.us-east-1.amazonaws.com",
    io={"is_actual_route": False, "base_url_host_matches": True})
ram({**A, "provider": "nous"}, io={"is_actual_route": False, "nous_api_mode": "anthropic_messages"})
ram(A, base_url="https://api.meta.ai", io={"is_actual_route": False,
                                           "host_mandated_api_mode": "codex_responses"})
ram(A, io={"is_actual_route": False, "host_mandated_api_mode": None})
ram(A, io={"is_actual_route": False, "host_mandated_api_mode": raise_spec("ValueError", "x")})
ram(A, io={})
ram(A, io={"is_actual_route": raise_spec("SystemExit", "1")})

# ── _build_client ───────────────────────────────────────────────────────────
def bc(agent, api_key=None, base_url=None, fallback_model=None, io=None):
    case("_build_client",
         {"agent": agent, "api_key": api_key, "base_url": base_url,
          "fallback_model": fallback_model}, io=io)

BA = {"provider": "openai", "model": "gpt-4", "api_mode": "chat_completions"}
bc({**BA, "api_mode": "anthropic_messages"}, api_key="k", base_url="https://api.anthropic.com",
   io={"get_provider_request_timeout": 30, "init_anthropic_client": None})
bc({**BA, "provider": "moa"}, api_key="k", io={"get_provider_request_timeout": None,
                                               "init_moa_client": None})
bc({**BA, "api_mode": "bedrock_converse"}, base_url="https://bedrock",
   io={"get_provider_request_timeout": 30, "init_bedrock_client": None})
bc(BA, api_key="k", base_url="https://api.openai.com/v1",
   io={"get_provider_request_timeout": 30, "init_openai_client": None})
bc(BA, io={"get_provider_request_timeout": raise_spec("ValueError", "x")})
bc(BA, io={"get_provider_request_timeout": 30,
           "init_openai_client": raise_spec("RuntimeError", "fail")})
bc({**BA, "provider": "moa"}, io={"get_provider_request_timeout": None,
                                  "init_moa_client": raise_spec("KeyboardInterrupt", "")})
bc(BA, io={})

# ── _compression_threshold ──────────────────────────────────────────────────
CT_A = {"model": "gpt-5.5", "provider": "openai-codex"}
case("_compression_threshold", {"agent": CT_A, "cfg": {"threshold": 0.5}},
     io={"_compression_threshold_for_model": None, "_is_codex_gpt54_or_gpt55": False,
         "_is_codex_spark": False, "resolve_compression_threshold": [0.5, None]})
case("_compression_threshold", {"agent": CT_A, "cfg": {"threshold": 0.5}},
     io={"_compression_threshold_for_model": 0.85, "_is_codex_gpt54_or_gpt55": True,
         "resolve_compression_threshold": [0.85, {"model": "gpt-5.5", "from": 0.5, "to": 0.85}]})
case("_compression_threshold", {"agent": CT_A, "cfg": {"threshold": 0.5}},
     io={"_compression_threshold_for_model": 0.8, "_is_codex_gpt54_or_gpt55": False,
         "_is_codex_spark": True, "resolve_compression_threshold": [0.8, None]})
case("_compression_threshold", {"agent": CT_A, "cfg": {"threshold": 0.5}},
     io={"_compression_threshold_for_model": raise_spec("ValueError", "x")})
case("_compression_threshold", {"agent": CT_A, "cfg": {"threshold": 0.5}},
     io={"_compression_threshold_for_model": 0.85, "_is_codex_gpt54_or_gpt55": True,
         "resolve_compression_threshold": raise_spec("SystemExit", "1")})
case("_compression_threshold", {"agent": CT_A, "cfg": {"threshold": "0.75"}},
     io={"_compression_threshold_for_model": None, "_is_codex_gpt54_or_gpt55": False,
         "_is_codex_spark": False, "resolve_compression_threshold": [0.75, None]})
case("_compression_threshold", {"agent": CT_A, "cfg": {"threshold": "abc"}}, io={})
case("_compression_threshold", {"agent": CT_A, "cfg": {}}, io={})

# ── _compression_codex_settings ─────────────────────────────────────────────
case("_compression_codex_settings", {"cfg": {}}, io={"is_truthy_value": False})
case("_compression_codex_settings", {"cfg": {"codex_app_server_auto": "hermes"}},
     io={"is_truthy_value": False})
case("_compression_codex_settings", {"cfg": {"codex_app_server_auto": "bogus"}},
     io={"is_truthy_value": False, "ra.logger.warning": None})
case("_compression_codex_settings", {"cfg": {"codex_responses_native": "yes"}},
     io={"is_truthy_value": True})
case("_compression_codex_settings", {"cfg": {"codex_responses_compact_threshold": 5000}},
     io={"is_truthy_value": False})
case("_compression_codex_settings", {"cfg": {"codex_responses_compact_threshold": "abc"}},
     io={"is_truthy_value": False, "ra.logger.warning": None})
case("_compression_codex_settings", {"cfg": {"codex_responses_compact_threshold": True}},
     io={"is_truthy_value": False, "ra.logger.warning": None})
case("_compression_codex_settings", {"cfg": {}},
     io={"is_truthy_value": raise_spec("SystemExit", "1")})

# ── _parse_compression_config ───────────────────────────────────────────────
PC_A = {"model": "gpt-4", "provider": "openai", "api_mode": "chat_completions"}
PCC_BASE = {"_compression_threshold_for_model": None, "_is_codex_gpt54_or_gpt55": False,
            "_is_codex_spark": False, "resolve_compression_threshold": [0.5, None],
            "_refuse_checkpoint_required_on_codex_app_server": None}
case("_parse_compression_config", {"agent": PC_A, "agent_cfg": {}},
     io={**PCC_BASE, "is_truthy_value": {"$seq": [False, False, True, False, False]}})
case("_parse_compression_config",
     {"agent": PC_A, "agent_cfg": {"compression": {
         "enabled": "no", "threshold": 0.6, "target_ratio": 0.3, "protect_last_n": 10,
         "max_attempts": 5, "tail_mode": "LEGACY", "in_place": "off",
         "model_thresholds": {"gpt-4": 0.7, "bad": "x", "flag": True}}}},
     io={**PCC_BASE, "resolve_compression_threshold": [0.6, None],
         "is_truthy_value": {"$seq": [False, False, False, False, False]}})
case("_parse_compression_config",
     {"agent": PC_A, "agent_cfg": {"compression": {"target_ratio": "abc"}}},
     io=dict(PCC_BASE))
case("_parse_compression_config", {"agent": PC_A, "agent_cfg": {}},
     io={**PCC_BASE, "is_truthy_value": {"$seq": [False]},
         "_refuse_checkpoint_required_on_codex_app_server": raise_spec("ValueError", "codex")})
case("_parse_compression_config",
     {"agent": PC_A, "agent_cfg": {"compression": {"codex_app_server_auto": "bogus"}}},
     io={**PCC_BASE, "is_truthy_value": {"$seq": [False, False, True, False, False]},
         "ra.logger.warning": None})
case("_parse_compression_config", {"agent": PC_A, "agent_cfg": {}},
     io={**PCC_BASE, "resolve_compression_threshold": raise_spec("SystemExit", "1")})
case("_parse_compression_config", {"agent": PC_A, "agent_cfg": {}}, io={})

# ── _custom_provider_configured_base_url ────────────────────────────────────
IDS = ["custom:myprov", "myprov"]
case("_custom_provider_configured_base_url",
     {"configured_provider": "MyProv",
      "agent_cfg": {"providers": {"myprov": {"name": "myprov", "api": "https://x.com/v1"}}},
      "custom_providers": []},
     io={"normalize_custom_provider_name": "myprov",
         "custom_provider_runtime_ids": {"$seq": [IDS, IDS]},
         "is_provider_enabled": True, "normalize_route_base_url": "https://x.com/v1"})
case("_custom_provider_configured_base_url",
     {"configured_provider": "myprov",
      "agent_cfg": {"providers": {"myprov": {"name": "myprov", "api": "https://x.com/v1"}}},
      "custom_providers": [{"provider_key": "myprov", "base_url": "https://twin.com"}]},
     io={"normalize_custom_provider_name": "myprov",
         "custom_provider_runtime_ids": {"$seq": [IDS, IDS, IDS]},
         "is_provider_enabled": False, "normalize_route_base_url": ""})
case("_custom_provider_configured_base_url",
     {"configured_provider": "myprov", "agent_cfg": {},
      "custom_providers": [{"provider_key": "myprov", "base_url": "https://y.com"}]},
     io={"normalize_custom_provider_name": "myprov",
         "custom_provider_runtime_ids": {"$seq": [IDS, IDS]},
         "normalize_route_base_url": "https://y.com"})
case("_custom_provider_configured_base_url",
     {"configured_provider": "other", "agent_cfg": {},
      "custom_providers": [{"provider_key": "myprov", "base_url": "https://y.com"}]},
     io={"normalize_custom_provider_name": "other",
         "custom_provider_runtime_ids": {"$seq": [IDS, IDS]},
         "normalize_route_base_url": "https://y.com"})
case("_custom_provider_configured_base_url",
     {"configured_provider": "myprov", "agent_cfg": {},
      "custom_providers": ["notdict", {"provider_key": "myprov", "base_url": "https://y.com"}]},
     io={"normalize_custom_provider_name": "myprov",
         "custom_provider_runtime_ids": {"$seq": [IDS, IDS]},
         "normalize_route_base_url": "https://y.com"})
case("_custom_provider_configured_base_url",
     {"configured_provider": "myprov", "agent_cfg": {}, "custom_providers": []},
     io={"normalize_custom_provider_name": raise_spec("SystemExit", "1")})
case("_custom_provider_configured_base_url",
     {"configured_provider": "myprov", "agent_cfg": {}, "custom_providers": []}, io={})

# ── _configured_default_base_url ────────────────────────────────────────────
case("_configured_default_base_url",
     {"agent_cfg": {}, "model_cfg": {"base_url": "https://x.com/v1"}, "custom_providers": []},
     io={"normalize_route_base_url": "https://x.com/v1", "normalize_custom_provider_name": ""})
case("_configured_default_base_url",
     {"agent_cfg": {}, "model_cfg": {"provider": "auto"}, "custom_providers": []},
     io={"normalize_route_base_url": "", "normalize_custom_provider_name": "auto"})
case("_configured_default_base_url",
     {"agent_cfg": {}, "model_cfg": {"provider": "myprov"},
      "custom_providers": [{"provider_key": "myprov", "base_url": "https://y.com"}]},
     io={"normalize_route_base_url": {"$seq": ["", "https://y.com"]},
         "normalize_custom_provider_name": "myprov", "resolve_provider": "other",
         "custom_provider_runtime_ids": {"$seq": [IDS, IDS]}})
case("_configured_default_base_url",
     {"agent_cfg": {}, "model_cfg": {"provider": "myprov"}, "custom_providers": []},
     io={"normalize_route_base_url": "", "normalize_custom_provider_name": "myprov",
         "resolve_provider": "myprov"})
case("_configured_default_base_url",
     {"agent_cfg": {}, "model_cfg": {"provider": "myprov"},
      "custom_providers": [{"provider_key": "myprov", "base_url": "https://y.com"}]},
     io={"normalize_route_base_url": {"$seq": ["", "https://y.com"]},
         "normalize_custom_provider_name": "myprov",
         "resolve_provider": raise_spec("ValueError", "x"),
         "custom_provider_runtime_ids": {"$seq": [IDS, IDS]}})
case("_configured_default_base_url",
     {"agent_cfg": {}, "model_cfg": {"base_url": "https://x.com"}, "custom_providers": []},
     io={"normalize_route_base_url": raise_spec("SystemExit", "1")})
case("_configured_default_base_url",
     {"agent_cfg": {}, "model_cfg": {}, "custom_providers": []}, io={})

# ── _active_route_url ───────────────────────────────────────────────────────
AR_A = {"base_url": "https://x.com/v1"}
case("_active_route_url", {"agent": AR_A, "base_url": "https://x.com/v1"},
     io={"normalize_route_base_url": "https://x.com/v1"})
case("_active_route_url", {"agent": AR_A, "base_url": "https://x.com/v1?api-version=1"},
     io={"normalize_route_base_url": {"$seq": ["https://x.com/v1", "https://x.com/v1",
                                               "https://x.com/v1?api-version=1"]}})
case("_active_route_url", {"agent": {"base_url": "https://b.com"},
                           "base_url": "https://a.com?q=1"},
     io={"normalize_route_base_url": {"$seq": ["https://a.com", "https://b.com", "https://b.com"]}})
case("_active_route_url", {"agent": AR_A, "base_url": "https://x.com/v1?q=1#frag"},
     io={"normalize_route_base_url": {"$seq": ["https://x.com/v1#frag", "https://x.com/v1",
                                               "https://x.com/v1?q=1#frag"]}})
case("_active_route_url", {"agent": AR_A, "base_url": "https://x.com/v1"}, io={})
case("_active_route_url", {"agent": AR_A, "base_url": "https://x.com/v1"},
     io={"normalize_route_base_url": raise_spec("KeyboardInterrupt", "")})

# ── _scope_context_length_to_default_runtime ────────────────────────────────
SCOPE_A = {"model": "m1", "provider": "openai", "base_url": "https://x.com/v1"}
SCOPE_IO = {"normalize_route_base_url": "https://x.com/v1",
            "normalize_custom_provider_name": "",
            "context_route_mismatch": False,
            "normalize_model_for_provider": {"$seq": ["m1", "m1"]}}
case("_scope_context_length_to_default_runtime",
     {"agent": SCOPE_A, "agent_cfg": {}, "model_cfg": {"default": "m1", "provider": "openai"},
      "custom_providers": [], "config_context_length": 256000, "base_url": "https://x.com/v1"},
     io=dict(SCOPE_IO))
case("_scope_context_length_to_default_runtime",
     {"agent": {**SCOPE_A, "model": "m2"}, "agent_cfg": {},
      "model_cfg": {"default": "m1", "provider": "openai"}, "custom_providers": [],
      "config_context_length": 256000, "base_url": "https://x.com/v1"},
     io={**SCOPE_IO, "normalize_model_for_provider": {"$seq": ["m1", "m2"]},
         "ra.logger.debug": None})
case("_scope_context_length_to_default_runtime",
     {"agent": SCOPE_A, "agent_cfg": {}, "model_cfg": {"default": "m1", "provider": "openai"},
      "custom_providers": [], "config_context_length": 256000, "base_url": "https://x.com/v1"},
     io={**SCOPE_IO, "context_route_mismatch": True, "ra.logger.debug": None})
case("_scope_context_length_to_default_runtime",
     {"agent": SCOPE_A, "agent_cfg": {},
      "model_cfg": {"default": {"model": "m1"}, "provider": "openai"},
      "custom_providers": [], "config_context_length": 128000, "base_url": "https://x.com/v1"},
     io={**SCOPE_IO, "split_model_config_default": ["m1", {}]})
case("_scope_context_length_to_default_runtime",
     {"agent": SCOPE_A, "agent_cfg": {}, "model_cfg": {"default": "m1", "provider": "openai"},
      "custom_providers": [], "config_context_length": 256000, "base_url": "https://x.com/v1"},
     io={**SCOPE_IO, "normalize_model_for_provider": raise_spec("ValueError", "x")})
case("_scope_context_length_to_default_runtime",
     {"agent": SCOPE_A, "agent_cfg": {}, "model_cfg": {"default": "m1", "provider": "openai"},
      "custom_providers": [], "config_context_length": 256000, "base_url": "https://x.com/v1"},
     io={**SCOPE_IO, "context_route_mismatch": raise_spec("SystemExit", "1")})
case("_scope_context_length_to_default_runtime",
     {"agent": SCOPE_A, "agent_cfg": {}, "model_cfg": {"default": "m1"},
      "custom_providers": [], "config_context_length": 256000, "base_url": "https://x.com/v1"},
     io={})

# ── set_config_context_length ───────────────────────────────────────────────
case("set_config_context_length",
     {"agent": {"context_compressor": {"context_length": 100}}, "value": 256000})
case("set_config_context_length", {"agent": {"context_compressor": None}, "value": 128000})
case("set_config_context_length", {"agent": {}, "value": 64000})
case("set_config_context_length", {"agent": {"context_compressor": {}}, "value": None})
case("set_config_context_length",
     {"agent": {"context_compressor": {"x": 1}}, "value": 256000})

# ── config_context_length_for_runtime ───────────────────────────────────────
CCFG = {"model": {"context_length": 256000, "default": "m1", "provider": "openai"}}
CC_IO = {"get_compatible_custom_providers": [],
         "normalize_route_base_url": "https://x.com/v1", "normalize_custom_provider_name": "",
         "context_route_mismatch": False, "normalize_model_for_provider": {"$seq": ["m1", "m1"]}}
case("config_context_length_for_runtime", {"agent": SCOPE_A, "config": CCFG}, io=dict(CC_IO))
case("config_context_length_for_runtime", {"agent": SCOPE_A, "config": None},
     io={**CC_IO, "load_config": CCFG})
case("config_context_length_for_runtime",
     {"agent": SCOPE_A, "config": {"model": {"context_length": True}}}, io={})
case("config_context_length_for_runtime",
     {"agent": SCOPE_A, "config": {"model": {"context_length": "abc"}}},
     io={"logger.debug": None})
case("config_context_length_for_runtime",
     {"agent": SCOPE_A, "config": {"model": {"context_length": 0}}}, io={})
case("config_context_length_for_runtime", {"agent": SCOPE_A, "config": {"model": "x"}}, io={})
case("config_context_length_for_runtime", {"agent": SCOPE_A, "config": None},
     io={"load_config": raise_spec("SystemExit", "1")})

# ── _select_context_engine ──────────────────────────────────────────────────
case("_select_context_engine", {"agent_cfg": {}}, io={"safe_worker_enabled": True})
case("_select_context_engine", {"agent_cfg": {}}, io={"safe_worker_enabled": False})
case("_select_context_engine", {"agent_cfg": {"context": {"engine": "lcm"}}},
     io={"safe_worker_enabled": False, "load_context_engine": {"name": "lcm"}})
case("_select_context_engine", {"agent_cfg": {"context": {"engine": "lcm"}}},
     io={"safe_worker_enabled": False, "load_context_engine": None,
         "get_plugin_context_engine": {"name": "lcm"},
         "engine.clone_for_agent": {"name": "lcm-copy"}})
case("_select_context_engine", {"agent_cfg": {"context": {"engine": "lcm"}}},
     io={"safe_worker_enabled": False, "load_context_engine": raise_spec("ValueError", "x"),
         "ra.logger.debug": None, "get_plugin_context_engine": {"name": "other"},
         "ra.logger.warning": None})
case("_select_context_engine", {"agent_cfg": {"context": {"engine": "lcm"}}},
     io={"safe_worker_enabled": False, "load_context_engine": None,
         "get_plugin_context_engine": {"name": "lcm"},
         "engine.clone_for_agent": raise_spec("ValueError", "uncopyable"),
         "ra.logger.warning": None})
case("_select_context_engine", {"agent_cfg": {}},
     io={"safe_worker_enabled": raise_spec("SystemExit", "1")})

# ── _compressor_max_tokens ──────────────────────────────────────────────────
case("_compressor_max_tokens", {"agent": {"max_tokens": 4096, "provider": "openai",
                                          "base_url": "https://x.com"}}, io={})
case("_compressor_max_tokens", {"agent": {"max_tokens": None, "provider": "gemini",
                                          "base_url": "https://x.com"}}, io={})
case("_compressor_max_tokens", {"agent": {"max_tokens": None, "provider": "openai",
                                          "base_url": "https://generativelanguage.googleapis.com"}},
     io={"is_native_gemini_base_url": True})
case("_compressor_max_tokens", {"agent": {"max_tokens": None, "provider": "openai",
                                          "base_url": "https://x.com"}},
     io={"is_native_gemini_base_url": False})
case("_compressor_max_tokens", {"agent": {"max_tokens": None, "provider": "openai",
                                          "base_url": "https://x.com"}},
     io={"is_native_gemini_base_url": raise_spec("ValueError", "x")})
case("_compressor_max_tokens", {"agent": {"max_tokens": None, "provider": "openai",
                                          "base_url": "https://x.com"}},
     io={"is_native_gemini_base_url": raise_spec("SystemExit", "1")})

# ── _enforce_minimum_context ────────────────────────────────────────────────
def emc(ctx_len, provider="openai", ollama=None, ccl=None, io=None):
    case("_enforce_minimum_context",
         {"agent": {"context_compressor": {"context_length": ctx_len},
                    "_ollama_num_ctx": ollama, "base_url": "https://x.com",
                    "provider": provider, "_config_context_length": ccl, "model": "m1"}},
         io=io)

emc(128000, io={})
emc(32000, io={"is_local_server": False})
emc(32000, io={"is_local_server": True})
emc(32000, provider="lmstudio", ccl=32000, io={})
emc(32000, ollama=64000, io={"is_local_server": True})
emc(0, io={})
emc(32000, io={"is_local_server": raise_spec("SystemExit", "1")})

# ── _warn_nonagentic_hermes_model ───────────────────────────────────────────
WN_A = {"quiet_mode": False, "platform": "telegram", "model": "hermes-4"}
case("_warn_nonagentic_hermes_model", {"agent": {**WN_A, "quiet_mode": True}}, io={})
case("_warn_nonagentic_hermes_model", {"agent": {**WN_A, "platform": "cli"}}, io={})
case("_warn_nonagentic_hermes_model", {"agent": WN_A},
     io={"_check_hermes_model_warning": ""})
case("_warn_nonagentic_hermes_model", {"agent": WN_A}, methods=["_emit_warning"],
     io={"_check_hermes_model_warning": "Hermes warn", "agent._emit_warning": None,
         "ra.logger.warning": None})
case("_warn_nonagentic_hermes_model", {"agent": WN_A},
     io={"_check_hermes_model_warning": "text"})
case("_warn_nonagentic_hermes_model", {"agent": WN_A},
     io={"_check_hermes_model_warning": raise_spec("ValueError", "x")})
case("_warn_nonagentic_hermes_model", {"agent": WN_A},
     io={"_check_hermes_model_warning": raise_spec("SystemExit", "1")})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print(f"  {fn}: {counts[fn]}{flag}")
    assert all(v >= 5 for v in counts.values()), "every function needs >=5 cases"


if __name__ == "__main__":
    main()