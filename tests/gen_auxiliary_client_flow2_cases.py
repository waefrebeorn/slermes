#!/usr/bin/env python3
"""Generator for tests/fixtures/auxiliary_client_flow2.jsonl.

Emits >=5 cases per ported function of agent/auxiliary_client.py (the UNCLAIMED
remainder ported as src/agent/auxiliary_client_flow2.c). Each case is
{"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...}}}.

Script shapes (shared with the C universal seam ac2_call and the oracle):
  - absent key            -> None
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  - any other JSON value  -> reused for every call

Domain restrictions (documented in the port header):
  - str()/int()/float() operands are ASCII strings, numbers, bools or null.
  - object-valued seam results (pool/profile/client/refreshed) are null or a
    NON-EMPTY record; the empty record {} is outside the live domain for
    truthiness-gated getters (pool) and is only used where `is not None`
    identity gates it (profile.api_mode AttributeError branch).
  - client doubles carry a "kind" tag for isinstance checks; attribute reads
    (api_key/base_url/runtime_api_key) use the record's fields.
  - integral floats collapse to ints on both sides.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "auxiliary_client_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def req(**over):
    """Full _ResolveRequest field set (NamedTuple) with defaults; overlay."""
    base = {
        "provider": "openrouter", "original_provider": "openrouter", "model": None,
        "async_mode": False, "raw_codex": False, "explicit_base_url": None,
        "explicit_api_key": None, "api_mode": None, "main_runtime": None,
        "is_vision": False, "task": None, "pool_choice": None,
    }
    base.update(over)
    return base


# ── _tick_hook ──────────────────────────────────────────────────────────────
case("_tick_hook", local={}, label="progress")  # no hook -> no seam
case("_tick_hook", local={"hook": None}, label="dispatch")  # hook None -> return
case("_tick_hook", local={"hook": {"h": 1}}, label="progress",
     io=io(hook=None))  # hook() -> None
case("_tick_hook", local={"hook": {"h": 1}}, label="dispatch",
     io=io(hook=raise_("ValueError", "boom")))  # Exception caught -> logger.debug
case("_tick_hook", local={"hook": {"h": 1}}, label="progress",
     io=io(hook=raise_("SystemExit", "bye")))  # BaseException propagates
case("_tick_hook", local={"hook": {"h": 1}}, label="x",
     io=io(hook=raise_("KeyboardInterrupt")))  # BaseException propagates

# ── _notify_aux_dispatch ────────────────────────────────────────────────────
case("_notify_aux_dispatch", local={})
case("_notify_aux_dispatch", local={"hook": {"h": 1}}, io=io(hook=None))
case("_notify_aux_dispatch", local={"hook": {"h": 1}},
     io=io(hook=raise_("RuntimeError", "x")))
case("_notify_aux_dispatch", local={"hook": None})
case("_notify_aux_dispatch", local={"hook": {"h": 1}},
     io=io(hook=raise_("GeneratorExit")))

# ── _notify_aux_timing_response ─────────────────────────────────────────────
case("_notify_aux_timing_response", local={})
case("_notify_aux_timing_response", local={"hook": {"h": 1}}, io=io(hook=None))
case("_notify_aux_timing_response", local={"hook": {"h": 1}},
     io=io(hook=raise_("ValueError", "x")))
case("_notify_aux_timing_response", local={"hook": None})
case("_notify_aux_timing_response", local={"hook": {"h": 1}},
     io=io(hook=raise_("SystemExit")))

# ── _notify_aux_provider_response ───────────────────────────────────────────
case("_notify_aux_provider_response", local={}, io=io(_notify_aux_progress=None))
case("_notify_aux_provider_response", local={"hook": {"h": 1}},
     io=io(hook=None, _notify_aux_progress=None))
case("_notify_aux_provider_response", local={"hook": {"h": 1}},
     io=io(hook=raise_("ValueError", "x"), _notify_aux_progress=None))
case("_notify_aux_provider_response", local={"hook": {"h": 1}},
     io=io(hook=None, _notify_aux_progress=raise_("RuntimeError", "p")))
case("_notify_aux_provider_response", local={"hook": {"h": 1}},
     io=io(hook=raise_("SystemExit"), _notify_aux_progress=None))

# ── _ai_gateway_headers ─────────────────────────────────────────────────────
case("_ai_gateway_headers", io=io(hermes_user_agent="HermesAgent/1.0"))
case("_ai_gateway_headers", io=io(hermes_user_agent=""))
case("_ai_gateway_headers", io=io(hermes_user_agent=raise_("RuntimeError", "ua")))
case("_ai_gateway_headers", io=io(hermes_user_agent=None))
case("_ai_gateway_headers", io=io(hermes_user_agent="X/2"))

# ── _auth_json_path ─────────────────────────────────────────────────────────
case("_auth_json_path", env={"auth_json_path": "/a/auth.json",
                            "auth_json_path_at_import": "/b/auth.json"})  # patched wins
case("_auth_json_path", env={"auth_json_path": "/a/auth.json",
                            "auth_json_path_at_import": "/a/auth.json"},
     io=io(_auth_file_path="/live/auth.json"))  # equal -> live reader
case("_auth_json_path", env={"auth_json_path": "/a/auth.json",
                            "auth_json_path_at_import": "/a/auth.json"},
     io=io(_auth_file_path=raise_("RuntimeError", "no")))
case("_auth_json_path", env={"auth_json_path": "/x", "auth_json_path_at_import": "/y"})
case("_auth_json_path", env={"auth_json_path": "/a", "auth_json_path_at_import": "/a"},
     io=io(_auth_file_path=None))

# ── _load_pool_with_credentials ─────────────────────────────────────────────
case("_load_pool_with_credentials", provider="nous", note="",
     io=io(load_pool=None))  # pool None -> None
case("_load_pool_with_credentials", provider="nous", note=" n",
     io=io(load_pool={"p": 1}, **{"pool.has_credentials": True}))
case("_load_pool_with_credentials", provider="nous",
     io=io(load_pool={"p": 1}, **{"pool.has_credentials": False}))
case("_load_pool_with_credentials", provider="nous", note=" note",
     io=io(load_pool=raise_("RuntimeError", "load fail")))  # caught -> logger.debug -> None
case("_load_pool_with_credentials", provider="nous",
     io=io(load_pool=raise_("SystemExit")))  # BaseException propagates
case("_load_pool_with_credentials", provider="x",
     io=io(load_pool={"p": 1}, **{"pool.has_credentials": raise_("ValueError", "hc")}))

# ── _close_quietly ──────────────────────────────────────────────────────────
case("_close_quietly", target={}, failure_note=None)  # no close attr
case("_close_quietly", target={"close": True}, failure_note=None, io=io(close=None))
case("_close_quietly", target={"close": True}, failure_note="note",
     io=io(close=raise_("RuntimeError", "c")))  # caught + note -> logger.debug
case("_close_quietly", target={"close": True}, failure_note=None,
     io=io(close=raise_("RuntimeError", "c")))  # caught, no note -> silent
case("_close_quietly", target={"close": True}, failure_note="n",
     io=io(close=raise_("SystemExit")))  # BaseException propagates
case("_close_quietly", target={"close": None}, failure_note="n")  # close not callable

# ── _attempt_stream_socket ──────────────────────────────────────────────────
case("_attempt_stream_socket", stream={})  # no response -> None
case("_attempt_stream_socket", stream={"response": {}})  # no extensions -> None
case("_attempt_stream_socket", stream={"response": {"extensions": {}}})  # no network_stream
case("_attempt_stream_socket",
     stream={"response": {"extensions": {"network_stream": {"s": 1}}}},
     io=io(_socket_from_stream={"sock": 1}))
case("_attempt_stream_socket",
     stream={"response": {"extensions": {"network_stream": {"s": 1}}}},
     io=io(_socket_from_stream=None))
case("_attempt_stream_socket",
     stream={"response": {"extensions": "notdict"}})  # extensions not dict -> None

# ── _route_client_headers ───────────────────────────────────────────────────
case("_route_client_headers", base_url="https://x", provider="openrouter",
     io=io(route_answer={"X": "1"}))
case("_route_client_headers", base_url="https://x", provider="",
     io=io(route_answer=None))  # falsy -> None
case("_route_client_headers", base_url="https://x", provider="p", is_vision=True,
     io=io(route_answer={"A": "b"}))
case("_route_client_headers", base_url="https://x", provider="p",
     io=io(route_answer={}))  # empty dict falsy -> None
case("_route_client_headers", base_url="https://x", provider="p",
     io=io(route_answer=raise_("RuntimeError", "ra")))

# ── _profile_default_headers ────────────────────────────────────────────────
case("_profile_default_headers", provider="")  # falsy -> None
case("_profile_default_headers", provider="gmi",
     io=io(get_provider_profile={"default_headers": {"User-Agent": "gmi"}}))
case("_profile_default_headers", provider="gmi",
     io=io(get_provider_profile=None))  # profile None -> None
case("_profile_default_headers", provider="gmi",
     io=io(get_provider_profile={"default_headers": None}))  # falsy headers -> None
case("_profile_default_headers", provider="gmi",
     io=io(get_provider_profile=raise_("RuntimeError", "gp")))  # suppress -> None
case("_profile_default_headers", provider="gmi",
     io=io(get_provider_profile=raise_("SystemExit")))  # BaseException propagates
case("_profile_default_headers", provider="gmi",
     io=io(get_provider_profile={"other": 1}))  # missing default_headers -> AttributeError -> suppress

# ── _endpoint_default_headers ───────────────────────────────────────────────
case("_endpoint_default_headers", base_url="https://api.kimi.com/v1", provider="kimi",
     io=io(base_url_host_matches=True, _apply_user_default_headers={"User-Agent": "claude-code/0.1.0"}))
case("_endpoint_default_headers", base_url="https://x", provider="p",
     io=io(base_url_host_matches=False, route_answer={"R": "1"},
           _apply_user_default_headers={"R": "1"}))
case("_endpoint_default_headers", base_url="https://integrate.api.nvidia.com", provider="nvidia",
     io=io(base_url_host_matches=seq(False, True), build_nvidia_nim_headers={"N": "1"},
           _apply_user_default_headers={"N": "1"}))
case("_endpoint_default_headers", base_url="https://api.x.ai", provider="xai", xai=True,
     io=io(base_url_host_matches=seq(False, False, True), hermes_xai_default_headers={"X": "1"},
           _apply_user_default_headers={"X": "1"}))
case("_endpoint_default_headers", base_url="https://other", provider="gmi",
     io=io(base_url_host_matches=False, route_answer=None,
           get_provider_profile={"default_headers": {"G": "1"}},
           _apply_user_default_headers={"G": "1"}))
case("_endpoint_default_headers", base_url="https://other", provider="",
     io=io(base_url_host_matches=False, route_answer=None,
           _apply_user_default_headers=None))  # headers {} -> None -> None
case("_endpoint_default_headers", base_url="https://api.kimi.com", provider="kimi",
     io=io(base_url_host_matches=True, _apply_user_default_headers=None))  # user headers None -> None

# ── _codex_base_url_override ────────────────────────────────────────────────
case("_codex_base_url_override", io=io(_scoped_key_env="https://x/v1/"))
case("_codex_base_url_override", io=io(_scoped_key_env="https://x///"))
case("_codex_base_url_override", io=io(_scoped_key_env=""))
case("_codex_base_url_override", io=io(_scoped_key_env="https://x"))
case("_codex_base_url_override", io=io(_scoped_key_env=None))  # None.rstrip -> AttributeError
case("_codex_base_url_override", io=io(_scoped_key_env=raise_("RuntimeError", "env")))

# ── _profile_declared_messages_wire ─────────────────────────────────────────
case("_profile_declared_messages_wire", provider="Anthropic",
     io=io(get_provider_profile={"api_mode": "anthropic_messages"}))
case("_profile_declared_messages_wire", provider="openai",
     io=io(get_provider_profile={"api_mode": "chat_completions"}))
case("_profile_declared_messages_wire", provider=None,
     io=io(get_provider_profile=None))  # profile None -> None
case("_profile_declared_messages_wire", provider="  X  ",
     io=io(get_provider_profile={"api_mode": "anthropic_messages"}))  # strip+lower
case("_profile_declared_messages_wire", provider="p",
     io=io(get_provider_profile=raise_("RuntimeError", "gp")))  # no guard -> propagates
case("_profile_declared_messages_wire", provider="p",
     io=io(get_provider_profile={}))  # is not None -> .api_mode AttributeError

# ── _route_client ───────────────────────────────────────────────────────────
case("_route_client", req=req(), client_obj={"kind": "OpenAI"}, final_model_str="m")
case("_route_client", req=req(async_mode=True), client_obj={"kind": "OpenAI"},
     final_model_str="m", io=io(_to_async_client=[{"kind": "AsyncOpenAI"}, "m"]))
case("_route_client", req=req(async_mode=True, is_vision=True), client_obj={"kind": "C"},
     final_model_str="m", io=io(_to_async_client=[{"kind": "AC"}, "m"]))
case("_route_client", req=req(), client_obj=None, final_model_str=None)
case("_route_client", req=req(async_mode=True), client_obj={"kind": "C"},
     final_model_str="m", io=io(_to_async_client=raise_("RuntimeError", "async")))

# ── _route_or_warn ──────────────────────────────────────────────────────────
case("_route_or_warn", req=req(), client=None, default="d", unavailable_msg="no %s",
     args=["x"])  # None -> logger.warning -> (None,None)
case("_route_or_warn", req=req(model="m"), client={"kind": "C"}, default="d",
     unavailable_msg="msg", io=io(_normalize_resolved_model="m"))
case("_route_or_warn", req=req(model=None, provider="p"), client={"kind": "C"},
     default="d", unavailable_msg="msg", io=io(_normalize_resolved_model="d"))
case("_route_or_warn", req=req(async_mode=True, model="m"), client={"kind": "C"},
     default="d", unavailable_msg="msg",
     io=io(_normalize_resolved_model="m", _to_async_client=[{"kind": "AC"}, "m"]))
case("_route_or_warn", req=req(), client=None, default=None, unavailable_msg="m")

# ── _resolve_openrouter_branch ──────────────────────────────────────────────
case("_resolve_openrouter_branch", req=req(model="m"),
     io=io(_try_openrouter=[{"kind": "C"}, "d"], _normalize_resolved_model="m"))
case("_resolve_openrouter_branch", req=req(model="m"),
     io=io(_try_openrouter=[None, None], _describe_openrouter_unavailable="no key"))
case("_resolve_openrouter_branch", req=req(model=None, provider="p"),
     io=io(_try_openrouter=[{"kind": "C"}, "d"], _normalize_resolved_model="d"))
case("_resolve_openrouter_branch", req=req(model="m", async_mode=True),
     io=io(_try_openrouter=[{"kind": "C"}, "d"], _normalize_resolved_model="m",
           _to_async_client=[{"kind": "AC"}, "m"]))
case("_resolve_openrouter_branch", req=req(model="m"),
     io=io(_try_openrouter=raise_("RuntimeError", "or")))

# ── _resolve_nous_branch ────────────────────────────────────────────────────
case("_resolve_nous_branch", req=req(model="m", provider="nous"),
     io=io(_try_nous=[{"kind": "C", "api_key": "k", "base_url": "https://b"}, "d"],
           _normalize_resolved_model="m", nous_api_mode="chat_completions",
           _maybe_wrap_anthropic={"kind": "C", "api_key": "k", "base_url": "https://b"}))
case("_resolve_nous_branch", req=req(model=None, provider="nous"),
     io=io(_try_nous=[None, None]))  # client None -> warning -> (None,None)
case("_resolve_nous_branch", req=req(model="mimo-v2-omni", provider="nous"),
     io=io(_try_nous=[{"kind": "C", "api_key": "", "base_url": ""}, "d"],
           _normalize_resolved_model="mimo-v2-omni", nous_api_mode="anthropic_messages",
           _maybe_wrap_anthropic={"kind": "W"}))
case("_resolve_nous_branch", req=req(model="xiaomi", is_vision=True, provider="nous"),
     io=io(_try_nous=[{"kind": "C"}, "d"], _normalize_resolved_model="xiaomi",
           nous_api_mode="chat_completions", _maybe_wrap_anthropic={"kind": "C"}))
case("_resolve_nous_branch", req=req(model="m", provider="nous", async_mode=True),
     io=io(_try_nous=[{"kind": "C", "api_key": "k", "base_url": "b"}, "d"],
           _normalize_resolved_model="m", nous_api_mode="chat_completions",
           _maybe_wrap_anthropic={"kind": "C"}, _to_async_client=[{"kind": "AC"}, "m"]))
case("_resolve_nous_branch", req=req(model="m", provider="nous"),
     io=io(_try_nous=raise_("SystemExit")))

# ── _resolve_xai_oauth_branch ───────────────────────────────────────────────
case("_resolve_xai_oauth_branch", req=req(model="grok"),
     io=io(_build_xai_oauth_aux_client=[{"kind": "C"}, "grok"], _normalize_resolved_model="grok"))
case("_resolve_xai_oauth_branch", req=req(model=None),
     io=io(_build_xai_oauth_aux_client=[None, None]))
case("_resolve_xai_oauth_branch", req=req(model=None, provider="xai-oauth"),
     io=io(_build_xai_oauth_aux_client=[{"kind": "C"}, "d"], _normalize_resolved_model="d"))
case("_resolve_xai_oauth_branch", req=req(model="grok", async_mode=True),
     io=io(_build_xai_oauth_aux_client=[{"kind": "C"}, "grok"], _normalize_resolved_model="grok",
           _to_async_client=[{"kind": "AC"}, "grok"]))
case("_resolve_xai_oauth_branch", req=req(model="grok"),
     io=io(_build_xai_oauth_aux_client=raise_("RuntimeError", "xai")))

# ── _resolve_azure_foundry_branch ───────────────────────────────────────────
case("_resolve_azure_foundry_branch", req=req(model="m"),
     io=io(_try_azure_foundry=[{"kind": "C"}, "m"], _normalize_resolved_model="m"))
case("_resolve_azure_foundry_branch", req=req(model=None),
     io=io(_try_azure_foundry=[None, None]))
case("_resolve_azure_foundry_branch", req=req(model=None, provider="azure-foundry"),
     io=io(_try_azure_foundry=[{"kind": "C"}, "d"], _normalize_resolved_model="d"))
case("_resolve_azure_foundry_branch", req=req(model="m", async_mode=True),
     io=io(_try_azure_foundry=[{"kind": "C"}, "m"], _normalize_resolved_model="m",
           _to_async_client=[{"kind": "AC"}, "m"]))
case("_resolve_azure_foundry_branch", req=req(model="m"),
     io=io(_try_azure_foundry=raise_("RuntimeError", "az")))

# ── _resolve_openai_codex_branch ────────────────────────────────────────────
case("_resolve_openai_codex_branch", req=req(model=None))  # no model -> warning
case("_resolve_openai_codex_branch", req=req(model="gpt-5-codex", raw_codex=True),
     io=io(_resolve_codex_credential_and_base=["tok", "https://b"],
           _codex_cloudflare_headers={"CF": "1"},
           _create_openai_client={"kind": "RawOpenAI"}, _normalize_resolved_model="gpt-5-codex"))
case("_resolve_openai_codex_branch", req=req(model="gpt-5-codex", raw_codex=True),
     io=io(_resolve_codex_credential_and_base=[None, "https://b"]))  # no token -> warning
case("_resolve_openai_codex_branch", req=req(model="gpt-5-codex", raw_codex=False),
     io=io(_build_codex_client=[{"kind": "Codex"}, "d"], _normalize_resolved_model="gpt-5-codex"))
case("_resolve_openai_codex_branch", req=req(model="gpt-5-codex", raw_codex=False),
     io=io(_build_codex_client=[None, None]))  # route_or_warn None
case("_resolve_openai_codex_branch", req=req(model="m", raw_codex=True, async_mode=True),
     io=io(_resolve_codex_credential_and_base=["tok", "https://b"],
           _codex_cloudflare_headers={"CF": "1"}, _create_openai_client={"kind": "Raw"},
           _normalize_resolved_model="m"))  # raw path ignores async_mode

# ── _discovery_chain_allowed ────────────────────────────────────────────────
case("_discovery_chain_allowed", main_provider="")
case("_discovery_chain_allowed", main_provider="auto")
case("_discovery_chain_allowed", main_provider="  AUTO  ")
case("_discovery_chain_allowed", main_provider="openrouter", task="compression")
case("_discovery_chain_allowed", main_provider="nous", task=None)
case("_discovery_chain_allowed", main_provider=None)

# ── _log_once_debug ─────────────────────────────────────────────────────────
case("_log_once_debug", seen=[], key="k", msg="m %s", args=["a"])
case("_log_once_debug", seen=["k"], key="k", msg="m", args=[])  # already seen -> silent
case("_log_once_debug", seen=["other"], key="k", msg="m", args=[])
case("_log_once_debug", seen=[], key="k", msg="m", args=["a", "b"])
case("_log_once_debug", seen=["k", "j"], key="k", msg="m %s %s", args=["a", "b"])

# ── _warn_stale_openai_base_url ─────────────────────────────────────────────
case("_warn_stale_openai_base_url", runtime_provider="openrouter",
     env={"warned": False}, io=io(**{"os.getenv": "https://stale"}))
case("_warn_stale_openai_base_url", runtime_provider="custom",
     env={"warned": False}, io=io(**{"os.getenv": "https://stale"}))  # custom -> no warn
case("_warn_stale_openai_base_url", runtime_provider="custom:x",
     env={"warned": False}, io=io(**{"os.getenv": "https://stale"}))  # custom: -> no warn
case("_warn_stale_openai_base_url", runtime_provider="",
     env={"warned": False}, io=io(**{"os.getenv": "https://stale"}, _read_main_provider="nous"))
case("_warn_stale_openai_base_url", runtime_provider="openrouter",
     env={"warned": True}, io=io(**{"os.getenv": "https://stale"}))  # already warned -> return
case("_warn_stale_openai_base_url", runtime_provider="openrouter",
     env={"warned": False}, io=io(**{"os.getenv": ""}))  # empty env -> no warn
case("_warn_stale_openai_base_url", runtime_provider="openrouter",
     env={"warned": False}, io=io(**{"os.getenv": None}))  # None.strip -> AttributeError

# ── _is_actual_auxiliary_route ──────────────────────────────────────────────
case("_is_actual_auxiliary_route", req=req(provider="actual"), base_url="https://x",
     io=io(is_actual_route=True))
case("_is_actual_auxiliary_route", req=req(provider="p", main_runtime=None), base_url="https://x",
     io=io(is_actual_route=seq(False, True),
           _normalize_main_runtime={"provider": "actual", "base_url": "https://x"},
           normalize_actual_base_url=seq("https://x", "https://x"),
           normalize_route_base_url=seq("https://x", "https://x")))
case("_is_actual_auxiliary_route", req=req(provider="p", main_runtime=None), base_url="",
     io=io(is_actual_route=False, _normalize_main_runtime={"provider": "a", "base_url": "b"}))
case("_is_actual_auxiliary_route", req=req(provider="p", main_runtime=None), base_url="https://x",
     io=io(is_actual_route=seq(False, False),
           _normalize_main_runtime={"provider": "a", "base_url": "b"}))
case("_is_actual_auxiliary_route", req=req(provider="p", main_runtime=None), base_url="https://x",
     io=io(is_actual_route=seq(False, True),
           _normalize_main_runtime={"provider": "a", "base_url": "https://y"},
           normalize_actual_base_url=seq("https://x", "https://y"),
           normalize_route_base_url=seq("https://x", "https://y")))  # mismatch -> False
case("_is_actual_auxiliary_route", req=req(provider="p", main_runtime=None), base_url="https://x",
     io=io(is_actual_route=False, _normalize_main_runtime=None))  # None.get -> AttributeError

# ── _main_route_target ──────────────────────────────────────────────────────
case("_main_route_target", runtime={"provider": "nous", "model": "m", "base_url": "b",
                                    "api_key": "k", "api_mode": "chat_completions"}, task=None)
case("_main_route_target", runtime={}, task=None,
     io=io(_read_main_provider="openrouter", _read_main_model="d"))
case("_main_route_target", runtime={"provider": "nous", "model": "m"}, task="title",
     io=io(_task_prefers_fast_model=True, fast_catalog_provider="nous",
           _get_aux_model_for_provider="fast"))
case("_main_route_target", runtime={"provider": "moa", "model": "preset", "base_url": "moa://x",
                                    "api_key": "k", "api_mode": "chat"}, task=None,
     io=io(_resolve_moa_aggregator=["agg", "aggmodel"]))
case("_main_route_target", runtime={"provider": "moa", "model": "preset"}, task=None,
     io=io(_resolve_moa_aggregator=[None, None]))  # no aggregator -> keep moa
case("_main_route_target", runtime={"provider": "auto", "model": "m"}, task="title",
     io=io(_task_prefers_fast_model=True))  # auto -> no fast model
case("_main_route_target", runtime=None, task=None)  # None.get -> AttributeError

# ── _read_main_field ────────────────────────────────────────────────────────
case("_read_main_field", field="model", readonly=True,
     io=io(_runtime_main_value="  Override  "))
case("_read_main_field", field="provider", readonly=True, lower=True,
     io=io(_runtime_main_value="  Nous  "))
case("_read_main_field", field="model", readonly=True,
     io=io(_runtime_main_value=None, load_config_readonly={"model": {"default": "cfgmodel"}}))
case("_read_main_field", field="model", readonly=True,
     io=io(_runtime_main_value="", load_config_readonly={"model": "  strmodel  "}))
case("_read_main_field", field="api_key", readonly=False,
     io=io(_runtime_main_value=None, load_config={"model": {"api_key": "  key  "}}))
case("_read_main_field", field="base_url", readonly=False, lower=True,
     io=io(_runtime_main_value=None, load_config={"model": {"base_url": "  URL  "}}))
case("_read_main_field", field="model", readonly=True,
     io=io(_runtime_main_value=None, load_config_readonly=raise_("RuntimeError", "cfg")))
case("_read_main_field", field="model", readonly=True, env={"config_import_fails": True},
     io=io(_runtime_main_value=None))  # import fails -> suppress -> ""
case("_read_main_field", field="model", readonly=True,
     io=io(_runtime_main_value=None, load_config_readonly=None))  # None.get -> suppress -> ""

# ── _read_main_api_key_if_same_origin ───────────────────────────────────────
case("_read_main_api_key_if_same_origin", aux_base_url="https://x/v1",
     io=io(base_url_origin=seq(["https", "x", 443], ["https", "x", 443]),
           _normalize_main_runtime={"base_url": "https://x/v1", "api_key": "k"},
           _read_main_base_url="https://x/v1", _read_main_api_key="cfgkey"))
case("_read_main_api_key_if_same_origin", aux_base_url="",
     io=io(base_url_origin=["", "", 0]))  # no host -> ""
case("_read_main_api_key_if_same_origin", aux_base_url="https://x/v1",
     io=io(base_url_origin=seq(["https", "x", 443], ["https", "x", 443]),
           _normalize_main_runtime={"base_url": "https://x/v1", "api_key": "livekey"}))
case("_read_main_api_key_if_same_origin", aux_base_url="https://x/v1",
     io=io(base_url_origin=seq(["https", "x", 443], ["https", "y", 443]),
           _normalize_main_runtime={"base_url": "https://y/v1", "api_key": "livekey"}))  # mismatch
case("_read_main_api_key_if_same_origin", aux_base_url="https://x/v1",
     io=io(base_url_origin=seq(["https", "x", 443], ["https", "x", 443]),
           _normalize_main_runtime={}, _read_main_base_url="https://x/v1",
           _read_main_api_key="cfgkey"))
case("_read_main_api_key_if_same_origin", aux_base_url="https://x/v1",
     io=io(base_url_origin=seq(["https", "x", 443], ["https", "y", 443]),
           _normalize_main_runtime={}, _read_main_base_url="https://y/v1"))  # cfg mismatch
case("_read_main_api_key_if_same_origin", aux_base_url="https://x/v1",
     io=io(base_url_origin=["https", "x", 443], _normalize_main_runtime=None))  # None.get -> AttributeError

# ── _borrowed_main_credential_key ───────────────────────────────────────────
case("_borrowed_main_credential_key", provider="nous", base_url="https://x", api_key="k",
     runtime={}, io=io(_normalize_aux_provider="nous"))  # not custom -> ()
case("_borrowed_main_credential_key", provider="custom", base_url=None, api_key=None,
     runtime={"base_url": "https://rt", "api_key": "rtkey"},
     io=io(_normalize_aux_provider="custom", _runtime_cache_discriminator="disc"))
case("_borrowed_main_credential_key", provider="custom", base_url="https://x", api_key="explicit",
     runtime={}, io=io(_normalize_aux_provider="custom", _normalize_api_key="explicit"))  # -> ()
case("_borrowed_main_credential_key", provider="custom", base_url="https://x", api_key=None,
     runtime={},
     io=io(_normalize_aux_provider="custom", _normalize_api_key=None,
           _to_openai_base_url="https://x/v1",
           base_url_origin=["https", "x", 443], _normalize_main_runtime={},
           _read_main_base_url="https://x/v1", _read_main_api_key="borrowed",
           _runtime_cache_discriminator="disc"))
case("_borrowed_main_credential_key", provider="custom", base_url="https://x", api_key=None,
     runtime={},
     io=io(_normalize_aux_provider="custom", _normalize_api_key=None,
           _to_openai_base_url="https://x/v1",
           base_url_origin=["https", "x", 443], _normalize_main_runtime={},
           _read_main_base_url="https://y/v1", _read_main_api_key="",
           declared_custom_credential_identity=seq(None, "declared"),
           _runtime_cache_discriminator="disc"))
case("_borrowed_main_credential_key", provider="custom", base_url="https://x", api_key=None,
     runtime={},
     io=io(_normalize_aux_provider="custom", _normalize_api_key=None,
           _to_openai_base_url="https://x/v1",
           base_url_origin=["https", "x", 443], _normalize_main_runtime={},
           _read_main_base_url="https://y/v1", _read_main_api_key="",
           declared_custom_credential_identity=seq(None, None),
           _runtime_cache_discriminator="disc"))  # borrowed None

# ── _cached_semaphore ───────────────────────────────────────────────────────
case("_cached_semaphore", store={}, key="k", limit=4, io=io(factory={"sem": 1}))
case("_cached_semaphore", store={"k": [4, {"sem": 1}]}, key="k", limit=4)  # cached, no factory
case("_cached_semaphore", store={"k": [2, {"sem": 1}]}, key="k", limit=4,
     io=io(factory={"sem": 2}))  # limit changed -> rebuild
case("_cached_semaphore", store={"k": None}, key="k", limit=4, io=io(factory={"sem": 1}))
case("_cached_semaphore", store={}, key="k", limit=4, io=io(factory=raise_("RuntimeError", "f")))
case("_cached_semaphore", store={"k": [4, {"sem": 1}]}, key="k", limit=8,
     io=io(factory={"sem": 8}))

# ── _current_event_loop ─────────────────────────────────────────────────────
case("_current_event_loop", io=io(**{"asyncio.get_event_loop": {"loop": 1}}))
case("_current_event_loop", io=io(**{"asyncio.get_event_loop": raise_("RuntimeError", "no loop")}))
case("_current_event_loop", io=io(**{"asyncio.get_event_loop": None}))
case("_current_event_loop", io=io(**{"asyncio.get_event_loop": raise_("ValueError", "x")}))
case("_current_event_loop", io=io(**{"asyncio.get_event_loop": raise_("SystemExit")}))

# ── _async_client_streams_internally ────────────────────────────────────────
case("_async_client_streams_internally", client={"kind": "AsyncCodexAuxiliaryClient"})
case("_async_client_streams_internally", client={"kind": "AsyncAnthropicAuxiliaryClient"})
case("_async_client_streams_internally", client={"kind": "AsyncBedrockAuxiliaryClient"})
case("_async_client_streams_internally", client={"kind": "CodexAuxiliaryClient"})
case("_async_client_streams_internally", client={"kind": "OpenAI"})
case("_async_client_streams_internally", client=None)
case("_async_client_streams_internally", client={"kind": "SubAsyncCodex"})  # subclass

# ── _refresh_codex_credentials ──────────────────────────────────────────────
case("_refresh_codex_credentials",
     io=io(resolve_codex_runtime_credentials={"api_key": "k"}, _creds_have_api_key=True))
case("_refresh_codex_credentials",
     io=io(resolve_codex_runtime_credentials={}, _creds_have_api_key=False))
case("_refresh_codex_credentials",
     io=io(resolve_codex_runtime_credentials=raise_("RuntimeError", "r")))
case("_refresh_codex_credentials",
     io=io(resolve_codex_runtime_credentials={"api_key": "k"},
           _creds_have_api_key=raise_("ValueError", "c")))
case("_refresh_codex_credentials",
     io=io(resolve_codex_runtime_credentials=None, _creds_have_api_key=False))

# ── _refresh_nous_credentials ───────────────────────────────────────────────
case("_refresh_nous_credentials",
     io=io(env_float=15, resolve_nous_runtime_credentials={"api_key": "k"}, _creds_have_api_key=True))
case("_refresh_nous_credentials",
     io=io(env_float=30, resolve_nous_runtime_credentials={}, _creds_have_api_key=False))
case("_refresh_nous_credentials",
     io=io(env_float=15, resolve_nous_runtime_credentials=raise_("RuntimeError", "r")))
case("_refresh_nous_credentials",
     io=io(env_float=raise_("ValueError", "ef")))
case("_refresh_nous_credentials",
     io=io(env_float=15, resolve_nous_runtime_credentials={"api_key": "k"},
           _creds_have_api_key=raise_("SystemExit")))

# ── _refresh_anthropic_credentials ──────────────────────────────────────────
case("_refresh_anthropic_credentials", failed_api_key="")  # empty -> False
case("_refresh_anthropic_credentials", failed_api_key="tok",
     io=io(load_pool={"p": 1}, **{"pool.entry_id_for_api_key": "eid",
                                   "pool.try_refresh_matching": {"r": 1}}))
case("_refresh_anthropic_credentials", failed_api_key="tok",
     io=io(load_pool={"p": 1}, **{"pool.entry_id_for_api_key": "eid",
                                   "pool.try_refresh_matching": None}))  # None -> False
case("_refresh_anthropic_credentials", failed_api_key="tok",
     io=io(load_pool={"p": 1}, **{"pool.entry_id_for_api_key": None},
           read_claude_code_credentials={"accessToken": "tok", "refreshToken": "rt"},
           _refresh_oauth_token={"new": 1}))
case("_refresh_anthropic_credentials", failed_api_key="tok",
     io=io(load_pool={"p": 1}, **{"pool.entry_id_for_api_key": None},
           read_claude_code_credentials={"accessToken": "other", "refreshToken": "rt"}))
case("_refresh_anthropic_credentials", failed_api_key="tok",
     io=io(load_pool={"p": 1}, **{"pool.entry_id_for_api_key": None},
           read_claude_code_credentials=None))  # creds None -> False
case("_refresh_anthropic_credentials", failed_api_key="tok",
     io=io(load_pool=None))  # pool None -> .entry_id AttributeError

# ── _refresh_xai_oauth_credentials ──────────────────────────────────────────
case("_refresh_xai_oauth_credentials",
     io=io(load_pool={"p": 1}, **{"pool.has_credentials": True, "pool.select": None,
                                   "pool.try_refresh_current": {"runtime_api_key": "new"}}))
case("_refresh_xai_oauth_credentials",
     io=io(load_pool={"p": 1}, **{"pool.has_credentials": True, "pool.select": None,
                                   "pool.try_refresh_current": None},
           resolve_xai_oauth_runtime_credentials={"api_key": "k"}, _creds_have_api_key=True))
case("_refresh_xai_oauth_credentials",
     io=io(load_pool=None, resolve_xai_oauth_runtime_credentials={"api_key": "k"},
           _creds_have_api_key=True))
case("_refresh_xai_oauth_credentials",
     io=io(load_pool={"p": 1}, **{"pool.has_credentials": False},
           resolve_xai_oauth_runtime_credentials={}, _creds_have_api_key=False))
case("_refresh_xai_oauth_credentials",
     io=io(load_pool={"p": 1}, **{"pool.has_credentials": True, "pool.select": None,
                                   "pool.try_refresh_current": {"runtime_api_key": "  "}},
           resolve_xai_oauth_runtime_credentials={"api_key": "k"}, _creds_have_api_key=True))
case("_refresh_xai_oauth_credentials",
     io=io(load_pool=raise_("SystemExit")))

# ── _refresh_vertex_credentials ─────────────────────────────────────────────
case("_refresh_vertex_credentials", io=io(get_vertex_config=["tok", "https://b"]))
case("_refresh_vertex_credentials", io=io(get_vertex_config=["", "https://b"]))
case("_refresh_vertex_credentials", io=io(get_vertex_config=["tok", ""]))
case("_refresh_vertex_credentials", io=io(get_vertex_config=[None, "https://b"]))
case("_refresh_vertex_credentials", io=io(get_vertex_config=["tok", None]))
case("_refresh_vertex_credentials", io=io(get_vertex_config=raise_("RuntimeError", "v")))

# ── _deepinfra_strict_vision_backend ────────────────────────────────────────
case("_deepinfra_strict_vision_backend", model="vision-m",
     io=io(resolve_provider_client=[{"kind": "C"}, "vision-m"]))
case("_deepinfra_strict_vision_backend", model=None,
     io=io(_resolve_provider_vision_default="dv", resolve_provider_client=[{"kind": "C"}, "dv"]))
case("_deepinfra_strict_vision_backend", model=None,
     io=io(_resolve_provider_vision_default=None))  # no vision model -> (None,None)
case("_deepinfra_strict_vision_backend", model="",
     io=io(_resolve_provider_vision_default="dv", resolve_provider_client=[{"kind": "C"}, "dv"]))
case("_deepinfra_strict_vision_backend", model="m",
     io=io(resolve_provider_client=raise_("RuntimeError", "rpc")))

# ── _finalize_vision_client ─────────────────────────────────────────────────
case("_finalize_vision_client", resolved_provider="nous", sync_client=None,
     default_model="d", resolved_model=None, async_mode=False)
case("_finalize_vision_client", resolved_provider="nous", sync_client={"kind": "C"},
     default_model="d", resolved_model=None, async_mode=False)
case("_finalize_vision_client", resolved_provider="nous", sync_client={"kind": "C"},
     default_model="d", resolved_model="override", async_mode=False)
case("_finalize_vision_client", resolved_provider="nous", sync_client={"kind": "C"},
     default_model="d", resolved_model=None, async_mode=True,
     io=io(_to_async_client=[{"kind": "AC"}, "d"]))
case("_finalize_vision_client", resolved_provider="nous", sync_client={"kind": "C"},
     default_model="d", resolved_model="ov", async_mode=True,
     io=io(_to_async_client=[{"kind": "AC"}, "ov"]))

# ── _vision_auto_route ──────────────────────────────────────────────────────
case("_vision_auto_route", runtime={"provider": "nous", "model": "m"}, resolved_model=None,
     resolved_api_mode=None, async_mode=False,
     io=io(_vision_main_provider_client=[{"kind": "C"}, "dm"]))
case("_vision_auto_route", runtime={}, resolved_model=None, resolved_api_mode=None,
     async_mode=False,
     io=io(_read_main_provider="openrouter", _read_main_model="m",
           _vision_main_provider_client=[None, None],
           _resolve_strict_vision_backend=[{"kind": "C"}, "dv"]))
case("_vision_auto_route", runtime={"provider": "moa", "model": "preset"}, resolved_model=None,
     resolved_api_mode=None, async_mode=False,
     io=io(_resolve_moa_aggregator=["agg", "aggmodel"],
           _vision_main_provider_client=[{"kind": "C"}, "dm"]))
case("_vision_auto_route", runtime={"provider": "auto"}, resolved_model=None,
     resolved_api_mode=None, async_mode=False,
     io=io(_read_main_provider="", _read_main_model="",
           _resolve_strict_vision_backend=seq([None, None], [{"kind": "C"}, "dv"])))
case("_vision_auto_route", runtime={"provider": "nous", "model": "m"}, resolved_model="ov",
     resolved_api_mode=None, async_mode=True,
     io=io(_vision_main_provider_client=[{"kind": "C"}, "dm"],
           _to_async_client=[{"kind": "AC"}, "ov"]))
case("_vision_auto_route", runtime={"provider": "nous", "model": "m"}, resolved_model=None,
     resolved_api_mode=None, async_mode=False,
     io=io(_vision_main_provider_client=[None, None],
           _resolve_strict_vision_backend=seq([None, None], [None, None], [None, None])))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    # validate: every func >=5 cases
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    bad = {f: n for f, n in counts.items() if n < 5}
    if bad:
        print(f"GEN: funcs with <5 cases: {bad}", file=sys.stderr)
        sys.exit(1)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"GEN auxiliary_client_flow2: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")


if __name__ == "__main__":
    main()