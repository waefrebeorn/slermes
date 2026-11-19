#!/usr/bin/env python3
"""Generate tests/fixtures/ws_revive1.jsonl — branch-covering cases for the
ws_revive1 differential oracle (17 ports, >=5 cases each).

Seam scripts ("in".io): bare JSON array = per-call queue; any other value =
constant return; {"__script__": true, "ret": X} scripts a constant X (needed
for list-valued returns); {"__raise__": cls, "msg": m} scripts an exception.
Run:  python tests/gen_ws_revive1_cases.py
"""
import json
from pathlib import Path

CASES = []


def case(func, inp):
    CASES.append({"func": func, "in": inp})


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_(kind, msg):
    return {"__raise__": kind, "msg": msg}


NULL = None

# ---------------------------------------------------------------- 1. schema
F = "ws_u_normalize_memory_provider_schema"
case(F, {"name": "mem", "provider": NULL, "io": {}})
case(F, {"name": "mem", "provider": {"plain": "dict"}, "io": {}})
case(F, {"name": "mem", "provider": {"get_config_schema": "fn"}, "io": {
    "provider_get_config_schema": script([
        {"key": "api_key", "secret": True, "env_var": "MEM_KEY", "required": True},
        {"key": "   ", "label": "skipped"},
        {"key": "mode", "choices": ["a", "b"], "default": "a",
         "when": {"field": "x"}, "minimum": 1, "maximum": 9, "step": 1},
        {"key": "count", "default": 3},
        {"key": "ratio", "default": 0.5},
        {"key": "flag", "default": True},
        {"key": "base_url"},
        "notadict",
    ])}})
case(F, {"name": "mem", "provider": {"get_config_schema": "fn"}, "io": {
    "provider_get_config_schema": raise_("RuntimeError", "boom"),
    "log_warning": NULL}})
case(F, {"name": "mem", "provider": {"get_config_schema": "fn"}, "io": {
    "provider_get_config_schema": {"not": "a list"}}})
case(F, {"name": "mem", "provider": {"get_config_schema": "fn"}, "io": {
    "provider_get_config_schema": script([
        {"key": "a", "options": ["x", 7, True]},
        {"key": "b", "choices": "notlist", "options": ["y"]},
        {"key": "c", "choices": [], "kind": "INT"},
        {"key": "d", "type": "Number"},
    ])}})
case(F, {"name": "mem", "provider": {"get_config_schema": "fn"}, "io": {
    "provider_get_config_schema": script([
        {"key": "k", "label": "Explicit", "env_var": "", "when": "notdict",
         "description": "d", "placeholder": "p", "url": "https://u"},
    ])}})

# ---------------------------------------------------------------- 2. existing values
F = "ws_u_read_memory_provider_existing_values"
case(F, {"name": "holographic", "io": {
    "get_hermes_home": "/home/u/.hermes",
    "read_json_file": [{}, {}], "load_config": {}}})
case(F, {"name": "mem0", "io": {
    "get_hermes_home": "/home/u/.hermes",
    "read_json_file": [{"a": 1, "k": "f1"}, {"a": 2, "b": 3}],
    "load_config": {"memory": {"mem0": {"c": 4},
                               "provider_config": {"a": 0, "d": 5}}}}})
case(F, {"name": "mem0", "io": {
    "get_hermes_home": "/home/u/.hermes",
    "read_json_file": [{"a": 1}, {}],
    "load_config": raise_("ValueError", "bad config")}})
case(F, {"name": "holographic", "io": {
    "get_hermes_home": "/home/u/.hermes",
    "read_json_file": [{}, {}],
    "load_config": {"plugins": {"hermes-memory-store": {"e": 6}}}}})
case(F, {"name": "other", "io": {
    "get_hermes_home": "/home/u/.hermes",
    "read_json_file": [{}, {}],
    "load_config": {"plugins": {"hermes-memory-store": {"e": 6}}}}})
case(F, {"name": "mem0", "io": {
    "get_hermes_home": "/home/u/.hermes",
    "read_json_file": [{}, {}], "load_config": {"memory": "notdict"}}})
case(F, {"name": "mem0", "io": {
    "get_hermes_home": "/home/u/.hermes",
    "read_json_file": [{}, {}], "load_config": [1, 2]}})

# ---------------------------------------------------------------- 3. discover statuses
F = "ws_u_discover_memory_provider_statuses"
case(F, {"io": {"discover_memory_providers": script([]), "load_config": {}}})
case(F, {"io": {
    "discover_memory_providers": script([["holographic", "Holo memory", True]]),
    "load_config": {"memory": {"provider": "holographic"}},
    "is_core_memory_provider": False,
    "load_memory_provider": script({"get_config_schema": "fn"}),
    "provider_get_config_schema": script([{"key": "path", "default": "/x"}]),
    "memory_provider_setup_info": script({"dependencies_installed": True}),
    "memory_provider_is_configured": True,
    "parked_provider_notice": False}})
