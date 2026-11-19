#!/usr/bin/env python3
"""Generate tests/fixtures/auth_flow.jsonl — scripted-seam FLOW cases for the
auth_flow port (hermes_cli/auth.py UNCLAIMED auth surface: registry/config
leaves, secret/env gates, lock cluster, token/TTL markers, persistence/status).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header):
- ASCII strings, POSIX paths; integral ints (no integral-float literals in
  type-name-sensitive positions).
- Truthiness-gated records (get_provider, registry pconfig) are null or
  NON-EMPTY dicts carrying auth_type/api_key_env_vars.
- has_usable_secret / looks_like_openrouter_key scripts are bools.
- int() tokens: [+-]?digits, |pid| < 2^53, no underscores.
- os_name / lock_backend are fixture-driven platform doubles.
Run: python tests/gen_auth_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "auth_flow.jsonl")

CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def auth_err(message="boom", provider="", code=None, relogin=False):
    return {"kind": "AuthError", "message": message, "provider": provider,
            "code": code, "relogin_required": relogin}


CFG_IMPORT = {"import_load_config": None}
DOTENV_IMPORT = {"import_get_env_value_prefer_dotenv": None}


def cfg_seams(cfg, **over):
    s = dict(CFG_IMPORT)
    s["load_config"] = cfg
    s.update(over)
    return s


def dotenv_seams(value, usable, **over):
    s = dict(DOTENV_IMPORT)
    s["get_env_value_prefer_dotenv"] = value
    s["has_usable_secret"] = usable
    s.update(over)
    return s


# ── _api_key_provider ──
add("api_key_provider", {"id": "openai-api", "name": "OpenAI API",
                         "inference_base_url": "https://api.openai.com/v1",
                         "api_key_env_vars": ["OPENAI_API_KEY"],
                         "base_url_env_var": "OPENAI_BASE_URL"}, {})
add("api_key_provider", {"id": "x", "name": "X", "inference_base_url": "",
                         "api_key_env_vars": []}, {})
add("api_key_provider", {"id": "bedrock", "name": "AWS Bedrock",
                         "inference_base_url": "https://bedrock.example",
                         "api_key_env_vars": [], "base_url_env_var": "BEDROCK_BASE_URL",
                         "auth_type": "aws_sdk"}, {})
add("api_key_provider", {"id": "vertex", "name": "Google Vertex AI",
                         "inference_base_url": "", "api_key_env_vars": [],
                         "auth_type": "vertex"}, {})
add("api_key_provider", {"id": "multi", "name": "Multi",
                         "inference_base_url": "https://m.example/v1",
                         "api_key_env_vars": ["A_KEY", "B_KEY", "C_KEY"]}, {})
add("api_key_provider", {"id": "", "name": "", "inference_base_url": "",
                         "api_key_env_vars": None}, {})

# ── _empty_auth_store ──
for _ in range(5):
    add("empty_auth_store", {}, {})

# ── _store_section ──
add("store_section", {"state": {}, "key": "providers"}, {})
add("store_section", {"state": {"providers": {"nous": {"a": 1}}}, "key": "providers"}, {})
add("store_section", {"state": {"providers": None}, "key": "providers"}, {})
add("store_section", {"state": {"providers": [1, 2]}, "key": "providers"}, {})
add("store_section", {"state": {"providers": "x"}, "key": "providers"}, {})
add("store_section", {"state": {"providers": 5}, "key": "providers"}, {})
add("store_section", {"state": {"credential_pool": {}, "version": 1},
                      "key": "credential_pool"}, {})
add("store_section", {"state": {"providers": {}}, "key": "suppressed_sources"}, {})

# ── _provider_state_in ──
add("provider_state_in", {"store": {"providers": {"nous": {"access_token": "t"}}},
                          "provider_id": "nous"}, {})
add("provider_state_in", {"store": {"providers": {"nous": None}}, "provider_id": "nous"}, {})
add("provider_state_in", {"store": {"providers": {"nous": "x"}}, "provider_id": "nous"}, {})
add("provider_state_in", {"store": {"providers": {}}, "provider_id": "nous"}, {})
add("provider_state_in", {"store": {"providers": [1]}, "provider_id": "nous"}, {})
add("provider_state_in", {"store": {}, "provider_id": "nous"}, {})
add("provider_state_in", {"store": None, "provider_id": "nous"}, {})
add("provider_state_in", {"store": 0, "provider_id": "nous"}, {})
add("provider_state_in", {"store": {"providers": {"nous": {"a": 1, "b": None}}},
                          "provider_id": "nous"}, {})

# ── _slot_selects ──
add("slot_selects", {"slot": {"provider": "Anthropic"}, "normalized": "anthropic"}, {})
add("slot_selects", {"slot": {"provider": 5}, "normalized": "anthropic"}, {})
add("slot_selects", {"slot": {"provider": 5.5}, "normalized": "anthropic"}, {})
add("slot_selects", {"slot": {"provider": True}, "normalized": "anthropic"}, {})
add("slot_selects", {"slot": {"provider": 0}, "normalized": ""}, {})
add("slot_selects", {"slot": {"provider": None}, "normalized": ""}, {})
add("slot_selects", {"slot": {"provider": "  "}, "normalized": ""}, {})
add("slot_selects", {"slot": {}, "normalized": "x"}, {})
add("slot_selects", {"slot": None, "normalized": "x"}, {})
add("slot_selects", {"slot": "model", "normalized": "x"}, {})
add("slot_selects", {"slot": {"provider": " X "}, "normalized": "x"}, {})
add("slot_selects", {"slot": {"provider": "x"}, "normalized": "y"}, {})

# ── _model_level_key_env ──
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams({"model": {"provider": "openai-api", "key_env": "MY_KEY"}}))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams({"model": {"provider": " OpenAI-API ", "key_env": " MY_KEY "}}))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams({"model": {"provider": "openai-api", "key_env": None,
                         "api_key_env": "FALLBACK_KEY"}}))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams({"model": {"provider": "openai-api"}}))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams({"model": {"provider": "other", "key_env": "MY_KEY"}}))
add("model_level_key_env", {"provider_id": "OpenAI-API"},
    cfg_seams({"model": {"provider": "openai-api", "key_env": "MY_KEY"}}))
add("model_level_key_env", {"provider_id": "openai-api"}, cfg_seams(None))
add("model_level_key_env", {"provider_id": "openai-api"}, cfg_seams({}))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams({"model": "not-a-dict"}))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams({"model": {"provider": 5, "key_env": "MY_KEY"}}))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams(rs("RuntimeError", "cfg boom")))
add("model_level_key_env", {"provider_id": "openai-api"},
    {"import_load_config": rs("ImportError", "no config"), "load_config": {}})
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams(rs("KeyboardInterrupt")))
add("model_level_key_env", {"provider_id": "openai-api"},
    cfg_seams(5))

# ── _active_provider_is ──
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {"active_provider": "Nous"}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {"active_provider": " nous "}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {"active_provider": ""}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {"active_provider": None}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {"active_provider": "xai"}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": None})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": 5})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {"active_provider": 5}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": {"active_provider": 0}})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": rs("RuntimeError", "io")})
add("active_provider_is", {"normalized": "nous"}, {"load_auth_store": rs("SystemExit", "bye")})
add("active_provider_is", {"normalized": "nous"}, {})  # fail-closed

# ── _config_selects_provider ──
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"model": {"provider": "anthropic"}}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"model": {"provider": "x"},
               "auxiliary": {"summarize": {"provider": "Anthropic"}}}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"model": {"provider": "x"},
               "moa": {"aggregator": {"provider": "anthropic"}}}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"model": None,
               "moa": {"reference_models": [{"provider": "x"}, {"provider": "anthropic"}]}}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"moa": {"presets": {"p1": {"aggregator": {"provider": "ANTHROPIC"}}}}}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"model": {"provider": "x"}, "moa": {"presets": {"p1": {"reference_models": []}}}}))
add("config_selects_provider", {"normalized": "anthropic"}, cfg_seams({}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"auxiliary": "not-dict", "moa": [1]}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"moa": {"reference_models": None, "aggregator": None}}))
add("config_selects_provider", {"normalized": "anthropic"}, cfg_seams(None))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams({"model": {"provider": 5}}))
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams(rs("RuntimeError", "boom")))
add("config_selects_provider", {"normalized": "anthropic"},
    {"import_load_config": rs("ImportError", "no"), "load_config": {}})
add("config_selects_provider", {"normalized": "anthropic"},
    cfg_seams(rs("GeneratorExit")))

# ── _env_secret ──
add("env_secret", {"name": "DEEPSEEK_API_KEY"}, dotenv_seams("sk-real-key", True))
add("env_secret", {"name": "DEEPSEEK_API_KEY"}, dotenv_seams(None, False))
add("env_secret", {"name": "DEEPSEEK_API_KEY"}, dotenv_seams("", False))
add("env_secret", {"name": "DEEPSEEK_API_KEY"}, dotenv_seams("  ", False))
add("env_secret", {"name": "K"},
    {"import_get_env_value_prefer_dotenv": None,
     "get_env_value_prefer_dotenv": rs("RuntimeError", "env boom"),
     "has_usable_secret": True})
add("env_secret", {"name": "K"},
    {"import_get_env_value_prefer_dotenv": rs("ImportError", "no"),
     "get_env_value_prefer_dotenv": "x", "has_usable_secret": True})
add("env_secret", {"name": "K"}, dotenv_seams("sk-x", rs("ValueError", "bad")))
add("env_secret", {"name": "K"}, dotenv_seams("sk-x", rs("KeyboardInterrupt")))

# ── _explicit_env_credentials_present ──
REG_OPENAI = {"openai-api": {"auth_type": "api_key", "api_key_env_vars": ["OPENAI_API_KEY"]}}
add("explicit_env_credentials_present", {"normalized": "openai-api", "registry": REG_OPENAI},
    dotenv_seams("sk-real", True))
add("explicit_env_credentials_present", {"normalized": "openai-api", "registry": REG_OPENAI},
    dotenv_seams(None, False))
add("explicit_env_credentials_present",
    {"normalized": "anthropic",
     "registry": {"anthropic": {"auth_type": "api_key",
                                "api_key_env_vars": ["CLAUDE_CODE_OAUTH_TOKEN", "ANTHROPIC_API_KEY"]}}},
    dotenv_seams("sk-ant-real", True))
add("explicit_env_credentials_present",
    {"normalized": "anthropic",
     "registry": {"anthropic": {"auth_type": "api_key",
                                "api_key_env_vars": ["CLAUDE_CODE_OAUTH_TOKEN"]}}},
    {})
add("explicit_env_credentials_present",
    {"normalized": "multi",
     "registry": {"multi": {"auth_type": "api_key", "api_key_env_vars": ["A", "B"]}}},
    {"import_get_env_value_prefer_dotenv": None,
     "get_env_value_prefer_dotenv": ["", "sk-b"],
     "has_usable_secret": [False, True]})
add("explicit_env_credentials_present", {"normalized": "openrouter", "registry": {}},
    dict({"import_get_provider": None,
          "get_provider": {"auth_type": "api_key", "api_key_env_vars": ["OPENROUTER_API_KEY"]}},
         **dotenv_seams("sk-or-v1-x", True)))
add("explicit_env_credentials_present", {"normalized": "openrouter", "registry": {}},
    {"import_get_provider": None, "get_provider": None})
add("explicit_env_credentials_present", {"normalized": "openrouter", "registry": {}},
    {"import_get_provider": None, "get_provider": rs("RuntimeError", "boom")})
add("explicit_env_credentials_present",
    {"normalized": "bedrock", "registry": {"bedrock": {"auth_type": "aws_sdk", "api_key_env_vars": []}}},
    {"import_get_env_value_prefer_dotenv": None,
     "get_env_value_prefer_dotenv": "bearer-tok", "has_usable_secret": True})
add("explicit_env_credentials_present",
    {"normalized": "bedrock", "registry": {"bedrock": {"auth_type": "aws_sdk", "api_key_env_vars": []}}},
    {"import_get_env_value_prefer_dotenv": None,
     "get_env_value_prefer_dotenv": ["", "akid", "secret"],
     "has_usable_secret": [False, True, False]})
add("explicit_env_credentials_present",
    {"normalized": "bedrock", "registry": {"bedrock": {"auth_type": "aws_sdk", "api_key_env_vars": []}}},
    {"import_get_env_value_prefer_dotenv": None,
     "get_env_value_prefer_dotenv": ["", ""], "has_usable_secret": [False, False]})
add("explicit_env_credentials_present",
    {"normalized": "nous", "registry": {"nous": {"auth_type": "oauth_device_code", "api_key_env_vars": []}}},
    {})
add("explicit_env_credentials_present",
    {"normalized": "empty", "registry": {"empty": {"auth_type": "api_key", "api_key_env_vars": []}}},
    {})

# ── _keyless_provider_has_explicit_config ──
add("keyless_provider_has_explicit_config", {"normalized": "vertex"},
    {"import_has_explicit_vertex_config": None, "has_explicit_vertex_config": True})
add("keyless_provider_has_explicit_config", {"normalized": "google-vertex"},
    {"import_has_explicit_vertex_config": None, "has_explicit_vertex_config": False})
add("keyless_provider_has_explicit_config", {"normalized": "vertexai"},
    {"import_has_explicit_vertex_config": rs("ImportError", "no"),
     "has_explicit_vertex_config": True})
add("keyless_provider_has_explicit_config", {"normalized": "bedrock"},
    cfg_seams({"bedrock": {"region": "us-east-1"}}))
add("keyless_provider_has_explicit_config", {"normalized": "bedrock"},
    cfg_seams({"bedrock": {"region": "  "}}))
add("keyless_provider_has_explicit_config", {"normalized": "bedrock"},
    cfg_seams({"bedrock": None}))
add("keyless_provider_has_explicit_config", {"normalized": "bedrock"}, cfg_seams({}))
add("keyless_provider_has_explicit_config", {"normalized": "bedrock"}, cfg_seams(None))
add("keyless_provider_has_explicit_config", {"normalized": "openai-api"}, {})
add("keyless_provider_has_explicit_config", {"normalized": "bedrock"},
    cfg_seams(rs("KeyboardInterrupt")))

# ── _refuse_env_adoption_if_config_corrupt ──
PARSE_IMPORT = {"import_get_active_config_parse_failure": None}
add("refuse_env_adoption_if_config_corrupt", {},
    dict(PARSE_IMPORT, get_active_config_parse_failure=None))
add("refuse_env_adoption_if_config_corrupt", {},
    dict(PARSE_IMPORT, get_active_config_parse_failure=""))
add("refuse_env_adoption_if_config_corrupt", {},
    dict(PARSE_IMPORT, get_active_config_parse_failure="bad yaml at line 3",
         get_config_path="/h/.hermes/config.yaml"))
add("refuse_env_adoption_if_config_corrupt", {},
    {"import_get_active_config_parse_failure": rs("ImportError", "no"),
     "get_active_config_parse_failure": "x", "get_config_path": "/p"})
add("refuse_env_adoption_if_config_corrupt", {},
    dict(PARSE_IMPORT, get_active_config_parse_failure=rs("RuntimeError", "probe boom")))
add("refuse_env_adoption_if_config_corrupt", {},
    dict(PARSE_IMPORT, get_active_config_parse_failure="err",
         get_config_path=rs("OSError", "no path")))
add("refuse_env_adoption_if_config_corrupt", {},
    dict(PARSE_IMPORT, get_active_config_parse_failure=rs("SystemExit", "bye")))
add("refuse_env_adoption_if_config_corrupt", {}, {})  # fail-closed import

# ── _plugin_aliases ──
LP_IMPORT = {"import_list_providers": None}
add("plugin_aliases", {}, dict(LP_IMPORT, list_providers=[]))
add("plugin_aliases", {},
    dict(LP_IMPORT, list_providers=[{"name": "plug-x", "aliases": ["px", "plug"],
                                     "ambient_credentials": False}]))
add("plugin_aliases", {},
    dict(LP_IMPORT, list_providers=[{"name": "plug-x", "aliases": ["glm", "px"],
                                     "ambient_credentials": False}]))
add("plugin_aliases", {},
    dict(LP_IMPORT, list_providers=[{"name": "p1", "aliases": ["shared"],
                                     "ambient_credentials": False},
                                    {"name": "p2", "aliases": ["shared", "other"],
                                     "ambient_credentials": False}]))
add("plugin_aliases", {},
    {"import_list_providers": rs("ImportError", "no"), "list_providers": []})
add("plugin_aliases", {}, dict(LP_IMPORT, list_providers=rs("RuntimeError", "boom")))
add("plugin_aliases", {}, dict(LP_IMPORT, list_providers=None))
add("plugin_aliases", {}, dict(LP_IMPORT, list_providers=rs("KeyboardInterrupt")))

# ── _scoped_key_env_reader ──
add("scoped_key_env_reader", {}, {"import_scoped_key_env": None})
add("scoped_key_env_reader", {}, {"import_scoped_key_env": rs("ImportError", "no aux")})
add("scoped_key_env_reader", {}, {"import_scoped_key_env": rs("ModuleNotFoundError", "no module")})
add("scoped_key_env_reader", {}, {"import_scoped_key_env": rs("RuntimeError", "aux boom")})
add("scoped_key_env_reader", {}, {"import_scoped_key_env": rs("KeyboardInterrupt")})

# ── _openrouter_auto_detected ──
def or_seams(h1, h2=None, looks=None, pool=None, hc=None, keyenv=None, **over):
    s = {"has_usable_secret": [h1] if h2 is None else [h1, h2]}
    if looks is not None:
        s["looks_like_openrouter_key"] = looks
    if pool is not None or hc is not None:
        s["import_load_pool"] = None
        s["load_pool"] = pool
        s["has_credentials"] = hc
    s.update(over)
    return s


OR_KEY = {"keyenv": {"__callable__": "scoped_key_env"},
          "scoped_key_env": ["sk-or-v1-real", ""]}
add("openrouter_auto_detected", OR_KEY, or_seams(True))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"},
     "scoped_key_env": ["", "sk-or-v1-legacy"]},
    or_seams(False, True, looks=True))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"},
     "scoped_key_env": ["", "sk-real-openai"]},
    or_seams(False, True, looks=False, pool={"id": "p"}, hc=True))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"}, "scoped_key_env": ["", ""]},
    or_seams(False, False, pool={"id": "p"}, hc=False))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"}, "scoped_key_env": ["", ""]},
    or_seams(False, False, pool=None))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"}, "scoped_key_env": ["", ""]},
    or_seams(False, False, pool=rs("RuntimeError", "pool boom")))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"}, "scoped_key_env": ["", ""]},
    or_seams(False, False, pool={"id": "p"}, hc=rs("OSError", "io")))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"}, "scoped_key_env": ["", ""]},
    {"has_usable_secret": [False, False],
     "import_load_pool": rs("ImportError", "no pool"), "load_pool": {"id": "p"},
     "has_credentials": True})
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"},
     "scoped_key_env": rs("RuntimeError", "key boom")},
    {"has_usable_secret": True})
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "os_getenv"}, "os_getenv": ["sk-or-v1-env", ""]},
    or_seams(True))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "os_getenv"}, "os_getenv": [None, None]},
    or_seams(False, False, pool={"id": "p"}, hc=True))
add("openrouter_auto_detected",
    {"keyenv": {"__callable__": "scoped_key_env"}, "scoped_key_env": ["", ""]},
    or_seams(False, rs("SystemExit", "bye")))

# ── _logged_in_oauth_active_provider ──
REG_NOUS = {"nous": {"auth_type": "oauth_device_code"},
            "openai-codex": {"auth_type": "oauth_external"}}
ANON_SEAMS = {"import_free_inference_active": None, "import_has_guest": None}
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    dict(ANON_SEAMS, load_auth_store={"active_provider": "nous"}, has_guest=True,
         free_inference_active=True, get_auth_status={"logged_in": True}))
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    dict(ANON_SEAMS, load_auth_store={"active_provider": "nous"}, has_guest=True,
         free_inference_active=False))
add("logged_in_oauth_active_provider", {"registry": REG_NOUS, "skip_free_tier": True},
    dict(ANON_SEAMS, load_auth_store={"active_provider": "nous"}, has_guest=True))
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    dict(ANON_SEAMS, load_auth_store={"active_provider": "nous"}, has_guest=False,
         get_auth_status={"logged_in": True}))
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    {"load_auth_store": {"active_provider": "openai-codex"},
     "get_auth_status": {"logged_in": True}})
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    {"load_auth_store": {"active_provider": "openai-codex"},
     "get_auth_status": {"logged_in": False}})
add("logged_in_oauth_active_provider", {"registry": {}},
    {"load_auth_store": {"active_provider": "openai-codex"},
     "get_auth_status": {"logged_in": True}})
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    {"load_auth_store": {"active_provider": ""}})
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    {"load_auth_store": None})
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    {"load_auth_store": rs("RuntimeError", "io")})
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    {"load_auth_store": rs("KeyboardInterrupt")})
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    {"load_auth_store": {"active_provider": "nous"},
     "import_free_inference_active": None, "import_has_guest": None,
     "has_guest": True, "free_inference_active": True,
     "get_auth_status": None})
add("logged_in_oauth_active_provider", {"registry": REG_NOUS},
    dict(ANON_SEAMS, load_auth_store={"active_provider": "nous"},
         has_guest=rs("RuntimeError", "guest boom")))

# ── _config_model_provider ──
def cmp_seams(cfg, lp=None, **over):
    s = cfg_seams(cfg)
    s["import_list_providers"] = None
    s["list_providers"] = [] if lp is None else lp
    s.update(over)
    return s


add("config_model_provider", {"registry": REG_NOUS},
    cmp_seams({"model": {"provider": "custom"}}))
add("config_model_provider", {"registry": REG_NOUS},
    cmp_seams({"model": {"provider": "custom:local-llama"}}))
add("config_model_provider", {"registry": {"zai": {"auth_type": "api_key"}}},
    cmp_seams({"model": {"provider": " GLM "}}))
add("config_model_provider", {"registry": REG_NOUS},
    cmp_seams({"model": {"provider": "openrouter", "base_url": "https://mirror.example"}}))
add("config_model_provider", {"registry": REG_NOUS},
    cmp_seams({"model": {"provider": "nous"}}))
add("config_model_provider", {"registry": {}},
    cmp_seams({"model": {"provider": "my-local"}},
              import_has_named_custom_provider=None, has_named_custom_provider=True))
add("config_model_provider", {"registry": {}},
    cmp_seams({"model": {"provider": "my-local"}},
              import_has_named_custom_provider=None, has_named_custom_provider=False))
add("config_model_provider", {"registry": {}},
    cmp_seams({"model": {"base_url": "http://127.0.0.1:8080/v1"}},
              import_config_base_url_trustworthy_for_bare_custom=None,
              config_base_url_trustworthy_for_bare_custom=True))
add("config_model_provider", {"registry": {}},
    cmp_seams({"model": {"base_url": "http://127.0.0.1:8080/v1"}},
              import_config_base_url_trustworthy_for_bare_custom=None,
              config_base_url_trustworthy_for_bare_custom=False))
add("config_model_provider", {"registry": {}}, cmp_seams({"model": {}}))
add("config_model_provider", {"registry": {}}, cmp_seams(None))
add("config_model_provider", {"registry": {}}, cmp_seams(rs("RuntimeError", "cfg")))
add("config_model_provider", {"registry": {}}, cmp_seams({"model": "x"}))
add("config_model_provider", {"registry": {}},
    cmp_seams({"model": {"provider": "plug-alias"}},
              lp=[{"name": "plug-x", "aliases": ["plug-alias"], "ambient_credentials": False}]))
add("config_model_provider", {"registry": {}},
    cmp_seams({"model": {"provider": "nous"}},
              list_providers=rs("RuntimeError", "lp boom")))
add("config_model_provider", {"registry": {}},
    cmp_seams(rs("GeneratorExit")))

# ── _credential_profile ──
add("credential_profile", {"provider_id": "nous"},
    {"import_get_provider_profile": None,
     "get_provider_profile": {"id": "nous", "runtime_base_url": "https://x"}})
add("credential_profile", {"provider_id": "unknown"},
    {"import_get_provider_profile": None, "get_provider_profile": None})
add("credential_profile", {"provider_id": "x"},
    {"import_get_provider_profile": rs("ImportError", "no"), "get_provider_profile": {}})
add("credential_profile", {"provider_id": "x"},
    {"import_get_provider_profile": None, "get_provider_profile": rs("RuntimeError", "boom")})
add("credential_profile", {"provider_id": "x"},
    {"import_get_provider_profile": None, "get_provider_profile": rs("SystemExit", "bye")})
add("credential_profile", {"provider_id": "x"},
    {"import_get_provider_profile": None, "get_provider_profile": {}})

# ── _env_key_auto_detected ──
def ek_seams(rows, lp=None, keys=None, usable=None, **over):
    s = {"import_list_providers": None,
         "list_providers": [] if lp is None else lp,
         "canonical_registry_rows": rows}
    if keys is not None:
        s["scoped_key_env"] = keys
    if usable is not None:
        s["has_usable_secret"] = usable
    s.update(over)
    return s


EK_ROWS = [["openai-api", {"auth_type": "api_key", "api_key_env_vars": ["OPENAI_API_KEY"]}],
           ["gemini", {"auth_type": "api_key", "api_key_env_vars": ["GOOGLE_API_KEY", "GEMINI_API_KEY"]}]]
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(EK_ROWS, keys=["sk-openai"], usable=[True]))
add("env_key_auto_detected", {"oauth_active": "nous"},
    ek_seams(EK_ROWS, keys=["sk-openai"], usable=[True]))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(EK_ROWS, keys=["", "sk-gemini"], usable=[False, True]))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(EK_ROWS, keys=["", ""], usable=[False, False]))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams([["lmstudio", {"auth_type": "api_key", "api_key_env_vars": ["LM_API_KEY"]}],
              ["openai-api", {"auth_type": "api_key", "api_key_env_vars": ["OPENAI_API_KEY"]}]],
             keys=["sk-openai"], usable=[True]))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(EK_ROWS, lp=[{"name": "openai-api", "aliases": [], "ambient_credentials": True}],
             keys=["sk-gemini"], usable=[True]))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams([["nous", {"auth_type": "oauth_device_code", "api_key_env_vars": []}]]))
add("env_key_auto_detected", {"oauth_active": None}, ek_seams([]))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(EK_ROWS, lp=None, keys=["sk-x"], usable=[True], list_providers=None))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(rs("RuntimeError", "rows boom")))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(EK_ROWS, keys=rs("RuntimeError", "key boom"), usable=[True]))
add("env_key_auto_detected", {"oauth_active": None,
                              "keyenv": {"__callable__": "os_getenv"}},
    ek_seams(EK_ROWS, keys=None, usable=[True], os_getenv=["sk-env"]))
add("env_key_auto_detected", {"oauth_active": None},
    ek_seams(EK_ROWS, keys=["sk-x"], usable=[rs("KeyboardInterrupt")]))

# ── _pool_entry_is_explicit ──
add("pool_entry_is_explicit", {"entry": {"source": "manual"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": "device_code"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": "loopback_pkce"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": "hermes_pkce"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": "manual:extra"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": "gh_cli"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": "claude_code"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": None}}, {})
add("pool_entry_is_explicit", {"entry": {"source": 5}}, {})
add("pool_entry_is_explicit", {"entry": {"source": ""}}, {})
add("pool_entry_is_explicit", {"entry": {}}, {})
add("pool_entry_is_explicit", {"entry": None}, {})
add("pool_entry_is_explicit", {"entry": "row"}, {})
add("pool_entry_is_explicit", {"entry": {"source": "env:MY_KEY"}},
    dotenv_seams("sk-live", True))
add("pool_entry_is_explicit", {"entry": {"source": "env:MY_KEY"}},
    dotenv_seams(None, False))
add("pool_entry_is_explicit", {"entry": {"source": "env:"}}, {})
add("pool_entry_is_explicit", {"entry": {"source": "ENV: MY_KEY "}},
    dotenv_seams("sk-live", True))
add("pool_entry_is_explicit", {"entry": {"source": " Manual "}}, {})

# ── _explicit_pool_entry_present ──
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": []})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": [{"source": "manual"}]})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": [{"source": "gh_cli"}, {"source": "device_code"}]})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": [{"source": "gh_cli"}, {"source": "claude_code"}]})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": [None, 5, "x"]})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": None})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": 5})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    {"read_credential_pool": rs("RuntimeError", "pool io")})
add("explicit_pool_entry_present", {"normalized": "anthropic"},
    dict({"read_credential_pool": [{"source": "env:KEY"}]},
         **dotenv_seams("sk-live", True)))
add("explicit_pool_entry_present", {"normalized": "anthropic"}, {})  # fail-closed

# ── _is_lock_contention ──
add("is_lock_contention", {"exc": {"kind": "BlockingIOError", "errno": 11}}, {})
add("is_lock_contention", {"exc": {"kind": "BlockingIOError", "errno": None}}, {})
add("is_lock_contention", {"exc": {"kind": "OSError", "errno": 13}}, {})
add("is_lock_contention", {"exc": {"kind": "OSError", "errno": 11}}, {})
add("is_lock_contention", {"exc": {"kind": "OSError", "errno": 36}}, {})
add("is_lock_contention", {"exc": {"kind": "OSError", "errno": 2}}, {})
add("is_lock_contention", {"exc": {"kind": "OSError", "errno": None}}, {})
add("is_lock_contention", {"exc": {"kind": "PermissionError", "errno": 13}}, {})
add("is_lock_contention", {"exc": {"kind": "TimeoutError", "errno": 11}}, {})
add("is_lock_contention", {"exc": {"kind": "ValueError", "message": "no errno"}}, {})
add("is_lock_contention", {"exc": {"kind": "OSError", "errno": "13"}}, {})

# ── _lock_holder_hint ──
def hint_seams(text, pid=999, **over):
    s = {"path_read_text": text, "os_getpid": pid}
    s.update(over)
    return s


add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("12345\n"))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("12345\n", pid=12345))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("0\n"))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("-5\n"))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams(""))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("   \n\t "))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("abc"))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("12abc"))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams("+42 rest"))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams(rs("OSError", "io")))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams(rs("ValueError", "bad")))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams(None))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams(5))
add("lock_holder_hint", {"path": "/h/auth.lock", "os_name": "posix"},
    hint_seams("777\n", os_kill=rs("ProcessLookupError", "gone")))
add("lock_holder_hint", {"path": "/h/auth.lock", "os_name": "posix"},
    hint_seams("777\n", os_kill=rs("PermissionError", "not signalable")))
add("lock_holder_hint", {"path": "/h/auth.lock", "os_name": "posix"},
    hint_seams("777\n", os_kill=None))
add("lock_holder_hint", {"path": "/h/auth.lock", "os_name": "posix"},
    hint_seams("777\n", os_kill=rs("RuntimeError", "kill boom")))
add("lock_holder_hint", {"path": "/h/auth.lock"}, hint_seams(rs("KeyboardInterrupt")))
add("lock_holder_hint", {"path": "/h/auth.lock"}, {"os_getpid": 999})  # fail-closed read

# ── _kernel_lock ──
add("kernel_lock", {"lock_backend": "msvcrt", "acquire": True},
    {"file_seek": None, "file_fileno": 7, "msvcrt_locking": None})
add("kernel_lock", {"lock_backend": "msvcrt", "acquire": False},
    {"file_seek": None, "file_fileno": 7, "msvcrt_locking": None})
add("kernel_lock", {"lock_backend": "fcntl", "acquire": True},
    {"file_fileno": 3, "fcntl_flock": None})
add("kernel_lock", {"lock_backend": "fcntl", "acquire": False},
    {"file_fileno": 3, "fcntl_flock": None})
add("kernel_lock", {"lock_backend": "none", "acquire": True},
    {"file_seek": None, "file_fileno": 7})
add("kernel_lock", {"lock_backend": "msvcrt", "acquire": True},
    {"file_seek": None, "file_fileno": rs("OSError", "bad fd"), "msvcrt_locking": None})
add("kernel_lock", {"lock_backend": "msvcrt", "acquire": True},
    {"file_seek": None, "file_fileno": 7, "msvcrt_locking": rs("BlockingIOError", "held")})
add("kernel_lock", {"lock_backend": "fcntl", "acquire": True},
    {"file_fileno": 3, "fcntl_flock": rs("KeyboardInterrupt")})

# ── _stamp_lock_holder_pid ──
STAMP_OK = {"file_truncate": None, "os_getpid": 4242, "file_write": None, "file_flush": None}
add("stamp_lock_holder_pid", {}, dict(STAMP_OK))
add("stamp_lock_holder_pid", {}, dict(STAMP_OK, file_truncate=rs("OSError", "ro")))
add("stamp_lock_holder_pid", {}, dict(STAMP_OK, file_write=rs("PermissionError", "no")))
add("stamp_lock_holder_pid", {}, dict(STAMP_OK, file_flush=rs("OSError", "io")))
add("stamp_lock_holder_pid", {}, dict(STAMP_OK, os_getpid=rs("RuntimeError", "pid boom")))
add("stamp_lock_holder_pid", {}, dict(STAMP_OK, file_truncate=rs("SystemExit", "bye")))
add("stamp_lock_holder_pid", {}, {"os_getpid": 4242})  # fail-closed truncate

# ── _clear_stamped_lock_holder_pid ──
add("clear_stamped_lock_holder_pid", {"lock_backend": "msvcrt"},
    {"file_truncate": None, "file_write": None, "file_flush": None})
add("clear_stamped_lock_holder_pid", {"lock_backend": "fcntl"},
    {"file_truncate": None, "file_flush": None})
add("clear_stamped_lock_holder_pid", {"lock_backend": "none"},
    {"file_truncate": None, "file_flush": None})
add("clear_stamped_lock_holder_pid", {"lock_backend": "msvcrt"},
    {"file_truncate": rs("OSError", "ro"), "file_write": None, "file_flush": None})
add("clear_stamped_lock_holder_pid", {"lock_backend": "msvcrt"},
    {"file_truncate": None, "file_write": rs("OSError", "no"), "file_flush": None})
add("clear_stamped_lock_holder_pid", {"lock_backend": "msvcrt"},
    {"file_truncate": None, "file_write": None, "file_flush": rs("KeyboardInterrupt")})

# ── _tls_state_from_verify ──
add("tls_state_from_verify", {"verify": False}, {})
add("tls_state_from_verify", {"verify": True}, {})
add("tls_state_from_verify", {"verify": None}, {})
add("tls_state_from_verify", {"verify": 0}, {})
add("tls_state_from_verify", {"verify": "/etc/ssl/ca.pem"}, {})
add("tls_state_from_verify", {"verify": ""}, {})
add("tls_state_from_verify", {"verify": 5}, {})
add("tls_state_from_verify", {}, {})

# ── _utc_now_z ──
add("utc_now_z", {}, {"datetime_now": "2026-11-18T12:34:56.789012+00:00"})
add("utc_now_z", {}, {"datetime_now": "2026-11-18T12:34:56"})
add("utc_now_z", {}, {"datetime_now": "2026-11-18T12:34:56+00:00+00:00"})
add("utc_now_z", {}, {"datetime_now": None})
add("utc_now_z", {}, {"datetime_now": 5})
add("utc_now_z", {}, {"datetime_now": rs("RuntimeError", "clock boom")})
add("utc_now_z", {}, {"datetime_now": rs("GeneratorExit")})

# ── _last_auth_error_marker ──
add("last_auth_error_marker",
    {"provider": "nous", "error": auth_err("dead token", code="invalid_grant"),
     "reason": "runtime_refresh_failure", "default_code": None},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("last_auth_error_marker",
    {"provider": "nous", "error": auth_err("dead", code=None),
     "reason": "runtime_refresh_failure", "default_code": "refresh_failed"},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("last_auth_error_marker",
    {"provider": "xai-oauth", "error": auth_err("dead", code=""),
     "reason": "r", "default_code": "refresh_failed"},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("last_auth_error_marker",
    {"provider": "nous", "error": auth_err("dead", code="invalid_grant"),
     "reason": "r", "default_code": "refresh_failed"},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("last_auth_error_marker",
    {"provider": "nous", "error": auth_err("dead", code="x"), "reason": "r"},
    {"datetime_now": rs("RuntimeError", "clock boom")})
add("last_auth_error_marker",
    {"provider": "nous", "error": auth_err("dead", code="x"), "reason": "r"},
    {"datetime_now": None})
add("last_auth_error_marker",
    {"provider": "nous", "error": auth_err("dead", code="x"), "reason": "r"},
    {"datetime_now": rs("SystemExit", "bye")})

# ── _quarantine_flat_oauth_state ──
FLAT_STATE = {"access_token": "a", "refresh_token": "r", "expires_at": "2026-11-18T12:34:56Z",
              "expires_in": 3600, "obtained_at": "2026-11-18T11:34:56Z",
              "portal_base_url": "https://portal.nousresearch.com", "scope": "inference"}
add("quarantine_flat_oauth_state",
    {"state": FLAT_STATE, "provider": "nous", "error": auth_err("refresh died", code="invalid_grant")},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("quarantine_flat_oauth_state",
    {"state": {}, "provider": "xai-oauth", "error": auth_err("dead")},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("quarantine_flat_oauth_state",
    {"state": FLAT_STATE, "provider": "nous", "error": auth_err("dead", code=None)},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("quarantine_flat_oauth_state",
    {"state": FLAT_STATE, "provider": "nous", "error": auth_err("dead", code="x")},
    {"datetime_now": rs("RuntimeError", "clock boom")})
add("quarantine_flat_oauth_state",
    {"state": {"access_token": "a", "other": 1}, "provider": "codex",
     "error": auth_err("dead", code="")},
    {"datetime_now": "2026-11-18T12:34:56+00:00"})
add("quarantine_flat_oauth_state",
    {"state": FLAT_STATE, "provider": "nous", "error": auth_err("dead")},
    {"datetime_now": rs("KeyboardInterrupt")})

# ── _is_terminal_refresh_error ──
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="nous", code="invalid_grant", relogin=True),
     "provider": "nous"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="nous", code="invalid_grant", relogin=False),
     "provider": "nous"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="xai-oauth", code="invalid_grant", relogin=True),
     "provider": "nous"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="nous", code="transient", relogin=True),
     "provider": "nous"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="nous", code=None, relogin=True),
     "provider": "nous"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="qwen-oauth", code="invalid_grant", relogin=True),
     "provider": "qwen-oauth"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="openai-codex", code="codex_refresh_failed", relogin=True),
     "provider": "openai-codex"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="xai-oauth", code="xai_refresh_failed", relogin=True),
     "provider": "xai-oauth"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="nous", code="nous_auth_missing", relogin=True),
     "provider": "nous"}, {})
add("is_terminal_refresh_error",
    {"exc": {"kind": "ValueError", "message": "not auth"}, "provider": "nous"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="nous", code="invalid_grant", relogin=True),
     "provider": "unknown-provider"}, {})
add("is_terminal_refresh_error",
    {"exc": auth_err("dead", provider="minimax-oauth", code="invalid_grant", relogin=True),
     "provider": "minimax-oauth"}, {})

# ── _codex_pool_rate_limited_status ──
add("codex_pool_rate_limited_status", {}, {"codex_pool_rate_limit_status": None})
add("codex_pool_rate_limited_status", {}, {"codex_pool_rate_limit_status": {}})
add("codex_pool_rate_limited_status", {},
    {"codex_pool_rate_limit_status": {"label": "primary", "message": "quota hit",
                                      "last_refresh": "2026-11-18T12:34:56Z",
                                      "reset_at": "2026-11-19T00:00:00Z"},
     "auth_file_path": "/h/.hermes/auth.json"})
add("codex_pool_rate_limited_status", {},
    {"codex_pool_rate_limit_status": {"label": None, "message": None},
     "auth_file_path": "/h/.hermes/auth.json"})
add("codex_pool_rate_limited_status", {},
    {"codex_pool_rate_limit_status": {"label": "", "message": "custom"},
     "auth_file_path": "/h/.hermes/auth.json"})
add("codex_pool_rate_limited_status", {},
    {"codex_pool_rate_limit_status": {"label": "x"},
     "auth_file_path": rs("RuntimeError", "path boom")})
add("codex_pool_rate_limited_status", {},
    {"codex_pool_rate_limit_status": rs("OSError", "io")})
add("codex_pool_rate_limited_status", {},
    {"codex_pool_rate_limit_status": rs("SystemExit", "bye")})

# ── _resolved_key ──
add("resolved_key", {"path": "/h/.hermes/auth.json"},
    {"path_resolve": "/real/h/.hermes/auth.json"})
add("resolved_key", {"path": "/h/../x/auth.json"},
    {"path_resolve": rs("OSError", "no resolve")})
add("resolved_key", {"path": "/h/auth.json"},
    {"path_resolve": rs("RuntimeError", "boom")})
add("resolved_key", {"path": "/h/auth.json"},
    {"path_resolve": rs("KeyboardInterrupt")})
add("resolved_key", {"path": "/h/auth.json"}, {"path_resolve": None})
add("resolved_key", {"path": "rel/auth.json"}, {"path_resolve": "/abs/rel/auth.json"})

# ── _save_private_json ──
def spj_seams(**over):
    s = {"import_mkdir_under_hermes_home": None, "mkdir_under_hermes_home": None,
         "secure_parent_dir": None, "atomic_json_write": None}
    s.update(over)
    return s


add("save_private_json", {"target": "/h/.hermes/auth.json", "data": {"version": 1}}, spj_seams())
add("save_private_json", {"target": "/h/.hermes/auth.json", "data": {"version": 1},
                          "fsync_dir": True}, spj_seams())
add("save_private_json", {"target": "/h/.hermes/auth.json", "data": [1, 2],
                          "dump_kwargs": {"indent": 2}}, spj_seams())
add("save_private_json", {"target": "/auth.json", "data": None}, spj_seams())
add("save_private_json", {"target": "/h/.hermes/auth.json", "data": {}},
    spj_seams(mkdir_under_hermes_home=rs("PermissionError", "no")))
add("save_private_json", {"target": "/h/.hermes/auth.json", "data": {}},
    spj_seams(atomic_json_write=rs("OSError", "disk full")))
add("save_private_json", {"target": "/h/.hermes/auth.json", "data": {}},
    spj_seams(import_mkdir_under_hermes_home=rs("ImportError", "no")))
add("save_private_json", {"target": "/h/.hermes/auth.json", "data": {}},
    spj_seams(secure_parent_dir=rs("KeyboardInterrupt")))

# ── _save_active_provider_state ──
def saps_seams(store=None, path="/h/.hermes/auth.json", **over):
    s = {"auth_store_lock": None, "auth_store_lock_enter": None,
         "load_auth_store": {"version": 1, "providers": {}} if store is None else store,
         "save_provider_state": None, "save_auth_store": path,
         "auth_store_lock_exit": None}
    s.update(over)
    return s


add("save_active_provider_state", {"provider_id": "nous", "state": {"access_token": "t"}},
    saps_seams())
add("save_active_provider_state", {"provider_id": "xai-oauth", "state": {}},
    saps_seams(store={"version": 1, "providers": {"xai-oauth": {"old": 1}}}))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    saps_seams(load_auth_store=rs("OSError", "io")))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    saps_seams(save_provider_state=rs("RuntimeError", "save boom")))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    saps_seams(save_auth_store=rs("OSError", "disk"),
               auth_store_lock_exit=rs("RuntimeError", "exit boom")))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    saps_seams(auth_store_lock_exit=rs("ValueError", "exit only")))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    saps_seams(auth_store_lock=rs("TimeoutError", "lock timeout")))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    saps_seams(auth_store_lock_enter=rs("RuntimeError", "enter boom")))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    saps_seams(load_auth_store=rs("KeyboardInterrupt")))
add("save_active_provider_state", {"provider_id": "nous", "state": {}},
    {"auth_store_lock": None, "auth_store_lock_enter": None})  # fail-closed load

# ── suppressed_credential_sources ──
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {"suppressed_sources": {"anthropic": ["gh_cli", "claude_code"]}}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {"suppressed_sources": {"anthropic": ["b", "a", "b"]}}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {"suppressed_sources": {"anthropic": {"gh_cli": 1, "env:X": 2}}}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {"suppressed_sources": {"anthropic": [5, None, True, "x"]}}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {"suppressed_sources": {"other": ["a"]}}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {"suppressed_sources": None}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {"suppressed_sources": {"anthropic": "not-list"}}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": {}})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": None})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": rs("OSError", "unreadable")})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": rs("ValueError", "not oserror")})
add("suppressed_credential_sources", {"provider_id": "anthropic"},
    {"load_auth_store": rs("KeyboardInterrupt")})

# ── _get_aws_sdk_auth_status ──
add("get_aws_sdk_auth_status", {"target": "bedrock"},
    {"import_has_aws_credentials": None, "has_aws_credentials": True})
add("get_aws_sdk_auth_status", {"target": "bedrock"},
    {"import_has_aws_credentials": None, "has_aws_credentials": False})
add("get_aws_sdk_auth_status", {"target": "bedrock"},
    {"import_has_aws_credentials": rs("ImportError", "no boto3"),
     "has_aws_credentials": True})
add("get_aws_sdk_auth_status", {"target": "bedrock"},
    {"import_has_aws_credentials": rs("ModuleNotFoundError", "no module"),
     "has_aws_credentials": True})
add("get_aws_sdk_auth_status", {"target": "bedrock"},
    {"import_has_aws_credentials": rs("RuntimeError", "boom"),
     "has_aws_credentials": True})
add("get_aws_sdk_auth_status", {"target": "bedrock"},
    {"import_has_aws_credentials": None, "has_aws_credentials": rs("RuntimeError", "chain boom")})
add("get_aws_sdk_auth_status", {"target": "bedrock"},
    {"import_has_aws_credentials": None, "has_aws_credentials": rs("SystemExit", "bye")})

# ── _external_process_spec ──
def eps_seams(profile=None, env_url=None, cmd="acp-cli", resolved="/usr/bin/acp-cli",
              args=None, raw="", **over):
    s = {"provider_env_base_url": env_url,
         "import_get_provider_profile": None,
         "get_provider_profile": profile,
         "import_external_process_command": None,
         "import_resolve_external_process_command": None,
         "external_process_command": cmd,
         "resolve_external_process_command": resolved}
    if args is not None:
        s["shlex_split"] = args
    if raw:
        s["os_getenv"] = raw
    s.update(over)
    return s


PCFG = {"id": "acp-x", "inference_base_url": "https://acp.example/v1"}
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile={"process_command_env_vars": ["ACP_X_BIN"],
                       "process_args_env_var": "ACP_X_ARGS",
                       "process_args": ["--default"]},
              env_url="https://env.example/v1", raw="  --flag  x ", args=["--flag", "x"]))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile={"process_args": ["--stdio"]}, env_url=None))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile=None, env_url=""))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile={}, env_url=None))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile={"process_command_env_vars": [], "process_args_env_var": "",
                       "process_args": []},
              get_provider_profile=rs("RuntimeError", "profile boom")))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile={"process_args_env_var": "ACP_ARGS"},
              os_getenv=rs("RuntimeError", "env boom")))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile={"process_args_env_var": "ACP_ARGS"}, raw="--x",
              shlex_split=rs("ValueError", "No closing quotation")))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile=None, resolve_external_process_command=None))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile=None, import_external_process_command=rs("ImportError", "no")))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile=None, external_process_command=rs("KeyboardInterrupt")))
add("external_process_spec", {"pconfig": PCFG},
    eps_seams(profile={"process_command_env_vars": ["A", "B"],
                       "process_args_env_var": None, "process_args": None}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()