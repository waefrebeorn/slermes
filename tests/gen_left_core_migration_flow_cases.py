#!/usr/bin/env python3
"""Generate tests/fixtures/left_core_migration_flow.jsonl for the
left_core_migration_flow FLOW oracle (hermes_cli/left_core_migration.py).
>=5 cases per ported function, exercising decision branches, error paths,
BaseException propagation and fail-closed missing scripts. Script semantics
match the harness/oracle: array = per-call queue (last repeats), single value =
constant, missing/empty = fail-closed RuntimeError, null = None,
{"__raise__": cls, "msg": ...} = exception. ASCII / POSIX-lexical domain.

Domain notes: get_hermes_home / hermes_home_key / install_command are scripted
when reached; PlatformConfig descs carry {enabled,token,extra}; str()-reached
seam values are scalars; .get/.strip on a scripted null models the live
AttributeError. The pure table leaves (profile_env_names etc.) take no input;
their >=5 cases vary ignored inputs/seams (constant output by construction).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "left_core_migration_flow.jsonl")

H = "/h/u"
CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# ── _no_notice (constant "") ──
for pe in (False, True):
    add("_no_notice", {"home": H, "process_env": pe})
add("_no_notice", {"home": "/a/b"})
add("_no_notice", {"home": ""})
add("_no_notice", {})

# ── profile_env_names / launch_env_names (constant sorted sets) ──
for i in range(5):
    add("profile_env_names", {"home": H, "process_env": bool(i % 2)})
    add("launch_env_names", {"home": H, "process_env": bool(i % 2)})

# ── legacy_plugin_alias ──
for nm in ("observability/langfuse", "platforms/yuanbao", "yuanbao-platform",
           "langfuse", "unknown", ""):
    add("legacy_plugin_alias", {"name": nm})

# ── legacy_plugin_keys ──
for pl in ("langfuse", "yuanbao", "spotify", "github-copilot", "unknown", ""):
    add("legacy_plugin_keys", {"plugin": pl})

# ── _provider_feature ──
for nm in ("copilot", "GITHUB", "  Copilot-Acp  ", "github-models", "nope", "", None):
    add("_provider_feature", {"name": nm})

# ── _notice_file ──
for hm in ("/h/u", "/h/u/", "h\\u", "/a/b/c", "/"):
    add("_notice_file", {"home": hm})

# ── _install_sentence ──
IC = "hermes plugins install spotify"
add("_install_sentence", {"feature": "spotify"}, {"get_hermes_home": H, "install_command": IC})
add("_install_sentence", {"feature": "langfuse"},
    {"get_hermes_home": H, "install_command": "hermes plugins install langfuse"})
add("_install_sentence", {"feature": "github-copilot"},
    {"get_hermes_home": "/p/x", "install_command": "hermes -p x plugins install github-copilot"})
add("_install_sentence", {"feature": "spotify"}, {"get_hermes_home": H})  # fail-closed install_command
add("_install_sentence", {"feature": "spotify"}, {"get_hermes_home": R("SystemExit", "bye"), "install_command": IC})
add("_install_sentence", {"feature": "spotify"}, {"get_hermes_home": H, "install_command": R("ValueError", "bad")})
add("_install_sentence", {"feature": "zzz"})  # StopIteration

# ── platform_install_hint ──
for pl in ("homeassistant", "yuanbao", "unknown", "", "webhook"):
    add("platform_install_hint", {"platform": pl},
        {"get_hermes_home": H, "install_command": "hermes plugins install " + pl})
add("platform_install_hint", {"platform": "homeassistant"}, {"get_hermes_home": R("KeyboardInterrupt", "int")})

# ── terminal_backend_install_hint ──
for bk in ("singularity", "unknown", "", "local", "docker"):
    add("terminal_backend_install_hint", {"backend": bk},
        {"get_hermes_home": H, "install_command": "hermes plugins install singularity"})

# ── moved_plugin_hint ──
for nm in ("observability/langfuse", "langfuse", "platforms/yuanbao", "yuanbao", "unknown", ""):
    add("moved_plugin_hint", {"name": nm},
        {"get_hermes_home": H, "install_command": "hermes plugins install x"})

# ── _load_error ──
add("_load_error", {"feature": "github-copilot"}, {"plugin_load_error": "import failed"})
add("_load_error", {"feature": "github-copilot"}, {"plugin_load_error": ""})
add("_load_error", {"feature": "github-copilot"}, {"plugin_load_error": None})
add("_load_error", {"feature": "github-copilot"}, {"plugin_load_error": R("OSError", "io")})
add("_load_error", {"feature": "github-copilot"})  # fail-closed
add("_load_error", {"feature": "zzz"})  # StopIteration

# ── provider_left_core ──
add("provider_left_core", {"name": "copilot"}, {"get_provider_profile": None})
add("provider_left_core", {"name": "copilot"}, {"get_provider_profile": {"id": "copilot"}})
add("provider_left_core", {"name": "  COPILOT "}, {"get_provider_profile": None})
add("provider_left_core", {"name": "nope"})
add("provider_left_core", {"name": ""})
add("provider_left_core", {"name": None})
add("provider_left_core", {"name": "copilot"}, {"get_provider_profile": R("SystemExit", "bye")})

# ── provider_install_hint ──
GC = "hermes plugins install github-copilot"
add("provider_install_hint", {"name": "nope"})
add("provider_install_hint", {"name": "copilot"},
    {"get_hermes_home": H, "path_is_dir": False, "install_command": GC})
add("provider_install_hint", {"name": "copilot"},
    {"get_hermes_home": H, "path_is_dir": True, "install_command": GC,
     "read_yaml_layers": {"plugins": {"disabled": ["github-copilot"]}}})
add("provider_install_hint", {"name": "copilot"},
    {"get_hermes_home": H, "path_is_dir": True, "install_command": GC,
     "read_yaml_layers": {"plugins": {"enabled": ["github-copilot"]}},
     "plugin_load_error": "bad import"})
add("provider_install_hint", {"name": "copilot"},
    {"get_hermes_home": H, "path_is_dir": True, "install_command": GC,
     "read_yaml_layers": {}, "plugin_load_error": ""})
add("provider_install_hint", {"name": "copilot"}, {"get_hermes_home": R("SystemExit", "bye")})
add("provider_install_hint", {"name": "copilot"},
    {"get_hermes_home": H, "path_is_dir": R("KeyboardInterrupt", "int")})
add("provider_install_hint", {"name": "copilot"},
    {"get_hermes_home": H, "path_is_dir": True, "install_command": R("ValueError", "bad")})

# ── feature_in_use ──
add("feature_in_use", {"feature": "spotify", "home": H}, {"in_use": True})
add("feature_in_use", {"feature": "spotify", "home": H}, {"in_use": False})
add("feature_in_use", {"feature": "github-copilot", "home": H},
    {"in_use": False, "routes_at_hosts": True})
add("feature_in_use", {"feature": "github-copilot", "home": H},
    {"in_use": False, "routes_at_hosts": False})
add("feature_in_use", {"feature": "github-copilot", "home": H, "process_env": True},
    {"in_use": True, "routes_at_hosts": False})
add("feature_in_use", {"feature": "spotify", "home": H}, {"in_use": R("SystemExit", "bye")})
add("feature_in_use", {"feature": "github-copilot", "home": H},
    {"in_use": False, "routes_at_hosts": R("ValueError", "bad")})
add("feature_in_use", {"feature": "zzz", "home": H})

# ── host_install_hint ──
URL = "https://api.githubcopilot.com/v1"
add("host_install_hint", {"base_url": URL}, {"at_host": False})
add("host_install_hint", {"base_url": None}, {"at_host": False})
add("host_install_hint", {"base_url": URL},
    {"at_host": True, "get_provider_profile": None, "get_hermes_home": H,
     "path_is_dir": False, "install_command": GC})
add("host_install_hint", {"base_url": URL},
    {"at_host": True, "get_provider_profile": [None, {"id": "x"}]})
add("host_install_hint", {"base_url": URL}, {"at_host": R("ValueError", "bad url")})
add("host_install_hint", {"base_url": "https://models.github.ai/x"},
    {"at_host": [False, True], "get_provider_profile": None, "get_hermes_home": H,
     "path_is_dir": False, "install_command": GC})

# ── moved_command_hint ──
add("moved_command_hint", {"prog": "hermes", "value": "mcp-serve"},
    {"get_hermes_home": H, "install_command": "hermes plugins install mcp-serve", "path_is_dir": False})
add("moved_command_hint", {"prog": "hermes", "value": "mcp-serve"},
    {"get_hermes_home": H, "install_command": "hermes plugins install mcp-serve", "path_is_dir": True})
add("moved_command_hint", {"prog": "hermes auth", "value": "spotify", "action": "login"},
    {"get_hermes_home": H, "install_command": "hermes plugins install spotify", "path_is_dir": False})
add("moved_command_hint", {"prog": "hermes auth", "value": "spotify", "action": ""},
    {"get_hermes_home": H, "install_command": "hermes plugins install spotify", "path_is_dir": True})
add("moved_command_hint", {"prog": "hermes auth", "value": "spotify", "action": "status"},
    {"get_hermes_home": H, "install_command": "hermes plugins install spotify", "path_is_dir": False})
add("moved_command_hint", {"prog": "hermes", "value": "spotify"},
    {"get_hermes_home": H, "install_command": "hermes plugins install spotify", "path_is_dir": True})
add("moved_command_hint", {"prog": "hermes", "value": "claw"},
    {"get_hermes_home": H, "install_command": "hermes plugins install agent-importers", "path_is_dir": False})
add("moved_command_hint", {"prog": "hermes", "value": "import-agent"},
    {"get_hermes_home": H, "install_command": "hermes plugins install agent-importers", "path_is_dir": False})
add("moved_command_hint", {"prog": "other", "value": "spotify"}, {"get_hermes_home": H})
add("moved_command_hint", {"prog": "hermes", "value": "unknown"}, {"get_hermes_home": H})
add("moved_command_hint", {"prog": "hermes", "value": ""}, {"get_hermes_home": H})
add("moved_command_hint", {"prog": "hermes", "value": "spotify"}, {"get_hermes_home": R("SystemExit", "bye")})

# ── _noticed ──
add("_noticed", {"message": "msg", "home": H, "feature": "spotify"}, {"notice": ""})
add("_noticed", {"message": "msg", "home": H, "feature": "spotify"}, {"notice": "extra"})
add("_noticed", {"message": "msg", "home": H, "feature": "spotify"}, {"notice": None})
add("_noticed", {"message": "", "home": H, "feature": "yuanbao"}, {"notice": "n"})
add("_noticed", {"message": "msg", "home": H, "feature": "spotify"}, {"notice": R("ValueError", "bad")})
add("_noticed", {"message": "msg", "home": H, "feature": "zzz"})

# ── _read_config ──
add("_read_config", {"home": H}, {"read_config_doc": {"model": "x"}})
add("_read_config", {"home": H}, {"read_config_doc": R("ValueError", "bad yaml")})
add("_read_config", {"home": H}, {"read_config_doc": R("SystemExit", "bye")})
add("_read_config", {"home": H}, {"read_config_doc": None})
add("_read_config", {"home": H}, {"read_config_doc": [1, 2]})
add("_read_config", {"home": H})  # fail-closed -> swallowed -> {}

# ── _auth_store ──
add("_auth_store", {"home": H}, {"read_json_or_empty": {"providers": {}}})
add("_auth_store", {"home": H}, {"read_json_or_empty": {}})
add("_auth_store", {"home": H}, {"read_json_or_empty": None})
add("_auth_store", {"home": H}, {"read_json_or_empty": R("OSError", "io")})
add("_auth_store", {"home": H})  # fail-closed -> propagate

# ── plugin_present ──
add("plugin_present", {"plugin": "spotify", "home": H}, {"path_is_dir": True})
add("plugin_present", {"plugin": "spotify", "home": H}, {"path_is_dir": False})
add("plugin_present", {"plugin": "spotify", "home": H}, {"path_is_dir": None})
add("plugin_present", {"plugin": "spotify", "home": H}, {"path_is_dir": R("OSError", "io")})
add("plugin_present", {"plugin": "spotify", "home": H})  # fail-closed

# ── agent_imports_in_use ──
add("agent_imports_in_use", {"home": H}, {"path_is_file": True})
add("agent_imports_in_use", {"home": H}, {"path_is_file": False})
add("agent_imports_in_use", {"home": H, "process_env": True}, {"path_is_file": True})
add("agent_imports_in_use", {"home": H}, {"path_is_file": R("OSError", "io")})
add("agent_imports_in_use", {"home": H})  # fail-closed

# ── _plugins_listed ──
add("_plugins_listed", {"home": H, "names": ["langfuse"]},
    {"read_yaml_layers": {"plugins": {"enabled": ["langfuse"], "disabled": []}}})
add("_plugins_listed", {"home": H, "names": ["langfuse"]},
    {"read_yaml_layers": {"plugins": {"enabled": [], "disabled": ["langfuse"]}}})
add("_plugins_listed", {"home": H, "names": ["x"]},
    {"read_yaml_layers": {"plugins": {"enabled": [1, "x"], "disabled": [None]}}})
add("_plugins_listed", {"home": H, "names": ["x"]}, {"read_yaml_layers": R("ValueError", "bad")})
add("_plugins_listed", {"home": H, "names": ["x"]}, {"read_yaml_layers": R("SystemExit", "bye")})
add("_plugins_listed", {"home": H, "names": ["x"]}, {"read_yaml_layers": None})
add("_plugins_listed", {"home": H, "names": ["x"]}, {"read_yaml_layers": [1]})
add("_plugins_listed", {"home": H, "names": ["x"]})  # fail-closed -> swallowed

# ── _plugin_lists ──
add("_plugin_lists", {"home": H, "plugin": "langfuse"},
    {"read_yaml_layers": {"plugins": {"enabled": ["observability/langfuse"]}}})
add("_plugin_lists", {"home": H, "plugin": "yuanbao"},
    {"read_yaml_layers": {"plugins": {"disabled": ["yuanbao-platform"]}}})
add("_plugin_lists", {"home": H, "plugin": "spotify"},
    {"read_yaml_layers": {"plugins": {"enabled": ["spotify"]}}})
add("_plugin_lists", {"home": H, "plugin": "langfuse", "also": ["extra"]},
    {"read_yaml_layers": {"plugins": {"enabled": ["extra"]}}})
add("_plugin_lists", {"home": H, "plugin": "zzz"})  # StopIteration
add("_plugin_lists", {"home": H, "plugin": "langfuse"}, {"read_yaml_layers": R("OSError", "io")})

# ── _bundled_plugin_selected ──
add("_bundled_plugin_selected", {"home": H, "plugin": "langfuse"},
    {"read_yaml_layers": {"plugins": {"enabled": ["langfuse"], "disabled": []}}})
add("_bundled_plugin_selected", {"home": H, "plugin": "langfuse"},
    {"read_yaml_layers": {"plugins": {"enabled": ["langfuse"], "disabled": ["langfuse"]}}})
add("_bundled_plugin_selected", {"home": H, "plugin": "langfuse"},
    {"read_yaml_layers": {"plugins": {"enabled": []}}})
add("_bundled_plugin_selected", {"home": H, "plugin": "langfuse"},
    {"read_yaml_layers": {"plugins": {"enabled": ["observability/langfuse"]}}})
add("_bundled_plugin_selected", {"home": H, "plugin": "zzz"})  # StopIteration

# ── langfuse_in_use ──
add("langfuse_in_use", {"home": H},
    {"read_yaml_layers": {"plugins": {"enabled": ["langfuse"]}}})
add("langfuse_in_use", {"home": H},
    {"read_yaml_layers": {"plugins": {"enabled": ["langfuse"], "disabled": ["langfuse"]}}})
add("langfuse_in_use", {"home": H}, {"read_yaml_layers": {"plugins": {}}})
add("langfuse_in_use", {"home": H, "process_env": True},
    {"read_yaml_layers": {"plugins": {"enabled": ["observability/langfuse"]}}})
add("langfuse_in_use", {"home": H}, {"read_yaml_layers": R("ValueError", "bad")})

# ── spotify_in_use ──
add("spotify_in_use", {"home": H},
    {"read_json_or_empty": {"providers": {"spotify": {"access_token": "t"}}}})
add("spotify_in_use", {"home": H},
    {"read_json_or_empty": {"providers": {"spotify": {"refresh_token": "r"}}}})
add("spotify_in_use", {"home": H},
    {"read_json_or_empty": {"providers": {"spotify": {}}},
     "read_config_doc": {"toolsets": ["spotify"]}, "toolset_listed": True})
add("spotify_in_use", {"home": H},
    {"read_json_or_empty": {}, "read_config_doc": {}, "toolset_listed": False})
add("spotify_in_use", {"home": H}, {"read_json_or_empty": None})  # AttributeError
add("spotify_in_use", {"home": H},
    {"read_json_or_empty": {"providers": [1]}, "read_config_doc": {}, "toolset_listed": False})
add("spotify_in_use", {"home": H},
    {"read_json_or_empty": {"providers": {"spotify": {"access_token": None, "refresh_token": "r"}}}})

# ── _home_env ──
add("_home_env", {"home": H, "name": "X"},
    {"resolve_home_dotenv": {"X": "  v  "}, "current_secret_scope_home": None})
add("_home_env", {"home": H, "name": "X"},
    {"resolve_home_dotenv": {}, "current_secret_scope_home": "/h/u",
     "hermes_home_key": "k", "current_secret_scope": {"X": "s"}})
add("_home_env", {"home": H, "name": "X", "process_env": True},
    {"resolve_home_dotenv": {}, "current_secret_scope_home": None,
     "hermes_home_key": "k", "get_routing_process_hermes_home": "/h/u", "os_environ_get": "e"})
add("_home_env", {"home": H, "name": "X"},
    {"resolve_home_dotenv": {}, "current_secret_scope_home": "/other",
     "hermes_home_key": ["a", "b"]})  # keys differ -> scope skipped
add("_home_env", {"home": H, "name": "X"}, {"resolve_home_dotenv": None})  # AttributeError get
add("_home_env", {"home": H, "name": "X"},
    {"resolve_home_dotenv": {"X": None}, "current_secret_scope_home": None})  # AttributeError strip
add("_home_env", {"home": H, "name": "X"},
    {"resolve_home_dotenv": R("SystemExit", "bye")})
add("_home_env", {"home": H, "name": "X", "process_env": True},
    {"resolve_home_dotenv": {}, "current_secret_scope_home": None,
     "hermes_home_key": "k", "get_routing_process_hermes_home": "/h/u", "os_environ_get": None})
add("_home_env", {"home": H, "name": "X"},
    {"resolve_home_dotenv": {}, "current_secret_scope_home": "/h/u",
     "hermes_home_key": "k", "current_secret_scope": None})

# ── camofox_in_use ──
add("camofox_in_use", {"home": H}, {"read_config_doc": {"browser": {"cdp_url": "http://x"}}})
add("camofox_in_use", {"home": H},
    {"read_config_doc": {}, "resolve_home_dotenv": {"BROWSER_CDP_URL": "http://y"},
     "current_secret_scope_home": None})
add("camofox_in_use", {"home": H},
    {"read_config_doc": {"browser": {"use_gateway": True}},
     "resolve_home_dotenv": {}, "current_secret_scope_home": None, "is_truthy_value": True})
add("camofox_in_use", {"home": H},
    {"read_config_doc": {"browser": {"cloud_provider": "Camofox"}},
     "resolve_home_dotenv": {}, "current_secret_scope_home": None, "is_truthy_value": False})
add("camofox_in_use", {"home": H},
    {"read_config_doc": {"browser": {"cloud_provider": "other"}},
     "resolve_home_dotenv": {}, "current_secret_scope_home": None, "is_truthy_value": False})
add("camofox_in_use", {"home": H},
    {"read_config_doc": {}, "resolve_home_dotenv": {"CAMOFOX_URL": "http://z"},
     "current_secret_scope_home": None, "is_truthy_value": False})
add("camofox_in_use", {"home": H},
    {"read_config_doc": {}, "resolve_home_dotenv": {},
     "current_secret_scope_home": None, "is_truthy_value": False})
add("camofox_in_use", {"home": H},
    {"read_config_doc": {"browser": {"use_gateway": 1}},
     "resolve_home_dotenv": {}, "current_secret_scope_home": None,
     "is_truthy_value": R("SystemExit", "bye")})

# ── singularity_in_use ──
add("singularity_in_use", {"home": H}, {"read_yaml_layers": {"terminal": {"backend": "Singularity"}}})
add("singularity_in_use", {"home": H}, {"read_yaml_layers": {"terminal": {"env_type": "singularity"}}})
add("singularity_in_use", {"home": H},
    {"read_yaml_layers": {}, "resolve_home_dotenv": {"TERMINAL_ENV": "singularity"}})
add("singularity_in_use", {"home": H, "process_env": True},
    {"read_yaml_layers": {}, "resolve_home_dotenv": {}, "hermes_home_key": "k",
     "get_routing_process_hermes_home": "/h/u", "os_environ_get": "Singularity"})
add("singularity_in_use", {"home": H}, {"read_yaml_layers": {"terminal": {"backend": "local"}}})
add("singularity_in_use", {"home": H}, {"read_yaml_layers": R("ValueError", "bad")})
add("singularity_in_use", {"home": H}, {"read_yaml_layers": R("SystemExit", "bye")})
add("singularity_in_use", {"home": H}, {"read_yaml_layers": None, "resolve_home_dotenv": {}})
add("singularity_in_use", {"home": H},
    {"read_yaml_layers": {}, "resolve_home_dotenv": {"TERMINAL_ENV": None}})

# ── homeassistant_in_use ──
add("homeassistant_in_use", {"home": H}, {"load_env_file": {"HASS_TOKEN": "t"}})
add("homeassistant_in_use", {"home": H, "process_env": True},
    {"load_env_file": {}, "get_secret": "secret"})
add("homeassistant_in_use", {"home": H},
    {"load_env_file": {}, "merged_platform_block": {"enabled": True},
     "platform_config_from_dict": {"enabled": True, "token": None, "extra": {}}})
add("homeassistant_in_use", {"home": H},
    {"load_env_file": {}, "merged_platform_block": None,
     "read_config_doc": {"toolsets": ["homeassistant"]}, "toolset_listed": True})
add("homeassistant_in_use", {"home": H},
    {"load_env_file": {}, "merged_platform_block": None, "read_config_doc": {},
     "toolset_listed": False})
add("homeassistant_in_use", {"home": H}, {"load_env_file": None})  # AttributeError
add("homeassistant_in_use", {"home": H, "process_env": True},
    {"load_env_file": {}, "get_secret": R("ValueError", "bad"),
     "merged_platform_block": None, "read_config_doc": {}, "toolset_listed": False})
add("homeassistant_in_use", {"home": H, "process_env": True},
    {"load_env_file": {}, "get_secret": R("SystemExit", "bye")})
add("homeassistant_in_use", {"home": H, "process_env": True},
    {"load_env_file": {}, "get_secret": None, "merged_platform_block": None,
     "read_config_doc": {}, "toolset_listed": False})

# ── platform_configured_on ──
add("platform_configured_on", {"home": H, "name": "homeassistant"}, {"merged_platform_block": None})
add("platform_configured_on", {"home": H, "name": "homeassistant"},
    {"merged_platform_block": {"enabled": True},
     "platform_config_from_dict": {"enabled": True, "token": None, "extra": {}}})
add("platform_configured_on", {"home": H, "name": "homeassistant"},
    {"merged_platform_block": {"enabled": False},
     "platform_config_from_dict": {"enabled": False, "token": "t", "extra": {}}})
add("platform_configured_on", {"home": H, "name": "homeassistant"},
    {"merged_platform_block": {"token": "t"},
     "platform_config_from_dict": {"enabled": False, "token": "t", "extra": {}}})
add("platform_configured_on", {"home": H, "name": "homeassistant"},
    {"merged_platform_block": {},
     "platform_config_from_dict": {"enabled": False, "token": None, "extra": {}}})
add("platform_configured_on", {"home": H, "name": "homeassistant"},
    {"merged_platform_block": R("SystemExit", "bye")})
add("platform_configured_on", {"home": H, "name": "homeassistant"},
    {"merged_platform_block": {"token": "t"}, "platform_config_from_dict": R("ValueError", "bad")})

# ── _platform_config ──
add("_platform_config", {"block": {"enabled": True, "token": "t"}},
    {"platform_config_from_dict": {"enabled": True, "token": "t", "extra": {"a": 1}}})
add("_platform_config", {"block": {}},
    {"platform_config_from_dict": {"enabled": False, "token": None, "extra": {}}})
add("_platform_config", {"block": {"home_channel": "x"}},
    {"platform_config_from_dict": {"enabled": False, "token": None, "extra": {}}})
add("_platform_config", {"block": {}}, {"platform_config_from_dict": None})
add("_platform_config", {"block": {}}, {"platform_config_from_dict": R("ValueError", "bad")})
add("_platform_config", {"block": {"enabled": True}}, {"platform_config_from_dict": {"enabled": True}})

# ── _yuanbao_notice ──
add("_yuanbao_notice", {"home": H},
    {"path_is_dir": True, "read_yaml_layers": {"plugins": {"disabled": ["yuanbao"]}}})
add("_yuanbao_notice", {"home": H},
    {"path_is_dir": True, "read_yaml_layers": {"plugins": {"enabled": ["yuanbao"]}}})
add("_yuanbao_notice", {"home": H}, {"path_is_dir": False})
add("_yuanbao_notice", {"home": H}, {"path_is_dir": True, "read_yaml_layers": {}})
add("_yuanbao_notice", {"home": H}, {"path_is_dir": R("SystemExit", "bye")})

# ── _scope_kept ──
add("_scope_kept", {"config": {}, "feature": "spotify"})  # no toolsets -> True
add("_scope_kept", {"config": {}, "feature": "camofox"}, {"marked": True})
add("_scope_kept", {"config": {}, "feature": "camofox"}, {"marked": False})
add("_scope_kept", {"config": {}, "feature": "homeassistant"}, {"marked": False})
add("_scope_kept", {"config": {}, "feature": "zzz"})  # StopIteration
add("_scope_kept", {"config": {}, "feature": "camofox"}, {"marked": R("SystemExit", "bye")})

# ── _mark ──
add("_mark", {"config": {}, "key": "_left_core_installed", "feature": "spotify"})
add("_mark", {"config": {"_left_core_installed": ["camofox", "spotify"]},
              "key": "_left_core_installed", "feature": "langfuse"})
add("_mark", {"config": {"_left_core_installed": ["spotify", "spotify"]},
              "key": "_left_core_installed", "feature": "spotify"})
add("_mark", {"config": {"_left_core_installed": "notalist"},
              "key": "_left_core_installed", "feature": "camofox"})
add("_mark", {"config": {"_left_core_installed": [1, "camofox"]},
              "key": "_left_core_installed", "feature": "spotify"})
add("_mark", {"config": {}, "key": "_left_core_installed", "feature": "zzz"})  # StopIteration

# ── defer_startup_notice ──
add("defer_startup_notice", {"message": "hi"},
    {"logger_warning": None, "get_hermes_home": H, "queue_notice": None})
add("defer_startup_notice", {"message": "hi", "log": False},
    {"get_hermes_home": H, "queue_notice": None})
add("defer_startup_notice", {"message": "hi"},
    {"logger_warning": R("SystemExit", "bye"), "get_hermes_home": H, "queue_notice": None})
add("defer_startup_notice", {"message": "hi"},
    {"logger_warning": None, "get_hermes_home": R("ValueError", "bad"), "queue_notice": None})
add("defer_startup_notice", {"message": "hi"},
    {"logger_warning": None, "get_hermes_home": H, "queue_notice": R("OSError", "io")})
add("defer_startup_notice", {"message": "hi"}, {"logger_warning": None})  # fail-closed get_hermes_home

# ── _say_post_split ──
add("_say_post_split", {"home": H, "feature": "spotify"}, {"path_is_dir": True})
add("_say_post_split", {"home": H, "feature": "spotify", "backoff": True},
    {"path_is_dir": False, "failed_recently": True})
add("_say_post_split", {"home": H, "feature": "spotify"},
    {"path_is_dir": False, "note_failure": None,
     "install_command": "hermes plugins install spotify", "notice": "", "say": None})
add("_say_post_split", {"home": H, "feature": "spotify", "process_env": True},
    {"path_is_dir": False, "note_failure": None,
     "install_command": "hermes plugins install spotify", "notice": "extra", "say": None})
add("_say_post_split", {"home": H, "feature": "spotify", "backoff": True},
    {"path_is_dir": False, "failed_recently": False, "note_failure": None,
     "install_command": "hermes plugins install spotify", "notice": "", "say": None})
add("_say_post_split", {"home": H, "feature": "spotify"},
    {"path_is_dir": False, "note_failure": R("SystemExit", "bye")})
add("_say_post_split", {"home": H, "feature": "spotify"},
    {"path_is_dir": False, "note_failure": None, "install_command": R("ValueError", "bad")})
add("_say_post_split", {"home": H, "feature": "spotify"},
    {"path_is_dir": False, "note_failure": None,
     "install_command": "hermes plugins install spotify", "notice": "", "say": R("OSError", "io")})
add("_say_post_split", {"home": H, "feature": "zzz"})  # StopIteration


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-function counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if short:
        print("FUNCS WITH <5 CASES: %s" % short)
        raise SystemExit(1)
    print("all funcs have >=5 cases")


if __name__ == "__main__":
    main()