case(F, {"io": {
    "discover_memory_providers": raise_("RuntimeError", "no plugins"),
    "log_exception": NULL,
    "load_config": {"memory": {"provider": "ghost"}},
    "is_core_memory_provider": False,
    "memory_provider_setup_info": script({})}})
case(F, {"io": {
    "discover_memory_providers": script([]),
    "load_config": {"memory": {"provider": "builtin"}},
    "is_core_memory_provider": True}})
case(F, {"io": {
    "discover_memory_providers": script([["p", "d", True]]),
    "load_config": {},
    "load_memory_provider": NULL,
    "memory_provider_setup_info": script({"dependencies_installed": True}),
    "memory_provider_is_configured": True,
    "parked_provider_notice": True}})
case(F, {"io": {
    "discover_memory_providers": script([["p", "d", False]]),
    "load_config": {},
    "load_memory_provider": script({"get_config_schema": "fn"}),
    "provider_get_config_schema": script([{"key": "k"}]),
    "memory_provider_setup_info": script({"dependencies_installed": True}),
    "memory_provider_is_configured": False,
    "parked_provider_notice": False}})
case(F, {"io": {
    "discover_memory_providers": script([["p", "d", True]]),
    "load_config": {},
    "load_memory_provider": NULL,
    "memory_provider_setup_info": script({"dependencies_installed": False}),
    "memory_provider_is_configured": True,
    "parked_provider_notice": False}})
case(F, {"io": {
    "discover_memory_providers": script([["a", "b"]]),
    "log_exception": NULL,
    "load_config": {}}})

# ---------------------------------------------------------------- 4. platform ports
F = "ws_u_profile_platform_ports"
PH = "/profiles/p1"
case(F, {"profile_home": PH, "runtime": NULL, "io": {}})
case(F, {"profile_home": PH,
         "runtime": {"platforms": {"webhook": {"state": "running"},
                                   "api_server": {"state": "running"}}},
         "io": {"read_user_config_raw": {
             "gateway": {"platforms": {"webhook": {"port": 9001}}},
             "platforms": {"api_server": {"port": 9002}}}}})
case(F, {"profile_home": PH,
         "runtime": {"platforms": {"webhook": {"state": "fatal"},
                                   "api_server": {"state": "disconnected"},
                                   "teams": {"state": "stopped"},
                                   "sms": {"state": "running"}}},
         "io": {"read_user_config_raw": raise_("OSError", "unreadable")}})
case(F, {"profile_home": PH,
         "runtime": {"platforms": {"webhook": {"state": "running"},
                                   "feishu": {"state": "running"}}},
         "io": {"read_user_config_raw": {
             "platforms": {"webhook": {"extra": {"port": 9100}},
                           "feishu": {"webhook_port": "8766"}}}}})
case(F, {"profile_home": PH,
         "runtime": {"platforms": {"webhook": {"state": "running"},
                                   "api_server": {"state": "running"},
                                   "teams": {"state": "running"},
                                   "sms": {"state": "running"}}},
         "io": {"read_user_config_raw": {
             "platforms": {"webhook": {"port": "abc"},
                           "api_server": {"port": NULL},
                           "teams": {"port": 3979.9},
                           "sms": {"webhook_port": True}}}}})
case(F, {"profile_home": PH,
         "runtime": {"platforms": {"webhook": {"state": "running"}}},
         "io": {"read_user_config_raw": {
             "gateway": {"platforms": {"webhook": {"port": 1111}}},
             "platforms": {"webhook": {"port": 2222}}}}})
case(F, {"profile_home": PH,
         "runtime": {"platforms": {"webhook": "running"}}, "io": {}})
case(F, {"profile_home": PH, "runtime": {"platforms": [1]}, "io": {}})

# ---------------------------------------------------------------- 5. custom provider options
F = "ws_u_custom_provider_options"
case(F, {"kind": "tts", "builtin_names": ["Edge", "openai"],
         "cfg": {"tts": {"providers": {"My Voice": {"command": "say"},
                                       "edge": {"command": "x"}},
                         "Legacy Voice": {"type": "command", "command": "run"},
                         "provider": "active-voice"}},
         "io": {"list_voice_providers": script([
             {"name": "Plugin Voice"}, {"name": "my voice"}, {}])}})
case(F, {"kind": "stt", "builtin_names": ["local"],
         "cfg": {"stt": {"providers": {"local_command": {"command": "x"},
                                       "Custom STT": {"command": "run"}}}},
         "io": {"list_voice_providers": raise_("ImportError", "no registry")}})
case(F, {"kind": "tts", "builtin_names": [],
         "cfg": {"tts": {"providers": {
             "httpish": {"type": "http", "command": "x"},
             "empty": {"command": "   "},
             "nocommand": {"type": "command"},
             "ok": {"command": "run"}}}},
         "io": {"list_voice_providers": script([])}})
