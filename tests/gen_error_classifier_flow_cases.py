#!/usr/bin/env python3
"""Generate tests/fixtures/error_classifier_flow.jsonl for the ec port.

Each line: {"func": <live_name>, "in": {...inputs..., "io": {seam scripts}}}.
Script semantics (shared with the C harness dispatcher and the oracle Script):
  absent key            -> fail-closed RuntimeError "no script for <name>"
  non-list value        -> constant result reused every call
  list value            -> per-call queue (exhausted -> fail-closed)
  {"__raise__":K,"message":M} -> raised exception of kind K
  {"__script__":true,"ret":X} -> list-valued result X (queue-ambiguity wrap)
  null                  -> None result
>=5 cases per function; covers branches, seam order, raises, BaseException
passthrough and fail-closed paths. ASCII inputs; str()-positions integral;
exc doubles use kinds whose str(cls(msg))==msg; ctx fixtures carry every raw
field; status_code attrs int-or-null; response is a plain dict or a tagged
__response__ object double.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "error_classifier_flow.jsonl"
CASES = []


def add(func, inp, io=None):
    inp = dict(inp)
    inp["io"] = io or {}
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def L(ret):
    return {"__script__": True, "ret": ret}


def exc(kind="APIError", msg="boom", attrs=None, cause=None, context=None):
    e = {"kind": kind, "message": msg, "attrs": attrs or {}}
    if cause is not None:
        e["cause"] = cause
    if context is not None:
        e["context"] = context
    return e


def resp(status_code=None, headers=None, tag=True):
    r = {}
    if tag:
        r["__response__"] = True
    if status_code is not None:
        r["status_code"] = status_code
    if headers is not None:
        r["headers"] = headers
    return r


def ctx(**over):
    c = {"error": exc(), "status_code": None, "body": {}, "msg": "", "provider": "",
         "model": "", "approx_tokens": 0, "context_length": 200000, "num_messages": 0,
         "base_url": "", "anonymous": False}
    c.update(over)
    return c


EC = {"extract_error_code": ""}  # baseline post_init script


def cio(**over):
    d = dict(EC)
    d.update(over)
    return d


# 1. _v
F = "_v"
add(F, {"reason": "billing", "hints": {}})
add(F, {"reason": "rate_limit", "hints": {"should_rotate_credential": True, "should_fallback": True}})
add(F, {"reason": "context_overflow", "hints": {"should_compress": True}})
add(F, {"reason": "auth", "hints": {"retryable": False, "error_context": {"x": 1}}})
add(F, {"reason": "unknown", "hints": {"retryable": True, "should_compress": False}})

# 2. _first_match
F = "_first_match"
add(F, {"error_msg": "insufficient credits", "rules": [[["insufficient credits"], {"reason": "billing", "retryable": False}]]})
add(F, {"error_msg": "out of extra usage", "rules": [[["out of extra usage"], {"__fn__": "billing_hints"}]]})
add(F, {"error_msg": "xyz", "rules": [[["xyz"], {"__callable__": True}]]}, {"first_match.verdict": {"reason": "billing"}})
add(F, {"error_msg": "nomatch", "rules": [[["abc"], {"reason": "billing"}]]})
add(F, {"error_msg": "second hit", "rules": [[["first"], {"reason": "auth"}], [["second"], {"reason": "timeout"}]]})
add(F, {"error_msg": "xyz", "rules": [[["xyz"], {"__callable__": True}]]}, {"first_match.verdict": R("ValueError", "v")})
add(F, {"error_msg": "xyz", "rules": [[["xyz"], {"__callable__": True}]]})  # fail-closed

# 3. _billing_hints
F = "_billing_hints"
add(F, {"error_msg": "out of extra usage"})
add(F, {"error_msg": "insufficient credits"})
add(F, {"error_msg": ""})
add(F, {"error_msg": "OUT OF EXTRA USAGE"})  # case-sensitive: no match
add(F, {"error_msg": "prefix out of extra usage suffix"})

# 4. _is_anthropic_thinking_rejection
F = "_is_anthropic_thinking_rejection"
add(F, {"msg": "thinking signature invalid"})
add(F, {"msg": "thinking cannot be modified"})
add(F, {"msg": "invalid redacted_thinking block"})
add(F, {"msg": "thinking block"})  # thinking but no mutation word
add(F, {"msg": "signature invalid"})  # no thinking
add(F, {"msg": "redacted_thinking invalid"})

# 5. _gemini_prompt_block_marker
F = "_gemini_prompt_block_marker"
add(F, {"exc": exc("GeminiAPIError", "x", {"code": "gemini_prompt_blocked"})})
add(F, {"exc": exc("GeminiAPIError", "x", {"code": "other"})})
add(F, {"exc": exc("APIError", "x", {"code": "gemini_prompt_blocked"})})
add(F, {"exc": exc("GeminiAPIError", "x", {})})
add(F, {"exc": exc("GeminiAPIError", "x", {"code": None})})

# 6. _is_gemini_prompt_block
F = "_is_gemini_prompt_block"
add(F, {"error": exc("GeminiAPIError", "x", {"code": "gemini_prompt_blocked"})})
add(F, {"error": exc("APIError", "x", {}, cause=exc("GeminiAPIError", "y", {"code": "gemini_prompt_blocked"}))})
add(F, {"error": exc("APIError", "x", {}, context=exc("GeminiAPIError", "y", {"code": "gemini_prompt_blocked"}))})
add(F, {"error": exc("APIError", "x")})
add(F, {"error": exc("GeminiAPIError", "x", {"code": "no"})})

# 7. _from_cause_chain
F = "_from_cause_chain"
add(F, {"error": exc(), "default": False}, {"pick": True})
add(F, {"error": exc(), "default": "d"}, {"pick": None})
add(F, {"error": exc(), "default": None}, {"pick": [None, None, None, None, None]})
add(F, {"error": exc(cause=exc()), "default": False}, {"pick": [None, True]})
add(F, {"error": exc(context=exc()), "default": False}, {"pick": [None, "found"]})
add(F, {"error": exc(), "default": False}, {"pick": R("ValueError", "p")})
add(F, {"error": exc(), "default": False}, {"pick": R("SystemExit", "bye")})
add(F, {"error": exc(), "default": False})  # fail-closed
add(F, {"error": exc(), "default": False}, {"pick": False})  # False is non-None -> returned

# 8. _status_of
F = "_status_of"
add(F, {"exc": exc(attrs={"status_code": 429})})
add(F, {"exc": exc(attrs={"status_code": 999})})  # first branch no range check
add(F, {"exc": exc(attrs={"status": 404})})
add(F, {"exc": exc(attrs={"status": 99})})  # out of range
add(F, {"exc": exc(attrs={"response": {"ResponseMetadata": {"HTTPStatusCode": 503}}})})
add(F, {"exc": exc(attrs={"response": resp(status_code=502)})})
add(F, {"exc": exc(attrs={"response": resp(status_code=99)})})
add(F, {"exc": exc(attrs={})})
add(F, {"exc": exc(attrs={"status_code": None, "status": 400})})

# 9. _body_of
F = "_body_of"
add(F, {"exc": exc(attrs={"body": {"error": {"code": "x"}}})})
add(F, {"exc": exc(attrs={"response": resp()})}, {"response.json": {"error": {"code": "y"}}})
add(F, {"exc": exc(attrs={"response": resp()})}, {"response.json": R("ValueError", "j")})
add(F, {"exc": exc(attrs={"response": resp()})}, {"response.json": R("SystemExit", "bye")})
add(F, {"exc": exc(attrs={"response": {"ResponseMetadata": {}}})})  # dict response, no .json
add(F, {"exc": exc(attrs={})})
add(F, {"exc": exc(attrs={"response": resp()})}, {"response.json": None})
add(F, {"exc": exc(attrs={"response": resp()})})  # fail-closed
add(F, {"exc": exc(attrs={"response": resp()})}, {"response.json": "notdict"})

# 10. _headers_of
F = "_headers_of"
add(F, {"exc": exc(attrs={"response": resp(headers={"retry-after": "5"})})})
add(F, {"exc": exc(attrs={"response": resp(headers={})})})
add(F, {"exc": exc(attrs={"response": resp()})})
add(F, {"exc": exc(attrs={"response": {"headers": {"x": 1}}})})  # dict response: no .headers attr
add(F, {"exc": exc(attrs={})})
add(F, {"exc": exc(attrs={"response": None})})

# 11. _status_code_from_body
F = "_status_code_from_body"
add(F, {"body": {"code": 429}}, {"error_obj": {}})
add(F, {"body": {}}, {"error_obj": {"status_code": 404}})
add(F, {"body": {}}, {"error_obj": {"status": 503}})
add(F, {"body": {}}, {"error_obj": {"http_status": 400}})
add(F, {"body": {}}, {"error_obj": {"code": 599}})
add(F, {"body": {}}, {"error_obj": {"code": 399}})  # out of range
add(F, {"body": {}}, {"error_obj": {"code": 200}})
add(F, {"body": "notdict"}, {"error_obj": {}})
add(F, {"body": {}}, {"error_obj": R("ValueError", "e")})
add(F, {"body": {}})  # fail-closed

# 12. _code_from_payload
F = "_code_from_payload"
TK = ["code", "error_code", "errorCode"]
add(F, {"payload": {"error": {"code": "invalid_model"}}, "top_keys": TK, "peek_message": False})
add(F, {"payload": {"error": {"type": "invalid_request_error"}}, "top_keys": TK, "peek_message": False})
add(F, {"payload": {"error": {"code": 429, "status": "UNAVAILABLE"}}, "top_keys": TK, "peek_message": False})
add(F, {"payload": {"error": {"code": "400"}}, "top_keys": TK, "peek_message": False})
add(F, {"payload": {"error": {"message": '{"code": "invalid_encrypted_content"}'}}, "top_keys": TK, "peek_message": True},
    {"json_dict": {"code": "invalid_encrypted_content"}})
add(F, {"payload": {"code": "top_code"}, "top_keys": TK, "peek_message": False})
add(F, {"payload": {"error_code": 429}, "top_keys": TK, "peek_message": False})
add(F, {"payload": "notdict", "top_keys": TK, "peek_message": False})
add(F, {"payload": {"error": {"code": "  spaced  "}}, "top_keys": TK, "peek_message": False})
add(F, {"payload": {"error": {"message": '{"code": "x"}'}}, "top_keys": TK, "peek_message": True},
    {"json_dict": R("ValueError", "j")})

# 13. _body_message_candidates
F = "_body_message_candidates"
add(F, {"body": {"error": {"message": "a"}, "message": "b", "errorMessage": "c"}}, {"error_obj": {"message": "a"}})
add(F, {"body": {"errorArgs": {"reason": "r"}}}, {"error_obj": {}})
add(F, {"body": {"detail": "d"}}, {"error_obj": {}})
add(F, {"body": {"detail": {"message": "dm"}}}, {"error_obj": {}})
add(F, {"body": {}}, {"error_obj": {}})
add(F, {"body": {"error": {"message": "a"}}}, {"error_obj": R("ValueError", "e")})
add(F, {"body": {}})  # fail-closed

# 14. _openrouter_wrapped_message
F = "_openrouter_wrapped_message"
add(F, {"err_obj": {"metadata": {"raw": '{"error": {"message": "Up"}}'}}},
    {"json_dict": {"error": {"message": "Up"}}, "error_obj": {"message": "Up"}})
add(F, {"err_obj": {"metadata": {}}}, {"json_dict": None})
add(F, {"err_obj": {}}, {})
add(F, {"err_obj": {"metadata": "notdict"}}, {})
add(F, {"err_obj": {"metadata": {"raw": "x"}}}, {"json_dict": {"error": {"message": "M"}}})
add(F, {"err_obj": {"metadata": {"raw": "x"}}}, {"json_dict": R("ValueError", "j")})

# 15. _build_error_msg
F = "_build_error_msg"
add(F, {"error": exc(msg="Error code: 400"), "body": {}}, {"error_obj": {}})
add(F, {"error": exc(msg="raw"), "body": {"error": {"message": "Body"}}}, {"error_obj": {"message": "Body"}, "json_dict": None})
add(F, {"error": exc(msg="raw body"), "body": {"error": {"message": "body"}}}, {"error_obj": {"message": "body"}, "json_dict": None})
add(F, {"error": exc(msg="raw"), "body": {"message": "flat"}}, {"error_obj": {}})
add(F, {"error": exc(msg="raw"), "body": {"error": {"message": "b", "metadata": {"raw": "x"}}}},
    {"error_obj": [{"message": "b", "metadata": {"raw": "x"}}, {"message": "up"}],
     "json_dict": {"error": {"message": "up"}}})
add(F, {"error": exc(msg="raw"), "body": "notdict"}, {})
add(F, {"error": exc(msg="raw"), "body": {"error": {"message": "b"}}}, {"error_obj": R("SystemExit", "bye")})

# 16. _is_server_injected_param_rejection
F = "_is_server_injected_param_rejection"
add(F, {"error_msg": "unsupported parameter prompt_cache_retention", "provider": "openai"})
add(F, {"error_msg": "unsupported parameter prompt_cache_retention", "provider": "meta"})
add(F, {"error_msg": "unsupported parameter prompt_cache_retention", "provider": "bedrock-mantle"})
add(F, {"error_msg": "prompt_cache_retention", "provider": "openai"})  # no rejection word
add(F, {"error_msg": "unsupported parameter max_tokens", "provider": "openai"})  # wrong param
add(F, {"error_msg": "", "provider": "openai"})
add(F, {"error_msg": "unknown prompt_cache_retention", "provider": "  META  "})

# 17. _is_codex_masked_replay_rejection
F = "_is_codex_masked_replay_rejection"
add(F, {"ctx": ctx(provider="openai-codex", status_code=400, body={"detail": "Unsupported content type"})}, cio())
add(F, {"ctx": ctx(provider="openai-codex", status_code=None, msg="unsupported content type detail")}, cio())
add(F, {"ctx": ctx(provider="openai-codex", status_code=400, body={"error": {"message": "Request blocked."}}, msg="x")},
    cio(extract_error_code="invalid_prompt", error_obj={"message": "Request blocked."}))
add(F, {"ctx": ctx(provider="openai-codex", status_code=400, msg="invalid_prompt: Request blocked.")}, cio(error_obj={}))
add(F, {"ctx": ctx(provider="openai", status_code=400, msg="invalid_prompt: Request blocked.")}, cio())
add(F, {"ctx": ctx(provider="openai-codex", status_code=500, msg="invalid_prompt: Request blocked.")}, cio())
add(F, {"ctx": ctx(provider="openai-codex", status_code=400, body={"error": {"message": "other"}})}, cio(error_obj={"message": "other"}))

# 18. _moa_special_cases
F = "_moa_special_cases"
add(F, {"ctx": ctx(provider="moa", error=exc(msg="'types.SimpleNamespace' object is not iterable"))}, cio())
add(F, {"ctx": ctx(provider="moa", error=exc(msg="'types.SimpleNamespace' object has no attribute 'index'"))}, cio())
add(F, {"ctx": ctx(error=exc("MoAPresetNotFoundError", "x"))}, cio())
add(F, {"ctx": ctx(error=exc("InvalidProviderResponseError", "x"))}, cio())
add(F, {"ctx": ctx(error=exc("APIError", "x"))}, cio())
add(F, {"ctx": ctx(provider="moa", error=exc(msg="normal"))}, cio())

# 19. _nous_welcome_tier
F = "_nous_welcome_tier"
add(F, {"ctx": ctx(provider="nous", status_code=400, msg="x")}, cio(welcome_route_refusal="named_on_welcome_host"))
add(F, {"ctx": ctx(provider="nous", status_code=400, msg="x")}, cio(welcome_route_refusal=None))
add(F, {"ctx": ctx(provider="other", status_code=400)}, cio())
add(F, {"ctx": ctx(anonymous=True, status_code=402, base_url="https://welcome")}, cio(route_is_welcome_host=True))
add(F, {"ctx": ctx(anonymous=True, status_code=402, base_url="https://other")}, cio(route_is_welcome_host=False, welcome_route_refusal=None))
add(F, {"ctx": ctx(anonymous=True, status_code=429)}, cio(parse_welcome_refusal={"reason": "model_not_free", "retry_after": 0}))
add(F, {"ctx": ctx(anonymous=True, status_code=429)}, cio(parse_welcome_refusal={"reason": "at_capacity", "retry_after": 5}, **{"time.time": 100}))
add(F, {"ctx": ctx(anonymous=True, status_code=429)}, cio(parse_welcome_refusal=None, rate_limit_reset_seconds=7,
                                                          welcome_refusal_from_headers={"reason": "rate_limited", "retry_after": 0}))
add(F, {"ctx": ctx(anonymous=True, status_code=403, msg="plain", base_url="https://welcome")}, cio(welcome_route_refusal="dark_tier"))
add(F, {"ctx": ctx(anonymous=True, status_code=403, msg="content_filter")}, cio(welcome_route_refusal=None))
add(F, {"ctx": ctx(anonymous=True, status_code=400, msg="x", base_url="https://welcome")}, cio(welcome_route_refusal="bad_host"))
add(F, {"ctx": ctx(anonymous=True, status_code=429)}, cio(parse_welcome_refusal=R("SystemExit", "bye")))

# 20. _provider_special_cases
F = "_provider_special_cases"
add(F, {"ctx": ctx(msg="x")}, cio(extract_error_code="data_policy_denied"))
add(F, {"ctx": ctx(msg="flagged for possible cybersecurity risk")}, cio())
add(F, {"ctx": ctx(msg="prompt injection patterns detected")}, cio())
add(F, {"ctx": ctx(msg="bad-credentials", status_code=403)}, cio())
add(F, {"ctx": ctx(msg="blocked for a previous policy violation")}, cio())
add(F, {"ctx": ctx(provider="openai-codex", status_code=400, msg="invalid_prompt: Request blocked.")}, cio())
add(F, {"ctx": ctx(status_code=400, msg="thinking_signature_invalid")}, cio(extract_error_code="thinking_signature_invalid"))
add(F, {"ctx": ctx(status_code=400, msg="thinking signature cannot be modified")}, cio())
add(F, {"ctx": ctx(status_code=429, msg="extra usage long context")}, cio())
add(F, {"ctx": ctx(status_code=400, msg="long context beta not yet available")}, cio())
add(F, {"ctx": ctx(status_code=400, msg="error parsing grammar")}, cio())
add(F, {"ctx": ctx(status_code=400, msg="error parsing grammar no user query found")}, cio())
add(F, {"ctx": ctx(msg="do not have an active grok subscription")}, cio())
add(F, {"ctx": ctx(msg="nothing special")}, cio())
add(F, {"ctx": ctx(error=exc("GeminiAPIError", "x", {"code": "gemini_prompt_blocked"}))}, cio())

# 21. _status_403
F = "_status_403"
add(F, {"ctx": ctx(status_code=403)}, cio(extract_error_code="upstream_unavailable"))
add(F, {"ctx": ctx(status_code=403, provider="xai-oauth")}, cio(extract_error_code="personal-team-blocked:spending-limit"))
add(F, {"ctx": ctx(status_code=403, msg="insufficient credits")}, cio())
add(F, {"ctx": ctx(status_code=403, msg="your request was blocked")}, cio())
add(F, {"ctx": ctx(status_code=403, msg="forbidden")}, cio())
add(F, {"ctx": ctx(status_code=403, msg="resource exhausted")}, cio())

# 22. _status_404
F = "_status_404"
add(F, {"ctx": ctx(status_code=404)}, cio(extract_error_code="insufficient_quota"))
add(F, {"ctx": ctx(status_code=404, msg="insufficient credits")}, cio())
add(F, {"ctx": ctx(status_code=404, msg="no endpoints available matching your data policy")}, cio())
add(F, {"ctx": ctx(status_code=404, msg="model not found")}, cio())
add(F, {"ctx": ctx(status_code=404, msg="404 page not found", model="bare", provider="nvidia")}, cio(model_id_missing_known_prefix=True))
add(F, {"ctx": ctx(status_code=404, msg="404 page not found", model="bare", provider="nvidia")}, cio(model_id_missing_known_prefix=False))
add(F, {"ctx": ctx(status_code=404, msg="x")}, cio(model_id_missing_known_prefix=R("SystemExit", "bye")))

# 23. _status_429
F = "_status_429"
add(F, {"ctx": ctx(status_code=429)}, cio(extract_error_code="terminal_quota_exhausted"))
add(F, {"ctx": ctx(status_code=429, msg="overloaded")}, cio())
add(F, {"ctx": ctx(status_code=429, provider="openrouter")}, cio(is_openrouter_upstream_error=True, extract_upstream_provider_name="anthropic"))
add(F, {"ctx": ctx(status_code=429, provider="openrouter")}, cio(is_openrouter_upstream_error=True, extract_upstream_provider_name=None))
add(F, {"ctx": ctx(status_code=429, msg="daily limit")}, cio(is_openrouter_upstream_error=False, has_usage_limit_transient_signal=False))
add(F, {"ctx": ctx(status_code=429, msg="usage limit")}, cio(is_openrouter_upstream_error=False, has_usage_limit_transient_signal=False))
add(F, {"ctx": ctx(status_code=429, msg="rate limit")}, cio(is_openrouter_upstream_error=False, has_usage_limit_transient_signal=False, rate_limit_reset_seconds=30, **{"time.time": 100}))
add(F, {"ctx": ctx(status_code=429, msg="rate limit")}, cio(is_openrouter_upstream_error=False, has_usage_limit_transient_signal=False, rate_limit_reset_seconds=None))
add(F, {"ctx": ctx(status_code=429, msg="quota")}, cio(is_openrouter_upstream_error=False, has_usage_limit_transient_signal=True, rate_limit_reset_seconds=None))
add(F, {"ctx": ctx(status_code=429)}, cio(is_openrouter_upstream_error=R("KeyboardInterrupt", "ki")))

# 24. _status_5xx
F = "_status_5xx"
add(F, {"ctx": ctx(status_code=500, msg="unknown parameter")}, cio())
add(F, {"ctx": ctx(status_code=500, msg="unsupported parameter prompt_cache_retention", provider="openai")}, cio())
add(F, {"ctx": ctx(status_code=500)}, cio(extract_error_code="invalid_request_error"))
add(F, {"ctx": ctx(status_code=502, msg="returned an empty response")}, cio())
add(F, {"ctx": ctx(status_code=500, msg="out of memory")}, cio())
add(F, {"ctx": ctx(status_code=500, msg="context length exceeded")}, cio())
add(F, {"ctx": ctx(status_code=500, msg="boom")}, cio())

# 25. _is_nous_route
F = "_is_nous_route"
add(F, {"ctx": ctx(provider="nous")}, cio())
add(F, {"ctx": ctx(provider="  Nous  ")}, cio())
add(F, {"ctx": ctx(provider="x", base_url="https://nousresearch.com")}, cio(base_url_host_matches=True))
add(F, {"ctx": ctx(provider="x", base_url="https://other.com")}, cio(base_url_host_matches=False))
add(F, {"ctx": ctx(provider="x", base_url="")}, cio())
add(F, {"ctx": ctx(provider="x", base_url="https://nousresearch.com")}, cio(base_url_host_matches=R("ValueError", "h")))

# 26. _by_status
F = "_by_status"
add(F, {"ctx": ctx(status_code=None)}, cio())
add(F, {"ctx": ctx(status_code=401)}, cio())
add(F, {"ctx": ctx(status_code=400)}, cio(classify_400={"reason": "format_error"}))
add(F, {"ctx": ctx(status_code=402, provider="nous")}, cio(extract_error_code="usage_window_and_credits_exhausted", classify_402={"reason": "billing"}))
add(F, {"ctx": ctx(status_code=403, msg="forbidden")}, cio())
add(F, {"ctx": ctx(status_code=404, msg="model not found")}, cio())
add(F, {"ctx": ctx(status_code=408)}, cio())
add(F, {"ctx": ctx(status_code=413)}, cio())
add(F, {"ctx": ctx(status_code=422)}, cio(classify_image_tool_422={"reason": "image_too_large"}))
add(F, {"ctx": ctx(status_code=429, msg="rate limit")}, cio(is_openrouter_upstream_error=False, has_usage_limit_transient_signal=False, rate_limit_reset_seconds=None))
add(F, {"ctx": ctx(status_code=500, msg="boom")}, cio())
add(F, {"ctx": ctx(status_code=503, msg="out of memory")}, cio())
add(F, {"ctx": ctx(status_code=529)}, cio())
add(F, {"ctx": ctx(status_code=418)}, cio())
add(F, {"ctx": ctx(status_code=599)}, cio())
add(F, {"ctx": ctx(status_code=200)}, cio())
add(F, {"ctx": ctx(status_code=400)}, cio(classify_400=R("SystemExit", "bye")))

# 27. _by_error_code
F = "_by_error_code"
add(F, {"ctx": ctx(msg="request validation failed: x")}, cio(extract_error_code="provider_stream_non_json_data"))
add(F, {"ctx": ctx()}, cio(extract_error_code="resource_exhausted"))
add(F, {"ctx": ctx()}, cio(extract_error_code="throttled"))
add(F, {"ctx": ctx()}, cio(extract_error_code="insufficient_quota"))
add(F, {"ctx": ctx()}, cio(extract_error_code="model_not_found"))
add(F, {"ctx": ctx()}, cio(extract_error_code="context_length_exceeded"))
add(F, {"ctx": ctx()}, cio(extract_error_code="prefill_memory_exceeded"))
add(F, {"ctx": ctx()}, cio(extract_error_code="invalid_encrypted_content"))
add(F, {"ctx": ctx(provider="openai-codex")}, cio(extract_error_code="server_error"))
add(F, {"ctx": ctx(provider="google")}, cio(extract_error_code="unavailable"))
add(F, {"ctx": ctx(provider="vertex")}, cio(extract_error_code="deadline_exceeded"))
add(F, {"ctx": ctx(provider="anthropic")}, cio(extract_error_code="rate_limit_error"))
add(F, {"ctx": ctx(provider="other")}, cio(extract_error_code="server_error"))
add(F, {"ctx": ctx()}, cio(extract_error_code="unknown_code"))

# 28. _by_transport
F = "_by_transport"
add(F, {"ctx": ctx(msg="certificate verify failed")}, cio())
add(F, {"ctx": ctx(msg="ssl alert")}, cio())
add(F, {"ctx": ctx(status_code=None, msg="server disconnected", model="reasoning")}, cio(get_reasoning_stale_timeout_floor=5.0))
add(F, {"ctx": ctx(status_code=None, msg="server disconnected", approx_tokens=200000)}, cio(get_reasoning_stale_timeout_floor=None))
add(F, {"ctx": ctx(status_code=None, msg="server disconnected")}, cio(get_reasoning_stale_timeout_floor=None))
add(F, {"ctx": ctx(status_code=None, msg="connection reset")}, cio())
add(F, {"ctx": ctx(error=exc("RuntimeError", "consecutive stale attempts aborting this call"), msg="consecutive stale attempts aborting this call")}, cio())
add(F, {"ctx": ctx(error=exc("ReadTimeout", "x"))}, cio())
add(F, {"ctx": ctx(error=exc("MyConnectionThing", "x"))}, cio())
add(F, {"ctx": ctx(error=exc("TimeoutError", "x"))}, cio())
add(F, {"ctx": ctx(error=exc("APIError", "x"), msg="nothing")}, cio())
add(F, {"ctx": ctx(status_code=None, msg="server disconnected")}, cio(get_reasoning_stale_timeout_floor=R("GeneratorExit", "ge")))

# 29. _shape_context
F = "_shape_context"
add(F, {"ctx": ctx(error=exc("TimeoutError", "x"), msg="x"), "verdict": {"reason": "timeout"}, "hooked": False}, cio())
add(F, {"ctx": ctx(error=exc("APIError", "timed out"), msg="timed out"), "verdict": {"reason": "unknown"}, "hooked": False}, cio())
add(F, {"ctx": ctx(status_code=None, msg="server disconnected"), "verdict": {"reason": "timeout"}, "hooked": False}, cio())
add(F, {"ctx": ctx(error=exc(attrs={"body": {"error": {"code": "x"}}}), msg="x"), "verdict": {"reason": "unknown"}, "hooked": False}, cio())
add(F, {"ctx": ctx(msg="x"), "verdict": {"reason": "billing", "should_fallback": False}, "hooked": True}, cio())
add(F, {"ctx": ctx(msg="x"), "verdict": {"reason": "billing", "should_fallback": True}, "hooked": True}, cio())
add(F, {"ctx": ctx(status_code=429, msg="quota exceeded"), "verdict": {"reason": "rate_limit"}, "hooked": False}, cio())
add(F, {"ctx": ctx(status_code=400, msg="model is not supported"), "verdict": {"reason": "format_error"}, "hooked": False}, cio())
add(F, {"ctx": ctx(status_code=400, msg="model is not supported insufficient credits"), "verdict": {"reason": "format_error"}, "hooked": False}, cio())

# 30. _off_route_host
F = "_off_route_host"
add(F, {"ctx": ctx(base_url="https://other.example/v1")}, cio(base_url_hostname="other.example", provider_owns_route=False))
add(F, {"ctx": ctx(base_url="https://own.example/v1")}, cio(base_url_hostname="own.example", provider_owns_route=True))
add(F, {"ctx": ctx(base_url="")}, cio(base_url_hostname=None))
add(F, {"ctx": ctx(base_url="https://other.example")}, cio(base_url_hostname="other.example", provider_owns_route=None))
add(F, {"ctx": ctx(base_url="https://other.example")}, cio(base_url_hostname=R("ValueError", "h")))
add(F, {"ctx": ctx(base_url="https://other.example")}, cio(base_url_hostname="other.example", provider_owns_route=R("SystemExit", "bye")))

# 31. _plugin_verdict
F = "_plugin_verdict"
add(F, {"ctx": ctx()}, cio(get_plugin_error_classification={"reason": "billing"}, **{"logger.info": None}))
add(F, {"ctx": ctx()}, cio(get_plugin_error_classification=None))
add(F, {"ctx": ctx()}, cio(get_plugin_error_classification=R("ValueError", "x"), **{"logger.debug": None}))
add(F, {"ctx": ctx()}, cio(get_plugin_error_classification=R("SystemExit", "bye")))
add(F, {"ctx": ctx()}, cio(get_plugin_error_classification=R("KeyboardInterrupt", "ki")))
add(F, {"ctx": ctx()}, cio())  # fail-closed
add(F, {"ctx": ctx()}, cio(get_plugin_error_classification={"reason": "billing"}, **{"logger.info": R("ValueError", "l")}))

# 32. _profile_verdict
F = "_profile_verdict"
PC = "profile.classify_api_error"
add(F, {"ctx": ctx(provider="")}, cio())
add(F, {"ctx": ctx(provider="x")}, cio(get_provider_profile={}))
add(F, {"ctx": ctx(provider="x")}, dict(EC, get_provider_profile={"classify_api_error": True},
                                        **{PC: {"reason": "billing", "should_fallback": True}, "logger.info": None}))
add(F, {"ctx": ctx(provider="x")}, dict(EC, get_provider_profile={"classify_api_error": True},
                                        **{PC: {"reason": "not_a_reason"}}))
add(F, {"ctx": ctx(provider="x")}, dict(EC, get_provider_profile={"classify_api_error": True},
                                        **{PC: "notdict"}))
add(F, {"ctx": ctx(provider="x")}, dict(EC, get_provider_profile={"classify_api_error": True},
                                        **{PC: {"reason": "rate_limit", "should_fallback": True}, "logger.info": None}))
add(F, {"ctx": ctx(provider="x")}, cio(get_provider_profile=R("ValueError", "p"), **{"logger.debug": None}))
add(F, {"ctx": ctx(provider="x")}, cio(get_provider_profile=R("SystemExit", "bye")))
add(F, {"ctx": ctx(provider="x")}, dict(EC, get_provider_profile={"classify_api_error": True},
                                        **{PC: R("GeneratorExit", "ge")}))
add(F, {"ctx": ctx(provider="x")}, dict(EC, get_provider_profile={"classify_api_error": True},
                                        **{PC: {"reason": "billing", "error_context": {"k": 1}}, "logger.info": None}))

# 33. __post_init__
F = "__post_init__"
add(F, {"ctx": ctx(provider="  OpenRouter  ", model=" M ")}, cio(extract_error_code="Some_Code"))
add(F, {"ctx": ctx(error=exc("TimeoutError", "x"))}, cio(extract_error_code=""))
add(F, {"ctx": ctx(error=exc(attrs={"response": resp(headers={"retry-after": "5"})}))}, cio(extract_error_code=""))
add(F, {"ctx": ctx()}, cio(extract_error_code=R("ValueError", "c")))
add(F, {"ctx": ctx()}, cio(extract_error_code=R("SystemExit", "bye")))
add(F, {"ctx": ctx()}, {})  # fail-closed

# 34. large_session
F = "large_session"
add(F, {"ctx": ctx(approx_tokens=100000, context_length=200000), "frac": 0.4, "tokens": 80000, "messages": 80})
add(F, {"ctx": ctx(approx_tokens=90000, context_length=200000), "frac": 0.4, "tokens": 80000, "messages": 80})
add(F, {"ctx": ctx(approx_tokens=10, context_length=100000, num_messages=100), "frac": 0.6, "tokens": 120000, "messages": 200})
add(F, {"ctx": ctx(approx_tokens=10, context_length=300000, num_messages=100), "frac": 0.6, "tokens": 120000, "messages": 200})
add(F, {"ctx": ctx(approx_tokens=130000, context_length=200000), "frac": 0.6, "tokens": 120000, "messages": 200})
add(F, {"ctx": ctx(approx_tokens=0, context_length=0), "frac": 0.4, "tokens": 80000, "messages": 80})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-function count audit
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    short = {k: v for k, v in cnt.items() if v < 5}
    print("wrote %d cases to %s" % (len(CASES), OUT))
    print("functions: %d" % len(cnt))
    if short:
        print("UNDER 5:", short)
        raise SystemExit(1)
    print("all functions >=5 cases")


if __name__ == "__main__":
    main()