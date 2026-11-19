#!/usr/bin/env python3
"""Generate tests/fixtures/fallback_cooldown_flow.jsonl for the fc port.

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
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "fallback_cooldown_flow.jsonl"

CASES = []


def add(func, inp, io=None):
    inp = dict(inp)
    inp["io"] = io or {}
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def L(ret):
    return {"__script__": True, "ret": ret}


MARKER = "model is not supported when using codex with a chatgpt account"

# 1. _provider_reset_delay
F = "_provider_reset_delay"
add(F, {"reset_at": "x"}, {"parse_absolute_timestamp": None})
add(F, {"reset_at": 100}, {"parse_absolute_timestamp": 100, "time.time": 50})
add(F, {"reset_at": 100}, {"parse_absolute_timestamp": 100, "time.time": 150})
add(F, {"reset_at": 100}, {"parse_absolute_timestamp": 100, "time.time": 100})
add(F, {"reset_at": 1e308}, {"parse_absolute_timestamp": 1e308, "time.time": -1e308})
add(F, {"reset_at": "bad"}, {"parse_absolute_timestamp": R("ValueError", "bad ts")})
add(F, {"reset_at": 100}, {"parse_absolute_timestamp": 100, "time.time": R("SystemExit", "bye")})
add(F, {"reset_at": 100}, {})
add(F, {"reset_at": 90.5}, {"parse_absolute_timestamp": 90.5, "time.time": 60})
add(F, {"reset_at": None}, {"parse_absolute_timestamp": None})

# 2. switch_deferred_by_reset
F = "switch_deferred_by_reset"
CFG60 = {"fallback": {"min_switch_reset_seconds": 60}}
add(F, {"reason": None}, {})
add(F, {"reason": "auth"}, {})
add(F, {"reason": "rate_limit", "agent": {"_fallback_activated": True}}, {})
add(F, {"reason": "rate_limit"}, {"load_config": R("ValueError", "cfg")})
add(F, {"reason": "billing"}, {"load_config": R("SystemExit", "bye")})
add(F, {"reason": "upstream_rate_limit"}, {"load_config": R("KeyboardInterrupt", "ki")})
add(F, {"reason": "rate_limit"}, {"load_config": None})
add(F, {"reason": "rate_limit"}, {"load_config": {"fallback": None}})
add(F, {"reason": "rate_limit"}, {"load_config": {"fallback": {"min_switch_reset_seconds": "abc"}}})
add(F, {"reason": "rate_limit", "reset_at": 100},
    {"load_config": CFG60, "parse_absolute_timestamp": 100, "time.time": 70, "logging.info": None})
add(F, {"reason": "rate_limit", "reset_at": 120},
    {"load_config": CFG60, "parse_absolute_timestamp": 120, "time.time": 60})
add(F, {"reason": "rate_limit", "reset_at": "x"},
    {"load_config": CFG60, "parse_absolute_timestamp": None})
add(F, {"reason": "rate_limit", "reset_at": 51},
    {"load_config": {"fallback": {"min_switch_reset_seconds": "2.5"}},
     "parse_absolute_timestamp": 51, "time.time": 50, "logging.info": None})
add(F, {"reason": "rate_limit"}, {})
add(F, {"reason": "rate_limit"}, {"load_config": {"fallback": {}}})
add(F, {"reason": "rate_limit"}, {"load_config": L([])})
add(F, {"reason": "rate_limit"}, {"load_config": "x"})
add(F, {"reason": "rate_limit"}, {"load_config": {"fallback": {"min_switch_reset_seconds": {"a": 1}}}})
add(F, {"reason": "rate_limit"}, {"load_config": {"fallback": {"min_switch_reset_seconds": 0}}})
add(F, {"reason": "rate_limit"}, {"load_config": {"fallback": {"min_switch_reset_seconds": ""}}})
add(F, {"reason": "rate_limit", "reset_at": 100},
    {"load_config": CFG60, "parse_absolute_timestamp": 100, "time.time": 70,
     "logging.info": R("RuntimeError", "log")})
add(F, {"reason": "rate_limit"}, {"load_config": R("GeneratorExit", "ge")})
add(F, {"reason": "rate_limit"}, {"load_config": {"fallback": {"min_switch_reset_seconds": "5\x1c"}}})
add(F, {"reason": "rate_limit", "reset_at": 51},
    {"load_config": {"fallback": {"min_switch_reset_seconds": " 7 "}},
     "parse_absolute_timestamp": 51, "time.time": 50, "logging.info": None})
add(F, {"reason": "rate_limit", "agent": {"_fallback_activated": False}, "reset_at": 100},
    {"load_config": CFG60, "parse_absolute_timestamp": 100, "time.time": 70, "logging.info": None})

# 3. _on_primary_route
F = "_on_primary_route"
add(F, {"agent": {}}, {})
add(F, {"agent": {"_primary_runtime": {}}}, {})
add(F, {"agent": {"_primary_runtime": {"provider": "  "}}}, {})
add(F, {"agent": {"provider": "OpenAI", "model": "gpt", "base_url": "https://a.com/",
                  "_primary_runtime": {"provider": "openai", "model": "gpt", "base_url": "https://a.com"}}},
    {"normalize_route_base_url": ["https://a.com", "https://a.com"]})
add(F, {"agent": {"provider": "openai", "model": "gpt", "base_url": "u",
                  "_primary_runtime": {"provider": "openai", "model": "other", "base_url": "u"}}},
    {"normalize_route_base_url": ["u", "u"]})
add(F, {"agent": {"provider": "openai", "model": "gpt", "base_url": "u",
                  "_primary_runtime": {"provider": "openai", "model": "gpt", "base_url": "u"}}},
    {"normalize_route_base_url": ["https://a.com", "https://b.com"]})
add(F, {"agent": {"provider": "p", "model": "m", "base_url": "u",
                  "_primary_runtime": {"provider": "p", "model": "m", "base_url": "u"}}},
    {"normalize_route_base_url": R("ValueError", "bad url")})
add(F, {"agent": {"_primary_runtime": {"provider": None, "model": "m"}}}, {})
add(F, {"agent": {"model": "m", "_primary_runtime": {"provider": "openai", "model": "m"}}},
    {"normalize_route_base_url": ["", ""]})
add(F, {"agent": {"provider": "p", "model": "m", "base_url": "u",
                  "_primary_runtime": {"provider": "p", "model": "m", "base_url": "u"}}},
    {"normalize_route_base_url": ["u", R("SystemExit", "x")]})
add(F, {"agent": {"provider": "p", "_primary_runtime": {"provider": "p"}}}, {})

# 4. ident (nested closure)
F = "ident"
add(F, {"provider": "OpenAI", "model": " GPT ", "base_url": "https://a.com/"},
    {"normalize_route_base_url": "https://a.com"})
add(F, {"provider": None, "model": None, "base_url": None}, {"normalize_route_base_url": ""})
add(F, {"provider": 0, "model": False, "base_url": ""}, {"normalize_route_base_url": ""})
add(F, {"provider": "p", "model": "m", "base_url": "u"}, {"normalize_route_base_url": R("ValueError", "x")})
add(F, {"provider": "openai", "model": "gpt", "base_url": "u"}, {"normalize_route_base_url": None})
add(F, {"provider": "  CUSTOM  ", "model": "m", "base_url": "url"}, {"normalize_route_base_url": "URL"})
add(F, {"provider": "p", "model": "m", "base_url": "u"}, {})

# 5. _arm_rate_limit_cooldown
F = "_arm_rate_limit_cooldown"
add(F, {"reason": "auth"}, {})
add(F, {"reason": None, "agent": {"_rate_limit_backoff_count": 3, "_rate_limited_until": 5}}, {})
add(F, {"reason": "rate_limit"},
    {"parse_absolute_timestamp": None, "time.monotonic": 1000, "logging.info": None})
add(F, {"reason": "billing", "agent": {"_rate_limit_backoff_count": 3}},
    {"parse_absolute_timestamp": None, "time.monotonic": 10, "logging.info": None})
add(F, {"reason": "rate_limit", "agent": {"_rate_limit_backoff_count": 8}},
    {"parse_absolute_timestamp": None, "time.monotonic": 0, "logging.info": None})
add(F, {"reason": "upstream_rate_limit", "agent": {"_rate_limit_backoff_count": 20}},
    {"parse_absolute_timestamp": None, "time.monotonic": 0, "logging.info": None})
add(F, {"reason": "rate_limit", "reset_at": 150.7},
    {"parse_absolute_timestamp": 150.7, "time.time": 60, "time.monotonic": 5, "logging.info": None})
add(F, {"reason": "rate_limit",
        "agent": {"_fallback_activated": True, "provider": "other",
                  "_primary_runtime": {"provider": "openai"}}},
    {"normalize_route_base_url": ["", ""]})
add(F, {"reason": "rate_limit",
        "agent": {"_fallback_activated": True, "provider": "openai", "model": "g", "base_url": "u",
                  "_primary_runtime": {"provider": "openai", "model": "g", "base_url": "u"}}},
    {"normalize_route_base_url": ["u", "u"], "parse_absolute_timestamp": None,
     "time.monotonic": 100, "logging.info": None})
add(F, {"reason": "rate_limit", "reset_at": "x"}, {"parse_absolute_timestamp": R("ValueError", "x")})
add(F, {"reason": "rate_limit"}, {"parse_absolute_timestamp": None, "time.monotonic": R("SystemExit", "x")})
add(F, {"reason": "rate_limit"},
    {"parse_absolute_timestamp": None, "time.monotonic": 0, "logging.info": R("GeneratorExit", "x")})
add(F, {"reason": "rate_limit", "agent": {"_rate_limit_backoff_count": "2"}}, {})
add(F, {"reason": "rate_limit", "agent": {"_rate_limit_backoff_count": None}}, {})
add(F, {"reason": "rate_limit"}, {})
add(F, {"reason": "rate_limit", "agent": {"_rate_limit_backoff_count": -1}},
    {"parse_absolute_timestamp": None, "time.monotonic": 0, "logging.info": None})

# 6. _mark_entitlement_rejected_model
F = "_mark_entitlement_rejected_model"
add(F, {"api_error": {"status_code": 500, "message": MARKER}}, {})
add(F, {"api_error": {"message": MARKER}}, {})
add(F, {"api_error": {"status_code": "400", "message": MARKER}}, {})
add(F, {"api_error": {"status_code": 400, "message": "other"}}, {})
add(F, {"api_error": {"status_code": 400, "message": MARKER.upper()},
        "agent": {"provider": "OpenAI", "model": "gpt-5"}},
    {"logger.warning": None, "buffer_diagnostic_status": None})
add(F, {"api_error": {"status_code": 400, "message": "", "str": MARKER},
        "agent": {"provider": "p", "model": "m"}},
    {"logger.warning": None, "buffer_diagnostic_status": None})
add(F, {"api_error": {"status_code": 400, "message": None}, "agent": {"provider": "p", "model": "m"}}, {})
add(F, {"api_error": {"status_code": 400, "message": MARKER}, "agent": {"provider": "", "model": "m"}}, {})
add(F, {"api_error": {"status_code": 400, "message": MARKER}, "agent": {"provider": "p", "model": "  "}}, {})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_credential_pool": {}, "provider": "p", "model": "m"}},
    {"pool.has_available": True})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_credential_pool": {}, "provider": "p", "model": "m"}},
    {"pool.has_available": False, "logger.warning": None, "buffer_diagnostic_status": None})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_credential_pool": None, "provider": "p", "model": "m"}},
    {"logger.warning": None, "buffer_diagnostic_status": None})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_entitlement_rejected_models": [["p", "m"]], "provider": "p", "model": "m"}}, {})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_entitlement_rejected_models": None, "provider": "p", "model": "m"}},
    {"logger.warning": None, "buffer_diagnostic_status": None})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_credential_pool": {}, "provider": "p", "model": "m"}},
    {"pool.has_available": R("KeyboardInterrupt", "x")})
add(F, {"api_error": {"status_code": 400, "message": MARKER}, "agent": {"provider": "p", "model": "m"}},
    {"logger.warning": None, "buffer_diagnostic_status": R("RuntimeError", "x")})
add(F, {"api_error": {"status_code": 400, "message": MARKER}, "agent": {"provider": "p", "model": "m"}},
    {"logger.warning": R("SystemExit", "x")})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_entitlement_rejected_models": [["z", "m2"], ["a", "m1"]], "provider": "b", "model": "m3"}},
    {"logger.warning": None, "buffer_diagnostic_status": None})
add(F, {"api_error": {"status_code": 400, "message": MARKER}, "agent": {"provider": "p", "model": "m"}}, {})
add(F, {"api_error": {"status_code": 400.0, "message": MARKER}, "agent": {"provider": "p", "model": "m"}},
    {"logger.warning": None, "buffer_diagnostic_status": None})
add(F, {"api_error": None}, {})
add(F, {"api_error": {"status_code": 400, "message": MARKER},
        "agent": {"_entitlement_rejected_models": [], "provider": "p", "model": "m"}},
    {"logger.warning": None, "buffer_diagnostic_status": None})

# 7. _is_entitlement_rejected
F = "_is_entitlement_rejected"
add(F, {"provider": "p", "model": "m"}, {})
add(F, {"provider": "p", "model": "m", "agent": {"_entitlement_rejected_models": []}}, {})
add(F, {"provider": "p", "model": "m", "agent": {"_entitlement_rejected_models": None}}, {})
add(F, {"provider": "p", "model": "m", "agent": {"_entitlement_rejected_models": [["p", "m"]]}}, {})
add(F, {"provider": "p", "model": "claude", "agent": {"_entitlement_rejected_models": [["p", "anthropic/claude"]]}},
    {"normalize_model_for_provider": "anthropic/claude"})
add(F, {"provider": "p", "model": "claude", "agent": {"_entitlement_rejected_models": [["p", "anthropic/claude"]]}},
    {"normalize_model_for_provider": "other"})
add(F, {"provider": "p", "model": "m", "agent": {"_entitlement_rejected_models": [["p", "x"]]}},
    {"normalize_model_for_provider": R("ValueError", "x")})
add(F, {"provider": "p", "model": "m", "agent": {"_entitlement_rejected_models": [["p", "x"]]}},
    {"normalize_model_for_provider": None})
add(F, {"provider": "p", "model": "m", "agent": {"_entitlement_rejected_models": [["p", "x"]]}}, {})
add(F, {"provider": "q", "model": "n", "agent": {"_entitlement_rejected_models": [["p", "m"], ["q", "n"]]}}, {})

# 8. provider_owns_route
F = "provider_owns_route"
add(F, {"provider": "openai", "base_url": "x"}, {"base_url_hostname": ""})
add(F, {"provider": "openai", "base_url": "x"}, {"base_url_hostname": None})
add(F, {"provider": "Custom", "base_url": "https://h.com"}, {"base_url_hostname": "h.com", "normalize_provider": "custom"})
add(F, {"provider": "local", "base_url": "https://h.com"}, {"base_url_hostname": "h.com", "normalize_provider": "local"})
add(F, {"provider": "OpenAI", "base_url": "https://h.com/v1"},
    {"base_url_hostname": ["h.com", "h.com"], "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": "https://h.com/v1"}})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": ["h.com", "other.com"], "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": "https://other.com"}})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": ["h.com", "h.com"], "normalize_provider": "openai",
     "resolve_user_provider": None, "resolve_custom_provider": {"base_url": "https://h.com"}})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": "h.com", "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": ""}, "get_provider": None,
     "infer_provider_from_url": None})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": ["h.com", "h.com"], "normalize_provider": "openai",
     "resolve_user_provider": None, "resolve_custom_provider": None,
     "get_provider": {"base_url": "https://h.com"}})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": "h.com", "normalize_provider": "openai",
     "resolve_user_provider": None, "resolve_custom_provider": None, "get_provider": None})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": ["h.com", "other.com"], "normalize_provider": ["openai", "openai"],
     "resolve_user_provider": None, "resolve_custom_provider": None,
     "get_provider": {"base_url": "https://other.com"}, "infer_provider_from_url": "openai"})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": ["h.com", "other.com"], "normalize_provider": ["openai", "anthropic"],
     "resolve_user_provider": None, "resolve_custom_provider": None,
     "get_provider": {"base_url": "https://other.com"}, "infer_provider_from_url": "anthropic"})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": "h.com", "normalize_provider": "openai",
     "resolve_user_provider": None, "resolve_custom_provider": None,
     "get_provider": {"base_url": "https://other.com"}, "infer_provider_from_url": None})
add(F, {"provider": "openai", "base_url": "https://h.com",
        "config": {"custom_providers": [{"name": "x"}]}},
    {"base_url_hostname": "h.com", "normalize_provider": "openai",
     "resolve_user_provider": None, "resolve_custom_provider": None, "get_provider": None})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": "h.com", "normalize_provider": R("SystemExit", "x")})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": "h.com", "normalize_provider": "openai",
     "resolve_user_provider": None, "resolve_custom_provider": None,
     "get_provider": R("ValueError", "x")})
add(F, {"provider": "openai", "base_url": "https://h.com"}, {})
add(F, {"provider": None, "base_url": "https://h.com"},
    {"base_url_hostname": "h.com", "normalize_provider": "openai",
     "resolve_user_provider": None, "resolve_custom_provider": None, "get_provider": None})
add(F, {"provider": "openai", "base_url": None}, {"base_url_hostname": ""})
add(F, {"provider": "openai", "base_url": "https://h.com", "config": {"providers": {"p": {}}}},
    {"base_url_hostname": ["h.com", "h.com"], "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": "https://h.com"}})
add(F, {"provider": "openai", "base_url": "https://h.com"},
    {"base_url_hostname": ["h.com", None], "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": "https://h.com"}})

# 9. drop_stale_model_route
F = "drop_stale_model_route"
add(F, {"model_cfg": None, "provider": "openai"}, {})
add(F, {"model_cfg": "x", "provider": "openai"}, {})
add(F, {"model_cfg": {}, "provider": "openai"}, {})
add(F, {"model_cfg": {"base_url": "u", "api_mode": "chat"}, "provider": "custom"},
    {"base_url_hostname": "h", "normalize_provider": "custom"})
add(F, {"model_cfg": {"base_url": "u", "api_mode": "chat"}, "provider": "openai"},
    {"base_url_hostname": ""})
add(F, {"model_cfg": {"base_url": "u", "api_mode": "chat"}, "provider": "openai"},
    {"base_url_hostname": ["h", "other"], "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": "x"}})
add(F, {"model_cfg": {"base_url": "", "api_mode": "chat"}, "provider": "openai"}, {})
add(F, {"model_cfg": {"base_url": "  ", "api_mode": "chat"}, "provider": "openai"}, {})
add(F, {"model_cfg": {"base_url": "u", "api_mode": None}, "provider": "openai"},
    {"base_url_hostname": ["h", "other"], "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": "x"}})
add(F, {"model_cfg": {"base_url": "u", "api_mode": 0}, "provider": "openai"},
    {"base_url_hostname": ["h", "other"], "normalize_provider": "openai",
     "resolve_user_provider": {"base_url": "x"}})
add(F, {"model_cfg": {"base_url": "u"}, "provider": "openai"},
    {"base_url_hostname": R("ValueError", "x")})
add(F, {"model_cfg": {"base_url": None, "api_mode": "chat"}, "provider": "openai"}, {})
add(F, {"model_cfg": [1, 2], "provider": "openai"}, {})

# 10. drop_credentials_of_other_route
F = "drop_credentials_of_other_route"
add(F, {"model_cfg": {"provider": "openai", "base_url": "u", "api_key": "k"},
        "prior_model_cfg": {"provider": "OpenAI", "base_url": "u"}},
    {"normalize_route_base_url": ["u", "u"]})
add(F, {"model_cfg": {"provider": "openai", "api_key": "k", "api": "a", "key_env": "E",
                      "api_key_env": "E2", "api_mode": "chat", "base_url": "u"},
        "prior_model_cfg": {"provider": "anthropic"}},
    {"clear_model_endpoint_credentials": None})
add(F, {"model_cfg": {"provider": "openai", "base_url": "u1", "api_key": "k"},
        "prior_model_cfg": {"provider": "openai", "base_url": "u2"}},
    {"normalize_route_base_url": ["u1", "u2"], "clear_model_endpoint_credentials": None})
add(F, {"model_cfg": {"provider": "openai", "api_key": "k"}, "prior_model_cfg": None},
    {"clear_model_endpoint_credentials": None})
add(F, {"model_cfg": {"provider": "openai", "api_key": "k"}, "prior_model_cfg": "x"},
    {"clear_model_endpoint_credentials": None})
add(F, {"model_cfg": {"base_url": "u"}, "prior_model_cfg": {}},
    {"normalize_route_base_url": ["", ""]})
add(F, {"model_cfg": {"provider": "openai", "base_url": "u"},
        "prior_model_cfg": {"provider": "openai", "base_url": "u"}},
    {"normalize_route_base_url": R("ValueError", "x")})
add(F, {"model_cfg": {"provider": "openai", "api_key": "k"}, "prior_model_cfg": {"provider": "anthropic"}},
    {"clear_model_endpoint_credentials": R("SystemExit", "x")})
add(F, {"model_cfg": {"provider": "openai", "base_url": None},
        "prior_model_cfg": {"provider": "openai", "base_url": None}},
    {"normalize_route_base_url": ["", ""]})
add(F, {"model_cfg": {"provider": "openai", "api_key": "k"}, "prior_model_cfg": {"provider": "anthropic"}}, {})
add(F, {"model_cfg": {}, "prior_model_cfg": {}}, {"normalize_route_base_url": ["", ""]})

# 11. _error_obj
F = "_error_obj"
add(F, {"body": {"error": {"code": "x"}}})
add(F, {"body": {"error": "str"}})
add(F, {"body": {"error": None}})
add(F, {"body": {}})
add(F, {"body": None})
add(F, {"body": "x"})
add(F, {"body": {"error": [1]}})
add(F, {"body": {"error": {"a": {"b": 1}}}})

# 12. _json_dict
F = "_json_dict"
add(F, {"text": '{"a": 1}'})
add(F, {"text": ""})
add(F, {"text": "   "})
add(F, {"text": None})
add(F, {"text": 5})
add(F, {"text": "not json"})
add(F, {"text": "[1,2]"})
add(F, {"text": "123"})
add(F, {"text": '{"a": 1,}'})
add(F, {"text": "01"})
add(F, {"text": '{"a": "x\\q"}'})
add(F, {"text": '  {"a": {"b": [1, true, null]}}  '})
add(F, {"text": '{"a": 1.5e2}'})
add(F, {"text": '"str"'})
add(F, {"text": '{"a": "\\u0041"}'})
add(F, {"text": "true"})
add(F, {"text": '{"a": 1} extra'})
add(F, {"text": "{a: 1}"})

# 13. _usage_window_display_message
F = "_usage_window_display_message"
add(F, {"body": {"display_message": "  hello  "}})
add(F, {"body": {"error": {"display_message": "win"}}})
add(F, {"body": {"display_message": "   "}})
add(F, {"body": {"display_message": 5}})
add(F, {"body": None})
add(F, {"body": "x"})
add(F, {"body": {"display_message": "a" * 600}})
add(F, {"body": {"display_message": "", "error": {"display_message": "second"}}})
add(F, {"body": {"error": "str"}})
add(F, {"body": {"display_message": "top", "error": {"display_message": "inner"}}})

# 14. _has_usage_limit_transient_signal
F = "_has_usage_limit_transient_signal"
add(F, {"error_msg": "please retry later", "body": {}, "response_headers": None})
add(F, {"error_msg": "ok", "body": {"resets_in_seconds": 30}, "response_headers": None})
add(F, {"error_msg": "ok", "body": {"error": {"reset_at": 100}}, "response_headers": None})
add(F, {"error_msg": "ok", "body": {}, "response_headers": {"retry-after": "5"}})
add(F, {"error_msg": "ok", "body": {}, "response_headers": {"Retry-After": "5"}})
add(F, {"error_msg": "ok", "body": {}, "response_headers": {}})
add(F, {"error_msg": "ok", "body": {"resets_at": ""}, "response_headers": {}})
add(F, {"error_msg": "ok", "body": {"retry_after": 0}, "response_headers": None})
add(F, {"error_msg": "ok", "body": {}, "response_headers": "x"})
add(F, {"error_msg": "ok", "body": {}, "response_headers": None})
add(F, {"error_msg": "window", "body": {}, "response_headers": None})
add(F, {"error_msg": "ok", "body": {"resets_at": None}, "response_headers": None})
add(F, {"error_msg": "ok", "body": {}, "response_headers": {"x-ratelimit-reset": "1"}})
add(F, {"error_msg": "ok", "body": None, "response_headers": None})
add(F, {"error_msg": "", "body": {"error": {"resets_in_seconds": ""}}, "response_headers": None})

# 15. _rate_limit_reset_seconds
F = "_rate_limit_reset_seconds"
add(F, {"error_msg": "", "body": {"resets_at": 100}, "response_headers": None}, {"time.time": 60})
add(F, {"error_msg": "", "body": {"resets_at": 50}, "response_headers": None}, {"time.time": 60})
add(F, {"error_msg": "", "body": {"resets_in_seconds": "30"}, "response_headers": None},
    {"parse_retry_after_seconds": 30})
add(F, {"error_msg": "", "body": {"error": {"reset_at": 100}}, "response_headers": None}, {"time.time": 60})
add(F, {"error_msg": "", "body": {"retry_after": "5"}, "response_headers": {"retry-after": "5"}},
    {"parse_retry_after_seconds": [None, 7]})
add(F, {"error_msg": "", "body": {}, "response_headers": {"retry-after": "5"}},
    {"parse_retry_after_seconds": 7})
add(F, {"error_msg": "resets in 4hr", "body": {}, "response_headers": None},
    {"parse_retry_after_seconds": None, "reset_delay_from_message": 12})
add(F, {"error_msg": "", "body": {}, "response_headers": None},
    {"parse_retry_after_seconds": None, "reset_delay_from_message": None})
add(F, {"error_msg": "", "body": {"resets_in_seconds": "x"}, "response_headers": None},
    {"parse_retry_after_seconds": R("ValueError", "x")})
add(F, {"error_msg": "", "body": {"reset_at": 100}, "response_headers": None},
    {"time.time": R("SystemExit", "x")})
add(F, {"error_msg": "", "body": {"resets_in_seconds": ""}, "response_headers": None},
    {"parse_retry_after_seconds": None, "reset_delay_from_message": None})
add(F, {"error_msg": "", "body": {"resets_at": "100"}, "response_headers": None},
    {"parse_retry_after_seconds": 100})
add(F, {"error_msg": "", "body": {"reset_at": True}, "response_headers": None}, {"time.time": 0})
add(F, {"error_msg": "", "body": {"resets_in_seconds": "30"}, "response_headers": None}, {})
add(F, {"error_msg": "", "body": {"resets_in_seconds": 30, "reset_at": 100}, "response_headers": None},
    {"parse_retry_after_seconds": 30})
add(F, {"error_msg": "", "body": {}, "response_headers": {"retry-after": "5"}},
    {"parse_retry_after_seconds": R("GeneratorExit", "x")})
add(F, {"error_msg": "", "body": {"error": {"retry_after": "9"}}, "response_headers": None},
    {"parse_retry_after_seconds": 9})
add(F, {"error_msg": "x", "body": {}, "response_headers": None},
    {"parse_retry_after_seconds": None, "reset_delay_from_message": R("RuntimeError", "m")})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    expected = {
        "_provider_reset_delay", "switch_deferred_by_reset", "_on_primary_route",
        "ident", "_arm_rate_limit_cooldown", "_mark_entitlement_rejected_model",
        "_is_entitlement_rejected", "provider_owns_route", "drop_stale_model_route",
        "drop_credentials_of_other_route", "_error_obj", "_json_dict",
        "_usage_window_display_message", "_has_usage_limit_transient_signal",
        "_rate_limit_reset_seconds",
    }
    assert set(counts) == expected, (set(counts) ^ expected)
    for f, n in counts.items():
        assert n >= 5, (f, n)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True) + "\n")
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    for f in sorted(counts):
        print("  %-36s %d" % (f, counts[f]))


if __name__ == "__main__":
    main()