case(F, {"kind": "tts", "builtin_names": ["a"], "cfg": {},
         "io": {"list_voice_providers": script("notlist")}})
case(F, {"kind": "tts", "builtin_names": [], "cfg": 5, "io": {}})
case(F, {"kind": "tts", "builtin_names": ["X"],
         "cfg": {"tts": {"provider": NULL}},
         "io": {"list_voice_providers": script([])}})
case(F, {"kind": "stt", "builtin_names": [7, True], "cfg": {},
         "io": {"list_voice_providers": script([])}})

# ---------------------------------------------------------------- 6. dynamic schema
F = "ws_u_schema_with_dynamic_provider_options"
SCHEMA = {
    "tts.provider": {"options": ["edge", "openai"], "label": "TTS"},
    "stt.provider": {"options": ["local"]},
    "memory.provider": {"options": ["holographic"]},
    "terminal.backend": {"options": ["local", "ssh"]},
    "plain": {"label": "no options"},
}
case(F, {"schema": SCHEMA, "io": {"load_config": raise_("ValueError", "bad")}})
case(F, {"schema": SCHEMA, "io": {
    "load_config": {"tts": {"providers": {"custom-tts": {"command": "x"}}}, "stt": {}},
    "memory_provider_schema_options": script(["holographic", "mem0"]),
    "plugin_terminal_backend_rows": script([]),
    "list_voice_providers": script([])}})
case(F, {"schema": SCHEMA, "io": {
    "load_config": {},
    "memory_provider_schema_options": script(["holographic"]),
    "plugin_terminal_backend_rows": script([
        {"name": "zeta"}, {"name": "alpha"}, {"name": "local"}, {"name": "alpha"}]),
    "list_voice_providers": script([])}})
case(F, {"schema": SCHEMA, "io": {
    "load_config": {},
    "memory_provider_schema_options": script(["holographic"]),
    "plugin_terminal_backend_rows": raise_("ValueError", "rows failed"),
    "list_voice_providers": script([])}})
case(F, {"schema": SCHEMA, "io": {
    "load_config": {},
    "memory_provider_schema_options": script(["holographic"]),
    "plugin_terminal_backend_rows": script([{"nope": 1}]),
    "list_voice_providers": script([])}})
case(F, {"schema": SCHEMA, "io": {
    "load_config": {},
    "memory_provider_schema_options": script(["holographic"]),
    "plugin_terminal_backend_rows": script([]),
    "list_voice_providers": script([])}})
case(F, {"schema": {"tts.provider": {"label": "x"},
                    "memory.provider": {"options": ["holographic"]}},
         "io": {"load_config": {"tts": {"providers": {"c": {"command": "x"}}}},
                "memory_provider_schema_options": script(["holographic"]),
                "list_voice_providers": script([])}})

# ---------------------------------------------------------------- 7. mcp create
F = "ws_u_normalize_mcp_server_create"


def mcp_body(**kw):
    body = {"name": "srv", "url": "", "command": "", "auth": "none",
            "bearer_token": NULL, "args": NULL, "env": NULL}
    body.update(kw)
    return body


case(F, {"body": mcp_body(url="https://x/mcp"),
         "io": {"validate_mcp_server_entry": script([])}})
case(F, {"body": mcp_body(name="   "), "io": {}})
case(F, {"body": mcp_body(url="https://x", command="run"), "io": {}})
case(F, {"body": mcp_body(), "io": {}})
case(F, {"body": mcp_body(url="https://x/mcp", auth="header", bearer_token="Bearer abc123"),
         "io": {"bearer_auth_headers": {"Authorization": "Bearer ${MCP_SRV}"},
                "validate_mcp_server_entry": script([])}})
case(F, {"body": mcp_body(url="https://x/mcp", auth="header"), "io": {}})
case(F, {"body": mcp_body(url="https://x/mcp", auth="header", bearer_token="bearer"), "io": {}})
case(F, {"body": mcp_body(url="https://x/mcp", auth="oauth"),
         "io": {"validate_mcp_server_entry": script([])}})
case(F, {"body": mcp_body(url="https://x/mcp", auth="oauth", bearer_token="tok"), "io": {}})
case(F, {"body": mcp_body(command="run", args=["a", "b"], env={"K": "V"}),
         "io": {"validate_mcp_server_entry": script([])}})
case(F, {"body": mcp_body(command="run", auth="header"), "io": {}})
case(F, {"body": mcp_body(url="https://x/mcp", args=["a"]), "io": {}})
case(F, {"body": mcp_body(url="https://x/mcp"),
         "io": {"validate_mcp_server_entry": script(["bad url", "nope"])}})
case(F, {"body": mcp_body(url="https://x/mcp", auth="bogus"), "io": {}})

# ---------------------------------------------------------------- 8. denormalize
F = "ws_u_denormalize_config_from_web"
case(F, {"config": {"model": "m1", "_model_meta": {"x": 1}},
         "io": {"load_config": {"model": {"default": "m1", "provider": "p"}}}})
