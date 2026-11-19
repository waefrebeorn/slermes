#!/usr/bin/env python3
"""Generate tests/fixtures/plugins_cmd_flow.jsonl — differential cases for the
unclaimed hermes_cli/plugins_cmd.py FLOW ports (src/cli/plugins_cmd_flow.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single value
reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, MISSING script is fail-closed
RuntimeError on both sides).

Domain (see the port header): ASCII inputs; paths are POSIX strings; integral
floats normalize to ints on both sides; set-valued inputs/results are lists of
strings; entries are 6-element arrays [name, version, description, source,
dir_path, key]; findings are {file:str}; console markers are non-empty dicts or
null; columns are 2-element [header, style] pairs; manifest name values are str.

Run:  python tests/gen_plugins_cmd_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "plugins_cmd_flow.jsonl")

CON = {"console": 1}   # a non-empty console marker


def case(func, seams=None, **inp):
    d = dict(inp)
    d["seams"] = dict(seams or {})
    d["env"] = {}
    return {"func": func, "in": d}


def pure(func, **inp):
    d = dict(inp)
    d["seams"] = {}
    d["env"] = {}
    return {"func": func, "in": d}


def raiser(kind, msg="boom", **extra):
    d = {"__raise__": kind, "msg": msg}
    d.update(extra)
    return d


def entry(name, key, source="user", dir_path=None, version="1.0", description="d"):
    return [name, version, description, source, dir_path if dir_path is not None else "/u/" + name, key]


# Minimal discovery seams yielding an EMPTY plugin list (both bases not dirs).
def disc_empty():
    return {
        "get_bundled_plugins_dir": "/bundled",
        "_plugins_dir": "/user",
        "is_dir": False,
        "discover_entrypoint_manifests": [[]],
        "legacy_plugin_keys": [[]],
    }


CASES = []

# ── _console ──
CASES += [
    case("console", {"Console": CON}),
    case("console", {"Console": {"console": 2, "x": "y"}}),
    case("console", {"Console": None}),
    case("console", {"Console": raiser("RuntimeError")}),
    pure("console"),   # fail-closed
]

# ── _table ──
CASES += [
    case("table", {"Table": {"table": 1}, "add_column": None},
         columns=[["Name", "bold"], ["Version", None]], kwargs={"show_header": True}),
    case("table", {"Table": {"table": 1}, "add_column": None}, columns=[], kwargs={}),
    case("table", {"Table": {"table": 1}, "add_column": raiser("RuntimeError")},
         columns=[["A", "red"]], kwargs={}),
    case("table", {"Table": raiser("ValueError")}, columns=[["A", None]], kwargs={}),
    pure("table", columns=[["A", None]], kwargs={}),   # fail-closed Table
]

# ── _is_tty ──
CASES += [
    case("is_tty", {"stdin_isatty": True, "stdout_isatty": True}),
    case("is_tty", {"stdin_isatty": False, "stdout_isatty": True}),   # short-circuit
    case("is_tty", {"stdin_isatty": True, "stdout_isatty": False}),
    case("is_tty", {"stdin_isatty": raiser("OSError")}),
    pure("is_tty"),   # fail-closed stdin
]

# ── _fail ──
CASES += [
    case("fail", {"console_print": None}, console=CON, message="[red]Error:[/red] nope"),
    case("fail", {"console_print": None}, console=None, message="x"),   # AttributeError
    case("fail", {"console_print": raiser("RuntimeError")}, console=CON, message="x"),
    pure("fail", console=CON, message="x"),   # fail-closed console_print
    case("fail", {"console_print": None}, console=CON, message=""),
]

# ── _ask_yes ──
CASES += [
    case("ask_yes", {"reader": "Y"}, prompt="Enable? "),
    case("ask_yes", {"reader": "  YES  "}, prompt="p"),
    case("ask_yes", {"reader": "n"}, prompt="p"),
    case("ask_yes", {"reader": ""}, prompt="p"),
    case("ask_yes", {"reader": raiser("EOFError")}, prompt="p"),
    case("ask_yes", {"reader": raiser("KeyboardInterrupt")}, prompt="p"),
    case("ask_yes", {"reader": raiser("RuntimeError")}, prompt="p"),   # propagates
    case("ask_yes", {"reader": 5}, prompt="p"),   # AttributeError strip
    pure("ask_yes", prompt="p"),   # fail-closed reader
]

# ── _config_value ──
CASES += [
    case("config_value", {"load_config": {"a": 1}, "cfg_get": "val"}, keys=["plugins", "x"], default="d"),
    case("config_value", {"load_config": raiser("RuntimeError"), "cfg_get": "val"}, keys=["k"], default="d"),
    case("config_value", {"load_config": {}, "cfg_get": raiser("ValueError")}, keys=["k"], default=7),
    case("config_value", {"load_config": raiser("SystemExit"), "cfg_get": "val"}, keys=["k"], default="d"),
    case("config_value", {"load_config": {}, "cfg_get": None}, keys=["k"], default="d"),
    pure("config_value", keys=["k"], default="d"),   # fail-closed load_config -> default
]

# ── _clone_timeout_seconds ──
CT_KEYS = ["plugins", "clone_timeout_seconds"]
CASES += [
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": 300, "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": 0, "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": True, "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": "x", "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": 2.5, "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": 4000, "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": 3600, "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": -5, "logger_warning": None}),
    case("clone_timeout_seconds", {"load_config": {}, "cfg_get": 60, "logger_warning": raiser("RuntimeError")}),
    pure("clone_timeout_seconds"),   # fail-closed -> default 300
]

# ── _config_name_set ──
CASES += [
    case("config_name_set", {"load_config": {}, "cfg_get": ["a", "b", "a"]}, keys=["plugins", "enabled"]),
    case("config_name_set", {"load_config": {}, "cfg_get": "x"}, keys=["k"]),
    case("config_name_set", {"load_config": {}, "cfg_get": []}, keys=["k"]),
    case("config_name_set", {"load_config": raiser("RuntimeError")}, keys=["k"]),
    case("config_name_set", {"load_config": {}, "cfg_get": ["z", "m"]}, keys=["k"]),
]

# ── _config_str ──
CASES += [
    case("config_str", {"load_config": {}, "cfg_get": "val"}, keys=["memory", "provider"], default=""),
    case("config_str", {"load_config": {}, "cfg_get": ""}, keys=["k"], default="d"),
    case("config_str", {"load_config": {}, "cfg_get": None}, keys=["k"], default="d"),
    case("config_str", {"load_config": {}, "cfg_get": 0}, keys=["k"], default="d"),
    case("config_str", {"load_config": raiser("OSError")}, keys=["k"], default="d"),
]

# ── _write_config_value ──
CASES += [
    case("write_config_value", {"load_config": {}, "save_config": None}, section="memory", key="provider", value="x"),
    case("write_config_value", {"load_config": {"memory": {"provider": "old"}}, "save_config": None},
         section="memory", key="provider", value="new"),
    case("write_config_value", {"load_config": {"memory": "notdict"}, "save_config": None},
         section="memory", key="provider", value="x"),   # TypeError
    case("write_config_value", {"load_config": "notdict", "save_config": None},
         section="s", key="k", value=1),   # AttributeError setdefault
    case("write_config_value", {"load_config": raiser("RuntimeError")}, section="s", key="k", value=1),
    case("write_config_value", {"load_config": {}, "save_config": raiser("OSError")}, section="s", key="k", value=1),
    pure("write_config_value", section="s", key="k", value=1),   # fail-closed load_config
]

# ── _native_manifest_file ──
CASES += [
    case("native_manifest_file", {"native_manifest_file": "/p/plugin.yaml"}, plugin_dir="/p"),
    case("native_manifest_file", {"native_manifest_file": None}, plugin_dir="/p"),
    case("native_manifest_file", {"native_manifest_file": raiser("ValueError", "bad manifest")}, plugin_dir="/p"),
    case("native_manifest_file", {"native_manifest_file": raiser("OSError")}, plugin_dir="/p"),
    pure("native_manifest_file", plugin_dir="/p"),   # fail-closed
]

# ── _has_portable_manifest ──
CASES += [
    case("has_portable_manifest", {"exists": True}, plugin_dir="/p"),   # short-circuit
    case("has_portable_manifest", {"exists": False, "is_symlink": True}, plugin_dir="/p"),
    case("has_portable_manifest", {"exists": False, "is_symlink": False}, plugin_dir="/p"),
    case("has_portable_manifest", {"exists": raiser("OSError")}, plugin_dir="/p"),
    pure("has_portable_manifest", plugin_dir="/p"),   # fail-closed exists
]

# ── _load_yaml_manifest ──
CASES += [
    case("load_yaml_manifest", {"read_native_manifest": {"name": "x"}}, manifest_file="/p/plugin.yaml"),
    case("load_yaml_manifest", {"read_native_manifest": {}}, manifest_file="/p/plugin.yaml"),
    case("load_yaml_manifest", {"read_native_manifest": raiser("ValueError")}, manifest_file="/p/plugin.yaml"),
    case("load_yaml_manifest", {"read_native_manifest": None}, manifest_file="/p/plugin.yaml"),
    pure("load_yaml_manifest", manifest_file="/p/plugin.yaml"),
]

# ── _looks_like_plugin_dir ──
CASES += [
    case("looks_like_plugin_dir", {"native_manifest_file": "/p/plugin.yaml"}, target="/p"),
    case("looks_like_plugin_dir", {"native_manifest_file": None, "exists": True}, target="/p"),
    case("looks_like_plugin_dir", {"native_manifest_file": None, "exists": [False, True]}, target="/p"),
    case("looks_like_plugin_dir", {"native_manifest_file": None, "exists": [False, False]}, target="/p"),
    case("looks_like_plugin_dir", {"native_manifest_file": raiser("ValueError", "bad")}, target="/p"),
    pure("looks_like_plugin_dir", target="/p"),   # fail-closed native -> PluginOp? no, RuntimeError propagate
]

# ── _read_manifest_info ──
CASES += [
    case("read_manifest_info", {"native_manifest_file": "/p/plugin.yaml",
                                "read_native_manifest": {"name": "foo", "version": "2", "description": "desc"}},
         d="/p/foo", prefix=""),
    case("read_manifest_info", {"native_manifest_file": "/p/plugin.yaml",
                                "read_native_manifest": {"version": "2"}}, d="/p/foo", prefix="web"),
    case("read_manifest_info", {"native_manifest_file": "/p/plugin.yaml",
                                "read_native_manifest": raiser("ValueError")}, d="/p/foo", prefix=""),
    case("read_manifest_info", {"native_manifest_file": None, "exists": True,
                                "read_agent_plugin_manifest": [[{"name": "bar"}]]}, d="/p/bar", prefix=""),
    case("read_manifest_info", {"native_manifest_file": None, "exists": False}, d="/p/bar", prefix=""),
    case("read_manifest_info", {"native_manifest_file": None, "exists": True,
                                "read_agent_plugin_manifest": [[]]}, d="/p/bar", prefix=""),
    case("read_manifest_info", {"native_manifest_file": None, "exists": True,
                                "read_agent_plugin_manifest": raiser("RuntimeError")}, d="/p/bar", prefix=""),
]

# ── _missing_env_specs ──
CASES += [
    case("missing_env_specs", {"get_env_value": [None, "set"]},
         manifest={"requires_env": ["A", "B"]}),
    case("missing_env_specs", {}, manifest={"requires_env": []}),
    case("missing_env_specs", {}, manifest={}),
    case("missing_env_specs", {"get_env_value": ""},
         manifest={"requires_env": [{"name": "X", "secret": True}, {"name": ""}, {"noname": 1}, 5]}),
    case("missing_env_specs", {"get_env_value": raiser("RuntimeError")}, manifest={"requires_env": ["A"]}),
    case("missing_env_specs", {}, manifest="notdict"),   # AttributeError
    case("missing_env_specs", {"get_env_value": [None, None]}, manifest={"requires_env": ["A", "B"]}),
]

# ── _clone_failure_message ──
CASES += [
    case("clone_failure_message", {"markup_escape": "fatal: repo not found"},
         git_url="https://github.com/o/r.git", git_error="  fatal: repo not found  "),
    case("clone_failure_message", {"markup_escape": "a\\[b]"}, git_url="u", git_error="a[b]"),
    case("clone_failure_message", {"markup_escape": ""}, git_url="u", git_error="   "),
    case("clone_failure_message", {"markup_escape": raiser("RuntimeError")}, git_url="u", git_error="e"),
    pure("clone_failure_message", git_url="u", git_error="e"),
]

# ── _unknown_plugin_message ──
CASES += [
    case("unknown_plugin_message", {"moved_plugin_hint": "It moved to X."}, name="old"),
    case("unknown_plugin_message", {"moved_plugin_hint": None}, name="old"),
    case("unknown_plugin_message", {"moved_plugin_hint": None}, name="old", downloaded_only=True),
    case("unknown_plugin_message", {"moved_plugin_hint": ""}, name="old"),
    case("unknown_plugin_message", {"moved_plugin_hint": raiser("RuntimeError")}, name="old"),
    pure("unknown_plugin_message", name="old"),
]

# ── _preserved_files_note ──
CASES += [
    pure("preserved_files_note", exc={"message": "blocked", "scan_result": {"findings": [{"file": "a.py"}, {"file": "b.py"}]}},
         merged=["a.py", "c.py"]),
    pure("preserved_files_note", exc={"message": "blocked", "scan_result": {"findings": [{"file": "z.py"}]}},
         merged=["a.py"]),
    pure("preserved_files_note", exc={"message": "blocked", "scan_result": None}, merged=["a.py"]),
    pure("preserved_files_note", exc={"message": "blocked", "scan_result": {"findings": []}}, merged=[]),
    pure("preserved_files_note", exc={"message": "m", "scan_result": {"findings": [{"file": "a.py"}, {"file": "a.py"}]}},
         merged=["a.py"]),
]

# ── _scan_merged_tree ──
CASES += [
    case("scan_merged_tree", {"_scan_plugin_tree": {"scan": "ok"}}, plugin_dir="/p", identifier="o/r",
         merged=["a.py"], kwargs={"force": True}),
    case("scan_merged_tree", {"_scan_plugin_tree": raiser("PluginScanBlocked", "blocked",
                                                          scan_result={"findings": [{"file": "a.py"}]})},
         plugin_dir="/p", identifier="o/r", merged=[], kwargs={"force": False}),
    case("scan_merged_tree", {"_scan_plugin_tree": raiser("PluginScanBlocked", "blocked",
                                                          scan_result={"findings": [{"file": "a.py"}]})},
         plugin_dir="/p", identifier="o/r", merged=["a.py"], kwargs={"force": False, "reviewed_pin": True}),
    case("scan_merged_tree", {"_scan_plugin_tree": raiser("RuntimeError")}, plugin_dir="/p", identifier="o/r",
         merged=["a.py"], kwargs={}),
    pure("scan_merged_tree", plugin_dir="/p", identifier="o/r", merged=["a.py"], kwargs={}),   # fail-closed
]

# ── _discard_key_and_leaf ──
CASES += [
    pure("discard_key_and_leaf", names=["web/exa", "exa", "other"], key="web/exa"),
    pure("discard_key_and_leaf", names=["exa", "other"], key="web/exa"),
    pure("discard_key_and_leaf", names=["a", "b"], key="c"),
    pure("discard_key_and_leaf", names=[], key="web/exa"),
    pure("discard_key_and_leaf", names=["x", "x/y"], key="x/y"),
]

# ── _plugin_aliases ──
CASES += [
    case("plugin_aliases", {"legacy_plugin_keys": [["old-exa"]]}, key="web/exa",
         entries=[entry("web-exa", "web/exa"), entry("other", "web/other")]),
    case("plugin_aliases", {"legacy_plugin_keys": [[]]}, key="exa", entries=[]),
    case("plugin_aliases", {"legacy_plugin_keys": [["l1", "l2"]]}, key="web/exa",
         entries=[entry("e1", "web/exa"), entry("e2", "web/exa")]),
    case("plugin_aliases", dict(disc_empty(), legacy_plugin_keys=[["legacy"]]), key="web/exa", entries=None),
    case("plugin_aliases", {"legacy_plugin_keys": raiser("RuntimeError")}, key="k", entries=[]),
]

# ── _disabled_by_update ──
CASES += [
    case("disabled_by_update", {"get_hermes_home": "/home", "read_home_selection":
         {"plugins": {"disabled_by_update": {"web/exa": {"reason": "x"}}}}}, key="web/exa", aliases=["exa"]),
    case("disabled_by_update", {"get_hermes_home": "/home", "read_home_selection":
         {"plugins": {"disabled_by_update": {"other": {}}}}}, key="web/exa", aliases=["exa"]),
    case("disabled_by_update", {"get_hermes_home": "/home", "read_home_selection": None}, key="k", aliases=[]),
    case("disabled_by_update", {"get_hermes_home": "/home", "read_home_selection":
         {"plugins": {"disabled_by_update": "notdict"}}}, key="k", aliases=[]),
    case("disabled_by_update", {"get_hermes_home": "/home", "read_home_selection":
         {"plugins": {"disabled_by_update": {"exa": {}}}}}, key="web/exa", aliases=["exa"]),
    case("disabled_by_update", {"get_hermes_home": raiser("RuntimeError")}, key="k", aliases=[]),
]

# ── _activate_key ──
CASES += [
    case("activate_key", dict(disc_empty(), get_enabled_set=[[]], get_disabled_set=[[]], set_plugin_enabled=None),
         key="web/exa", enable=True, console=CON),
    case("activate_key", dict(disc_empty(), get_enabled_set=[["web/exa"]], get_disabled_set=[[]], set_plugin_enabled=None),
         key="web/exa", enable=True, console=CON),   # already enabled -> False
    case("activate_key", dict(disc_empty(), get_enabled_set=[[]], get_disabled_set=[["exa"]], set_plugin_enabled=None),
         key="web/exa", enable=True, console=None),   # alias in other -> write
    case("activate_key", dict(disc_empty(), get_enabled_set=[[]], get_disabled_set=[["web/exa"]],
                              get_hermes_home="/home", read_home_selection={"plugins": {"disabled_by_update": {"web/exa": {}}}},
                              set_plugin_enabled=None),
         key="web/exa", enable=False, console=CON),   # disabled_by_update -> write
    case("activate_key", dict(disc_empty(), get_enabled_set=[[]], get_disabled_set=[["web/exa"]],
                              get_hermes_home="/home", read_home_selection=None, set_plugin_enabled=None),
         key="web/exa", enable=False, console=CON),   # already disabled, no update -> False
    case("activate_key", dict(disc_empty(), get_enabled_set=[[]], get_disabled_set=[[]],
                              set_plugin_enabled=raiser("PluginOperationError", "recalled")),
         key="web/exa", enable=True, console=CON),
    case("activate_key", dict(disc_empty(), get_enabled_set=raiser("RuntimeError")), key="k", enable=True),
]

# ── _apply_activation ──
CASES += [
    pure("apply_activation", enabled=["a"], disabled=["web/exa", "exa", "b"], key="web/exa", aliases=["exa"], enable=True),
    pure("apply_activation", enabled=["web/exa", "exa"], disabled=["b"], key="web/exa", aliases=["exa"], enable=False),
    pure("apply_activation", enabled=[], disabled=[], key="k", aliases=[], enable=True),
    pure("apply_activation", enabled=["k"], disabled=["k"], key="k", aliases=["k"], enable=False),
    pure("apply_activation", enabled=["x"], disabled=["y"], key="web/exa", aliases=["exa", "old"], enable=True),
]

# ── _forget_plugin_config ──
CASES += [
    case("forget_plugin_config", {"load_config": {"plugins": {"enabled": ["web/exa", "keep"], "disabled": ["exa"]}},
                                  "save_config": None}, aliases=["web/exa", "exa"]),
    case("forget_plugin_config", {"load_config": {"plugins": {"entries": {"web/exa": {"g": 1}, "keep": {}}}},
                                  "save_config": None}, aliases=["web/exa"]),
    case("forget_plugin_config", {"load_config": {"memory": {"provider": " web/exa "}}, "save_config": None},
         aliases=["web/exa"]),
    case("forget_plugin_config", {"load_config": {"plugins": {"enabled": ["other"]}}, "save_config": None},
         aliases=["web/exa"]),   # no change
    case("forget_plugin_config", {"load_config": {"memory": {"provider": None}}, "save_config": None}, aliases=[""]),
    case("forget_plugin_config", {"load_config": raiser("RuntimeError")}, aliases=["x"]),
    case("forget_plugin_config", {"load_config": {}, "save_config": raiser("OSError")}, aliases=["web/exa"]),
]

# ── _plugin_selection_version ──
CASES += [
    case("plugin_selection_version", {"get_hermes_home": "/home", "_digest": "abc123"}),
    case("plugin_selection_version", {"get_hermes_home": "/home", "_digest": None}),
    case("plugin_selection_version", {"get_hermes_home": "/home", "_digest": ""}),
    case("plugin_selection_version", {"get_hermes_home": raiser("RuntimeError")}),
    pure("plugin_selection_version"),   # fail-closed get_hermes_home
]

# ── _refuse_recalled_entry ──
CASES += [
    case("refuse_recalled_entry", {"refuse_if_installed_removed": None}, entry=entry("x", "web/x", "user", "/u/x")),
    case("refuse_recalled_entry", {}, entry=entry("x", "web/x", "bundled", "/b/x")),
    case("refuse_recalled_entry", {}, entry=None),
    case("refuse_recalled_entry", {"refuse_if_installed_removed": raiser("PluginOperationError", "recalled")},
         entry=entry("x", "web/x", "git", "/u/x")),
    pure("refuse_recalled_entry", entry=entry("x", "web/x", "user", "/u/x")),   # fail-closed
]

# ── _tri_state_flag ──
CASES += [
    pure("tri_state_flag", args={"enable": True}, yes_attr="enable", no_attr="no_enable"),
    pure("tri_state_flag", args={"no_enable": True}, yes_attr="enable", no_attr="no_enable"),
    pure("tri_state_flag", args={}, yes_attr="enable", no_attr="no_enable"),
    pure("tri_state_flag", args={"enable": False, "no_enable": False}, yes_attr="enable", no_attr="no_enable"),
    pure("tri_state_flag", args={"enable": True, "no_enable": True}, yes_attr="enable", no_attr="no_enable"),
    pure("tri_state_flag", args={"enable": None}, yes_attr="enable", no_attr="no_enable"),
]

# ── _category_active_names ──
CASES += [
    case("category_active_names", {"get_current_memory_provider": "mem0"}),
    case("category_active_names", {"get_current_memory_provider": ""}),
    case("category_active_names", {"get_current_memory_provider": None}),
    case("category_active_names", {"get_current_memory_provider": raiser("RuntimeError")}),
    pure("category_active_names"),
]

# ── _saved_list_toolset_key ──
CASES += [
    case("saved_list_toolset_key", {"load_config": {"platform_toolsets": {"cli": ["core"]}},
                                    "parse_platform_toolsets_value": [["core"]], "get_plugin_toolset_key": "ts"}, name="x"),
    case("saved_list_toolset_key", {"load_config": {"platform_toolsets": {"cli": "core"}},
                                    "parse_platform_toolsets_value": [None]}, name="x"),
    case("saved_list_toolset_key", {"load_config": {"platform_toolsets": "notdict"}}, name="x"),
    case("saved_list_toolset_key", {"load_config": {}}, name="x"),
    case("saved_list_toolset_key", {"load_config": {"platform_toolsets": {"a": "x", "b": "y"}},
                                    "parse_platform_toolsets_value": [None, ["t"]], "get_plugin_toolset_key": "ts"}, name="x"),
    case("saved_list_toolset_key", {"load_config": {"platform_toolsets": {"cli": ["core"]}},
                                    "parse_platform_toolsets_value": raiser("RuntimeError")}, name="x"),
]

# ── _scan_level ──
CASES += [
    case("scan_level", {"is_dir": False}, base="/b", source="bundled", skip_names=[], prefix="", depth=0, seen={}),
    case("scan_level", {"is_dir": [True, True], "iterdir": [["/b/foo"]],
                        "native_manifest_file": "/b/foo/plugin.yaml",
                        "read_native_manifest": {"name": "foo", "version": "1", "description": "d"}},
         base="/b", source="bundled", skip_names=[], prefix="", depth=0, seen={}),
    case("scan_level", {"is_dir": [True, True], "iterdir": [["/b/foo"]],
                        "native_manifest_file": "/b/foo/plugin.yaml",
                        "read_native_manifest": {"name": "foo"}, "exists": True},
         base="/b", source="user", skip_names=[], prefix="", depth=0, seen={}),   # git label
    case("scan_level", {"is_dir": [True, True], "iterdir": [["/b/memory"]],
                        "native_manifest_file": "/b/memory/plugin.yaml",
                        "read_native_manifest": {"name": "memory"}},
         base="/b", source="bundled", skip_names=["memory"], prefix="", depth=0, seen={}),   # skipped
    case("scan_level", {"is_dir": [True, False], "iterdir": raiser("OSError", "unreadable"), "logger_warning": None},
         base="/b", source="bundled", skip_names=[], prefix="", depth=0, seen={}),
    case("scan_level", {"is_dir": [True, True, True], "iterdir": [["/b/cat"]],
                        "native_manifest_file": [None, "/b/cat/sub/plugin.yaml"], "exists": False,
                        "read_native_manifest": {"name": "sub"}},
         base="/b", source="user", skip_names=[], prefix="", depth=0, seen={}),   # recursion
    case("scan_level", {"is_dir": [True, True], "iterdir": [["/b/foo"]],
                        "native_manifest_file": raiser("ValueError", "bad"), "logger_warning": None},
         base="/b", source="bundled", skip_names=[], prefix="", depth=0, seen={}),   # PluginOpError caught
]

# ── _discover_all_plugins ──
CASES += [
    case("discover_all_plugins", dict(disc_empty())),
    case("discover_all_plugins", {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user",
                                  "is_dir": [True, True, False], "iterdir": [["/bundled/foo"]],
                                  "native_manifest_file": "/bundled/foo/plugin.yaml",
                                  "read_native_manifest": {"name": "foo"},
                                  "discover_entrypoint_manifests": [[]]}),
    case("discover_all_plugins", {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user",
                                  "is_dir": False, "discover_entrypoint_manifests":
                                  [[{"name": "ep", "version": "1", "description": "d", "path": "mod:attr"}]]}),
    case("discover_all_plugins", {"get_bundled_plugins_dir": raiser("RuntimeError")}),
    pure("discover_all_plugins"),   # fail-closed
]

# ── _default_on ──
CASES += [
    case("default_on", {"is_dir": True, "native_manifest_file": "/b/x/plugin.yaml",
                        "read_native_manifest": {"kind": "backend"}}, dir_path="/b/x", source="bundled"),
    case("default_on", {"is_dir": True, "native_manifest_file": "/b/x/plugin.yaml",
                        "read_native_manifest": {"kind": "standalone"}}, dir_path="/b/x", source="bundled"),
    case("default_on", {"is_dir": False}, dir_path="/b/x", source="bundled"),
    case("default_on", {"is_dir": True, "_plugins_dir": "/user", "_declares_model_provider_kind": True},
         dir_path="/user/x", source="user"),
    case("default_on", {"is_dir": True, "_plugins_dir": "/user", "native_manifest_file": "/user/model-providers/x/plugin.yaml",
                        "read_native_manifest": {"kind": "model-provider"}},
         dir_path="/user/model-providers/x", source="user"),
    case("default_on", {"is_dir": True, "_plugins_dir": "/user"}, dir_path="/user/_hidden", source="user"),
    case("default_on", {"is_dir": True, "native_manifest_file": None}, dir_path="/b/x", source="bundled"),
    case("default_on", {"is_dir": True, "native_manifest_file": "/b/x/plugin.yaml",
                        "read_native_manifest": raiser("ValueError")}, dir_path="/b/x", source="bundled"),
]


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    seen = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
            seen[c["func"]] = seen.get(c["func"], 0) + 1
    thin = {k: v for k, v in seen.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(seen), OUT))
    if thin:
        print("WARNING funcs with <5 cases: %s" % thin)


if __name__ == "__main__":
    main()