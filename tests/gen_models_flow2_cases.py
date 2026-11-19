#!/usr/bin/env python3
"""Generator for tests/fixtures/models_flow2.jsonl.

Emits >=5 cases per ported function of hermes_cli/models.py (the UNCLAIMED
remainder ported as src/cli/models_flow2.c). Each case is
{"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...}}}.

Script shapes (shared with the C universal seam md2_call and the oracle):
  - absent key            -> None
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  - any other JSON value  -> reused for every call

Domain restrictions (documented in the port header):
  - str()/float() operands are ASCII strings, numbers, bools or null; float()
    tokens exclude underscores; URLs are scheme://host ASCII.
  - list subclasses travel tagged: CuratedFallbackModels {"$curated": [...]},
    _NativePickerModelList {"$native": [...]}.
  - cache rows carry fp/at/models; base_url_origin results are 3-element arrays.
  - integral floats collapse to ints on both sides.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "models_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def curated(*items):
    return {"$curated": list(items)}


def native(*items):
    return {"$native": list(items)}


# ── _zero_priced ────────────────────────────────────────────────────────────
case("_zero_priced", pricing={"prompt": "0", "completion": "0"}, keys=["prompt", "completion"], default="1")
case("_zero_priced", pricing={"prompt": "0", "completion": "0.5"}, keys=["prompt", "completion"], default="1")
case("_zero_priced", pricing={"prompt": "0"}, keys=["prompt", "completion"], default="0")
case("_zero_priced", pricing={"prompt": "0"}, keys=["prompt", "completion"], default="1")
case("_zero_priced", pricing="notdict", keys=["prompt", "completion"], default="1")
case("_zero_priced", pricing={"prompt": "abc"}, keys=["prompt", "completion"], default="1")
case("_zero_priced", pricing={"prompt": None}, keys=["prompt", "completion"], default="1")
case("_zero_priced", pricing={}, keys=[], default="1")
case("_zero_priced", pricing={"prompt": 0, "completion": 0}, keys=["prompt", "completion"], default="1")
case("_zero_priced", pricing={"prompt": True}, keys=["prompt", "completion"], default="1")

# ── _is_subscription_billed ─────────────────────────────────────────────────
case("_is_subscription_billed", entry={"billing_mode": "subscription"})
case("_is_subscription_billed", entry={"billing_mode": "credits"})
case("_is_subscription_billed", entry={})
case("_is_subscription_billed", entry=None)
case("_is_subscription_billed", entry="x")
case("_is_subscription_billed", entry={"billing_mode": None})

# ── _union_with_portal_recommendations ──────────────────────────────────────
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=["a"],
     pricing={"a": {"prompt": "1"}}, portal_base_url="https://p", force_refresh=False,
     synthesize_free_pricing=True, io=io(fetch_nous_recommended_models=raise_("RuntimeError", "net")))
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=["a"],
     pricing={"a": {"prompt": "1"}}, portal_base_url="https://p", force_refresh=True,
     synthesize_free_pricing=True,
     io=io(fetch_nous_recommended_models={"freeRecommendedModels": [{"modelName": "a"}, {"modelName": "b"}]},
           extract_model_name=seq("a", "b")))
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=["a"],
     pricing={}, portal_base_url="", force_refresh=False, synthesize_free_pricing=False,
     io=io(fetch_nous_recommended_models={"freeRecommendedModels": "notlist"}))
case("_union_with_portal_recommendations", tier_key="paidRecommendedModels", curated_ids=[],
     pricing={}, portal_base_url="", force_refresh=False, synthesize_free_pricing=False,
     io=io(fetch_nous_recommended_models=None))
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=[],
     pricing={}, portal_base_url="", force_refresh=False, synthesize_free_pricing=False,
     io=io(fetch_nous_recommended_models={"freeRecommendedModels": [{"modelName": "a"}, {"modelName": " "}]},
           extract_model_name=seq("a", None)))
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=[],
     pricing={}, portal_base_url="", force_refresh=False, synthesize_free_pricing=True,
     io=io(fetch_nous_recommended_models=raise_("KeyboardInterrupt")))
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=[],
     pricing={}, portal_base_url="", force_refresh=False, synthesize_free_pricing=False,
     io=io(fetch_nous_recommended_models={"freeRecommendedModels": [{"modelName": "a"}]},
           extract_model_name=raise_("ValueError", "bad")))
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=[],
     pricing={"b": None}, portal_base_url="", force_refresh=False, synthesize_free_pricing=True,
     io=io(fetch_nous_recommended_models={"freeRecommendedModels": [{"modelName": "b"}]},
           extract_model_name=seq("b")))
case("_union_with_portal_recommendations", tier_key="freeRecommendedModels", curated_ids=[],
     pricing={}, portal_base_url="", force_refresh=False, synthesize_free_pricing=False,
     io=io(fetch_nous_recommended_models={"freeRecommendedModels": [{"modelName": "a"}, {"modelName": "a"}]},
           extract_model_name=seq("a", "a")))

# ── union_with_nous_on_sale_models ──────────────────────────────────────────
case("union_with_nous_on_sale_models", curated_ids=["c"], pricing={})
case("union_with_nous_on_sale_models", curated_ids=["c"], pricing=None)
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"prompt": "1", "completion": "2", "original": {"prompt": "10", "completion": "20"}}},
     io=io(compute_sale_discount=[50, "10", "20"], price_float=1.0))
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"tools": False, "prompt": "1", "original": {"prompt": "10"}}})
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"generation": True, "prompt": "1", "original": {"prompt": "10"}}})
case("union_with_nous_on_sale_models", curated_ids=["m"],
     pricing={"m": {"prompt": "1", "original": {"prompt": "10"}}})
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"prompt": "1", "completion": "2", "original": {"prompt": "10"}}},
     io=io(compute_sale_discount=None, price_float=1.0))
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"prompt": "1", "original": "notdict"}},
     io=io(compute_sale_discount=[50, "10", ""], price_float=1.0))
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"prompt": "1", "completion": "2", "original": {"prompt": "10"}}},
     io=io(compute_sale_discount=[50, "10", ""], price_float=seq(None, None)))
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"prompt": "1", "completion": "2", "original": {"prompt": "10"}}},
     io=io(compute_sale_discount=[50, "10", ""], price_float=seq(None, 2.0)))
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m1": {"prompt": "1", "original": {"prompt": "10"}},
              "m2": {"prompt": "1", "original": {"prompt": "10"}}},
     io=io(compute_sale_discount=seq([50, "10", ""], [80, "10", ""]), price_float=1.0))
case("union_with_nous_on_sale_models", curated_ids=["c"],
     pricing={"m": {"prompt": "1", "original": {"prompt": "10"}}},
     io=io(compute_sale_discount=raise_("RuntimeError", "x")))

# ── _pricing_profile_key ────────────────────────────────────────────────────
case("_pricing_profile_key", io=io(hermes_home_key="home1"))
case("_pricing_profile_key", io=io(hermes_home_key=""))
case("_pricing_profile_key", io=io(hermes_home_key="profile/key"))
case("_pricing_profile_key", io=io(hermes_home_key=raise_("RuntimeError", "x")))
case("_pricing_profile_key", io=io(hermes_home_key=raise_("KeyboardInterrupt")))
case("_pricing_profile_key", io=io(hermes_home_key="another"))

# ── get_cached_nous_free_tier ───────────────────────────────────────────────
case("get_cached_nous_free_tier", env={"free_tier_cache": {}}, io=io(hermes_home_key="h"))
case("get_cached_nous_free_tier", env={"free_tier_cache": {"h": [True, 100]}, "free_tier_ttl": 180},
     io=io(hermes_home_key="h", **{"time.monotonic": 150}))
case("get_cached_nous_free_tier", env={"free_tier_cache": {"h": [False, 100]}, "free_tier_ttl": 180},
     io=io(hermes_home_key="h", **{"time.monotonic": 300}))
case("get_cached_nous_free_tier", env={"free_tier_cache": {"h": [True, 100]}, "free_tier_ttl": 180},
     io=io(hermes_home_key="h", **{"time.monotonic": 280}))
case("get_cached_nous_free_tier", env={"free_tier_cache": {"h": [True, 100]}},
     io=io(hermes_home_key=raise_("RuntimeError", "x")))
case("get_cached_nous_free_tier", env={"free_tier_cache": {"h": [True, 100]}},
     io=io(hermes_home_key="h", **{"time.monotonic": raise_("RuntimeError", "clock")}))
case("get_cached_nous_free_tier", env={"free_tier_cache": {"h": [True, 100]}, "free_tier_ttl": 180},
     io=io(hermes_home_key="h", **{"time.monotonic": 100}))

# ── _is_anthropic_frontier_tier ─────────────────────────────────────────────
case("_is_anthropic_frontier_tier", model_id="anthropic/claude-opus-4-6",
     io=io(strip_vendor_prefix="claude-opus-4-6"))
case("_is_anthropic_frontier_tier", model_id="claude-sonnet-4", io=io(strip_vendor_prefix="claude-sonnet-4"))
case("_is_anthropic_frontier_tier", model_id=None, io=io(strip_vendor_prefix=""))
case("_is_anthropic_frontier_tier", model_id="claude-fable-5:x", io=io(strip_vendor_prefix="claude-fable-5:x"))
case("_is_anthropic_frontier_tier", model_id="qwopus3.6", io=io(strip_vendor_prefix="qwopus3.6"))
case("_is_anthropic_frontier_tier", model_id="x", io=io(strip_vendor_prefix=raise_("RuntimeError", "x")))
case("_is_anthropic_frontier_tier", model_id="anthropic/claude-opus", io=io(strip_vendor_prefix="claude-opus"))

# ── _anthropic_models_url ───────────────────────────────────────────────────
case("_anthropic_models_url", base_url=None)
case("_anthropic_models_url", base_url="https://api.anthropic.com/v1")
case("_anthropic_models_url", base_url="https://x.com/")
case("_anthropic_models_url", base_url=None, after_id="model_123")
case("_anthropic_models_url", base_url="https://x.com/v1?foo=bar")
case("_anthropic_models_url", base_url="https://x.com", after_id="a b&c")
case("_anthropic_models_url", base_url="https://x.com", after_id="")
case("_anthropic_models_url", base_url="  https://x.com/v1  ")

# ── _anthropic_next_cursor ──────────────────────────────────────────────────
case("_anthropic_next_cursor", page={"has_more": True, "last_id": "m1"}, seen=[])
case("_anthropic_next_cursor", page={"has_more": False, "last_id": "m1"}, seen=[])
case("_anthropic_next_cursor", page={"has_more": True, "last_id": "m1"}, seen=["m1"])
case("_anthropic_next_cursor", page={"has_more": True, "last_id": ""}, seen=[])
case("_anthropic_next_cursor", page={"has_more": True, "last_id": 5}, seen=[])
case("_anthropic_next_cursor", page=None, seen=[])
case("_anthropic_next_cursor", page={"has_more": 1, "last_id": "m1"}, seen=[])
case("_anthropic_next_cursor", page={"has_more": True}, seen=["x"])

# ── _fast_mode_route_supported ──────────────────────────────────────────────
case("_fast_mode_route_supported", model_id="claude-opus-4-8", provider="anthropic",
     base_url="https://api.anthropic.com", io=io(is_anthropic_fast_model=True, normalize_provider="anthropic"))
case("_fast_mode_route_supported", model_id="claude-opus-4-8", provider="openai",
     base_url="https://api.anthropic.com", io=io(is_anthropic_fast_model=True, normalize_provider="openai"))
case("_fast_mode_route_supported", model_id="claude-opus-4-8", provider=None,
     base_url="https://api.anthropic.com", io=io(is_anthropic_fast_model=True))
case("_fast_mode_route_supported", model_id="claude-opus-4-8", provider=None,
     base_url="https://other.com", io=io(is_anthropic_fast_model=True))
case("_fast_mode_route_supported", model_id="grok-4-6", provider="xai", base_url="https://api.x.ai",
     io=io(is_anthropic_fast_model=False, is_grok_46_family=True, normalize_provider="xai"))
case("_fast_mode_route_supported", model_id="gpt-6", provider="openai", base_url="https://regional.openai.com",
     io=io(is_anthropic_fast_model=False, is_grok_46_family=False, openai_fast_region_allows=True,
           normalize_provider="openai"))
case("_fast_mode_route_supported", model_id="gpt-6", provider="openai", base_url="https://api.openai.com",
     io=io(is_anthropic_fast_model=False, is_grok_46_family=False, openai_fast_region_allows=False,
           normalize_provider="openai"))
case("_fast_mode_route_supported", model_id="gpt-6", provider="openai-codex", base_url="https://chatgpt.com",
     io=io(is_anthropic_fast_model=False, is_grok_46_family=False, openai_fast_region_allows=False,
           normalize_provider="openai-codex"))
case("_fast_mode_route_supported", model_id="gpt-6", provider=None, base_url=None,
     io=io(is_anthropic_fast_model=False, is_grok_46_family=False, openai_fast_region_allows=False))
case("_fast_mode_route_supported", model_id="gpt-6", provider="azure", base_url="https://api.openai.com",
     io=io(is_anthropic_fast_model=False, is_grok_46_family=False, openai_fast_region_allows=False,
           normalize_provider="azure"))
case("_fast_mode_route_supported", model_id="x", provider=None, base_url=None,
     io=io(is_anthropic_fast_model=raise_("RuntimeError", "x")))

# ── model_supports_ultrafast ────────────────────────────────────────────────
case("model_supports_ultrafast", model_id="gpt-6-astra", env={"ultrafast_models": ["gpt-6-astra"]},
     io=io(strip_codex_context_variant_suffix="gpt-6-astra", strip_vendor_prefix="gpt-6-astra"))
case("model_supports_ultrafast", model_id="openai/gpt-6-astra", env={"ultrafast_models": ["gpt-6-astra"]},
     io=io(strip_codex_context_variant_suffix="openai/gpt-6-astra", strip_vendor_prefix="gpt-6-astra"))
case("model_supports_ultrafast", model_id="gpt-6-astra:variant", env={"ultrafast_models": ["gpt-6-astra"]},
     io=io(strip_codex_context_variant_suffix="gpt-6-astra:variant", strip_vendor_prefix="gpt-6-astra:variant"))
case("model_supports_ultrafast", model_id="gpt-5", env={"ultrafast_models": ["gpt-6-astra"]},
     io=io(strip_codex_context_variant_suffix="gpt-5", strip_vendor_prefix="gpt-5"))
case("model_supports_ultrafast", model_id=None, env={"ultrafast_models": ["gpt-6-astra"]},
     io=io(strip_codex_context_variant_suffix="", strip_vendor_prefix=""))
case("model_supports_ultrafast", model_id="x", env={"ultrafast_models": []},
     io=io(strip_codex_context_variant_suffix=raise_("RuntimeError", "x")))
case("model_supports_ultrafast", model_id="gpt-6-astra", env={"ultrafast_models": []},
     io=io(strip_codex_context_variant_suffix="gpt-6-astra", strip_vendor_prefix="gpt-6-astra"))

# ── _cache_entry ────────────────────────────────────────────────────────────
case("_cache_entry", fp="f", models=["a", "b"], at=None, io=io(**{"time.time": 100}))
case("_cache_entry", fp="f", models=["a"], at=50)
case("_cache_entry", fp="f", models=curated("a"), at=10)
case("_cache_entry", fp="f", models=[], at=0)
case("_cache_entry", fp="f", models=["a"], at=None, io=io(**{"time.time": raise_("RuntimeError", "clock")}))
case("_cache_entry", fp="x", models=["m"], at=99.5)

# ── _live_result_entry ──────────────────────────────────────────────────────
case("_live_result_entry", fp="f", live=["a"], existing=None, at=10)
case("_live_result_entry", fp="f", live=curated("a"),
     existing={"fp": "f", "models": ["x"], "at": 5}, at=10, io=io(cache_entry_valid=True))
case("_live_result_entry", fp="f", live=curated("a"),
     existing={"fp": "f", "models": ["x"], "at": 5, "fallback": True}, at=None,
     io=io(cache_entry_valid=True, **{"time.time": 100}))
case("_live_result_entry", fp="f", live=curated("a"), existing=None, at=None,
     io=io(cache_entry_valid=False, **{"time.time": 100}))
case("_live_result_entry", fp="f", live=curated(), existing=None, at=7,
     io=io(cache_entry_valid=False))
case("_live_result_entry", fp="f", live=curated("a"), existing=None, at=10,
     io=io(cache_entry_valid=raise_("RuntimeError", "x")))
case("_live_result_entry", fp="f", live=curated("a"), existing=None, at=10,
     io=io(cache_entry_valid=True))

# ── _normalized_cache_slug ──────────────────────────────────────────────────
case("_normalized_cache_slug", provider="ollama")
case("_normalized_cache_slug", provider="  OLLAMA  ")
case("_normalized_cache_slug", provider="openai", io=io(normalize_provider="openai"))
case("_normalized_cache_slug", provider=None, io=io(normalize_provider=""))
case("_normalized_cache_slug", provider="x", io=io(normalize_provider=""))
case("_normalized_cache_slug", provider="auto", io=io(normalize_provider="auto"))
case("_normalized_cache_slug", provider="x", io=io(normalize_provider=raise_("RuntimeError", "x")))

# ── _model_requires_account_discovery ───────────────────────────────────────
case("_model_requires_account_discovery", provider="openai-codex", model="m",
     io=io(normalize_provider="openai-codex", codex_model_requires_account_discovery=True))
case("_model_requires_account_discovery", provider="openai", model="gpt-6-astra",
     io=io(normalize_provider="openai", is_astra_model=True))
case("_model_requires_account_discovery", provider="openai-api", model="gpt-5",
     io=io(normalize_provider="openai-api", is_astra_model=False))
case("_model_requires_account_discovery", provider="anthropic", model="m",
     io=io(normalize_provider="anthropic"))
case("_model_requires_account_discovery", provider="ollama", model="m")
case("_model_requires_account_discovery", provider="openai", model="m",
     io=io(normalize_provider="openai", is_astra_model=raise_("RuntimeError", "x")))
case("_model_requires_account_discovery", provider="openai-codex", model="m",
     io=io(normalize_provider="openai-codex", codex_model_requires_account_discovery=raise_("ValueError", "x")))

# ── _ollama_native_probe_reachable ──────────────────────────────────────────
case("_ollama_native_probe_reachable", env={"probe_reachable": {"k": True}},
     io=io(get_ollama_base_url="http://localhost:11434", get_ollama_native_headers={"h": 1},
           root_for_ollama_native_api="http://localhost:11434", ollama_probe_cache_key="k"))
case("_ollama_native_probe_reachable", env={"probe_reachable": {"k": False}},
     io=io(get_ollama_base_url="http://h", get_ollama_native_headers={"h": 1},
           root_for_ollama_native_api="http://h", ollama_probe_cache_key="k"))
case("_ollama_native_probe_reachable", env={"probe_reachable": {}},
     io=io(get_ollama_base_url="http://h", get_ollama_native_headers=None,
           root_for_ollama_native_api="http://h", ollama_probe_cache_key="k"))
case("_ollama_native_probe_reachable", env={"probe_reachable": {"k": True}},
     io=io(get_ollama_base_url="http://h", get_ollama_native_headers=None,
           root_for_ollama_native_api="http://h", ollama_probe_cache_key="k"))
case("_ollama_native_probe_reachable", env={"probe_reachable": {"k": 1}},
     io=io(get_ollama_base_url="http://h", get_ollama_native_headers={"h": 1},
           root_for_ollama_native_api="http://h", ollama_probe_cache_key="k"))
case("_ollama_native_probe_reachable", env={"probe_reachable": {}},
     io=io(get_ollama_base_url=raise_("RuntimeError", "x")))
case("_ollama_native_probe_reachable", env={"probe_reachable": {"k": True}},
     io=io(get_ollama_base_url="http://h", get_ollama_native_headers={},
           root_for_ollama_native_api="http://h", ollama_probe_cache_key="k"))

# ── _resolve_anthropic_pool_catalog_credentials ─────────────────────────────
case("_resolve_anthropic_pool_catalog_credentials",
     io=io(read_credential_pool=[{"auth_type": "api_key", "access_token": "tok", "base_url": "https://x"}]))
case("_resolve_anthropic_pool_catalog_credentials",
     io=io(read_credential_pool=[{"auth_type": "oauth", "access_token": "t"}]))
case("_resolve_anthropic_pool_catalog_credentials",
     io=io(read_credential_pool=[{"auth_type": "api_key", "access_token": "  "}]))
case("_resolve_anthropic_pool_catalog_credentials",
     io=io(read_credential_pool=[{"auth_type": "api_key", "access_token": "tok", "inference_base_url": "https://y"}]))
case("_resolve_anthropic_pool_catalog_credentials", io=io(read_credential_pool=None))
case("_resolve_anthropic_pool_catalog_credentials", io=io(read_credential_pool=raise_("RuntimeError", "x")))
case("_resolve_anthropic_pool_catalog_credentials", io=io(read_credential_pool=raise_("KeyboardInterrupt")))
case("_resolve_anthropic_pool_catalog_credentials",
     io=io(read_credential_pool=[{"auth_type": "api_key", "access_token": "tok", "base_url": "  https://z  "}]))
case("_resolve_anthropic_pool_catalog_credentials",
     io=io(read_credential_pool=["notdict", {"auth_type": "api_key", "access_token": "t2"}]))

# ── _probe_neg_key ──────────────────────────────────────────────────────────
case("_probe_neg_key", base_url="https://h:8080", io=io(base_url_origin=["https", "h", 8080]))
case("_probe_neg_key", base_url="https://h", io=io(base_url_origin=["https", "", 0]))
case("_probe_neg_key", base_url="https://h", io=io(base_url_origin=["https", "h", 443]))
case("_probe_neg_key", base_url="https://h", io=io(base_url_origin=raise_("RuntimeError", "x")))
case("_probe_neg_key", base_url="http://localhost:11434", io=io(base_url_origin=["http", "localhost", 11434]))
case("_probe_neg_key", base_url="https://h", io=io(base_url_origin=["h"]))

# ── _probe_result ───────────────────────────────────────────────────────────
case("_probe_result", models=["a"], probed_url="u", resolved_base_url="r")
case("_probe_result", models=None, probed_url="u", resolved_base_url="r", suggested_base_url="s", used_fallback=True)
case("_probe_result", models=None, probed_url=None, resolved_base_url=None)
case("_probe_result", models=[], probed_url="u", resolved_base_url="r")
case("_probe_result", models=["a", "b"], probed_url=None, resolved_base_url="r", used_fallback=False)
case("_probe_result", models=None, probed_url="u", resolved_base_url="", suggested_base_url=None)

# ── _deepinfra_env ──────────────────────────────────────────────────────────
case("_deepinfra_env", key="K", io=io(get_env_value_prefer_dotenv="  val  "))
case("_deepinfra_env", key="K", io=io(get_env_value_prefer_dotenv=None))
case("_deepinfra_env", key="K", io=io(get_env_value_prefer_dotenv=""))
case("_deepinfra_env", key="K", io=io(get_env_value_prefer_dotenv="val"))
case("_deepinfra_env", key="K", io=io(get_env_value_prefer_dotenv=raise_("RuntimeError", "x")))
case("_deepinfra_env", key="K", io=io(get_env_value_prefer_dotenv="   "))

# ── _chat_catalog_rows ──────────────────────────────────────────────────────
case("_chat_catalog_rows", models=["a", "b"], io=io(without_generation_models=["a", "b"]))
case("_chat_catalog_rows", models=curated("a"), io=io(without_generation_models=curated("a")))
case("_chat_catalog_rows", models=None, io=io(without_generation_models=None))
case("_chat_catalog_rows", models=[], io=io(without_generation_models=[]))
case("_chat_catalog_rows", models=["a"], io=io(without_generation_models=raise_("RuntimeError", "x")))
case("_chat_catalog_rows", models=native("a"), io=io(without_generation_models=native("a")))

# ── _codex_catalog ──────────────────────────────────────────────────────────
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials={"api_key": "tok", "base_url": "https://x"},
           codex_access_token_is_expiring=False, get_codex_model_ids=["m"]))
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials={"api_key": "tok", "base_url": "https://x"},
           codex_access_token_is_expiring=True, get_codex_model_ids=["m"]))
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials=None, get_codex_model_ids=["m"]))
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials=raise_("RuntimeError", "x"), get_codex_model_ids=["m"]))
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials={"api_key": "tok", "base_url": "https://x"},
           codex_access_token_is_expiring=raise_("RuntimeError", "x"), get_codex_model_ids=["m"]))
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials={"api_key": "tok"}, get_codex_model_ids=raise_("RuntimeError", "x")))
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials=raise_("KeyboardInterrupt")))
case("_codex_catalog", normalized="openai-codex", force_refresh=False,
     io=io(resolve_codex_runtime_credentials={"api_key": None, "base_url": None},
           codex_access_token_is_expiring=False, get_codex_model_ids=["m"]))

# ── _nous_catalog ───────────────────────────────────────────────────────────
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials={"api_key": "k", "base_url": "https://x"},
           fetch_nous_models=["m"]))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials={"api_key": "k"}, fetch_nous_models=[],
           get_curated_nous_model_ids=["c"]))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials=None, get_curated_nous_model_ids=["c"]))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials={}, get_curated_nous_model_ids=None))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials=raise_("RuntimeError", "x"), get_curated_nous_model_ids=["c"]))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials={"api_key": "k"}, fetch_nous_models=raise_("RuntimeError", "x"),
           get_curated_nous_model_ids=["c"]))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials={"api_key": "k"}, fetch_nous_models=["m"],
           get_curated_nous_model_ids=raise_("RuntimeError", "x")))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(background_caller=raise_("RuntimeError", "x"), get_curated_nous_model_ids=["c"]))
case("_nous_catalog", normalized="nous", force_refresh=False,
     io=io(resolve_nous_runtime_credentials=raise_("KeyboardInterrupt")))

# ── _anthropic_catalog ──────────────────────────────────────────────────────
case("_anthropic_catalog", normalized="anthropic", force_refresh=False,
     env={"provider_models": {"anthropic": ["c"]}},
     io=io(get_model_config_dict={"provider": "anthropic", "base_url": "https://x", "api_key": "k"},
           normalize_provider="anthropic", fetch_anthropic_models=["live"]))
case("_anthropic_catalog", normalized="anthropic", force_refresh=False,
     env={"provider_models": {"anthropic": ["c"]}},
     io=io(get_model_config_dict={"provider": "anthropic"}, normalize_provider="anthropic",
           fetch_anthropic_models=["live"], merge_unique=["c", "live"]))
case("_anthropic_catalog", normalized="anthropic", force_refresh=False,
     env={"provider_models": {"anthropic": ["c"]}},
     io=io(get_model_config_dict={"provider": "openai"}, normalize_provider="openai",
           fetch_anthropic_models=None))
case("_anthropic_catalog", normalized="anthropic", force_refresh=False,
     env={"provider_models": {"anthropic": ["c"]}}, io=io(get_model_config_dict=None))
case("_anthropic_catalog", normalized="anthropic", force_refresh=False,
     env={"provider_models": {"anthropic": ["c"]}},
     io=io(get_model_config_dict={"provider": "anthropic"}, normalize_provider="anthropic",
           fetch_anthropic_models=raise_("RuntimeError", "x")))
case("_anthropic_catalog", normalized="anthropic", force_refresh=False,
     env={"provider_models": {"anthropic": ["c"]}},
     io=io(get_model_config_dict={"provider": "anthropic", "base_url": "  "}, normalize_provider="anthropic",
           fetch_anthropic_models=["live"], merge_unique=["c", "live"]))
case("_anthropic_catalog", normalized="anthropic", force_refresh=False,
     env={"provider_models": {"anthropic": ["c"]}},
     io=io(get_model_config_dict={"provider": "anthropic"}, normalize_provider=raise_("RuntimeError", "x")))

# ── _static_catalog ─────────────────────────────────────────────────────────
case("_static_catalog", normalized="anthropic", fetcher=None,
     io=io(static_catalog=["a"], drop_delisted_opencode_models=["a"], without_generation_models=["a"]))
case("_static_catalog", normalized="anthropic", fetcher={"fn": 1},
     io=io(static_catalog=["a"], drop_delisted_opencode_models=curated("a"), without_generation_models=curated("a")))
case("_static_catalog", normalized="xai", fetcher=None, env={"models_dev_preferred": ["xai"]},
     io=io(static_catalog=["a"], merge_with_models_dev=["a", "b"], drop_delisted_opencode_models=["a", "b"],
           xai_finalize_catalog=["b", "a"], without_generation_models=["b", "a"]))
case("_static_catalog", normalized="openai", fetcher={"fn": 1}, env={"models_dev_preferred": ["openai"]},
     io=io(static_catalog=["a"], merge_with_models_dev=curated("a", "b"),
           drop_delisted_opencode_models=curated("a", "b"), without_generation_models=curated("a", "b")))
case("_static_catalog", normalized="xai-oauth", fetcher=None, env={"models_dev_preferred": ["xai-oauth"]},
     io=io(static_catalog=["a"], merge_with_models_dev=["a"], drop_delisted_opencode_models=["a"],
           xai_finalize_catalog=["a"], without_generation_models=["a"]))
case("_static_catalog", normalized="anthropic", fetcher=None, io=io(static_catalog=raise_("RuntimeError", "x")))
case("_static_catalog", normalized="kimi", fetcher=None, env={"models_dev_preferred": ["kimi"]},
     io=io(static_catalog=["a"], merge_with_models_dev=["a", "b"], drop_delisted_opencode_models=["a", "b"],
           without_generation_models=["a", "b"]))

# ── _store_cache_entry ──────────────────────────────────────────────────────
case("_store_cache_entry", cache_key="k", entry={"fp": "f"},
     io=io(provider_models_cache_path="/h/cache.json",
           load_provider_models_cache={"old": {"fp": "g"}}, save_provider_models_cache=None))
case("_store_cache_entry", cache_key="k", entry={"fp": "f"},
     io=io(provider_models_cache_path="/h/cache.json", load_provider_models_cache={},
           save_provider_models_cache=None))
case("_store_cache_entry", cache_key="k", entry={"fp": "f"},
     io=io(provider_models_cache_path="/h/cache.json", write_under_cache_lock=raise_("RuntimeError", "lock")))
case("_store_cache_entry", cache_key="k", entry={"fp": "f"},
     io=io(provider_models_cache_path="/h/cache.json", load_provider_models_cache=raise_("OSError", "read")))
case("_store_cache_entry", cache_key="k", entry={"fp": "f"},
     io=io(provider_models_cache_path=raise_("RuntimeError", "path")))
case("_store_cache_entry", cache_key="k", entry={"fp": "f"},
     io=io(provider_models_cache_path="/h/cache.json", load_provider_models_cache=None))
case("_store_cache_entry", cache_key="k", entry={"fp": "f"},
     io=io(provider_models_cache_path="/h/cache.json", load_provider_models_cache={},
           save_provider_models_cache=raise_("OSError", "write")))

# ── cached_fetch_api_models ─────────────────────────────────────────────────
case("cached_fetch_api_models", api_key="k", base_url=None, cache_only=True)
case("cached_fetch_api_models", api_key="k", base_url=None, cache_only=False, fetch_models={"fn": 1},
     io=io(fetch_models=["m"], without_generation_models=["m"]))
case("cached_fetch_api_models", api_key="k", base_url="https://x/", cache_only=False,
     io=io(custom_endpoint_fingerprint="f",
           load_provider_models_cache={"custom:https://x#f": {"fp": "f", "at": 100, "models": ["a"], "native_catalog": False}},
           **{"time.time": 150}, cache_entry_valid=True, without_generation_models=["a"]))
case("cached_fetch_api_models", api_key="k", base_url="https://x", cache_only=True,
     io=io(custom_endpoint_fingerprint="f", load_provider_models_cache={}, **{"time.time": 150},
           cache_entry_valid=False))
case("cached_fetch_api_models", api_key="k", base_url="https://x", force_refresh=True,
     io=io(custom_endpoint_fingerprint="f", load_provider_models_cache={}, **{"time.time": 100},
           materialize_probe_api_key="k", fetch_api_models=["live"],
           provider_models_cache_path="/h/c.json", save_provider_models_cache=None,
           without_generation_models=["live"]))
case("cached_fetch_api_models", api_key="k", base_url="https://x", cache_only=False,
     io=io(custom_endpoint_fingerprint="f",
           load_provider_models_cache={"custom:https://x#f": {"fp": "f", "at": 100, "models": ["a"], "native_catalog": False}},
           **{"time.time": 5000}, cache_entry_valid=True, spawn_swr_refresh=None, without_generation_models=["a"]))
case("cached_fetch_api_models", api_key="k", base_url="https://x", cache_only=False,
     io=io(custom_endpoint_fingerprint="f",
           load_provider_models_cache={"custom:https://x#f": {"fp": "f", "at": 100, "models": [], "native_catalog": True}},
           **{"time.time": 150}, cache_entry_valid=True, without_generation_models=native()))
case("cached_fetch_api_models", api_key="k", base_url="https://x", cache_only=False,
     io=io(custom_endpoint_fingerprint="f",
           load_provider_models_cache={"custom:https://x#f": {"fp": "f", "at": 100, "models": ["a"], "native_catalog": False}},
           **{"time.time": 150}, cache_entry_valid=seq(False, True), materialize_probe_api_key="k",
           fetch_api_models=None, without_generation_models=["a"]))
case("cached_fetch_api_models", api_key="k", base_url="https://x", cache_only=False,
     io=io(custom_endpoint_fingerprint="f", load_provider_models_cache={}, **{"time.time": 150},
           cache_entry_valid=False, materialize_probe_api_key="k", fetch_api_models=None,
           without_generation_models=None))
case("cached_fetch_api_models", api_key="k", base_url="https://x", fetch_models={"fn": 1}, force_refresh=True,
     io=io(custom_endpoint_fingerprint="f", load_provider_models_cache={}, **{"time.time": 100},
           fetch_models=["m"], provider_models_cache_path="/h/c.json", save_provider_models_cache=None,
           without_generation_models=["m"]))
case("cached_fetch_api_models", api_key="k", base_url="https://x",
     io=io(custom_endpoint_fingerprint=raise_("RuntimeError", "fp")))
case("cached_fetch_api_models", api_key="k", base_url="https://x", force_refresh=True,
     io=io(custom_endpoint_fingerprint="f", load_provider_models_cache={}, **{"time.time": 100},
           materialize_probe_api_key="k", fetch_api_models=native(),
           provider_models_cache_path="/h/c.json", save_provider_models_cache=None,
           without_generation_models=native()))


def main():
    # per-function coverage check (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        raise SystemExit(f"generator: functions with <5 cases: {short}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"gen_models_flow2_cases: wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")


if __name__ == "__main__":
    main()