case(F, {"config": {"model": "m2"}, "io": {
    "load_config": {"model": {"default": "m1", "provider": "ollama"}},
    "infer_provider_on_model_change": script(["openrouter", "m2"]),
    "normalize_main_model_assignment": script(["openrouter", "m2"]),
    "validated_main_model_selection": script({"new_model": "m2", "provider": "openrouter"}),
    "apply_main_model_assignment": script({"default": "x", "provider": "openrouter",
                                           "base_url": "y"})}})
case(F, {"config": {"model_context_length": "512"}, "io": {
    "load_config": {"model": {"default": "m1", "context_length": 100}}}})
case(F, {"config": {"model_context_length": 0}, "io": {
    "load_config": {"model": {"default": "m1", "context_length": 100}}}})
case(F, {"config": {"model_context_length": 2048}, "io": {
    "load_config": {"model": "m1"}}})
case(F, {"config": {"model": "m1", "model_context_length": 5}, "io": {
    "load_config": raise_("ValueError", "disk unreadable")}})
case(F, {"config": {"model_context_length": 3.9}, "io": {
    "load_config": {"model": {"default": "m1"}}}})
case(F, {"config": {"model_context_length": "abc"}, "io": {
    "load_config": {"model": {"default": "m1", "context_length": 7}}}})
case(F, {"config": {"model_context_length": True}, "io": {
    "load_config": {"model": {"default": "m1"}}}})
case(F, {"config": {"theme": "dark"}, "io": {}})
case(F, {"config": {"model": "m1"}, "io": {"load_config": [1, 2]}})
case(F, {"config": {"model": "m2"}, "io": {
    "load_config": {"model": {"default": "m1", "provider": "ollama"}},
    "infer_provider_on_model_change": script(["", "m2"])}})

# ---------------------------------------------------------------- 9. provider status
F = "ws_u_resolve_provider_status"
case(F, {"provider_id": "nous", "status_fn": "fn",
         "io": {"status_fn": {"logged_in": True, "custom": 1}}})
case(F, {"provider_id": "nous", "io": {
    "get_nous_auth_status_local": {
        "logged_in": True, "portal_base_url": "https://portal",
        "access_token": "aaa.bbb.cccccccccc", "access_expires_at": 123,
        "has_refresh_token": True, "free_tier": True, "account_tier": "pro"}}})
case(F, {"provider_id": "openai-codex", "io": {
    "get_codex_auth_status": {"logged_in": True, "api_key": "sk-123456789",
                              "last_refresh": "2026-01-01", "auth_mode": "chatgpt"}}})
case(F, {"provider_id": "qwen-oauth", "io": {
    "get_qwen_auth_status": {"logged_in": True, "api_key": "k",
                             "expires_at_ms": 1700000000000,
                             "auth_file": "/root/.qwen"}}})
case(F, {"provider_id": "minimax-oauth", "io": {
    "get_minimax_oauth_auth_status": {"logged_in": True, "region": "cn",
                                      "expires_at": 999}}})
case(F, {"provider_id": "xai-oauth", "io": {
    "get_xai_oauth_auth_status": {"logged_in": True, "source": "xai_oauth",
                                  "auth_store": "store1", "api_key": "tok"}}})
case(F, {"provider_id": "foo", "io": {
    "get_auth_status": {"logged_in": True, "provider": "foo", "name": "Foo",
                        "api_key": "abcdef123456", "has_refresh_token": False}}})
case(F, {"provider_id": "foo", "io": {"get_auth_status": [1, 2]}})
case(F, {"provider_id": "foo", "io": {"get_auth_status": {"other": 1}}})
case(F, {"provider_id": "nous", "io": {
    "get_nous_auth_status_local": raise_("RuntimeError", "boom")}})
case(F, {"provider_id": "nous", "status_fn": 5, "io": {}})
case(F, {"provider_id": "nous", "status_fn": "fn",
         "io": {"status_fn": raise_("SystemExit", "bye")}})

# ---------------------------------------------------------------- 10. auto archive
F = "ws_u_maybe_auto_archive_for_profile"
case(F, {"profile": NULL, "state": {"last_auto_archive_check": {"": 100.0}},
         "io": {"time_monotonic": 150.0}})
case(F, {"profile": NULL, "state": {}, "io": {
    "time_monotonic": 1000.0,
    "session_db_path_for_profile": "/homes/p1/state.db",
    "set_hermes_home_override": "tok",
    "load_config": {"sessions": {"auto_archive": False}},
    "reset_hermes_home_override": NULL}})
case(F, {"profile": "p1", "state": {}, "io": {
    "time_monotonic": 1000.0,
    "session_db_path_for_profile": "/homes/p1/state.db",
    "set_hermes_home_override": "tok",
    "load_config": {"sessions": {"auto_archive": True}},
    "reset_hermes_home_override": NULL,
    "check_gateway_running": True}})
