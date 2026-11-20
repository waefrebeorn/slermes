#!/usr/bin/env python3
"""Generator for tests/fixtures/auxiliary_client_flow4.jsonl.

>=5 cases per ported function of the UNCLAIMED deep async/client-builder
remainder ported as src/agent/auxiliary_client_flow4.c.

Case shape: {"func": name, "in": {...args..., "gate": {...}, "io": {seam: script}}}
Script shapes (shared by C ac4_call and the oracle):
  - absent key            -> None
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a real exception of that class
  - any other JSON value  -> reused for every call
Domain: objects passed through seams are JSON dicts; integral floats collapse to
ints on both sides; exception args in logger events project to {__raise__,msg}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "auxiliary_client_flow4.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def ok(**over):
    d = dict(gate={})
    d.update(over)
    return d


# ── _named_custom_openai_wire_client (6) ────────────────────────────────────
case("_named_custom_openai_wire_client", custom_base="https://x/v1", custom_key="k",
     io={"to_openai_base_url": "https://x/v1",
         "extract_url_query_params": ["https://x/v1", None],
         "apply_user_default_headers": None,
         "create_openai_client": "clientA"})
case("_named_custom_openai_wire_client", custom_base="https://x/v1", custom_key="k",
     extra_headers={"X": "y"},
     io={"to_openai_base_url": "https://x/v1",
         "extract_url_query_params": ["https://x/v1", {"a": "1"}],
         "apply_user_default_headers": {"H": "v"},
         "create_openai_client": "clientB"})
case("_named_custom_openai_wire_client", custom_base="b", custom_key="k",
     io={"to_openai_base_url": "b", "extract_url_query_params": ["b", None],
         "apply_user_default_headers": None,
         "create_openai_client": raise_("ValueError", "bad")})
case("_named_custom_openai_wire_client", custom_base="b", custom_key="k",
     io={"to_openai_base_url": raise_("RuntimeError", "url")})
case("_named_custom_openai_wire_client", custom_base="b", custom_key="k",
     io={"to_openai_base_url": "b", "extract_url_query_params": raise_("OSError", "x")})
case("_named_custom_openai_wire_client", custom_base="b", custom_key="k",
     io={"to_openai_base_url": "b", "extract_url_query_params": ["b", None],
         "apply_user_default_headers": raise_("TypeError", "hdr")})

# ── _api_key_profile_supplied_client (5) ────────────────────────────────────
case("_api_key_profile_supplied_client", provider="custom", io={"get_provider_profile": None})
case("_api_key_profile_supplied_client", provider="custom", client_kwargs={"a": 1},
     io={"get_provider_profile": {}, "profile.create_client": "client"})
case("_api_key_profile_supplied_client", provider="custom",
     io={"get_provider_profile": raise_("ValueError", "no")})
case("_api_key_profile_supplied_client", provider="custom",
     io={"get_provider_profile": {},
         "profile.create_client": raise_("RuntimeError", "boom")})
case("_api_key_profile_supplied_client", provider="custom",
     io={"get_provider_profile": {}, "profile.create_client": None})

# ── _build_vertex_client (7) ────────────────────────────────────────────────
case("_build_vertex_client", provider="vertex_ai", model="gemini",
     io={"has_vertex_credentials": True, "get_vertex_config": ["tok", "https://v"],
         "normalize_resolved_model": "gemini-2", "vertex_openai_ctor": "vc"})
case("_build_vertex_client", provider="vertex_ai", model=None,
     io={"has_vertex_credentials": True, "get_vertex_config": ["tok", "https://v"],
         "normalize_resolved_model": "default-m", "vertex_openai_ctor": "vc"})
case("_build_vertex_client", provider="vertex_ai", model="x", gate={"vertex": False},
     io={})
case("_build_vertex_client", provider="vertex_ai", model="x",
     io={"has_vertex_credentials": False})
case("_build_vertex_client", provider="vertex_ai", model="x",
     io={"has_vertex_credentials": True, "get_vertex_config": [None, "url"]})
case("_build_vertex_client", provider="vertex_ai", model="x",
     io={"has_vertex_credentials": True, "get_vertex_config": ["t", "u"],
         "normalize_resolved_model": "m",
         "vertex_openai_ctor": raise_("ValueError", "ctor")})
case("_build_vertex_client", provider="vertex_ai", model="x",
     io={"has_vertex_credentials": True, "get_vertex_config": ["t", "u"],
         "normalize_resolved_model": raise_("RuntimeError", "norm")})

# ── _build_bedrock_client (9) ───────────────────────────────────────────────
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     gate={"bedrock": False}, io={})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     io={"has_aws_credentials": False})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     io={"has_aws_credentials": True, "resolve_bedrock_runtime_region": "us-east-1",
         "normalize_resolved_model": "openai.gpt-1",
         "bedrock_openai_uses_mantle": True, "resolve_bedrock_bearer_token": "tok",
         "bedrock_openai_base_url": "https://mantle",
         "configure_bedrock_openai_client_kwargs": {"timeout": 9},
         "openai_ctor": "raw", "codex_client_ctor": "wrapped"})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=True,
     io={"has_aws_credentials": True, "resolve_bedrock_runtime_region": "us-east-1",
         "normalize_resolved_model": "openai.gpt-1",
         "bedrock_openai_uses_mantle": True, "resolve_bedrock_bearer_token": None,
         "bedrock_openai_base_url": "https://mantle",
         "configure_bedrock_openai_client_kwargs": {}, "openai_ctor": "raw"})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     io={"has_aws_credentials": True, "resolve_bedrock_runtime_region": "eu-west-1",
         "normalize_resolved_model": "anthropic.claude-x",
         "bedrock_openai_uses_mantle": False, "is_anthropic_bedrock_model": True,
         "build_anthropic_bedrock_client": "real", "anthropic_client_ctor": "ac"})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     io={"has_aws_credentials": True, "resolve_bedrock_runtime_region": "eu-west-1",
         "normalize_resolved_model": "other-model",
         "bedrock_openai_uses_mantle": False, "is_anthropic_bedrock_model": False,
         "bedrock_client_ctor": "bc"})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     io={"has_aws_credentials": True, "resolve_bedrock_runtime_region": "r",
         "normalize_resolved_model": "anthropic.claude-x",
         "bedrock_openai_uses_mantle": False, "is_anthropic_bedrock_model": True,
         "build_anthropic_bedrock_client": raise_("ImportError", "no sdk")})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     io={"has_aws_credentials": raise_("RuntimeError", "creds")})
case("_build_bedrock_client", provider="bedrock", model="claude", raw_codex=False,
     io={"has_aws_credentials": True, "resolve_bedrock_runtime_region": "r",
         "normalize_resolved_model": "m", "bedrock_openai_uses_mantle": False,
         "is_anthropic_bedrock_model": False,
         "bedrock_client_ctor": raise_("ValueError", "ctor")})

# ── _wrap_transport (9) ─────────────────────────────────────────────────────
def req(**over):
    d = {"provider": "openai", "original_provider": "openai", "model": "m",
         "async_mode": False, "raw_codex": False, "explicit_base_url": "",
         "explicit_api_key": "k", "api_mode": "", "main_runtime": None,
         "is_vision": False, "task": "call", "pool_choice": None}
    d.update(over)
    return d

case("_wrap_transport", req=req(), client={"id": "c"}, final_model="m", base_url="https://x",
     api_key="k", io={"is_actual_auxiliary_route": True})
case("_wrap_transport", req=req(), client={"kind": "codex", "real_client": {"id": "real"}, "model": "m"},
     final_model="m", base_url="https://x", api_key="k",
     io={"is_actual_auxiliary_route": True})
case("_wrap_transport", req=req(), client={"id": "c"}, final_model="m", base_url="https://x",
     api_key="k", io={"is_actual_auxiliary_route": False,
                      "opencode_transport": ["codex_responses", "https://y"],
                      "codex_client_ctor": "wrapped"})
case("_wrap_transport", req=req(api_mode=""), client={"id": "c"}, final_model="gpt-codex",
     base_url="https://api.openai.com/v1", api_key="k",
     io={"is_actual_auxiliary_route": False, "opencode_transport": [None, None],
         "base_url_hostname": "api.openai.com", "codex_client_ctor": "wrapped"})
case("_wrap_transport", req=req(api_mode="", raw_codex=False), client={"id": "c"},
     final_model="m", base_url="https://other.com", api_key="k",
     io={"is_actual_auxiliary_route": False, "opencode_transport": [None, None],
         "base_url_hostname": "other.com", "profile_declared_messages_wire": "anthropic_messages",
         "maybe_wrap_anthropic": "anthropic-client"})
case("_wrap_transport", req=req(api_mode="chat_completions"), client={"id": "c"},
     final_model="m", base_url="https://x", api_key="k",
     io={"is_actual_auxiliary_route": False, "opencode_transport": [None, None],
         "maybe_wrap_anthropic": "plain-client"})
case("_wrap_transport", req=req(), client={"id": "c"}, final_model="m", base_url="https://x",
     api_key="k", io={"is_actual_auxiliary_route": raise_("ValueError", "route")})
case("_wrap_transport", req=req(api_mode="chat_completions"), client={"id": "c"},
     final_model="m", base_url="https://x", api_key="k",
     io={"is_actual_auxiliary_route": False, "opencode_transport": [None, None],
         "maybe_wrap_anthropic": raise_("RuntimeError", "wrap")})
case("_wrap_transport", req=req(), client={"id": "c"}, final_model="m", base_url="https://x",
     api_key="k", io={"is_actual_auxiliary_route": False,
                      "opencode_transport": raise_("OSError", "opencode")})

# ── _project_provider_profile (7) ───────────────────────────────────────────
case("_project_provider_profile", provider="custom", provider_norm="custom", model="m",
     base_url="https://x", reasoning_config=None, data_use=[],
     io={"get_provider_profile": None, "routes_to_custom_endpoint": False})
case("_project_provider_profile", provider="main", provider_norm="main", model="m",
     base_url="https://x", reasoning_config={"level": "high"}, data_use=["a"],
     io={"get_provider_profile": seq(None, {"api_mode": "anthropic_messages"}),
         "routes_to_custom_endpoint": True,
         "build_extra_body": {"x": 1},
         "build_api_kwargs_extras": [{"r": 1}, {"t": 2}],
         "contains_profile_reasoning_fields": True})
case("_project_provider_profile", provider="custom", provider_norm="custom", model="m",
     base_url="https://x", reasoning_config=None, data_use=[],
     io={"get_provider_profile": {"__overridden__": True, "api_mode": "chat"},
         "build_extra_body": {}, "build_api_kwargs_extras": [{}, {}]})
case("_project_provider_profile", provider="x", provider_norm="x", model="m", base_url="u",
     reasoning_config=None, data_use=[],
     io={"get_provider_profile": raise_("ValueError", "no profile")})
case("_project_provider_profile", provider="x", provider_norm="x", model="m", base_url="u",
     reasoning_config=None, data_use=[],
     io={"get_provider_profile": {"api_mode": "chat"},
         "build_extra_body": raise_("RuntimeError", "body")})
case("_project_provider_profile", provider="x", provider_norm="x", model="m", base_url="u",
     reasoning_config=None, data_use=[],
     io={"get_provider_profile": {"api_mode": "chat"},
         "build_extra_body": {"a": 1},
         "build_api_kwargs_extras": raise_("TypeError", "extras")})
case("_project_provider_profile", provider="x", provider_norm="x", model="m", base_url="u",
     reasoning_config=None, data_use=[],
     io={"get_provider_profile": {"api_mode": "chat"},
         "build_extra_body": {"a": 1}, "build_api_kwargs_extras": [{}, {}],
         "contains_profile_reasoning_fields": seq(False, False, True)})

# ── _resolve_call_client (9) ────────────────────────────────────────────────
def rr(**over):
    d = dict(task="call", provider=None, model=None, base_url=None, api_key=None,
             resolved_provider="openrouter", resolved_model=None, resolved_base_url=None,
             resolved_api_key=None, resolved_api_mode=None, main_runtime=None,
             async_mode=False)
    d.update(over)
    return d

case("_resolve_call_client", **rr(), io={"get_cached_client": ["client", "m"],
     "effective_provider_for_client": "openrouter"})
case("_resolve_call_client", **rr(task="vision", resolved_provider="openrouter"),
     io={"resolve_vision_provider_client": ["vision", "vc", "vm"]})
case("_resolve_call_client", **rr(task="vision", resolved_provider="nous"),
     io={"resolve_vision_provider_client": seq(["nous", None, None], ["auto", "vc", "vm"])})
case("_resolve_call_client", **rr(resolved_provider="nous"),
     io={"get_cached_client": [None, None], "effective_provider_for_client": "nous",
         "try_configured_fallback_for_unavailable_client": [None, None, "nous"],
         "nous_credential_failure_detail": "expired",
         "missing_provider_credentials_message": "missing"})
case("_resolve_call_client", **rr(resolved_provider="foo", async_mode=True),
     io={"get_cached_client": [None, None], "effective_provider_for_client": "foo",
         "try_configured_fallback_for_unavailable_client": ["fb", "fm", "fallback"],
         "to_async_client": ["fb_async", "fm"]})
case("_resolve_call_client", **rr(resolved_provider="auto"),
     io={"get_cached_client": seq([None, None], ["autoc", "am"]),
         "effective_provider_for_client": "auto"})
case("_resolve_call_client", **rr(resolved_provider="nous"),
     io={"get_cached_client": [None, None], "effective_provider_for_client": "nous",
         "try_configured_fallback_for_unavailable_client": [None, None, "nous"],
         "missing_provider_credentials_message": "no creds"})
case("_resolve_call_client", **rr(),
     io={"get_cached_client": raise_("RuntimeError", "client")})
case("_resolve_call_client", **rr(resolved_provider="foo", async_mode=True),
     io={"get_cached_client": [None, None], "effective_provider_for_client": "foo",
         "try_configured_fallback_for_unavailable_client": ["fb", "fm", "fallback"],
         "to_async_client": raise_("ValueError", "toasync")})

# ── _acreate_with_progress (9) ──────────────────────────────────────────────
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="", io={"bypass_chat_sdk_request_transform": {"model": "m"},
                      "notify_aux_dispatch": None, "aux_progress_active": False,
                      "create": "resp", "async_client_streams_internally": False,
                      "notify_aux_provider_response": None})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="", io={"bypass_chat_sdk_request_transform": {"model": "m"},
                      "notify_aux_dispatch": None, "aux_progress_active": True,
                      "stream_request_plan": [{"model": "m", "stream": True}, "m", 100],
                      "create": {"chunks": [1]},
                      "aggregate_chat_stream_async": "aggregated"})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     force_stream=True, provider="", io={"bypass_chat_sdk_request_transform": {"model": "m"},
     "notify_aux_dispatch": None,
     "aux_progress_active": False, "async_client_streams_internally": True, "create": "resp"})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="openrouter", io={"bypass_chat_sdk_request_transform": {"model": "m"},
     "notify_aux_dispatch": None, "aux_progress_active": True,
     "stream_request_plan": [{"model": "m"}, "m", 100],
     "create": raise_("ValueError", "reject"),
     "classify_failure": {"verdict": "negotiation", "error": "bad"},
     "stream_negotiation_reraises": True})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="", io={"bypass_chat_sdk_request_transform": {"model": "m"},
     "notify_aux_dispatch": None, "aux_progress_active": True,
     "create": raise_("ValueError", "reject"),
     "classify_failure": {"verdict": "other", "error": "bad"},
     "stream_negotiation_reraises": False, "notify_aux_provider_response": None,
     "stream_request_plan": [{"model": "m"}, "m", 100]})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="", io={"bypass_chat_sdk_request_transform": {"model": "m"},
     "notify_aux_dispatch": None, "aux_progress_active": True,
     "stream_request_plan": [{"model": "m"}, "m", 100],
     "create": {"choices": [{"delta": {}}]}, "notify_aux_provider_response": None})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="", io={"bypass_chat_sdk_request_transform": raise_("RuntimeError", "bypass")})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="", io={"bypass_chat_sdk_request_transform": {"model": "m"},
     "notify_aux_dispatch": None, "aux_progress_active": True,
     "stream_request_plan": [{"model": "m"}, "m", 100],
     "create": {"chunks": [1]},
     "aggregate_chat_stream_async": raise_("ValueError", "agg")})
case("_acreate_with_progress", client={"id": "c"}, kwargs={"model": "m"}, task="call",
     provider="", io={"bypass_chat_sdk_request_transform": {"model": "m"},
                      "notify_aux_dispatch": None, "aux_progress_active": False,
                      "create": "resp", "async_client_streams_internally": raise_("OSError", "int")})

# ── _try_discovery_chain (6) ────────────────────────────────────────────────
case("_try_discovery_chain", io={"get_provider_chain": [["openrouter", "or"], ["nous", "no"]],
     "custom_health_base_url": "https://or", "is_provider_unhealthy": False,
     "try_fn.or": ["client", "model"]})
case("_try_discovery_chain", io={"get_provider_chain": [["openrouter", "or"], ["nous", "no"]],
     "custom_health_base_url": "https://or", "is_provider_unhealthy": True,
     "log_skip_unhealthy": None, "try_fn.no": ["c2", "m2"]})
case("_try_discovery_chain", io={"get_provider_chain": [["openrouter", "or"]],
     "custom_health_base_url": "https://or", "is_provider_unhealthy": False,
     "try_fn.or": [None, None]})
case("_try_discovery_chain", io={"get_provider_chain": [["openrouter", "or"]],
     "is_provider_unhealthy": raise_("ValueError", "health")})
case("_try_discovery_chain", io={"get_provider_chain": [["openrouter", "or"]],
     "custom_health_base_url": "https://or", "is_provider_unhealthy": False,
     "try_fn.or": raise_("RuntimeError", "try")})
case("_try_discovery_chain", io={"get_provider_chain": []})

# ── _translate_anthropic_response_format (6) ────────────────────────────────
case("_translate_anthropic_response_format", anthropic_kwargs={}, response_format="not-a-dict")
case("_translate_anthropic_response_format", anthropic_kwargs={},
     response_format={"type": "json_schema", "json_schema": {"schema": {"a": 1}}})
case("_translate_anthropic_response_format", anthropic_kwargs={},
     response_format={"type": "json_object"})
case("_translate_anthropic_response_format", anthropic_kwargs={"keep": 1},
     response_format={"type": "json_schema", "json_schema": {}})
case("_translate_anthropic_response_format", anthropic_kwargs={"output_config": {"other": 2}},
     response_format={"type": "json_object"})
case("_translate_anthropic_response_format", anthropic_kwargs={"output_config": "bad"},
     response_format={"type": "json_object"})

# ── _pool_credential_digest (6) ─────────────────────────────────────────────
case("_pool_credential_digest", pool="pool", entry={"k": "entry"},
     io={"pool_runtime_api_key": "key1", "blake2b_hexdigest": "abc123"})
case("_pool_credential_digest", pool="pool", io={"pool.entries": [{"k": "e1"}, {"k": "e2"}],
     "pool_runtime_api_key": seq("k2", "k1"), "blake2b_hexdigest": "digest"})
case("_pool_credential_digest", pool="pool", io={"pool.entries": raise_("RuntimeError", "list")})
case("_pool_credential_digest", pool="pool", io={"pool.entries": []})
case("_pool_credential_digest", pool="pool", io={"pool.entries": None})
case("_pool_credential_digest", pool="pool", io={"pool.entries": [{}],
     "pool_runtime_api_key": raise_("ValueError", "key")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases to {OUT}")
    for k in sorted(cnt):
        print(f"  {k}: {cnt[k]}")


if __name__ == "__main__":
    main()
