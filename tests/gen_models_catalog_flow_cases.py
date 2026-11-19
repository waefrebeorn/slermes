#!/usr/bin/env python3
"""Generate tests/fixtures/models_catalog_flow.jsonl — decision-sequence cases
for the hermes_cli/models.py catalog/selection FLOW port.

Each case: {"func", "in": {<args>, "env": {tables/constants}, "io": {scripts}}}.
Scripts: single value reused, {"$seq": [...]} consumed per call, or
{"__raise__": cls, "msg"} to raise. Absent seam key -> None.

--self-check imports the oracle module and drives every case through the LIVE
reference driver (no C comparison) to validate seeding/scripts before the gate.
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "models_catalog_flow.jsonl"

BASE_ENV = {
    "aliases": {"ollama": "custom", "claude": "anthropic", "gpt": "openai", "zen": "opencode-zen"},
    "provider_models": {
        "anthropic": ["claude-sonnet-5", "claude-opus-5"],
        "openai": ["gpt-6-astra"],
        "openai-codex": ["gpt-6-codex"],
        "stepfun": ["step-3.7-flash"],
        "opencode-zen": ["ox-alpha"],
        "kimi": ["k3"],
    },
    "aggregators": ["nous", "openrouter", "ai-gateway", "kilocode"],
    "borrowed": ["opencode-zen", "opencode-go"],
    "live_first": ["opencode-zen", "opencode-go", "meta-ai"],
    "opencode_excluded": ["ox-alpha-free", "deepseek-v4-flash-free", "x-preview-f-free"],
    "ollama_ttl": 300,
    "cache_ttl": 3600,
    "stale_max": 604800,
    "fallback_ttl": 60,
    "fetchers": ["openrouter", "openai-codex", "nous", "stepfun", "gmi", "anthropic",
                 "ai-gateway", "deepinfra", "ollama-cloud", "openai", "openai-api",
                 "custom", "bedrock", "azure-foundry"],
}

CASES = []


def case(func, env=None, io=None, **args):
    e = dict(BASE_ENV)
    if env:
        e.update(env)
    CASES.append({"func": func, "in": dict(args, env=e, io=io or {})})


def seq(*vals):
    return {"$seq": list(vals)}


def raises(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


NOSTEP = {"provider_models": {k: v for k, v in BASE_ENV["provider_models"].items() if k != "stepfun"}}


def pm(**overrides):
    base = dict(BASE_ENV["provider_models"])
    base.update(overrides)
    return {"provider_models": base}


# ── _merge_unique ───────────────────────────────────────────────────────────
case("_merge_unique", primary=[], secondary=[])
case("_merge_unique", primary=["a", "b"], secondary=[])
case("_merge_unique", primary=["Alpha", "beta"], secondary=["alpha", "BETA", "gamma"])
case("_merge_unique", primary=["x"], secondary=["y", "Y", "z", "y"])
case("_merge_unique", dedup_key=True, primary=["kimi-k3"], secondary=["k3", "K3", "other"],
     io={"model_alias_canonical": seq("kimi-k3", "kimi-k3", "kimi-k3", "other")})
case("_merge_unique", dedup_key=True, primary=["a"], secondary=["A", "b"],
     io={"model_alias_canonical": raises("ValueError", "alias table down")})
case("_merge_unique", dedup_key=True, primary=[" k3 "], secondary=["k3"],
     io={"model_alias_canonical": seq("kimi-k3", "kimi-k3")})

# ── _configured_provider_ids ────────────────────────────────────────────────
case("_configured_provider_ids", io={"load_config": {"providers": {"Nous": {}, " openai ": {}, "": {}, "custom:local": {}}}})
case("_configured_provider_ids", io={"load_config": {"providers": [1]}})
case("_configured_provider_ids", io={"load_config": None})
case("_configured_provider_ids", io={"load_config": raises("OSError", "config unreadable")})
case("_configured_provider_ids", io={"load_config": {}})
case("_configured_provider_ids", io={"load_config": "x"})
case("_configured_provider_ids", io={"load_config": {"providers": {"NOUS": {}, "nous": {}}}})

# ── _resolve_provider_prefix ────────────────────────────────────────────────
case("_resolve_provider_prefix", model_name="gpt-6")
case("_resolve_provider_prefix", model_name="/model")
case("_resolve_provider_prefix", model_name="nous/")
case("_resolve_provider_prefix", model_name="nous/deepseek-v4-pro",
     io={"load_config": {"providers": {"nous": {}}}})
case("_resolve_provider_prefix", model_name="ollama/qwen3.5:4b",
     io={"load_config": {"providers": {"custom": {}}}})
case("_resolve_provider_prefix", model_name="ollama/qwen",
     io={"load_config": {"providers": {"ollama": {}, "custom": {}}}})
case("_resolve_provider_prefix", model_name="unknown/model",
     io={"load_config": {"providers": {"nous": {}}}})
case("_resolve_provider_prefix", model_name=" Nous / deepseek-v4-pro ",
     io={"load_config": {"providers": {"nous": {}}}})
case("_resolve_provider_prefix", model_name="nous/x", io={"load_config": {"providers": {}}})

# ── static_catalog ──────────────────────────────────────────────────────────
case("static_catalog", provider="anthropic")
case("static_catalog", provider="fireworks",
     io={"get_provider_profile": {"name": "fireworks", "static_models": ["fw-1", "fw-2"]}})
case("static_catalog", provider="")
case("static_catalog", provider="ghost", io={"get_provider_profile": None})
case("static_catalog", provider="ghost", io={"get_provider_profile": {"name": "ghost"}})
case("static_catalog", provider="empty", env=pm(empty=[]))
case("static_catalog", provider="ghost", io={"get_provider_profile": {"name": "ghost", "static_models": None}})
case("static_catalog", provider="ghost", io={"get_provider_profile": raises("OSError", "registry down")})

# ── static_catalog_providers ────────────────────────────────────────────────
case("static_catalog_providers", io={"list_providers": []})
case("static_catalog_providers", io={"list_providers": [{"name": "fireworks", "static_models": ["fw"]}]})
case("static_catalog_providers", io={"list_providers": [{"name": "x", "static_models": []}]})
case("static_catalog_providers", io={"list_providers": [{"name": "anthropic", "static_models": ["x"]}]})
case("static_catalog_providers", io={"list_providers": [{"name": "fw", "static_models": ["a"]}, {"name": "fw", "static_models": ["b"]}]})
case("static_catalog_providers", io={"list_providers": raises("OSError", "plugins down")})
case("static_catalog_providers", io={"list_providers": [{"static_models": ["a"]}]})
case("static_catalog_providers", io={"list_providers": [{"name": "x"}]})

# ── _static_catalog_matches ─────────────────────────────────────────────────
# pm order: anthropic, openai, openai-codex, stepfun, opencode-zen, kimi
# current anthropic -> step1 calls openai, openai-codex, stepfun, kimi; step2 zen, go
case("_static_catalog_matches", name="gpt-6-astra", current_provider="anthropic",
     io={"list_providers": [], "model_in_provider_catalog": seq(True, False, False, False, False, False)})
case("_static_catalog_matches", name="gpt-6-astra", current_provider="anthropic",
     io={"list_providers": [], "model_in_provider_catalog": seq(True, True, False, False, False, False)})
case("_static_catalog_matches", name="m", current_provider="custom",
     io={"list_providers": [], "model_in_provider_catalog": seq(False, True)})
case("_static_catalog_matches", name="m", current_provider="custom:local",
     io={"list_providers": [], "model_in_provider_catalog": seq(True, False)})
case("_static_catalog_matches", name="m", current_provider="opencode-zen",
     io={"list_providers": [], "model_in_provider_catalog": seq(False, False, False, False, False, True)})
case("_static_catalog_matches", name="m", current_provider="openrouter",
     env=pm(nous=["hermes-4"]),
     io={"list_providers": [], "model_in_provider_catalog": seq(False, True, False, False, False, False, False)})
case("_static_catalog_matches", name="m", current_provider="anthropic",
     io={"list_providers": [], "model_in_provider_catalog": seq(False, False, False, False, False, False)})
case("_static_catalog_matches", name="m", current_provider="anthropic",
     io={"list_providers": [], "model_in_provider_catalog": seq(False, raises("OSError", "catalog down"))})
case("_static_catalog_matches", name="m", current_provider="kimi",
     io={"list_providers": [], "model_in_provider_catalog": seq(False, False, False, False, True, False)})

# ── _detection_candidates ───────────────────────────────────────────────────
case("_detection_candidates", name="claude-sonnet-5", current_provider="openai",
     io={"detect_static_provider_for_model": ["anthropic", "claude-sonnet-5"], "list_providers": [],
         "model_in_provider_catalog": seq(False, False, False, False, False, False, False),
         "find_openrouter_slug": None})
case("_detection_candidates", name="gpt-6-astra", current_provider="anthropic",
     io={"detect_static_provider_for_model": ["openai", "gpt-6-astra"], "list_providers": [],
         "model_in_provider_catalog": seq(True, True, False, False, False, False, False),
         "find_openrouter_slug": None})
case("_detection_candidates", name="owned", current_provider="openai",
     io={"detect_static_provider_for_model": None, "model_in_provider_catalog": True})
case("_detection_candidates", name="x", current_provider="openai",
     io={"detect_static_provider_for_model": ["openai", "x"], "list_providers": [],
         "model_in_provider_catalog": seq(False, False, False, False, False, False, True)})
case("_detection_candidates", name="claude-sonnet-5", current_provider="openai",
     io={"detect_static_provider_for_model": None, "model_in_provider_catalog": False,
         "find_openrouter_slug": "anthropic/claude-sonnet-5"})
case("_detection_candidates", name="anthropic/claude-sonnet-5", current_provider="openrouter",
     io={"detect_static_provider_for_model": None, "model_in_provider_catalog": False,
         "find_openrouter_slug": "anthropic/claude-sonnet-5"})
case("_detection_candidates", name="nous/deepseek-v4-pro", current_provider="openai",
     io={"detect_static_provider_for_model": None, "model_in_provider_catalog": False,
         "find_openrouter_slug": None, "load_config": {"providers": {"nous": {}}}})
case("_detection_candidates", name="nous/gpt-6", current_provider="anthropic",
     io={"detect_static_provider_for_model": ["openai", "gpt-6"], "list_providers": [],
         "model_in_provider_catalog": seq(True, True, False, False, False, False, False),
         "find_openrouter_slug": "openai/gpt-6", "load_config": {"providers": {"nous": {}}}})
case("_detection_candidates", name="m", current_provider="openai",
     io={"detect_static_provider_for_model": raises("OSError", "detect down")})
case("_detection_candidates", name="", current_provider="openai",
     io={"detect_static_provider_for_model": None, "model_in_provider_catalog": False,
         "find_openrouter_slug": None})

# ── _catalog_fetcher ────────────────────────────────────────────────────────
case("_catalog_fetcher", normalized="nous")
case("_catalog_fetcher", normalized="fireworks",
     io={"get_provider_profile": {"name": "fireworks", "picker_catalog": True}})
case("_catalog_fetcher", normalized="fireworks",
     io={"get_provider_profile": {"name": "fireworks", "picker_catalog": None}})
case("_catalog_fetcher", normalized="fireworks", io={"get_provider_profile": None})
case("_catalog_fetcher", normalized="fireworks",
     io={"get_provider_profile": {"is_provider_profile": False, "picker_catalog": True}})
case("_catalog_fetcher", normalized="fireworks",
     io={"get_provider_profile": {"name": "fireworks"}})
case("_catalog_fetcher", normalized="fireworks",
     io={"get_provider_profile": raises("OSError", "registry down")})
case("_catalog_fetcher", normalized="fireworks",
     io={"get_provider_profile": {"name": "fireworks", "picker_catalog": False}})
case("_catalog_fetcher", normalized="nous", env={"fetchers": []}, io={"get_provider_profile": None})

# ── _api_key_credentials ────────────────────────────────────────────────────
case("_api_key_credentials", normalized="stepfun",
     io={"resolve_api_key_provider_credentials": {"api_key": " sk-1 ", "base_url": " https://x/v1 "}})
case("_api_key_credentials", normalized="stepfun", io={"resolve_api_key_provider_credentials": None})
case("_api_key_credentials", normalized="stepfun", io={"resolve_api_key_provider_credentials": {}})
case("_api_key_credentials", normalized="stepfun",
     io={"resolve_api_key_provider_credentials": {"api_key": None, "base_url": None}})
case("_api_key_credentials", normalized="stepfun",
     io={"resolve_api_key_provider_credentials": raises("OSError", "auth down")})
case("_api_key_credentials", normalized="stepfun", io={"resolve_api_key_provider_credentials": "x"})
case("_api_key_credentials", normalized="stepfun",
     io={"resolve_api_key_provider_credentials": {"api_key": "", "base_url": ""}})

# ── _api_key_provider_live ──────────────────────────────────────────────────
case("_api_key_provider_live", normalized="gmi",
     io={"resolve_api_key_provider_credentials": {"api_key": "", "base_url": "https://x"}})
case("_api_key_provider_live", normalized="gmi",
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": ""}})
case("_api_key_provider_live", normalized="gmi",
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": ["m1", "m2"]})
case("_api_key_provider_live", normalized="gmi",
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": []})
case("_api_key_provider_live", normalized="gmi",
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": None})
case("_api_key_provider_live", normalized="gmi",
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": raises("OSError", "net down")})
case("_api_key_provider_live", normalized="gmi",
     io={"resolve_api_key_provider_credentials": raises("OSError", "auth down")})

# ── _stepfun_catalog ────────────────────────────────────────────────────────
case("_stepfun_catalog", normalized="stepfun",
     io={"resolve_api_key_provider_credentials": {"api_key": "", "base_url": ""}})
case("_stepfun_catalog", normalized="stepfun", env=NOSTEP,
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": ["live-1"], "get_provider_profile": None})
case("_stepfun_catalog", normalized="stepfun",
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": ["step-3.7-flash", "live-2"],
         "model_alias_canonical": seq("step-3.7-flash", "live-2", "step-3.7-flash", "step-2")},
     env=pm(stepfun=["step-3.7-flash", "step-2"]))
case("_stepfun_catalog", normalized="stepfun", env=NOSTEP,
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": ["live-1"], "get_provider_profile": {"name": "stepfun", "static_models": []}})
case("_stepfun_catalog", normalized="stepfun",
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": raises("OSError", "net down")})
case("_stepfun_catalog", normalized="kimi", env=pm(kimi=["kimi-k3"]),
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": ["k3"], "model_alias_canonical": seq("kimi-k3", "kimi-k3")})
case("_stepfun_catalog", normalized="stepfun", env=NOSTEP,
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": ["live-1"], "get_provider_profile": {"name": "stepfun"}})
case("_stepfun_catalog", normalized="stepfun", force_refresh=True,
     io={"resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://x"},
         "fetch_api_models": ["live-1"], "get_provider_profile": None}, env=NOSTEP)

# ── _profile_live_catalog ───────────────────────────────────────────────────
case("_profile_live_catalog", normalized="ghost", io={"get_provider_profile": None})
case("_profile_live_catalog", normalized="acp",
     io={"get_provider_profile": seq({"auth_type": "external_process", "static_models": [], "fallback_models": ["fb-1"]},
                                     {"auth_type": "external_process", "static_models": [], "fallback_models": ["fb-1"]}),
         "fetch_models": ["live-1", "live-2"],
         "model_alias_canonical": seq("fb-1", "live-1", "live-2")})
case("_profile_live_catalog", normalized="acp",
     io={"get_provider_profile": {"auth_type": "external_process", "fallback_models": ["fb-1"]},
         "fetch_models": raises("OSError", "spawn failed")})
case("_profile_live_catalog", normalized="oauthp",
     io={"get_provider_profile": {"auth_type": "oauth", "fallback_models": ["fb"]}})
case("_profile_live_catalog", normalized="k",
     io={"get_provider_profile": {"auth_type": "api_key", "base_url": "", "fallback_models": ["fb"]}})
case("_profile_live_catalog", normalized="k",
     io={"get_provider_profile": {"auth_type": "api_key", "base_url": "https://p", "static_models": [], "fallback_models": ["fb"]},
         "resolve_api_key_provider_credentials": {"api_key": "", "base_url": ""}})
case("_profile_live_catalog", normalized="k",
     io={"get_provider_profile": seq({"auth_type": "api_key", "base_url": "https://p", "static_models": ["cur-1"], "fallback_models": []},
                                     {"auth_type": "api_key", "base_url": "https://p", "static_models": ["cur-1"], "fallback_models": []}),
         "resolve_api_key_provider_credentials": {"api_key": "k", "base_url": ""},
         "fetch_models": ["live-1"], "model_alias_canonical": seq("cur-1", "live-1")})
case("_profile_live_catalog", normalized="k",
     io={"get_provider_profile": seq({"auth_type": "api_key", "base_url": "https://p", "static_models": ["cur-1"], "fallback_models": []},
                                     {"auth_type": "api_key", "base_url": "https://p", "static_models": ["cur-1"], "fallback_models": []}),
         "resolve_api_key_provider_credentials": {"api_key": "k", "base_url": "https://creds"},
         "fetch_models": ["live-1"], "model_alias_canonical": seq("cur-1", "live-1")})
case("_profile_live_catalog", normalized="oauthp",
     io={"get_provider_profile": {"auth_type": "oauth", "fallback_models": []}})
case("_profile_live_catalog", normalized="bad", io={"get_provider_profile": {"name": "bad"}})
case("_profile_live_catalog", normalized="opencode-zen",
     io={"get_provider_profile": seq({"auth_type": "external_process", "static_models": ["ox-alpha"], "fallback_models": []},
                                     {"auth_type": "external_process", "static_models": ["ox-alpha"], "fallback_models": []}),
         "fetch_models": ["ox-alpha-free", "live-z"],
         "model_alias_canonical": seq("ox-alpha-free", "live-z", "ox-alpha")})

# ── probe_profile_catalog ───────────────────────────────────────────────────
case("probe_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, api_key="")
case("probe_profile_catalog", normalized="x", profile={"static_models": [], "fallback_models": []}, api_key="k",
     io={"fetch_models": ["l1"], "get_provider_profile": {"static_models": [], "fallback_models": []}})
case("probe_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, api_key="k",
     io={"fetch_models": raises("OSError", "probe down")})
case("probe_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, api_key="k",
     io={"fetch_models": []})
case("probe_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, api_key="k",
     io={"fetch_models": None})
case("probe_profile_catalog", normalized="x", profile={"static_models": [], "fallback_models": []}, api_key="k", base_url=None,
     io={"fetch_models": ["l1"], "get_provider_profile": {"static_models": [], "fallback_models": []}})
case("probe_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, api_key="k", base_url="https://b",
     io={"fetch_models": ["l1"], "get_provider_profile": {"static_models": ["c1"]},
         "model_alias_canonical": seq("c1", "l1")})

# ── merge_profile_catalog ───────────────────────────────────────────────────
case("merge_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, live=None)
case("merge_profile_catalog", normalized="x", profile={"fallback_models": []}, live=None)
case("merge_profile_catalog", normalized="x", profile={"fallback_models": []}, live=["l1"],
     io={"get_provider_profile": {"static_models": []}})
case("merge_profile_catalog", normalized="fireworks", profile={"fallback_models": ["fb"]}, live=["l1", "c1"],
     io={"get_provider_profile": {"static_models": ["c1", "c2"]},
         "model_alias_canonical": seq("c1", "c2", "l1", "c1")})
case("merge_profile_catalog", normalized="opencode-go", profile={"fallback_models": []}, live=["l1", "c1"],
     io={"get_provider_profile": {"static_models": ["c1"]},
         "model_alias_canonical": seq("l1", "c1", "c1")})
case("merge_profile_catalog", normalized="opencode-go", profile={"fallback_models": []}, live=["ox-alpha-free", "l1"],
     io={"get_provider_profile": {"static_models": ["ox-alpha"]},
         "model_alias_canonical": seq("ox-alpha-free", "l1", "ox-alpha")})
case("merge_profile_catalog", normalized="kimi", profile={"fallback_models": []}, live=["k3"],
     env=pm(kimi=["kimi-k3"]),
     io={"get_provider_profile": {"static_models": ["kimi-k3"]},
         "model_alias_canonical": seq("kimi-k3", "kimi-k3")})
case("merge_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, live=["l1"],
     io={"get_provider_profile": {"static_models": []}, "model_alias_canonical": seq("fb", "l1")})
case("merge_profile_catalog", normalized="x", profile={"fallback_models": ["fb"]}, live=[])
case("merge_profile_catalog", normalized="x", profile={"fallback_models": []}, live={"$curated": ["l1"]},
     io={"get_provider_profile": {"static_models": []}})

# ── _drop_delisted_opencode_models ──────────────────────────────────────────
case("_drop_delisted_opencode_models", normalized="kimi", rows=["ox-alpha-free", "a"])
case("_drop_delisted_opencode_models", normalized="opencode-zen", rows=["ox-alpha-free", "OX-ALPHA-FREE", "keep"])
case("_drop_delisted_opencode_models", normalized="opencode-zen", rows=[])
case("_drop_delisted_opencode_models", normalized="opencode-zen", rows=None)
case("_drop_delisted_opencode_models", normalized="opencode-go", rows={"$curated": ["deepseek-v4-flash-free", "ok"]})
case("_drop_delisted_opencode_models", normalized="opencode-go", rows={"$curated": []})
case("_drop_delisted_opencode_models", normalized="opencode-zen", rows=["x-preview-f-free"])

# ── _configured_relay_base_url ──────────────────────────────────────────────
case("_configured_relay_base_url", provider="openai", io={"get_model_config_dict": raises("OSError", "cfg down")})
case("_configured_relay_base_url", provider="openai", io={"get_model_config_dict": {"base_url": "https://relay"}})
case("_configured_relay_base_url", provider="", io={"get_model_config_dict": {"provider": "openai"}})
case("_configured_relay_base_url", provider="openai", io={"get_model_config_dict": {"provider": "anthropic"}})
case("_configured_relay_base_url", provider="openai",
     io={"get_model_config_dict": {"provider": "gpt", "base_url": "https://relay/v1"},
         "get_provider_profile": None, "registry_entry": None,
         "normalize_route_base_url": "https://relay/v1"})
case("_configured_relay_base_url", provider="openai", io={"get_model_config_dict": {"provider": "openai"}})
case("_configured_relay_base_url", provider="openai",
     io={"get_model_config_dict": {"provider": "openai", "base_url": "https://api.openai.com/v1/"},
         "get_provider_profile": {"base_url": "https://api.openai.com/v1"}, "registry_entry": None,
         "normalize_route_base_url": seq("https://api.openai.com/v1", "https://api.openai.com/v1")})
case("_configured_relay_base_url", provider="openai",
     io={"get_model_config_dict": {"provider": "openai", "base_url": "https://api.openai.com/v1"},
         "get_provider_profile": None, "registry_entry": {"inference_base_url": "https://api.openai.com/v1"},
         "normalize_route_base_url": seq("https://api.openai.com/v1", "https://api.openai.com/v1")})
case("_configured_relay_base_url", provider="openai",
     io={"get_model_config_dict": {"provider": "openai", "base_url": "https://relay"},
         "get_provider_profile": raises("OSError", "profile down")})
case("_configured_relay_base_url", provider="openai",
     io={"get_model_config_dict": {"provider": "openai", "base_url": "https://relay"},
         "get_provider_profile": {"base_url": "https://p"}, "registry_entry": None,
         "normalize_route_base_url": raises("ValueError", "bad url")})
case("_configured_relay_base_url", provider="openai", io={"get_model_config_dict": None})
case("_configured_relay_base_url", provider="openai",
     io={"get_model_config_dict": {"provider": "openai", "base_url": " https://relay/v1/ "},
         "get_provider_profile": None, "registry_entry": None,
         "normalize_route_base_url": "https://relay/v1"})

# ── _relay_model_catalog ────────────────────────────────────────────────────
case("_relay_model_catalog", normalized="x", relay="https://relay", io={"get_provider_profile": None})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"auth_type": "oauth"}})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"auth_type": "api_key"},
         "resolve_api_key_provider_credentials": {"api_key": "ck", "base_url": "cb"},
         "relay_probe_api_key": "rk", "fetch_models": ["m1", "m2"]})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"auth_type": "api_key"},
         "resolve_api_key_provider_credentials": {"api_key": "ck", "base_url": "cb"},
         "relay_probe_api_key": "rk", "fetch_models": []})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"auth_type": "api_key"},
         "resolve_api_key_provider_credentials": {"api_key": "ck", "base_url": "cb"},
         "relay_probe_api_key": "rk", "fetch_models": ["", "m1"]})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"auth_type": "api_key"},
         "resolve_api_key_provider_credentials": {"api_key": "ck", "base_url": "cb"},
         "relay_probe_api_key": "rk", "fetch_models": raises("OSError", "relay down")})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"auth_type": "api_key"},
         "resolve_api_key_provider_credentials": {"api_key": "ck", "base_url": "cb"},
         "relay_probe_api_key": raises("OSError", "no probe key")})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"auth_type": "api_key"},
         "resolve_api_key_provider_credentials": {"api_key": "", "base_url": ""},
         "relay_probe_api_key": None, "fetch_models": ["m1"]})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": {"name": "x"}})
case("_relay_model_catalog", normalized="x", relay="https://relay",
     io={"get_provider_profile": raises("OSError", "registry down")})

# ── _disk_serve_tier ────────────────────────────────────────────────────────
E = {"fp": "f1", "at": 1000, "models": ["m1"]}
case("_disk_serve_tier", entry=None, fp="f1", now=1000, is_ollama=False)
case("_disk_serve_tier", entry=E, fp="other", now=1000, is_ollama=False)
case("_disk_serve_tier", entry={"fp": "f1", "at": 1000, "models": "x"}, fp="f1", now=1000, is_ollama=False)
case("_disk_serve_tier", entry={"fp": "f1", "at": 1000, "models": []}, fp="f1", now=1000, is_ollama=False)
case("_disk_serve_tier", entry={"fp": "f1", "at": 1000, "models": []}, fp="f1", now=1100, is_ollama=True)
case("_disk_serve_tier", entry={"fp": "f1", "at": True, "models": ["m"]}, fp="f1", now=1000, is_ollama=False)
case("_disk_serve_tier", entry={"fp": "f1", "at": "1000", "models": ["m"]}, fp="f1", now=1000, is_ollama=False)
case("_disk_serve_tier", entry=E, fp="f1", now=1100, is_ollama=False)
case("_disk_serve_tier", entry=E, fp="f1", now=8200, is_ollama=False)
case("_disk_serve_tier", entry=E, fp="f1", now=701000, is_ollama=False)
case("_disk_serve_tier", entry=dict(E, fallback=True), fp="f1", now=1030, is_ollama=False)
case("_disk_serve_tier", entry=dict(E, fallback=True), fp="f1", now=1100, is_ollama=False)
case("_disk_serve_tier", entry=E, fp="f1", now=1400, is_ollama=True)
case("_disk_serve_tier", entry=E, fp="f1", now=4600, is_ollama=False)
case("_disk_serve_tier", entry=E, fp="f1", now=1150, is_ollama=False, ttl_seconds=100)
case("_disk_serve_tier", entry={"fp": "f1", "models": ["m"]}, fp="f1", now=1000, is_ollama=False)
case("_disk_serve_tier", entry={"fp": "f1", "at": 1000, "models": []}, fp="f1", now=1400, is_ollama=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {OUT} ({len(CASES)} cases)")
    for fn, n in sorted(counts.items()):
        flag = "" if n >= 5 else "  <-- BELOW 5"
        print(f"  {fn}: {n}{flag}")
    if "--self-check" in sys.argv:
        self_check()


def self_check():
    """Drive every case through the oracle's LIVE reference driver (no C side)
    to validate AST seeding and script consistency before the gate."""
    sys.path.insert(0, str(HERE))
    import oracle_models_catalog_flow as O
    # intended error-path outcomes: (func, per-func case index) -> exception class
    expected_raises = {
        ("static_catalog", 4): "AttributeError",
        ("static_catalog", 6): "TypeError",
        ("static_catalog", 7): "OSError",
        ("static_catalog_providers", 5): "OSError",
        ("static_catalog_providers", 6): "AttributeError",
        ("static_catalog_providers", 7): "AttributeError",
        ("_static_catalog_matches", 7): "OSError",
        ("_detection_candidates", 8): "OSError",
        ("_catalog_fetcher", 5): "AttributeError",
        ("_catalog_fetcher", 6): "OSError",
        ("_stepfun_catalog", 6): "AttributeError",
        ("_profile_live_catalog", 9): "AttributeError",
        ("_configured_relay_base_url", 10): "AttributeError",
    }
    bad = 0
    seen = {}
    for i, c in enumerate(CASES):
        fn = c["func"]
        idx = seen.get(fn, 0)
        seen[fn] = idx + 1
        rec = dict(c)
        try:
            ns, trace, scripts = O.build_ns(rec)
            try:
                out = O.norm(O.drive(rec, ns, scripts))
            except BaseException as e:
                out = {"__raise__": type(e).__name__}
        except BaseException as e:
            print(f"SEED FAIL case {i} {fn}: {type(e).__name__}: {e}")
            bad += 1
            continue
        want_raise = expected_raises.get((fn, idx))
        got_raise = out.get("__raise__") if isinstance(out, dict) else None
        if got_raise != want_raise:
            print(f"RAISE MISMATCH case {i} {fn}[{idx}]: want {want_raise}, got {got_raise}")
            bad += 1
        for ev in trace:
            if ev["ev"] not in (rec["in"].get("io") or {}):
                if ev["ev"] not in ("find_openrouter_slug", "get_provider_profile", "registry_entry"):
                    print(f"UNSCRIPTED seam case {i} {fn}: {ev['ev']}")
                    bad += 1
    print(f"self-check: {len(CASES)} cases, {bad} problems")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