case(F, {"profile": "p1", "state": {}, "io": {
    "time_monotonic": 1000.0,
    "session_db_path_for_profile": "/homes/p1/state.db",
    "set_hermes_home_override": "tok",
    "load_config": {"sessions": {"auto_archive": True,
                                 "auto_archive_days": "7",
                                 "min_interval_hours": "12"}},
    "reset_hermes_home_override": NULL,
    "check_gateway_running": False,
    "profile_scope": "scope1",
    "open_persistence": script({"store": "s1"}),
    "run_auto_archive": NULL,
    "store_close": NULL}})
case(F, {"profile": "p1", "state": {}, "io": {
    "time_monotonic": 1000.0,
    "session_db_path_for_profile": "/homes/p1/state.db",
    "set_hermes_home_override": "tok",
    "load_config": raise_("ValueError", "bad config"),
    "reset_hermes_home_override": NULL,
    "log_debug": NULL}})
case(F, {"profile": "p1", "state": {}, "io": {
    "time_monotonic": 1000.0,
    "session_db_path_for_profile": "/homes/p1/state.db",
    "set_hermes_home_override": "tok",
    "load_config": {"sessions": {"auto_archive": True, "auto_archive_days": "abc"}},
    "reset_hermes_home_override": NULL,
    "check_gateway_running": False,
    "profile_scope": "scope1",
    "open_persistence": script({"store": "s1"}),
    "store_close": NULL,
    "log_debug": NULL}})
case(F, {"profile": NULL, "state": {"last_auto_archive_check": {"": 100.0}},
         "io": {
             "time_monotonic": 400.0,
             "session_db_path_for_profile": "/homes/p1/state.db",
             "set_hermes_home_override": "tok",
             "load_config": {"sessions": {"auto_archive": False}},
             "reset_hermes_home_override": NULL}})
case(F, {"profile": "p1", "state": {}, "io": {
    "time_monotonic": 1000.0,
    "session_db_path_for_profile": NULL,
    "log_debug": NULL}})
case(F, {"profile": "p1", "state": {}, "io": {
    "time_monotonic": 1000.0,
    "session_db_path_for_profile": "/homes/p1/state.db",
    "set_hermes_home_override": "tok",
    "load_config": raise_("SystemExit", "stop"),
    "reset_hermes_home_override": NULL}})

# ---------------------------------------------------------------- 11. gateway restart
F = "ws_u_spawn_gateway_restart"
SUB = ["hermes", "gateway", "restart"]
case(F, {"profile": NULL, "interrupt": False, "state": {}, "io": {
    "gateway_subcommand": script(SUB),
    "spawn_hermes_action": script({"pid": 101, "poll": NULL}),
    "time_monotonic": 500.0}})
case(F, {"profile": NULL, "state": {
    "action_procs": {"gateway-restart": {"pid": 50, "poll": NULL}},
    "action_commands": {"gateway-restart": SUB}},
    "io": {"gateway_subcommand": script(SUB)}})
case(F, {"profile": "p2", "state": {
    "action_procs": {"gateway-restart": {"pid": 50, "poll": NULL}},
    "action_commands": {"gateway-restart": ["other"]}},
    "io": {"gateway_subcommand": script(SUB)}})
case(F, {"profile": NULL, "state": {
    "action_procs": {"gateway-restart": {"pid": 50, "poll": 0}}},
    "io": {"gateway_subcommand": script(SUB),
           "spawn_hermes_action": script({"pid": 102, "poll": NULL}),
           "time_monotonic": 600.0}})
case(F, {"profile": NULL, "state": {
    "last_gateway_restart": [100.0, {"pid": 77, "poll": NULL}, SUB]},
    "io": {"gateway_subcommand": script(SUB),
           "time_monotonic": 105.0, "log_info": NULL}})
case(F, {"profile": NULL, "state": {
    "last_gateway_restart": [100.0, {"pid": 77, "poll": NULL}, SUB]},
    "io": {"gateway_subcommand": script(SUB),
           "time_monotonic": [200.0, 201.0],
           "spawn_hermes_action": script({"pid": 103, "poll": NULL})}})
case(F, {"profile": NULL, "interrupt": True, "state": {
    "last_gateway_restart": [100.0, {"pid": 77, "poll": NULL}, SUB]},
    "io": {"gateway_subcommand": script(SUB),
           "time_monotonic": 105.0,
           "spawn_hermes_action": script({"pid": 104, "poll": NULL})}})
case(F, {"profile": "p1", "state": {
    "last_gateway_restart": [100.0, {"pid": 77, "poll": NULL}, ["different"]]},
    "io": {"gateway_subcommand": script(SUB),
           "spawn_hermes_action": script({"pid": 105, "poll": NULL}),
           "time_monotonic": 106.0}})

