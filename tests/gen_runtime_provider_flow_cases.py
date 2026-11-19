#!/usr/bin/env python3
"""Generate tests/fixtures/runtime_provider_flow.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Representation
scope: ASCII text, scalar str()/repr() values, registry/provider/entry
descriptors are non-empty objects or null, no integral-float JSON literals
(documented in the oracle/header). Every ported function gets >=5 cases.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "runtime_provider_flow.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


def raises(kind, message, **extra):
    d = {"type": kind, "message": message}
    d.update(extra)
    return d


# ── _resolves_to_custom ───────────────────────────────────────────────────
add("_resolves_to_custom", name="ollama", io={"auth.resolve_provider": "custom"})
add("_resolves_to_custom", name="openai", io={"auth.resolve_provider": "openai"})
add("_resolves_to_custom", name="vllm", io={"auth.resolve_provider": script(raise_spec=raises("ValueError", "bad"))})
add("_resolves_to_custom", name="x", io={"auth.resolve_provider": script(raise_spec=raises("SystemExit", "halt", code="halt"))})
add("_resolves_to_custom", name="", io={"auth.resolve_provider": "custom"})
add("_resolves_to_custom", name="weird", io={"auth.resolve_provider": None})
add("_resolves_to_custom", name="kbd", io={"auth.resolve_provider": script(raise_spec=raises("KeyboardInterrupt", "ki"))})

# ── _cfg_provider ─────────────────────────────────────────────────────────
add("_cfg_provider", model_cfg={"provider": " OpenAI "})
add("_cfg_provider", model_cfg={"provider": None})
add("_cfg_provider", model_cfg={})
add("_cfg_provider", model_cfg={"provider": 0})
add("_cfg_provider", model_cfg={"provider": 5})
add("_cfg_provider", model_cfg={"provider": True})
add("_cfg_provider", model_cfg={"provider": "  CUSTOM  "})
add("_cfg_provider", model_cfg={"provider": "\tX\n"})

# ── _same_registered_provider ─────────────────────────────────────────────
add("_same_registered_provider", provider="openai", configured_provider="openai", io={})
add("_same_registered_provider", provider="openai", configured_provider="openai-codex",
    io={"PROVIDER_REGISTRY.get": {"id": "openai"}})
add("_same_registered_provider", provider="openai", configured_provider="anthropic",
    io={"PROVIDER_REGISTRY.get": [{"id": "openai"}, {"id": "anthropic"}]})
add("_same_registered_provider", provider="openai", configured_provider="ghost",
    io={"PROVIDER_REGISTRY.get": [{"id": "openai"}, None]})
add("_same_registered_provider", provider="ghost", configured_provider="openai",
    io={"PROVIDER_REGISTRY.get": [None, {"id": "openai"}]})
add("_same_registered_provider", provider="a", configured_provider="b", io={"PROVIDER_REGISTRY.get": None})
add("_same_registered_provider", provider="openai", configured_provider="openai-codex",
    io={"PROVIDER_REGISTRY.get": script(raise_spec=raises("RuntimeError", "reg boom"))})

# ── _config_base_url_for_provider ─────────────────────────────────────────
add("_config_base_url_for_provider", provider="openai",
    model_cfg={"provider": "openai", "base_url": " https://api.openai.com/v1/ "}, io={})
add("_config_base_url_for_provider", provider="anthropic",
    model_cfg={"provider": "openai", "base_url": "https://api.openai.com"},
    io={"PROVIDER_REGISTRY.get": [{"id": "anthropic"}, {"id": "openai"}]})
add("_config_base_url_for_provider", provider="actual",
    model_cfg={"provider": "Actual", "base_url": "http://localhost:8080/"},
    io={"_models.normalize_provider": "actual"})
add("_config_base_url_for_provider", provider="actual",
    model_cfg={"provider": "openai", "base_url": "http://x/"},
    io={"_models.normalize_provider": "other", "PROVIDER_REGISTRY.get": [{"id": "actual"}, {"id": "other"}]})
add("_config_base_url_for_provider", provider="openai", model_cfg={"provider": "openai", "base_url": None}, io={})
add("_config_base_url_for_provider", provider="openai", model_cfg={"provider": "openai", "base_url": 0}, io={})
add("_config_base_url_for_provider", provider="openai", model_cfg={"provider": "openai"}, io={})
add("_config_base_url_for_provider", provider="openai",
    model_cfg={"provider": "openai", "base_url": "https://x///"}, io={})

# ── _on_declared_anthropic_endpoint ───────────────────────────────────────
add("_on_declared_anthropic_endpoint", provider="minimax", base_url="https://api.minimax.io/anthropic",
    io={"get_provider": None, "base_url_path": ""})
add("_on_declared_anthropic_endpoint", provider="minimax", base_url="https://api.minimax.io/anthropic",
    io={"get_provider": {"base_url": "https://api.minimax.io/anthropic/v1"},
        "base_url_path": ["/anthropic/v1", "/anthropic"],
        "base_url_hostname": "api.minimax.io", "base_url_host_matches": True})
add("_on_declared_anthropic_endpoint", provider="minimax", base_url="https://evil.test/anthropic",
    io={"get_provider": {"base_url": "https://api.minimax.io/anthropic"},
        "base_url_path": "/anthropic", "base_url_hostname": "api.minimax.io", "base_url_host_matches": False})
add("_on_declared_anthropic_endpoint", provider="anthropic", base_url="https://api.anthropic.com",
    io={"get_provider": {"base_url": "https://api.anthropic.com"}, "base_url_path": ""})
add("_on_declared_anthropic_endpoint", provider="minimax", base_url=None,
    io={"get_provider": {"base_url": "https://api.minimax.io/anthropic"}, "base_url_path": "/anthropic"})
add("_on_declared_anthropic_endpoint", provider="minimax", base_url="https://api.minimax.io/anthropic/v1",
    io={"get_provider": {"base_url": "https://api.minimax.io/anthropic"},
        "base_url_path": ["/anthropic", "/anthropic/v1"],
        "base_url_hostname": "api.minimax.io", "base_url_host_matches": True})
add("_on_declared_anthropic_endpoint", provider="minimax", base_url="https://api.minimax.io/other",
    io={"get_provider": {"base_url": "https://api.minimax.io/anthropic"},
        "base_url_path": ["/anthropic", "/other"],
        "base_url_hostname": "api.minimax.io", "base_url_host_matches": True})
add("_on_declared_anthropic_endpoint", provider="minimax", base_url="https://api.minimax.io",
    io={"get_provider": {"base_url": "https://api.minimax.io/anthropic"},
        "base_url_path": ["/anthropic", ""],
        "base_url_hostname": "api.minimax.io", "base_url_host_matches": True})
add("_on_declared_anthropic_endpoint", provider="minimax", base_url="https://api.minimax.io/anthropic",
    io={"get_provider": script(raise_spec=raises("RuntimeError", "catalog boom"))})

# ── _azure_inferred_api_mode ──────────────────────────────────────────────
add("_azure_inferred_api_mode", effective_model="", api_mode="chat_completions", io={})
add("_azure_inferred_api_mode", effective_model="gpt-5", api_mode="anthropic_messages", io={})
add("_azure_inferred_api_mode", effective_model="gpt-5", api_mode="chat_completions",
    io={"_models.azure_foundry_model_api_mode": "codex_responses"})
add("_azure_inferred_api_mode", effective_model="gpt-5", api_mode="chat_completions",
    io={"_models.azure_foundry_model_api_mode": None})
add("_azure_inferred_api_mode", effective_model="gpt-5", api_mode="chat_completions",
    io={"_models.azure_foundry_model_api_mode": ""})
add("_azure_inferred_api_mode", effective_model="gpt-5", api_mode="chat_completions",
    io={"_models.azure_foundry_model_api_mode": script(raise_spec=raises("ValueError", "bad model"))})
add("_azure_inferred_api_mode", effective_model="gpt-5", api_mode="chat_completions",
    io={"_models.azure_foundry_model_api_mode": script(raise_spec=raises("SystemExit", "halt", code="halt"))})

# ── _runtime ──────────────────────────────────────────────────────────────
add("_runtime", provider="openai", api_mode="codex_responses", base_url="https://api.openai.com/v1",
    api_key="k", extra={"source": "explicit"}, io={"is_actual_route": False})
add("_runtime", provider="actual", api_mode="codex_responses", base_url="http://localhost:8080",
    api_key="", extra={"source": "env", "requested_provider": "actual"},
    io={"is_actual_route": True, "normalize_actual_base_url": "http://127.0.0.1:8080"})
add("_runtime", provider="x", api_mode="m", base_url=None, api_key=None, extra={}, io={"is_actual_route": False})
add("_runtime", provider="a", api_mode="m", base_url="u", api_key="k",
    extra={"provider": "b", "source": "s"}, io={"is_actual_route": False})
add("_runtime", provider="openai", api_mode="m", base_url="u", api_key="k", extra={},
    io={"is_actual_route": script(raise_spec=raises("RuntimeError", "route boom"))})
add("_runtime", provider="actual", api_mode="m", base_url="u", api_key="k", extra={},
    io={"is_actual_route": True, "normalize_actual_base_url": script(raise_spec=raises("ValueError", "norm boom"))})

# ── _creds_fallback ───────────────────────────────────────────────────────
add("_creds_fallback", api_key="explicit", explicit_base_url="https://e", base_url="https://b",
    expiry=123, expiry_key="expires_at", resolve_name="resolve_codex_runtime_credentials", io={})
add("_creds_fallback", api_key="", explicit_base_url="https://e", base_url="https://b",
    expiry=None, expiry_key="last_refresh", resolve_name="resolve_x",
    io={"resolve_x": {"api_key": "stored", "base_url": "https://s/", "last_refresh": 999}})
add("_creds_fallback", api_key=None, explicit_base_url=None, base_url="https://b",
    expiry=None, expiry_key="expires_at", resolve_name="resolve_y",
    io={"resolve_y": {"api_key": "k2", "base_url": "https://s/"}})
add("_creds_fallback", api_key="", explicit_base_url="", base_url="https://b",
    expiry=None, expiry_key="e", resolve_name="resolve_z",
    io={"resolve_z": {"api_key": "k3", "base_url": ""}})
add("_creds_fallback", api_key=0, explicit_base_url=None, base_url="https://b",
    expiry=None, expiry_key="e", resolve_name="resolve_w",
    io={"resolve_w": {"api_key": "", "base_url": "https://s"}})
add("_creds_fallback", api_key="", explicit_base_url=None, base_url="https://b",
    expiry=None, expiry_key="e", resolve_name="resolve_boom",
    io={"resolve_boom": script(raise_spec=raises("RuntimeError", "creds boom"))})
add("_creds_fallback", api_key="k", explicit_base_url=None, base_url=None,
    expiry=None, expiry_key="e", resolve_name="resolve_unused", io={})

# ── _azure_anthropic_env_key ──────────────────────────────────────────────
add("_azure_anthropic_env_key", model_cfg={"key_env": "MY_KEY"}, io={"get_secret_str": "tok"})
add("_azure_anthropic_env_key", model_cfg={"key_env": "MY_KEY", "api_key_env": "OTHER"},
    io={"get_secret_str": ["", "tok2"]})
add("_azure_anthropic_env_key", model_cfg={"api_key": "inline"}, io={})
add("_azure_anthropic_env_key", model_cfg={}, io={"get_secret_str": ["az", "an"]})
add("_azure_anthropic_env_key", model_cfg={}, io={"get_secret_str": ["", "an"]})
add("_azure_anthropic_env_key", model_cfg={}, io={"get_secret_str": ""})
add("_azure_anthropic_env_key", model_cfg={"key_env": "  "}, io={"get_secret_str": ""})
add("_azure_anthropic_env_key", model_cfg={"api_key": 0}, io={"get_secret_str": "x"})
add("_azure_anthropic_env_key", model_cfg={"key_env": "K"},
    io={"get_secret_str": script(raise_spec=raises("RuntimeError", "secret boom"))})

# ── openai_env_key_authoritative_for ──────────────────────────────────────
add("openai_env_key_authoritative_for", base_url="https://api.openai.com/v1", env_openai_base="",
    io={"base_url_host_matches": True})
add("openai_env_key_authoritative_for", base_url="https://x.openai.azure.com", env_openai_base="",
    io={"base_url_host_matches": [False, True]})
add("openai_env_key_authoritative_for", base_url="https://proxy.test", env_openai_base="https://proxy.test/",
    io={"base_url_host_matches": False})
add("openai_env_key_authoritative_for", base_url="https://proxy.test", env_openai_base="https://other.test",
    io={"base_url_host_matches": False})
add("openai_env_key_authoritative_for", base_url="https://proxy.test", env_openai_base="",
    io={"base_url_host_matches": False})
add("openai_env_key_authoritative_for", base_url=None, env_openai_base="https://p",
    io={"base_url_host_matches": False})
add("openai_env_key_authoritative_for", base_url=" https://p/ ", env_openai_base=" https://p/ ",
    io={"base_url_host_matches": False})
add("openai_env_key_authoritative_for", base_url="https://api.openai.com", env_openai_base="",
    io={"base_url_host_matches": script(raise_spec=raises("RuntimeError", "host boom"))})

# ── _host_gated_env_key_candidates ────────────────────────────────────────
add("_host_gated_env_key_candidates", base_url="https://api.openai.com/v1", ollama=False,
    io={"get_secret_str": ["ob", "ok"], "base_url_host_matches": [True, False], "_host_derived_api_key": ""})
add("_host_gated_env_key_candidates", base_url="https://api.ollama.com", ollama=True,
    io={"get_secret_str": ["", "oll"], "base_url_host_matches": [False, False, True, False], "_host_derived_api_key": ""})
add("_host_gated_env_key_candidates", base_url="https://openrouter.ai/api/v1", ollama=False,
    io={"get_secret_str": ["ob", "or"], "base_url_host_matches": [False, False, True], "_host_derived_api_key": ""})
add("_host_gated_env_key_candidates", base_url="https://api.deepseek.com", ollama=False,
    io={"get_secret_str": "", "base_url_host_matches": False, "_host_derived_api_key": "ds"})
add("_host_gated_env_key_candidates", base_url="https://api.ollama.com", ollama=True,
    io={"get_secret_str": "", "base_url_host_matches": False, "_host_derived_api_key": ""})
add("_host_gated_env_key_candidates", base_url="https://api.openai.com", ollama=False,
    io={"get_secret_str": script(raise_spec=raises("RuntimeError", "secret boom")),
        "base_url_host_matches": False, "_host_derived_api_key": ""})
add("_host_gated_env_key_candidates", base_url="https://api.deepseek.com", ollama=False,
    io={"get_secret_str": "", "base_url_host_matches": False,
        "_host_derived_api_key": script(raise_spec=raises("RuntimeError", "derived boom"))})

# ── _openrouter_should_use_pool ───────────────────────────────────────────
add("_openrouter_should_use_pool", requested_provider="openrouter",
    model_cfg={"provider": "openrouter", "base_url": "https://openrouter.ai/api/v1"},
    explicit_api_key=None, explicit_base_url=None, io={"get_secret_str": "", "base_url_host_matches": True})
add("_openrouter_should_use_pool", requested_provider="openrouter",
    model_cfg={"provider": "custom", "base_url": "https://mirror.test"},
    explicit_api_key=None, explicit_base_url=None, io={"get_secret_str": ""})
add("_openrouter_should_use_pool", requested_provider="auto", model_cfg={},
    explicit_api_key="k", explicit_base_url=None, io={"get_secret_str": ""})
add("_openrouter_should_use_pool", requested_provider="openai", model_cfg={},
    explicit_api_key=None, explicit_base_url=None, io={"get_secret_str": ""})
add("_openrouter_should_use_pool", requested_provider="openrouter",
    model_cfg={"provider": "openrouter", "base_url": "https://mirror.test"},
    explicit_api_key=None, explicit_base_url=None, io={"get_secret_str": "", "base_url_host_matches": False})
add("_openrouter_should_use_pool", requested_provider="openrouter", model_cfg={},
    explicit_api_key=None, explicit_base_url="https://e", io={"get_secret_str": ""})
add("_openrouter_should_use_pool", requested_provider="auto",
    model_cfg={"provider": "auto", "base_url": "https://local"},
    explicit_api_key=None, explicit_base_url=None, io={"get_secret_str": "envurl"})
add("_openrouter_should_use_pool", requested_provider="openrouter", model_cfg={},
    explicit_api_key=None, explicit_base_url=None,
    io={"get_secret_str": script(raise_spec=raises("RuntimeError", "secret boom"))})

# ── _raise_if_provider_disabled ───────────────────────────────────────────
add("_raise_if_provider_disabled", requested_provider="openai",
    io={"load_config": {"providers": {"openai": {"enabled": False}}}, "_config_mod.is_provider_enabled": False})
add("_raise_if_provider_disabled", requested_provider="openai",
    io={"load_config": {"providers": {"openai": {"enabled": True}}}, "_config_mod.is_provider_enabled": True})
add("_raise_if_provider_disabled", requested_provider="openai", io={"load_config": {"providers": {}}})
add("_raise_if_provider_disabled", requested_provider="openai", io={"load_config": {}})
add("_raise_if_provider_disabled", requested_provider="openai",
    io={"load_config": {"providers": {"openai": "notdict"}}})
add("_raise_if_provider_disabled", requested_provider="openai", io={"load_config": None})
add("_raise_if_provider_disabled", requested_provider="weird'name",
    io={"load_config": {"providers": {"weird'name": {"enabled": False}}}, "_config_mod.is_provider_enabled": False})
add("_raise_if_provider_disabled", requested_provider="a\tb",
    io={"load_config": {"providers": {"a\tb": {"enabled": False}}}, "_config_mod.is_provider_enabled": False})
add("_raise_if_provider_disabled", requested_provider="openai",
    io={"load_config": {"providers": {"openai": {"enabled": False}}},
        "_config_mod.is_provider_enabled": script(raise_spec=raises("RuntimeError", "enabled boom"))})

# ── _raise_for_credentialless_bare_custom ─────────────────────────────────
add("_raise_for_credentialless_bare_custom", requested_provider="custom", runtime={"provider": "custom", "api_key": ""})
add("_raise_for_credentialless_bare_custom", requested_provider="custom", runtime={"provider": "custom", "api_key": "k"})
add("_raise_for_credentialless_bare_custom", requested_provider="custom", runtime={"provider": "custom"})
add("_raise_for_credentialless_bare_custom", requested_provider="custom", runtime={"provider": "openai", "api_key": ""})
add("_raise_for_credentialless_bare_custom", requested_provider="openrouter", runtime={"provider": "custom", "api_key": ""})
add("_raise_for_credentialless_bare_custom", requested_provider="custom", runtime={"provider": "custom", "api_key": None})
add("_raise_for_credentialless_bare_custom", requested_provider="custom", runtime={"provider": "custom", "api_key": 0})
add("_raise_for_credentialless_bare_custom", requested_provider="custom",
    runtime={"provider": "custom", "api_key": "no-key-required"})

# ── _tag ──────────────────────────────────────────────────────────────────
add("_tag", runtime={"provider": "openai"}, requested_provider="openai")
add("_tag", runtime={}, requested_provider="x")
add("_tag", runtime=None, requested_provider="x")
add("_tag", runtime={"provider": "custom", "requested_provider": "old"}, requested_provider="new")
add("_tag", runtime={"api_key": "k"}, requested_provider="auto")
add("_tag", runtime={"provider": "x", "requested_provider": "keep"}, requested_provider="keep")

# ── _local_endpoint_bypass ────────────────────────────────────────────────
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": {"provider": "auto", "base_url": "http://localhost:11434"},
        "base_url_host_matches": False, "_resolve_openrouter_runtime": {"provider": "openrouter", "api_key": "k"}})
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": {"provider": "openai", "base_url": "http://localhost"}})
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": {"provider": "auto", "base_url": ""}})
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": {"provider": "auto", "base_url": "https://openrouter.ai/api/v1"},
        "base_url_host_matches": True})
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": {"provider": "", "base_url": "https://api.anthropic.com"},
        "base_url_host_matches": [False, True]})
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": {"provider": "auto", "base_url": "http://localhost"},
        "base_url_host_matches": False, "_resolve_openrouter_runtime": None})
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": script(raise_spec=raises("RuntimeError", "cfg boom"))})
add("_local_endpoint_bypass", requested_provider="auto", explicit_api_key=None, explicit_base_url=None,
    io={"_get_model_config": {"provider": "auto", "base_url": "http://localhost"},
        "base_url_host_matches": False,
        "_resolve_openrouter_runtime": script(raise_spec=raises("RuntimeError", "or boom"))})

# ── _resolve_requested_shortcuts ──────────────────────────────────────────
add("_resolve_requested_shortcuts", requested_provider="moa", explicit_api_key=None,
    explicit_base_url=None, target_model=None, io={"is_actual_route": False})
add("_resolve_requested_shortcuts", requested_provider="anthropic", explicit_api_key="azkey",
    explicit_base_url="https://x.openai.azure.com/", target_model=None,
    io={"base_url_host_matches": True, "is_actual_route": False})
add("_resolve_requested_shortcuts", requested_provider="anthropic", explicit_api_key="",
    explicit_base_url="https://x.azure.com", target_model=None,
    io={"base_url_host_matches": True, "get_secret_str": "envkey", "is_actual_route": False})
add("_resolve_requested_shortcuts", requested_provider="anthropic", explicit_api_key=None,
    explicit_base_url="https://x.azure.com", target_model=None,
    io={"base_url_host_matches": True, "get_secret_str": ""})
add("_resolve_requested_shortcuts", requested_provider="anthropic", explicit_api_key=None,
    explicit_base_url="https://api.anthropic.com", target_model=None, io={"base_url_host_matches": False})
add("_resolve_requested_shortcuts", requested_provider="azure-foundry", explicit_api_key="k",
    explicit_base_url="https://f", target_model="m",
    io={"_get_model_config": {"provider": "azure-foundry"},
        "_resolve_azure_foundry_runtime": {"provider": "azure-foundry", "api_mode": "chat_completions"}})
add("_resolve_requested_shortcuts", requested_provider="vertex", explicit_api_key=None,
    explicit_base_url=None, target_model=None,
    io={"_resolve_vertex_runtime": {"provider": "vertex", "api_mode": "chat_completions"}})
add("_resolve_requested_shortcuts", requested_provider="google-vertex", explicit_api_key=None,
    explicit_base_url=None, target_model=None, io={"_resolve_vertex_runtime": {"provider": "vertex"}})
add("_resolve_requested_shortcuts", requested_provider="openai", explicit_api_key=None,
    explicit_base_url=None, target_model=None, io={})
add("_resolve_requested_shortcuts", requested_provider="azure-foundry", explicit_api_key=None,
    explicit_base_url=None, target_model=None,
    io={"_get_model_config": script(raise_spec=raises("RuntimeError", "cfg boom"))})
add("_resolve_requested_shortcuts", requested_provider="moa", explicit_api_key=None,
    explicit_base_url=None, target_model=None,
    io={"is_actual_route": True, "normalize_actual_base_url": "moa://norm"})

# ── _refresh_nous_pool_entry ──────────────────────────────────────────────
add("_refresh_nous_pool_entry",
    entry={"agent_key": "jwt", "agent_key_expires_at": 9999, "scope": "s", "runtime_api_key": "k"},
    pool_api_key="k", io={"env_int": 1800, "_agent_key_is_usable": True})
add("_refresh_nous_pool_entry",
    entry={"agent_key": None, "agent_key_expires_at": 0, "scope": "s", "runtime_api_key": "k"},
    pool_api_key="k",
    io={"env_int": 1800, "_agent_key_is_usable": [False, True], "logger.debug": None,
        "pool.try_refresh_current": {"agent_key": "new", "runtime_api_key": "newk", "access_token": "at"}})
add("_refresh_nous_pool_entry", entry={"runtime_api_key": "k"}, pool_api_key="k",
    io={"env_int": 1800, "_agent_key_is_usable": False, "logger.debug": [None, None],
        "pool.try_refresh_current": None})
add("_refresh_nous_pool_entry", entry={"runtime_api_key": "k"}, pool_api_key="k",
    io={"env_int": 1800, "_agent_key_is_usable": False, "logger.debug": [None, None, None],
        "pool.try_refresh_current": script(raise_spec=raises("RuntimeError", "boom"))})
add("_refresh_nous_pool_entry", entry={"runtime_api_key": "k"}, pool_api_key="",
    io={"env_int": 1800, "_agent_key_is_usable": False, "logger.debug": [None, None],
        "pool.try_refresh_current": None})
add("_refresh_nous_pool_entry", entry={"runtime_api_key": "k"}, pool_api_key="k",
    io={"env_int": 30, "_agent_key_is_usable": True})
add("_refresh_nous_pool_entry", entry={"runtime_api_key": "k"}, pool_api_key="k",
    io={"env_int": 1800, "_agent_key_is_usable": False, "logger.debug": None,
        "pool.try_refresh_current": script(raise_spec=raises("SystemExit", "halt", code="halt"))})
add("_refresh_nous_pool_entry", entry={"runtime_api_key": "k"}, pool_api_key="k",
    io={"env_int": script(raise_spec=raises("ValueError", "ttl boom"))})
add("_refresh_nous_pool_entry", entry={"access_token": "at", "runtime_api_key": None}, pool_api_key="k",
    io={"env_int": 1800, "_agent_key_is_usable": [False, True], "logger.debug": None,
        "pool.try_refresh_current": {"access_token": "at2", "runtime_api_key": ""}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for f, n in sorted(counts.items()):
        print(f"  {f}: {n}")
    assert all(n >= 5 for n in counts.values()), "every port needs >=5 cases"
    assert len(counts) == 18, f"expected 18 ports, got {len(counts)}"


if __name__ == "__main__":
    main()