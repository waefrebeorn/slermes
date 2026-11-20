#!/usr/bin/env python3
"""Generate tests/fixtures/config_env_flow2.jsonl for the config_env_flow2 FLOW
oracle (hermes_cli/web_routers/config_env.py UNCLAIMED remainder). >=5 cases per
ported function (17 funcs), exercising decision branches, error paths,
BaseException propagation, mutation state (incl. partial on raise), identity
markers, kwarg-evaluation order and fail-closed missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; array = per-call queue (last repeats, empty -> None); single
value = constant; scripted null = None; {"__raise__": cls, "msg": ...} = exception
(HTTPException specs carry status_code). ASCII / POSIX-lexical domain; integral
numbers only in str()-computed positions; carriers are objects; body doubles expose
exactly the attributes the live body reads.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_env_flow2.jsonl")

REDACTED = ("Refusing to save a redacted credential preview; "
            "re-enter the full secret to replace it.")

CASES = []


def R(cls, msg="boom", **extra):
    return {"__raise__": cls, "msg": msg, **extra}


# ValueError-family probes at str(exc)-consuming catch sites (_store_endpoint_key,
# _env_write_errors) use UnicodeError, NOT UnicodeDecodeError: the latter's builtin
# __str__ formats from its 5 ctor args, so the scripted message cannot survive str()
# (the __new__ bypass keeps the kind but str(e) == ""). UnicodeError has a normal
# ctor (str(e) == msg) and still folds UnicodeError -> ValueError. The swallowed
# resp_json site keeps UnicodeDecodeError (no str() is taken there).


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# ── provider_card (pure; d.slug / d.label attribute order) ──
add("provider_card", {"d": {"slug": "openai", "label": "OpenAI"},
                      "description": "OpenAI API key", "url": "https://x.ai",
                      "is_password": True, "advanced": False})
add("provider_card", {"d": {"slug": "vertex", "label": "Vertex"},
                      "description": "", "url": None,
                      "is_password": False, "advanced": True})
add("provider_card", {"d": {"slug": "", "label": ""}, "description": "d",
                      "url": "", "is_password": False, "advanced": False})
add("provider_card", {"d": {"label": "NoSlug"}, "description": "d", "url": None,
                      "is_password": True, "advanced": False})
add("provider_card", {"d": {"slug": "nolabel"}, "description": "d", "url": None,
                      "is_password": True, "advanced": False})
add("provider_card", {"d": None, "description": "d", "url": None,
                      "is_password": False, "advanced": False})

# ── endpoint_api_mode (api_mode/transport read order; _canonical_api_mode seam) ──
add("endpoint_api_mode", {"entry": {"api_mode": "chat_completions"}},
    {"_canonical_api_mode": "chat_completions"})
add("endpoint_api_mode", {"entry": {"api_mode": "", "transport": "codex_responses"}},
    {"_canonical_api_mode": "codex_responses"})
add("endpoint_api_mode", {"entry": {"api_mode": None, "transport": None}},
    {"_canonical_api_mode": ""})
add("endpoint_api_mode", {"entry": {"api_mode": "responses"}},
    {"_canonical_api_mode": "codex_responses"})
add("endpoint_api_mode", {"entry": {"api_mode": "x"}},
    {"_canonical_api_mode": "CHAT_COMPLETIONS"})
add("endpoint_api_mode", {"entry": {"api_mode": "x"}},
    {"_canonical_api_mode": "unknown_mode"})
add("endpoint_api_mode", {"entry": {"api_mode": 0, "transport": "anthropic_messages"}},
    {"_canonical_api_mode": "anthropic_messages"})
add("endpoint_api_mode", {"entry": {"api_mode": "x"}},
    {"_canonical_api_mode": R("SystemExit", "bye")})
add("endpoint_api_mode", {"entry": {"api_mode": "x"}}, {})  # fail-closed
add("endpoint_api_mode", {"entry": {"api_mode": "x"}}, {"_canonical_api_mode": 5})
add("endpoint_api_mode", {"entry": None}, {"_canonical_api_mode": ""})

# ── endpoint_row (CLAIMED _api_key_display seam + REAL _endpoint_api_mode) ──
ROW = {"endpoint_id": "p1", "name": "Prov One", "base_url": "http://h/v1",
       "model": "m1", "models": ["m1", "m2"], "context_length": 128,
       "discover_models": True, "is_current": True, "source": "providers"}
add("endpoint_row", {**ROW, "key_entry": {"api_key": "sk-1"}},
    {"_api_key_display": [[True, "sk-1***"]], "_canonical_api_mode": "chat_completions"})
add("endpoint_row", {**ROW, "key_entry": {}, "context_length": None,
                     "models": [], "discover_models": False, "is_current": False},
    {"_api_key_display": [[False, None]], "_canonical_api_mode": ""})
add("endpoint_row", {**ROW, "key_entry": {"key_env": "E"}},
    {"_api_key_display": [[True, "${E}"]], "_canonical_api_mode": "anthropic_messages"})
add("endpoint_row", {**ROW, "key_entry": {"transport": "codex_responses"}},
    {"_api_key_display": [[False, None]], "_canonical_api_mode": "codex_responses"})
add("endpoint_row", {**ROW, "key_entry": {}},
    {"_api_key_display": R("ValueError", "bad")})
add("endpoint_row", {**ROW, "key_entry": {}}, {})  # fail-closed
add("endpoint_row", {**ROW, "key_entry": {}},
    {"_api_key_display": [[True, "x"]], "_canonical_api_mode": R("KeyboardInterrupt", "ki")})
add("endpoint_row", {**ROW, "key_entry": {}}, {"_api_key_display": [[True]]})
add("endpoint_row", {**ROW, "key_entry": {}}, {"_api_key_display": None})

# ── model_names_provider (coerce_provider_id x1..x3; custom: removeprefix) ──
add("model_names_provider",
    {"model_cfg": {"provider": "custom:myprov"}, "provider_key": "MyProv", "entry": None},
    {"coerce_provider_id": ["MyProv"]})
add("model_names_provider",
    {"model_cfg": {"provider": "MyProv"}, "provider_key": "myprov",
     "entry": {"name": "Other"}},
    {"coerce_provider_id": ["myprov", "Other", "Other"]})
add("model_names_provider",
    {"model_cfg": {"provider": "other"}, "provider_key": "myprov",
     "entry": {"name": ""}},
    {"coerce_provider_id": ["myprov", ""]})
add("model_names_provider",
    {"model_cfg": {"provider": "other"}, "provider_key": "myprov",
     "entry": {"name": None}},
    {"coerce_provider_id": ["myprov", ""]})
add("model_names_provider",
    {"model_cfg": {"provider": "myprov"}, "provider_key": "myprov", "entry": "x"},
    {"coerce_provider_id": ["myprov"]})
add("model_names_provider",
    {"model_cfg": {"provider": " Custom:X "}, "provider_key": "X", "entry": None},
    {"coerce_provider_id": ["X"]})
add("model_names_provider",
    {"model_cfg": {"provider": "myprov"}, "provider_key": "myprov", "entry": None},
    {"coerce_provider_id": R("SystemExit", "bye")})
add("model_names_provider",
    {"model_cfg": {"provider": "myprov"}, "provider_key": "myprov", "entry": None}, {})
add("model_names_provider",
    {"model_cfg": {"provider": "same"}, "provider_key": "same",
     "entry": {"name": "Same"}},
    {"coerce_provider_id": ["same", "Same", "Same"]})
add("model_names_provider",
    {"model_cfg": {"provider": "custom:custom:x"}, "provider_key": "custom:x",
     "entry": None},
    {"coerce_provider_id": ["custom:x"]})
add("model_names_provider",
    {"model_cfg": None, "provider_key": "p", "entry": None},
    {"coerce_provider_id": ["p"]})
add("model_names_provider",
    {"model_cfg": {"provider": "p"}, "provider_key": "p", "entry": None},
    {"coerce_provider_id": 5})
add("model_names_provider",
    {"model_cfg": {"provider": None}, "provider_key": "p", "entry": {"name": "N"}},
    {"coerce_provider_id": ["p", "N", "N"]})

# ── resolve_custom_endpoint_entry (find_provider_entry + CLAIMED _custom_endpoint_id) ──
PROVS = {"local-127.0.0.1:8283": {"base_url": "http://127.0.0.1:8283/v1"}}
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "local-127.0.0.1:8283"},
    {"find_provider_entry": [["local-127.0.0.1:8283", {"base_url": "http://127.0.0.1:8283/v1"}]]})
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "My Prov"},
    {"find_provider_entry": [[None, None], ["myprov", {"base_url": "http://x"}]],
     "_custom_endpoint_id": "myprov"})
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "myprov"},
    {"find_provider_entry": [[None, None]], "_custom_endpoint_id": "myprov"})
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "x"},
    {"find_provider_entry": R("ValueError", "bad")})
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "x"},
    {"find_provider_entry": [[None, None]], "_custom_endpoint_id": R("SystemExit", "bye")})
add("resolve_custom_endpoint_entry", {"providers": PROVS, "endpoint_id": "x"}, {})
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "x"},
    {"find_provider_entry": [[None, {}]]})
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "x"},
    {"find_provider_entry": None})
add("resolve_custom_endpoint_entry",
    {"providers": None, "endpoint_id": "x"},
    {"find_provider_entry": [[None, None]], "_custom_endpoint_id": "x"})
add("resolve_custom_endpoint_entry",
    {"providers": PROVS, "endpoint_id": "x"},
    {"find_provider_entry": [[None, None], R("KeyboardInterrupt", "ki")],
     "_custom_endpoint_id": "slug"})

# ── raw_provider_api_key (read_raw_config + find_provider_entry) ──
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": {"p": {"api_key": "sk"}}},
     "find_provider_entry": [["p", {"api_key": "sk"}]]})
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": {"p": {"api_key": None}}},
     "find_provider_entry": [["p", {"api_key": None}]]})
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": {"p": {}}},
     "find_provider_entry": [["p", {}]]})
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": {}}, "find_provider_entry": [[None, None]]})
add("raw_provider_api_key", {"endpoint_id": "p"}, {"read_raw_config": None})
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": {}},
     "find_provider_entry": R("KeyboardInterrupt", "ki")})
add("raw_provider_api_key", {"endpoint_id": "p"}, {})  # fail-closed
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": None}, "find_provider_entry": [[None, None]]})
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": {"p": "s"}}, "find_provider_entry": [["p", "s"]]})
add("raw_provider_api_key", {"endpoint_id": "p"},
    {"read_raw_config": {"providers": {"p": {"api_key": ""}}},
     "find_provider_entry": [["p", {"api_key": ""}]]})

# ── pop_legacy_custom_provider (CLAIMED _custom_endpoint_id; mutates cfg) ──
CFG2 = {"custom_providers": [{"name": "My Prov", "base_url": "http://a"},
                             {"name": "other", "base_url": "http://b"}],
        "model": {"default": "m"}}
add("pop_legacy_custom_provider", {"cfg": CFG2, "provider_key": "myprov"},
    {"_custom_endpoint_id": ["myprov", "other"]})
add("pop_legacy_custom_provider", {"cfg": CFG2, "provider_key": "other"},
    {"_custom_endpoint_id": ["myprov", "other"]})
add("pop_legacy_custom_provider", {"cfg": CFG2, "provider_key": "zzz"},
    {"_custom_endpoint_id": ["myprov", "other"]})
add("pop_legacy_custom_provider",
    {"cfg": {"custom_providers": {"name": "x"}}, "provider_key": "x"},
    {"_custom_endpoint_id": "x"})
add("pop_legacy_custom_provider", {"cfg": {}, "provider_key": "x"}, {})
add("pop_legacy_custom_provider",
    {"cfg": {"custom_providers": [None, "s", 5, {"name": "a"}]}, "provider_key": "a"},
    {"_custom_endpoint_id": ["a"]})
add("pop_legacy_custom_provider", {"cfg": CFG2, "provider_key": "myprov"},
    {"_custom_endpoint_id": R("SystemExit", "bye")})
add("pop_legacy_custom_provider", {"cfg": CFG2, "provider_key": "myprov"}, {})
add("pop_legacy_custom_provider",
    {"cfg": {"custom_providers": [{"name": None}]}, "provider_key": ""},
    {"_custom_endpoint_id": [""]})
add("pop_legacy_custom_provider",
    {"cfg": {"custom_providers": [{"name": 5}]}, "provider_key": "5"},
    {"_custom_endpoint_id": ["5"]})
add("pop_legacy_custom_provider", {"cfg": None, "provider_key": "x"}, {})
add("pop_legacy_custom_provider",
    {"cfg": {"custom_providers": []}, "provider_key": "x"}, {})

# ── detach_main_model_from_provider (REAL _model_names_provider; mutates cfg) ──
CFGD = {"model": {"provider": "custom:myprov", "base_url": "http://a",
                  "api_key": "sk", "key_env": "E", "default": "m"}}
add("detach_main_model_from_provider",
    {"cfg": CFGD, "provider_key": "myprov", "entry": None},
    {"coerce_provider_id": ["myprov"]})
add("detach_main_model_from_provider",
    {"cfg": {"model": {"provider": "other", "default": "m"}},
     "provider_key": "myprov", "entry": None},
    {"coerce_provider_id": ["myprov"]})
add("detach_main_model_from_provider", {"cfg": {"model": None},
                                        "provider_key": "p", "entry": None}, {})
add("detach_main_model_from_provider", {"cfg": {}, "provider_key": "p",
                                        "entry": None}, {})
add("detach_main_model_from_provider",
    {"cfg": {"model": {"provider": "custom:myprov", "api_key": "sk"}},
     "provider_key": "x", "entry": {"name": "MyProv"}},
    {"coerce_provider_id": ["x", "MyProv", "MyProv"]})
add("detach_main_model_from_provider",
    {"cfg": CFGD, "provider_key": "myprov", "entry": None},
    {"coerce_provider_id": R("GeneratorExit", "ge")})
add("detach_main_model_from_provider",
    {"cfg": CFGD, "provider_key": "myprov", "entry": None}, {})
add("detach_main_model_from_provider",
    {"cfg": {"model": {"provider": "myprov"}}, "provider_key": "myprov",
     "entry": None},
    {"coerce_provider_id": ["myprov"]})
add("detach_main_model_from_provider", {"cfg": None, "provider_key": "p",
                                        "entry": None}, {})

# ── drop_stale_addresses (pure pop; mutates entry) ──
add("drop_stale_addresses",
    {"entry": {"base_url": "http://a", "url": "http://b", "api": "http://c",
               "baseUrl": "http://d", "name": "n"}})
add("drop_stale_addresses", {"entry": {"base_url": "http://a"}})
add("drop_stale_addresses", {"entry": {}})
add("drop_stale_addresses", {"entry": {"url": None, "api": None}})
add("drop_stale_addresses", {"entry": {"baseUrl": "x", "api_mode": "m"}})
add("drop_stale_addresses", {"entry": None})

# ── store_endpoint_key (store_secret seam; ValueError -> 400; mutates entry) ──
add("store_endpoint_key",
    {"entry": {"api_key": "old"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": None},
    {"store_secret": "E"})
add("store_endpoint_key",
    {"entry": {"api_key": "old"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": None},
    {"store_secret": R("ValueError", "bad name")})
add("store_endpoint_key",
    {"entry": {"api_key": "old"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": None},
    {"store_secret": R("UnicodeError", "utf8")})
add("store_endpoint_key",
    {"entry": {"api_key": "old"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": None},
    {"store_secret": R("SystemExit", "bye")})
add("store_endpoint_key",
    {"entry": {"api_key": "old"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": None},
    {"store_secret": R("HTTPException", "no", status_code=403)})
add("store_endpoint_key",
    {"entry": {"api_key": "old"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": None}, {})  # fail-closed
add("store_endpoint_key",
    {"entry": {"api_key": None, "key_env": "OLD"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": "p"},
    {"store_secret": "E"})
add("store_endpoint_key",
    {"entry": {"api_key": "old"}, "env_var": "E", "key": "sk",
     "base_url": "http://a", "own_provider": None},
    {"store_secret": R("PermissionError", "denied")})
add("store_endpoint_key",
    {"entry": {}, "env_var": None, "key": "sk", "base_url": "http://a",
     "own_provider": None},
    {"store_secret": None})

# ── apply_endpoint_key (the key write/clear/migrate tree; mutates entry) ──
AK = {"entry": {"api_key": "old", "key_env": "OLD"}, "endpoint_id": "p1",
      "base_url": "http://a/v1", "stored_key": "p1"}
add("apply_endpoint_key", {**AK, "body": {"api_key": " sk-new "}},
    {"dashboard_endpoint_key_env": "E", "is_redacted_credential_preview": False,
     "store_secret": "E"})
add("apply_endpoint_key", {**AK, "body": {"api_key": "${KEY_ENV}"}},
    {"dashboard_endpoint_key_env": "E"})
add("apply_endpoint_key", {**AK, "body": {"api_key": "***"}},
    {"dashboard_endpoint_key_env": "E", "is_redacted_credential_preview": True})
add("apply_endpoint_key", {**AK, "body": {"api_key": "sk"}},
    {"dashboard_endpoint_key_env": "E",
     "is_redacted_credential_preview": R("SystemExit", "bye")})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E", "api_key": "old"}, "body": {"api_key": "  "},
     "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {"p": {}}},
     "env_reference_owners": [[]], "remove_env_value": True})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {"p": {}}},
     "env_reference_owners": [[{"__own__": True}]], "remove_env_value": True})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {"p": {}}},
     "env_reference_owners": [[{"model": "x"}]]})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {}},
     "env_reference_owners": [[None]], "remove_env_value": True})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {}},
     "env_reference_owners": [["x"]]})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {"p": {}}},
     "env_reference_owners": R("ValueError", "bad")})
add("apply_endpoint_key",
    {**AK, "entry": {"api_key": " sk-old "}, "body": {"api_key": None}},
    {"dashboard_endpoint_key_env": "E", "_config_api_key_is_env_ref": False,
     "store_secret": "E"})
add("apply_endpoint_key",
    {**AK, "entry": {"api_key": "sk-old"}, "body": {"api_key": None}},
    {"dashboard_endpoint_key_env": "E", "_config_api_key_is_env_ref": True})
add("apply_endpoint_key",
    {**AK, "entry": {"api_key": ""}, "body": {"api_key": None}},
    {"dashboard_endpoint_key_env": "E"})
add("apply_endpoint_key", {**AK, "body": {"api_key": "sk"}}, {})  # fail-closed
add("apply_endpoint_key", {**AK, "body": {"api_key": "sk"}},
    {"dashboard_endpoint_key_env": "E", "is_redacted_credential_preview": False,
     "store_secret": R("ValueError", "denylisted")})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {"p": {}}},
     "env_reference_owners": [[]], "remove_env_value": R("KeyboardInterrupt", "ki")})
add("apply_endpoint_key", {**AK, "entry": {}, "body": {"api_key": None}},
    {"dashboard_endpoint_key_env": "E"})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": None})
add("apply_endpoint_key",
    {**AK, "entry": {"api_key": "sk"}, "body": {"api_key": None}},
    {"dashboard_endpoint_key_env": "E",
     "_config_api_key_is_env_ref": R("SystemExit", "bye")})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E"}, "body": {"api_key": ""}, "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": None},
     "env_reference_owners": [[{"__own__": True}]], "remove_env_value": True})
add("apply_endpoint_key",
    {**AK, "entry": {"key_env": "E", "api_key": "old"}, "body": {"api_key": ""},
     "stored_key": "p"},
    {"dashboard_endpoint_key_env": "E", "read_raw_config": {"providers": {"p": {}}},
     "env_reference_owners": [[{"__own__": True}, {"other": 1}]]})
add("apply_endpoint_key",
    {**AK, "body": {"api_key": "${}"}},
    {"dashboard_endpoint_key_env": "E", "is_redacted_credential_preview": False,
     "store_secret": "E"})
add("apply_endpoint_key",
    {**AK, "entry": {"api_key": "sk", "key_env": "OLD"},
     "body": {"api_key": "ab...yz"}},
    {"dashboard_endpoint_key_env": "E", "is_redacted_credential_preview": True})

# ── save_env_credential (load -> save -> surface -> record order) ──
add("save_env_credential", {"key": "K", "value": "v", "provider_setup": False},
    {"load_env": {"K": "old"}, "save_provider_env_credential": {"ok": True},
     "web_setup_surface": "web", "record_api_key_saved": None})
add("save_env_credential", {"key": "K", "value": "v"},
    {"load_env": {}, "save_provider_env_credential": {"ok": True},
     "web_setup_surface": "web", "record_api_key_saved": None})
add("save_env_credential", {"key": "K", "value": "v"},
    {"load_env": {"K": None}, "save_provider_env_credential": {"ok": True},
     "web_setup_surface": "web", "record_api_key_saved": None})
add("save_env_credential", {"key": "K", "value": "v"},
    {"load_env": {"K": "old"}, "save_provider_env_credential": R("ValueError", "bad")})
add("save_env_credential", {"key": "K", "value": "v"},
    {"load_env": {}, "save_provider_env_credential": {"ok": True},
     "web_setup_surface": R("SystemExit", "bye")})
add("save_env_credential", {"key": "K", "value": "v"},
    {"load_env": {}, "save_provider_env_credential": {"ok": True},
     "web_setup_surface": "web", "record_api_key_saved": R("RuntimeError", "m")})
add("save_env_credential", {"key": "K", "value": "v"}, {})  # fail-closed
add("save_env_credential", {"key": "K", "value": "v", "provider_setup": True},
    {"load_env": {"K": "old"}, "save_provider_env_credential": {"ok": True},
     "web_setup_surface": "web", "record_api_key_saved": None})
add("save_env_credential", {"key": "K", "value": "v"}, {"load_env": None})
add("save_env_credential", {"key": "K", "value": "v"},
    {"load_env": {}, "save_provider_env_credential": None,
     "web_setup_surface": "web", "record_api_key_saved": None})

# ── drop_carried_route_state (model / delegation / aux call order) ──
M1 = {"provider": "a", "base_url": "http://a"}
M2 = {"provider": "b", "base_url": "http://b"}
add("drop_carried_route_state",
    {"existing": {"model": M1}, "merged": {"model": M2}},
    {"drop_carried_route_state": None})
add("drop_carried_route_state",
    {"existing": {"model": M1, "delegation": M1},
     "merged": {"model": M2, "delegation": {"base_url": "http://d", "provider": "named"}}},
    {"drop_carried_route_state": [None, None]})
add("drop_carried_route_state",
    {"existing": {"model": M1, "delegation": M1},
     "merged": {"model": M2, "delegation": {"base_url": "http://d", "provider": "custom"}}},
    {"drop_carried_route_state": None})
add("drop_carried_route_state",
    {"existing": {"delegation": M1},
     "merged": {"delegation": {"base_url": "http://d", "provider": " Auto "}}},
    {"drop_carried_route_state": None})
add("drop_carried_route_state",
    {"existing": {"delegation": M1},
     "merged": {"delegation": {"base_url": "", "provider": "custom"}}},
    {"drop_carried_route_state": [None, None]})
add("drop_carried_route_state",
    {"existing": {"delegation": M1}, "merged": {"delegation": "x"}},
    {"drop_carried_route_state": [None, None]})
add("drop_carried_route_state",
    {"existing": {"auxiliary": {"t1": M1, "t2": M2}},
     "merged": {"auxiliary": {"t1": M2, "t3": M1}}},
    {"drop_carried_route_state": [None, None, None]})
add("drop_carried_route_state",
    {"existing": {"model": M1, "delegation": M1},
     "merged": {"model": M2, "delegation": {"provider": "named"}}},
    {"drop_carried_route_state": [None, R("ValueError", "bad")]})
add("drop_carried_route_state", {"existing": {"model": M1}, "merged": {"model": M2}}, {})
add("drop_carried_route_state",
    {"existing": {"model": M1}, "merged": {"model": M2}},
    {"drop_carried_route_state": R("SystemExit", "bye")})
add("drop_carried_route_state", {"existing": None, "merged": {"model": M2}},
    {"drop_carried_route_state": None})
add("drop_carried_route_state",
    {"existing": {"auxiliary": "x"}, "merged": {"auxiliary": {"t1": M1}}},
    {"drop_carried_route_state": None})
add("drop_carried_route_state",
    {"existing": {}, "merged": {"delegation": {"base_url": "http://d", "provider": None}}},
    {"drop_carried_route_state": None})
add("drop_carried_route_state",
    {"existing": {"auxiliary": {"t1": M1}},
     "merged": {"auxiliary": {"t1": M2, "t2": M1}, "model": M2}},
    {"drop_carried_route_state": [None, None, R("KeyboardInterrupt", "ki")]})

# ── no_models_probe_message (resp.json seam; 3 message shapes) ──
URL = "http://h/v1"
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "text/html; charset=utf-8"}}, "base_url": URL},
    {"resp_json": R("ValueError", "not json")})
add("no_models_probe_message", {"resp": {"headers": {}}, "base_url": URL},
    {"resp_json": R("ValueError", "not json")})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "application/json"}}, "base_url": URL},
    {"resp_json": {"data": []}})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "application/json"}}, "base_url": URL},
    {"resp_json": {"data": "x"}})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "application/json"}}, "base_url": URL},
    {"resp_json": {"other": 1}})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "application/json"}}, "base_url": URL},
    {"resp_json": [[1, 2]]})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "text/plain"}}, "base_url": URL},
    {"resp_json": "text"})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "text/html"}}, "base_url": URL},
    {"resp_json": R("UnicodeDecodeError", "utf8")})
add("no_models_probe_message",
    {"resp": {"headers": {}}, "base_url": URL},
    {"resp_json": R("SystemExit", "bye")})
add("no_models_probe_message", {"resp": {"headers": {}}, "base_url": URL}, {})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "  application/json ; x"}}, "base_url": URL},
    {"resp_json": None})
add("no_models_probe_message", {"resp": None, "base_url": URL},
    {"resp_json": None})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": "application/json"}}, "base_url": URL},
    {"resp_json": {"data": [{"id": "m"}]}})
add("no_models_probe_message",
    {"resp": {"headers": {"content-type": 5}}, "base_url": URL},
    {"resp_json": None})

# ── auto_api_mode (_detect_api_mode_for_url seam + fallback) ──
add("auto_api_mode", {"base_url": "http://h"},
    {"_detect_api_mode_for_url": "anthropic_messages"})
add("auto_api_mode", {"base_url": "http://h"}, {"_detect_api_mode_for_url": None})
add("auto_api_mode", {"base_url": "http://h"}, {"_detect_api_mode_for_url": ""})
add("auto_api_mode", {"base_url": "http://h"}, {"_detect_api_mode_for_url": 0})
add("auto_api_mode", {"base_url": "http://h"},
    {"_detect_api_mode_for_url": R("SystemExit", "bye")})
add("auto_api_mode", {"base_url": "http://h"}, {})  # fail-closed
add("auto_api_mode", {"base_url": "http://h/v1/responses"},
    {"_detect_api_mode_for_url": "codex_responses"})

# ── env_write_errors (CM exit classification; _log.exception seam) ──
add("env_write_errors", {"log_msg": "PUT /api/env failed", "body_raise": None}, {})
add("env_write_errors",
    {"log_msg": "PUT /api/env failed", "body_raise": R("ValueError", "bad name")}, {})
add("env_write_errors",
    {"log_msg": "x", "body_raise": R("HTTPException", "not found", status_code=404)}, {})
add("env_write_errors",
    {"log_msg": "PUT /api/env failed", "body_raise": R("RuntimeError", "boom")},
    {"_log.exception": None})
add("env_write_errors", {"log_msg": "x", "body_raise": R("SystemExit", "bye")}, {})
add("env_write_errors", {"log_msg": "x", "body_raise": R("KeyboardInterrupt", "ki")}, {})
add("env_write_errors", {"log_msg": "x", "body_raise": R("GeneratorExit", "ge")}, {})
add("env_write_errors",
    {"log_msg": "x", "body_raise": R("UnicodeError", "u")}, {})
add("env_write_errors",
    {"log_msg": "x", "body_raise": R("RuntimeError", "boom")}, {})  # fail-closed log
add("env_write_errors",
    {"log_msg": "x", "body_raise": R("CustomKind", "x")}, {"_log.exception": None})
add("env_write_errors",
    {"log_msg": "x", "body_raise": R("Exception", "e")}, {"_log.exception": None})
add("env_write_errors",
    {"log_msg": "x", "body_raise": R("BaseExceptionGroup", "g")}, {})
add("env_write_errors",
    {"log_msg": "x", "body_raise": R("RuntimeError", "boom")},
    {"_log.exception": R("ValueError", "log fail")})
add("env_write_errors",
    {"log_msg": "DELETE /api/env failed", "body_raise": R("PermissionError", "p")},
    {"_log.exception": None})

# ── record_custom_endpoint_setup (surface -> record order) ──
add("record_custom_endpoint_setup", {"home": "/h/u"},
    {"web_setup_surface": "web", "record_provider_setup_done": None})
add("record_custom_endpoint_setup", {"home": None},
    {"web_setup_surface": "web", "record_provider_setup_done": None})
add("record_custom_endpoint_setup", {"home": "/h/u"},
    {"web_setup_surface": R("SystemExit", "bye")})
add("record_custom_endpoint_setup", {"home": "/h/u"},
    {"web_setup_surface": "web", "record_provider_setup_done": R("RuntimeError", "m")})
add("record_custom_endpoint_setup", {"home": "/h/u"}, {})  # fail-closed
add("record_custom_endpoint_setup", {"home": ""},
    {"web_setup_surface": "desktop", "record_provider_setup_done": None})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()