# ---------------------------------------------------------------- 12. endpoint response
F = "ws_u_custom_endpoint_response"
case(F, {"cfg": {"model": {"provider": "custom", "default": "m",
                           "base_url": "https://api"},
                 "providers": {"p1": {"base_url": "https://api", "name": "P One",
                                      "models": ["m1", "m2"], "api_key": "secret123",
                                      "context_length": 8192}}},
         "io": {"get_compatible_custom_providers": script([]),
                "redacted_credential_preview": "sec…123"}})
case(F, {"cfg": {"providers": {"p2": {"url": "https://x", "key_env": "P2_KEY",
                                      "model": "mx"}}},
         "io": {"get_compatible_custom_providers": script([])}})
case(F, {"cfg": {"model": {"provider": "custom", "base_url": "https://legacy/"},
                 "providers": {}},
         "io": {"get_compatible_custom_providers": script([
             {"name": "Legacy End", "base_url": "https://legacy", "model": "lm"}])}})
case(F, {"cfg": {}, "io": {"get_compatible_custom_providers": script([
    {"provider_key": "p1", "name": "X", "base_url": "u"}])}})
case(F, {"cfg": {"providers": {"legacy-end": {"base_url": "https://a"}}},
         "io": {"get_compatible_custom_providers": script([
             {"name": "Legacy End", "base_url": "https://b"}])}})
case(F, {"cfg": {"providers": {"p": {"base_url": ""}, "q": "notdict",
                                   "r": {"api": "https://r"}}},
         "io": {"get_compatible_custom_providers": script([])}})
case(F, {"cfg": {"providers": {
    "p": {"base_url": "https://x", "api_mode": "openai"},
    "q": {"base_url": "https://y", "transport": "responses"},
    "r": {"base_url": "https://z", "api_mode": "weird"}}},
    "io": {"get_compatible_custom_providers": script([])}})
case(F, {"cfg": {"providers": {"p": {"base_url": "https://x",
                                     "models": {"m2": {}, "m1": {}},
                                     "default_model": "m1"}}},
         "io": {"get_compatible_custom_providers": script([])}})

# ---------------------------------------------------------------- 13. models analytics
F = "ws_u_get_models_analytics"


def full_row(**kw):
    row = {"model": "m1", "billing_provider": "p1",
           "input_tokens": 0, "output_tokens": 0, "cache_read_tokens": 0,
           "reasoning_tokens": 0, "estimated_cost": 0, "actual_cost": 0,
           "sessions": 0, "api_calls": 0, "tool_calls": 0,
           "last_used_at": NULL, "avg_tokens_per_session": 0}
    row.update(kw)
    return row


ROW1 = full_row(input_tokens=100, output_tokens=50, estimated_cost=0.1,
                actual_cost=0.2, sessions=2, api_calls=3, tool_calls=1,
                last_used_at=900, avg_tokens_per_session=75)
AUX1 = {"model": "m1", "billing_provider": "p1", "input_tokens": 10,
        "output_tokens": 5, "sessions": 99, "last_used_at": 950}
AUX2 = {"model": "m2", "billing_provider": "", "input_tokens": 7,
        "output_tokens": 3, "sessions": 1, "api_calls": 1}
case(F, {"days": 30, "profile": NULL, "io": {
    "open_session_db": script({"db": "d1"}),
    "time_time": 1000000.0,
    "call_usage_by_route": script([ROW1]),
    "attach_tool_calls": NULL,
    "aux_usage_rows": script([AUX1, AUX2]),
    "model_capabilities": script({"context": 8192}),
    "session_usage_totals": script({"total_sessions": 5}),
    "persistence_close": NULL}})
case(F, {"days": 7, "profile": "p1", "io": {
    "open_session_db": script({"db": "d2"}),
    "time_time": 1000000.0,
    "call_usage_by_route": script([]),
    "session_usage_by_route": script([ROW1]),
    "aux_usage_rows": script([]),
    "model_capabilities": script({}),
    "session_usage_totals": script({"total_sessions": 2}),
    "persistence_close": NULL}})
case(F, {"days": 30, "io": {
    "open_session_db": script({"db": "d3"}),
    "time_time": 1000000.0,
    "call_usage_by_route": raise_("TransientStoreError", "busy"),
    "session_usage_by_route": script([]),
    "aux_usage_rows": script([]),
    "session_usage_totals": script({"total_sessions": 0}),
    "persistence_close": NULL}})
case(F, {"days": 30, "io": {
    "open_session_db": script({"db": "d4"}),
    "time_time": 1000000.0,
    "call_usage_by_route": script([]),
    "session_usage_by_route": script([
        full_row(input_tokens=10, sessions=1, last_used_at=5),
        full_row(billing_provider="", sessions=2, last_used_at=9)]),
    "aux_usage_rows": script([]),
    "model_capabilities": script({}),
    "session_usage_totals": script({"total_sessions": 3}),
    "persistence_close": NULL}})
