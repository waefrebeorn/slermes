#!/usr/bin/env python3
"""Generator for tests/fixtures/models_dev_flow2.jsonl.

Emits >=5 cases per ported function of agent/models_dev.py (the 21 UNCLAIMED
defs ported as src/agent/models_dev_flow2.c). Each case is
{"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...}}}.

Script shapes (shared with the C universal seam md2_call and the oracle):
  - absent key            -> None (script a value explicitly for every seam a
                             case reaches)
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  - any other JSON value  -> reused for every call

Domain restrictions (documented in the port header + oracle):
  - ASCII str/strip operands; simple absolute POSIX paths (no '.' components,
    no trailing slash except the root "/"); numeric coercion within int64.
  - provider objects carry "name"/"models_dev_id"; profile objects carry
    "model_capabilities"; builtin_metadata keyed "provider|model" (lowercase
    model); "_fetch_models_dev_from_network" scripts are 2-element arrays.
  - list_providers scripts are always arrays (None is out of domain).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "models_dev_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


PMAP = {"openai": "openai", "anthropic": "anthropic", "novita": "novita-ai",
        "gemini": "google"}


def env(**kw):
    d = {"provider_map": dict(PMAP), "openrouter_providers": ["openrouter"]}
    d.update(kw)
    return d


# ── _profile_models_dev_ids ─────────────────────────────────────────────────
case("_profile_models_dev_ids", env=env(),
     io=io(list_providers=[{"name": "newprov", "models_dev_id": "np"}]))
case("_profile_models_dev_ids", env=env(),
     io=io(list_providers=[{"name": "openai", "models_dev_id": "openai"}]))
case("_profile_models_dev_ids", env=env(),
     io=io(list_providers=[{"name": "nodel", "models_dev_id": ""},
                           {"name": "nomdev"}]))
case("_profile_models_dev_ids", env=env(), io=io(list_providers=[]))
case("_profile_models_dev_ids", env=env(),
     io=io(list_providers=raise_("RuntimeError", "boom")))
case("_profile_models_dev_ids", env=env(),
     io=io(list_providers=[{"name": "a", "models_dev_id": "x"},
                           {"name": "b", "models_dev_id": "y"}]))

# ── models_dev_provider_map ─────────────────────────────────────────────────
case("models_dev_provider_map", env=env(), io=io(list_providers=[]))
case("models_dev_provider_map", env=env(),
     io=io(list_providers=[{"name": "np", "models_dev_id": "np-id"}]))
case("models_dev_provider_map", env=env(),
     io=io(list_providers=[{"name": "openai", "models_dev_id": "OVERRIDE"}]))
case("models_dev_provider_map", env=env(),
     io=io(list_providers=raise_("ValueError", "bad")))
case("models_dev_provider_map", env=env(provider_map={}),
     io=io(list_providers=[{"name": "solo", "models_dev_id": "s"}]))
case("models_dev_provider_map", env=env(),
     io=io(list_providers=[{"name": "openai", "models_dev_id": "openai"},
                           {"name": "zz", "models_dev_id": "zz"}]))

# ── models_dev_id_for ───────────────────────────────────────────────────────
case("models_dev_id_for", provider="", env=env(), io=io())
case("models_dev_id_for", provider="openai", env=env(), io=io())
case("models_dev_id_for", provider="  openai  ", env=env(), io=io())
case("models_dev_id_for", provider="np", env=env(),
     io=io(list_providers=[{"name": "np", "models_dev_id": "np-id"}]))
case("models_dev_id_for", provider="np", env=env(),
     io=io(list_providers=[]))
case("models_dev_id_for", provider="np", env=env(),
     io=io(list_providers=raise_("RuntimeError", "net")))

# ── _dict_or_empty ──────────────────────────────────────────────────────────
case("_dict_or_empty", value={"a": 1, "b": 2})
case("_dict_or_empty", value=None)
case("_dict_or_empty", value=[1, 2])
case("_dict_or_empty", value="text")
case("_dict_or_empty", value=5)
case("_dict_or_empty", value=True)

# ── _configured_catalog_provider ────────────────────────────────────────────
# provider in map -> None, no cfg seams (models_dev_id_for short-circuits)
case("_configured_catalog_provider", provider="openai", env=env(), io=io())
case("_configured_catalog_provider", provider="", env=env(), io=io())
# custom:x -> strip prefix -> models_dev_id_for("x") needs list_providers
case("_configured_catalog_provider", provider="custom:x", env=env(),
     io=io(list_providers=[], cfg_get={"catalog_provider": "openai"},
           load_config_readonly={}))
# empty alias -> legacy custom_providers entry supplies it
case("_configured_catalog_provider", provider="custom:x", env=env(),
     io=io(list_providers=[],
           cfg_get=seq({}, {"custom_providers": [
               {"name": "x", "catalog_provider": "anthropic"}]}),
           load_config_readonly=seq({}, {})))
# no alias anywhere -> null
case("_configured_catalog_provider", provider="custom:x", env=env(),
     io=io(list_providers=[], cfg_get={}, load_config_readonly={}))
# cfg_get raises RuntimeError -> _cfg_get swallows -> default None -> null
case("_configured_catalog_provider", provider="custom:x", env=env(),
     io=io(list_providers=[], cfg_get=raise_("RuntimeError", "boom"),
           load_config_readonly={}))
# BaseException propagates through _cfg_get
case("_configured_catalog_provider", provider="custom:x", env=env(),
     io=io(list_providers=[], cfg_get=raise_("KeyboardInterrupt"),
           load_config_readonly={}))

# ── _models_dev_id ──────────────────────────────────────────────────────────
case("_models_dev_id", provider="openai", env=env(), io=io())
case("_models_dev_id", provider="  anthropic  ", env=env(), io=io())
# custom:x with alias openai (known in provider_map) -> "openai"
case("_models_dev_id", provider="custom:x", env=env(),
     io=io(list_providers=[], cfg_get={"catalog_provider": "openai"},
           load_config_readonly={}, fetch_models_dev={}))
# unknown alias -> warning + null
case("_models_dev_id", provider="custom:x", env=env(),
     io=io(list_providers=[], cfg_get={"catalog_provider": "ghost"},
           load_config_readonly={}, fetch_models_dev={}, **{"logger.warning": None}))
# unknown alias already warned -> no warning, null
case("_models_dev_id", provider="custom:x",
     env=env(warned=[["x", "ghost"]]),
     io=io(list_providers=[], cfg_get={"catalog_provider": "ghost"},
           load_config_readonly={}, fetch_models_dev={}))
# fetch_models_dev raises -> exception propagates (not caught)
case("_models_dev_id", provider="custom:x", env=env(),
     io=io(list_providers=[], cfg_get={"catalog_provider": "openai"},
           load_config_readonly={}, fetch_models_dev=raise_("RuntimeError", "net")))
# no alias -> null
case("_models_dev_id", provider="custom:y", env=env(),
     io=io(list_providers=[], cfg_get={}, load_config_readonly={}))

# ── _cfg_get ────────────────────────────────────────────────────────────────
case("_cfg_get", keys=["a", "b"], default="dflt", config={"a": 1},
     env=env(), io=io(cfg_get=7))
case("_cfg_get", keys=["a"], default=0, env=env(),
     io=io(load_config_readonly={"a": 2}, cfg_get=2))
case("_cfg_get", keys=["a"], default="fallback", env=env(),
     io=io(load_config_readonly={}, cfg_get=raise_("RuntimeError", "bad")))
case("_cfg_get", keys=["a"], default="fallback", env=env(),
     io=io(load_config_readonly=raise_("OSError", "io")))
case("_cfg_get", keys=["a"], default="fallback", env=env(),
     io=io(load_config_readonly={}, cfg_get=raise_("KeyboardInterrupt")))
case("_cfg_get", keys=["a"], default=None, config={"a": 1}, env=env(),
     io=io(cfg_get=None))

# ── _hermes_path ────────────────────────────────────────────────────────────
case("_hermes_path", name="models_dev_cache.json", env=env(),
     io=io(get_hermes_home="/home/u/.hermes"))
case("_hermes_path", name="x", env=env(), io=io(get_hermes_home="/"))
case("_hermes_path", name="x", env=env(),
     io=io(get_hermes_home=raise_("OSError", "no home")))
case("_hermes_path", name="etag", env=env(),
     io=io(get_hermes_home="C:/Users/u/.hermes"))
case("_hermes_path", name="", env=env(), io=io(get_hermes_home="/h"))

# ── _quietly ────────────────────────────────────────────────────────────────
case("_quietly", what="load cache", default="FB", env=env(),
     io=io(quietly_fn={"a": 1}))
case("_quietly", what="load cache", default="FB", env=env(),
     io=io(quietly_fn=raise_("RuntimeError", "boom"), **{"logger.debug": None}))
case("_quietly", what="stat", default=0, env=env(),
     io=io(quietly_fn=raise_("KeyboardInterrupt")))
case("_quietly", what="x", default=None, env=env(), io=io(quietly_fn=None))
case("_quietly", what="x", default={"k": 1}, env=env(),
     io=io(quietly_fn=raise_("ValueError", "bad"), **{"logger.debug": None}))
case("_quietly", what="x", default=None, env=env(),
     io=io(quietly_fn=raise_("OSError", "io"), **{"logger.debug": None}))

# ── _serve_stale ────────────────────────────────────────────────────────────
case("_serve_stale", msg="stale", args=[], env=env(cache={"a": 1}),
     io=io(_mark_stale_cache_grace=None, _start_background_refresh_models_dev=None,
           **{"logger.debug": None}))
case("_serve_stale", msg="stale %s", args=["x", 2], env=env(cache={"a": 1}),
     io=io(_mark_stale_cache_grace=None, _start_background_refresh_models_dev=None,
           **{"logger.debug": None}))
case("_serve_stale", msg="stale", args=[], env=env(cache={}),
     io=io(_mark_stale_cache_grace=raise_("RuntimeError", "x")))
case("_serve_stale", msg="stale", args=[], env=env(cache={"a": 1}),
     io=io(_mark_stale_cache_grace=None,
           _start_background_refresh_models_dev=raise_("OSError", "y")))
case("_serve_stale", msg="stale", args=[], env=env(cache=None),
     io=io(_mark_stale_cache_grace=None, _start_background_refresh_models_dev=None,
           **{"logger.debug": raise_("RuntimeError", "log")}))
case("_serve_stale", msg="stale", args=[], env=env(),
     io=io(_mark_stale_cache_grace=None, _start_background_refresh_models_dev=None,
           **{"logger.debug": None}))

# ── _refresh_locked ─────────────────────────────────────────────────────────
case("_refresh_locked", where="foreground", env=env(cache={"old": 1}),
     io=io(_fetch_models_dev_from_network=[{"new": 2}, "ET"],
           _commit_registry=None))
case("_refresh_locked", where="background", env=env(cache={"old": 1}),
     io=io(_fetch_models_dev_from_network=raise_("_NotModified"),
           _confirm_cache_not_modified=None))
case("_refresh_locked", where="foreground", env=env(cache={}),
     io=io(_fetch_models_dev_from_network=raise_("RuntimeError", "net"),
           _note_refresh_failure=None))
case("_refresh_locked", where="foreground", env=env(cache={"old": 1}),
     io=io(_fetch_models_dev_from_network=raise_("KeyboardInterrupt")))
case("_refresh_locked", where="fg", env=env(cache={"old": 1}),
     io=io(_fetch_models_dev_from_network=[{"n": 1}, ""],
           _commit_registry=raise_("OSError", "disk")))
case("_refresh_locked", where="fg", env=env(),
     io=io(_fetch_models_dev_from_network=[{"n": 1}, "e"],
           _commit_registry=None))

# ── _registry_provider ──────────────────────────────────────────────────────
case("_registry_provider", mdev_id="openai", allow_network=True, env=env(),
     io=io(fetch_models_dev={"openai": {"name": "OpenAI"}}))
case("_registry_provider", mdev_id="openai", allow_network=False, env=env(),
     io=io(fetch_models_dev={"openai": {"name": "OpenAI"}}))
case("_registry_provider", mdev_id="openai", allow_network=True, env=env(),
     io=io(fetch_models_dev={"openai": "notdict"}))
case("_registry_provider", mdev_id="ghost", allow_network=True, env=env(),
     io=io(fetch_models_dev={}))
case("_registry_provider", mdev_id="openai", allow_network=True, env=env(),
     io=io(fetch_models_dev=raise_("RuntimeError", "net")))
case("_registry_provider", mdev_id="openai", allow_network=False, env=env(),
     io=io(fetch_models_dev={"openai": {"models": {}}}))

# ── _registry_models ────────────────────────────────────────────────────────
case("_registry_models", mdev_id="openai", allow_network=True, env=env(),
     io=io(fetch_models_dev={"openai": {"models": {"gpt": {"limit": {}}}}}))
case("_registry_models", mdev_id="openai", allow_network=False, env=env(),
     io=io(fetch_models_dev={"openai": {}}))
case("_registry_models", mdev_id="openai", allow_network=True, env=env(),
     io=io(fetch_models_dev={"openai": {"models": "notdict"}}))
case("_registry_models", mdev_id="ghost", allow_network=True, env=env(),
     io=io(fetch_models_dev={}))
case("_registry_models", mdev_id="openai", allow_network=True, env=env(),
     io=io(fetch_models_dev=raise_("ValueError", "bad")))
case("_registry_models", mdev_id="openai", allow_network=False, env=env(),
     io=io(fetch_models_dev={"openai": {"models": {"a": 1, "b": 2}}}))

# ── _openrouter_catalog_lookup_base ─────────────────────────────────────────
case("_openrouter_catalog_lookup_base", provider="openrouter",
     model="m:nitro", env=env(), io=io(openrouter_variant_base="m"))
case("_openrouter_catalog_lookup_base", provider="anthropic",
     model="m:nitro", env=env(), io=io())
case("_openrouter_catalog_lookup_base", provider="openrouter",
     model="m:free", env=env(),
     io=io(openrouter_variant_base=raise_("RuntimeError", "x")))
case("_openrouter_catalog_lookup_base", provider="openrouter",
     model="m", env=env(), io=io(openrouter_variant_base=None))
case("_openrouter_catalog_lookup_base", provider="custom",
     model="m", env=env(openrouter_providers=["custom"]),
     io=io(openrouter_variant_base="base"))

# ── _iter_model_entries ─────────────────────────────────────────────────────
MODELS = {"Model-A": {"limit": {"context": 100}}, "m:cloud": {"limit": {"context": 64}}}
case("_iter_model_entries", models=MODELS, model="Model-A",
     suffix_fallback=True, provider="openrouter", env=env(), io=io())
case("_iter_model_entries", models=MODELS, model="model-a",
     suffix_fallback=True, provider="openrouter", env=env(), io=io())
case("_iter_model_entries", models=MODELS, model="m",
     suffix_fallback=True, provider="openrouter", env=env(), io=io())
case("_iter_model_entries", models=MODELS, model="m",
     suffix_fallback=False, provider="openrouter", env=env(), io=io())
case("_iter_model_entries", models=MODELS, model="ghost",
     suffix_fallback=False, provider="openrouter", env=env(), io=io())
case("_iter_model_entries", models=MODELS, model="model-a",
     suffix_fallback=False, provider="openrouter", env=env(),
     io=io(openrouter_variant_base="m"))
case("_iter_model_entries", models=MODELS, model="x",
     suffix_fallback=True, provider="openrouter", env=env(),
     io=io(openrouter_variant_base=raise_("RuntimeError", "boom")))

# ── _extract_limit ──────────────────────────────────────────────────────────
case("_extract_limit", entry={"limit": {"context": 1000}}, key="context")
case("_extract_limit", entry={"limit": {"context": 0}}, key="context")
case("_extract_limit", entry={"limit": {"context": 2.7}}, key="context")
case("_extract_limit", entry={"limit": {"context": "100"}}, key="context")
case("_extract_limit", entry="notadict", key="context")
case("_extract_limit", entry={"limit": {"context": True}}, key="context")
case("_extract_limit", entry={"limit": {"output": 50}}, key="context")

# ── _builtin_model_metadata ─────────────────────────────────────────────────
BM = {"openai|gpt-6-astra": {"limit": {"context": 1050000}},
      "anthropic|claude-x": {"limit": {"context": 200000}}}
case("_builtin_model_metadata", provider="openai", model="gpt-6-astra",
     env=env(builtin_metadata=BM), io=io())
case("_builtin_model_metadata", provider="openai", model="GPT-6-ASTRA",
     env=env(builtin_metadata=BM), io=io())
case("_builtin_model_metadata", provider="openai", model="ghost",
     env=env(builtin_metadata=BM), io=io())
case("_builtin_model_metadata", provider="anthropic", model="claude-x",
     env=env(builtin_metadata=BM), io=io())
case("_builtin_model_metadata", provider="custom:x", model="gpt-6-astra",
     env=env(builtin_metadata=BM),
     io=io(list_providers=[], cfg_get={"catalog_provider": "openai"},
           load_config_readonly={}, fetch_models_dev={}))
case("_builtin_model_metadata", provider="custom:x", model="gpt-6-astra",
     env=env(builtin_metadata=BM),
     io=io(list_providers=[], cfg_get={"catalog_provider": "ghost"},
           load_config_readonly={}, fetch_models_dev={}, **{"logger.warning": None}))

# ── _relay_vision_marker_metadata ───────────────────────────────────────────
UB = {"limit": {"context": 200000}, "tool_call": True}
case("_relay_vision_marker_metadata", provider="opencode",
     model="deepseek-v4-flash-vision-exp", env=env(unknown_model_base=UB),
     io=io(opencode_provider_family="opencode"))
case("_relay_vision_marker_metadata", provider="opencode", model="foo",
     env=env(unknown_model_base=UB), io=io())
case("_relay_vision_marker_metadata", provider="opencode",
     model="X-VISION", env=env(unknown_model_base=UB),
     io=io(opencode_provider_family="opencode"))
case("_relay_vision_marker_metadata", provider="opencode",
     model="m-vision", env=env(unknown_model_base=UB),
     io=io(opencode_provider_family=None))
case("_relay_vision_marker_metadata", provider="opencode",
     model="m-vision", env=env(unknown_model_base=UB),
     io=io(opencode_provider_family=raise_("RuntimeError", "x")))

# ── _provider_model_capabilities ────────────────────────────────────────────
case("_provider_model_capabilities", provider="openai", model="m", env=env(),
     io=io(get_provider_profile={"model_capabilities": {"m": {"supports_tools": True}}}))
case("_provider_model_capabilities", provider="openai", model="m", env=env(),
     io=io(get_provider_profile=None))
case("_provider_model_capabilities", provider="openai", model="m", env=env(),
     io=io(get_provider_profile={}))
case("_provider_model_capabilities", provider="openai", model="ghost", env=env(),
     io=io(get_provider_profile={"model_capabilities": {"m": {"x": 1}}}))
case("_provider_model_capabilities", provider="openai", model="m", env=env(),
     io=io(get_provider_profile=raise_("ValueError", "x")))

# ── _apply_overrides ────────────────────────────────────────────────────────
case("_apply_overrides", provider="openai", model="m",
     entry={"limit": {"context": 100}}, env=env(builtin_metadata=BM,
     unknown_model_base=UB),
     io=io(get_provider_profile=None, _override_for=None))
case("_apply_overrides", provider="openai", model="m",
     entry={"limit": {"context": 100}}, env=env(builtin_metadata=BM,
     unknown_model_base=UB),
     io=io(get_provider_profile={"model_capabilities": {"m": {"reasoning": True}}},
           _merge_catalog_entry_with_override={"limit": {"context": 50}, "reasoning": True},
           _override_for=None))
case("_apply_overrides", provider="openai", model="m",
     entry={"limit": {"context": 100}}, env=env(builtin_metadata=BM,
     unknown_model_base=UB),
     io=io(get_provider_profile={}, _override_for={"context_window": 400},
           _merge_catalog_entry_with_override={"limit": {"context": 400}}))
case("_apply_overrides", provider="openai", model="m-vision", entry=None,
     env=env(builtin_metadata=BM, unknown_model_base=UB),
     io=io(get_provider_profile={}, _override_for=None,
           opencode_provider_family="opencode"))
case("_apply_overrides", provider="openai", model="m",
     entry={"limit": {"context": 100}}, env=env(builtin_metadata=BM,
     unknown_model_base=UB),
     io=io(get_provider_profile={}, _override_for=None,
           _merge_catalog_entry_with_override=raise_("RuntimeError", "merge")))
case("_apply_overrides", provider="openai", model="m",
     entry={"limit": {"context": 100}}, env=env(builtin_metadata=BM,
     unknown_model_base=UB),
     io=io(get_provider_profile={}, _override_for=raise_("ValueError", "ov")))
case("_apply_overrides", provider="openai", model="gpt-6-astra", entry=None,
     env=env(builtin_metadata=BM, unknown_model_base=UB),
     io=io(get_provider_profile=None, _override_for=None))

# ── _entry_supports_vision ──────────────────────────────────────────────────
case("_entry_supports_vision", entry={"modalities": {"input": ["text", "image"]}})
case("_entry_supports_vision", entry={"modalities": {"input": ["text"]},
                                      "attachment": True})
case("_entry_supports_vision", entry={"modalities": {}, "attachment": True})
case("_entry_supports_vision", entry={"attachment": False})
case("_entry_supports_vision", entry={"modalities": {"input": "notlist"},
                                      "attachment": True})
case("_entry_supports_vision", entry={})
case("_entry_supports_vision", entry={"modalities": {"input": "image"},
                                      "attachment": False})


def main():
    # verify >=5/func
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    assert set(counts) == set(PORTED), (set(PORTED) - set(counts), set(counts) - set(PORTED))
    low = {k: v for k, v in counts.items() if v < 5}
    assert not low, low
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs to {OUT}")


PORTED = sorted(set(c["func"] for c in CASES))

if __name__ == "__main__":
    main()