case(F, {"days": 30, "io": {
    "open_session_db": script({"db": "d5"}),
    "time_time": 1000000.0,
    "call_usage_by_route": script([
        full_row(model="a", input_tokens=5, output_tokens=5),
        full_row(model="b", input_tokens=10),
        full_row(model="c", input_tokens=5, output_tokens=5)]),
    "attach_tool_calls": NULL,
    "aux_usage_rows": script([]),
    "model_capabilities": script({}),
    "session_usage_totals": script({}),
    "persistence_close": NULL}})
case(F, {"days": 30, "io": {
    "open_session_db": script({"db": "d6"}),
    "time_time": 1000000.0,
    "call_usage_by_route": script([{"billing_provider": "p"}]),
    "attach_tool_calls": NULL,
    "aux_usage_rows": script([]),
    "persistence_close": NULL}})

# ---------------------------------------------------------------- 14. telegram sync
F = "ws_u_telegram_onboarding_request_sync"
TG_IO = {"hermes_user_agent": "HermesDashboard/1.0",
         "telegram_onboarding_base_url": "https://tg.example"}
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 200, "json": {"pairing_id": "p1"}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 404, "json": {"error": "not_found"}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 500, "json": {"error": "expired"}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 502, "json": {"error": "claimed"}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 503, "json": {"error": "weird"}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 404,
                          "json_error": {"__raise__": "ValueError", "msg": "bad"}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 200,
                          "json_error": {"__raise__": "ValueError", "msg": "bad"}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 200, "json": [1, 2]})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request=raise_("OSError", "connection refused"))})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request=raise_("SystemExit", "halt"))})
case(F, {"method": "POST", "path": "/pair", "body": {"a": 1}, "bearer_token": "tok123",
         "io": dict(TG_IO, httpx_request={"status_code": 200, "json": {"ok": True}})})
case(F, {"method": "GET", "path": "/pair/x", "io": dict(
    TG_IO, httpx_request={"status_code": 404, "json": {"status": "unauthorized"}})})

# ---------------------------------------------------------------- 15. recommended default
F = "ws_get_recommended_default_model"
case(F, {"provider": "nous", "profile": NULL, "io": {
    "profile_scope_enter": NULL,
    "nous_recommended_default": {"provider": "nous", "model": "hermes-4",
                                 "free_tier": True},
    "profile_scope_exit": NULL}})
case(F, {"provider": "nous", "profile": "ghost", "io": {
    "profile_scope_enter": NULL,
    "nous_recommended_default": raise_("HTTPException", "404: unknown profile"),
    "profile_scope_exit": NULL}})
case(F, {"provider": "nous", "io": {
    "profile_scope_enter": NULL,
    "nous_recommended_default": raise_("ValueError", "tier lookup failed"),
    "profile_scope_exit": NULL,
    "log_exception": NULL}})
case(F, {"provider": "OpenRouter", "profile": NULL, "io": {
    "profile_scope_enter": NULL,
    "load_picker_context": {"ctx": 1},
    "build_models_payload": {"providers": [{"slug": "openrouter",
                                            "models": ["m1", "m2"]}]},
    "profile_scope_exit": NULL,
    "pick_silent_default_model": "m2"}})
case(F, {"provider": "foo", "io": {
    "profile_scope_enter": NULL,
    "load_picker_context": {},
    "build_models_payload": {"providers": [{"slug": "other"}]},
    "profile_scope_exit": NULL}})
case(F, {"provider": "foo", "io": {
    "profile_scope_enter": NULL,
    "load_picker_context": {},
    "build_models_payload": raise_("RuntimeError", "inventory down"),
    "profile_scope_exit": NULL,
    "log_exception": NULL}})
case(F, {"provider": "foo", "profile": "ghost", "io": {
    "profile_scope_enter": raise_("HTTPException", "404: unknown profile")}})
case(F, {"provider": "foo", "io": {
    "profile_scope_enter": NULL,
    "load_picker_context": {},
    "build_models_payload": [1, 2],
    "profile_scope_exit": NULL,
    "log_exception": NULL}})

# ---------------------------------------------------------------- 16. dashboard plugins
F = "ws_u_discover_dashboard_plugins"
MAN1 = {"name": "alpha", "label": "Alpha"}
case(F, {"io": {
    "dashboard_plugin_search_dirs": script([["/plugins", "user"]]),
    "path_is_dir": [True, True],
    "scandir": script(["/plugins/alpha"]),
    "path_exists": [True],
    "read_text": [json.dumps(MAN1)],
    "json_loads": [MAN1],
    "safe_plugin_api_relpath": [NULL]}})
case(F, {"io": {
    "dashboard_plugin_search_dirs": script([["/a", "user"], ["/b", "bundled"]]),
    "path_is_dir": [True, True, True, True],
    "scandir": [["/a/p"], ["/b/p"]],
    "path_exists": [True, True],
    "read_text": ['{"name":"same"}', '{"name":"same"}'],
    "json_loads": [{"name": "same"}, {"name": "same"}],
    "safe_plugin_api_relpath": [NULL, NULL]}})
case(F, {"io": {
    "dashboard_plugin_search_dirs": script([["/plugins", "user"]]),
    "path_is_dir": [False]}})
case(F, {"io": {
    "dashboard_plugin_search_dirs": script([["/plugins", "user"]]),
    "path_is_dir": [True],
    "scandir": raise_("OSError", "denied"),
    "log_warning": NULL}})
case(F, {"io": {
    "dashboard_plugin_search_dirs": script([["/plugins", "user"]]),
    "path_is_dir": [True, True],
    "scandir": script(["/plugins/alpha"]),
    "path_exists": [True],
    "read_text": ["not json"],
    "json_loads": [raise_("ValueError", "Expecting value")],
    "log_warning": NULL}})
case(F, {"io": {
    "dashboard_plugin_search_dirs": script([["/plugins", "user"]]),
    "path_is_dir": [True, True],
    "scandir": script(["/plugins/alpha"]),
    "path_exists": [True],
    "read_text": [json.dumps({"name": "alpha", "api": "../evil.py"})],
    "json_loads": [{"name": "alpha", "api": "../evil.py"}],
    "safe_plugin_api_relpath": [NULL],
    "log_warning": NULL}})
case(F, {"search_dirs": [["/x", "project"]], "io": {
    "path_is_dir": [True, True],
    "scandir": script(["/x/p"]),
    "path_exists": [True],
    "read_text": [json.dumps({
        "tab": {"path": "/custom", "override": "/built", "hidden": True,
                "position": "start"},
        "slots": ["s1", "", 5], "entry": "main.js", "css": "style.css",
        "version": "1.2.3", "icon": "Star", "description": "d",
        "api": "api.py"})],
    "json_loads": [{
        "tab": {"path": "/custom", "override": "/built", "hidden": True,
                "position": "start"},
        "slots": ["s1", "", 5], "entry": "main.js", "css": "style.css",
        "version": "1.2.3", "icon": "Star", "description": "d",
        "api": "api.py"}],
    "safe_plugin_api_relpath": ["api.py"]}})
case(F, {"io": {
    "dashboard_plugin_search_dirs": script([["/plugins", "user"]]),
    "path_is_dir": [True, True],
    "scandir": script(["/plugins/alpha"]),
    "path_exists": [True],
    "read_text": [json.dumps({"api": "api.py"})],
    "json_loads": [{"api": "api.py"}],
    "safe_plugin_api_relpath": ["api.py"]}})

# ---------------------------------------------------------------- 17. catalog env meta
F = "ws_u_catalog_provider_env_metadata"


def card(**kw):
    d = {"slug": "p1", "label": "P One", "tab": "keys",
         "api_key_env_vars": ["P1_KEY"], "base_url_env_var": "",
         "auth_type": "api_key", "description": "desc",
         "signup_url": "https://signup"}
    d.update(kw)
    return d


case(F, {"optional_env_vars": {}, "io": {
    "provider_catalog": script([card(api_key_env_vars=["P1_KEY", "P1_ALT"])])}})
case(F, {"optional_env_vars": {}, "io": {
    "provider_catalog": script([
        card(api_key_env_vars=["SHARED"]),
        card(slug="p2", label="P Two", api_key_env_vars=["SHARED"])])}})
case(F, {"optional_env_vars": {"GITHUB_TOKEN": {"category": "tool"}}, "io": {
    "provider_catalog": script([card(api_key_env_vars=["GITHUB_TOKEN", "P_KEY"])])}})
case(F, {"optional_env_vars": {}, "io": {
    "provider_catalog": script([card(base_url_env_var="P1_BASE")])}})
case(F, {"optional_env_vars": {}, "io": {
    "provider_catalog": script([card(api_key_env_vars=[], auth_type="aws_sdk")])}})
case(F, {"optional_env_vars": {}, "io": {
    "provider_catalog": script([card(api_key_env_vars=[], auth_type="vertex")])}})
case(F, {"optional_env_vars": {}, "io": {
    "provider_catalog": script([card(tab="models")])}})
case(F, {"catalog_import_fails": True, "io": {}})
case(F, {"optional_env_vars": {}, "io": {
    "provider_catalog": script([
        card(api_key_env_vars=[], auth_type="aws_sdk"),
        card(slug="p2", label="P Two", api_key_env_vars=[], auth_type="aws_sdk")])}})
case(F, {"optional_env_vars": {"P1_KEY": {"category": "provider"},
                               "OTHER": NULL}, "io": {
    "provider_catalog": script([card()])}})


def main():
    out = Path(__file__).resolve().parent / "fixtures" / "ws_revive1.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    with out.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases -> {out}")
    for fn, n in sorted(counts.items()):
        flag = "OK" if n >= 5 else "LOW"
        print(f"  {flag} {n:2d} {fn}")
    assert all(n >= 5 for n in counts.values()), "every port needs >=5 cases"
    assert len(counts) == 17


if __name__ == "__main__":
    main()