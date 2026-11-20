#!/usr/bin/env python3
"""Generate tests/fixtures/plugins_cmd_flow2.jsonl — differential cases for the
UNCLAIMED remainder of hermes_cli/plugins_cmd.py (src/cli/plugins_cmd_flow2.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single value
reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, MISSING script is fail-closed
RuntimeError on both sides). A seam whose live result is a LIST is scripted one level
deeper ([["a"]] / [[]]); a bare array is a per-call QUEUE.

Domain (see the port header): ASCII inputs; paths are POSIX strings; integral floats
normalize to ints on both sides; set-valued inputs are lists of strings in fixture
order; entries are 6-element arrays [name, version, description, source, dir_path, key]
with str name/key/dir_path; read_install_metadata scripts are dicts with str keys;
catalog records carry a str catalog_name; check_hours is a number; console markers are
non-empty dicts or null; args are plain dicts.

Run:  python tests/gen_plugins_cmd_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "plugins_cmd_flow2.jsonl")

CON = {"console": 1}
FAIL_EXIT = {"__raise__": "SystemExit", "msg": "1"}


def case(func, seams=None, **inp):
    d = dict(inp)
    d["seams"] = dict(seams or {})
    d.setdefault("env", {})
    return {"func": func, "in": d}


def pure(func, **inp):
    d = dict(inp)
    d["seams"] = {}
    d.setdefault("env", {})
    return {"func": func, "in": d}


def raiser(kind, msg="boom", **extra):
    d = {"__raise__": kind, "msg": msg}
    d.update(extra)
    return d


def entry(name, key, source="user", dir_path=None, version="1.0", description="d"):
    return [name, version, description, source, dir_path if dir_path is not None else "/u/" + name, key]


def set_base(**over):
    d = {
        "discover_all_plugins": [[]],
        "legacy_plugin_alias": None,
        "read_install_metadata": {},
        "refuse_recalled_entry": None,
        "hermes_home_key": "/h",
        "threading_lock": {"lock": 1},
        "plugin_selection_version": "v1",
        "get_hermes_home": "/home",
        "read_home_selection": {"plugins": {"enabled": ["x"], "disabled": ["y"]}},
        "apply_activation": {"state": {"enabled": ["x", "web/exa"], "disabled": []}},
        "_plugins_dir": "/user",
        "admit_plugin_set_change": None,
    }
    d.update(over)
    return d


CASES = []

# ── _admit_and_save_plugin_sets ──
CASES += [
    case("admit_and_save_plugin_sets", {"_plugins_dir": "/user", "admit_plugin_set_change": None},
         enabled=["a"], disabled=["b"], extra_dirs=[], console=CON, action="enable",
         expected_config="v1", plugin="a", settled=["a"]),
    case("admit_and_save_plugin_sets",
         {"_plugins_dir": "/user", "admit_plugin_set_change": raiser("DependencyConflict", "conflict"),
          "markup_escape": "esc", "console_print": None},
         enabled=["a"], disabled=[], console=CON, action="enable", settled=["a"]),
    case("admit_and_save_plugin_sets",
         {"_plugins_dir": "/user", "admit_plugin_set_change": raiser("AdmissionRefused", "refused"),
          "console_print": None},
         enabled=[], disabled=["b"], console=CON, action="disable", settled=["b"]),
    case("admit_and_save_plugin_sets",
         {"_plugins_dir": "/user", "admit_plugin_set_change": raiser("AdmissionRefused", "refused")},
         enabled=[], disabled=[], console=None, action="enable", settled=[]),
    case("admit_and_save_plugin_sets",
         {"_plugins_dir": "/user", "admit_plugin_set_change": raiser("RuntimeError")},
         enabled=["a"], console=CON, settled=["a"]),
    case("admit_and_save_plugin_sets",
         {"_plugins_dir": "/user", "admit_plugin_set_change": raiser("SystemExit", "1")},
         enabled=["a"], console=CON, settled=["a"]),
    case("admit_and_save_plugin_sets",
         {"_plugins_dir": "/user", "admit_plugin_set_change": raiser("DependencyConflict", "c"),
          "markup_escape": raiser("RuntimeError", "escfail")},
         enabled=["a"], console=CON, settled=["a"]),
    case("admit_and_save_plugin_sets",
         {"_plugins_dir": "/user", "admit_plugin_set_change": raiser("DependencyConflict", "c")},
         enabled=["a"], console=None, settled=["a"]),
    case("admit_and_save_plugin_sets", {"_plugins_dir": "/user", "admit_plugin_set_change": None},
         enabled=["z", "a", "z"], disabled=["m", "b"], extra_dirs=["/e"], console=CON,
         expected_config=None, plugin=None, settled=["z", "a"]),
    pure("admit_and_save_plugin_sets", enabled=["a"], console=CON, settled=["a"]),   # fail-closed _plugins_dir
]

# ── _selection_lock ──
CASES += [
    case("selection_lock", {"hermes_home_key": "/h", "threading_lock": {"lock": 1}}, locks={}),
    case("selection_lock", {"hermes_home_key": "/h", "threading_lock": {"lock": 1}}, locks={"/h": {"lock": 9}}),
    case("selection_lock", {"hermes_home_key": raiser("RuntimeError"), "threading_lock": {"lock": 1}}, locks={}),
    case("selection_lock", {"hermes_home_key": "/h", "threading_lock": raiser("RuntimeError")}, locks={}),
    case("selection_lock", {"hermes_home_key": "/h", "threading_lock": {"lock": 2}}, locks={"/other": {"lock": 5}}),
    pure("selection_lock", locks={}),   # fail-closed hermes_home_key
]

# ── _set_plugin_enabled ──
CASES += [
    case("set_plugin_enabled", set_base(), name="web/exa", enable=True, aliases=["exa"], console=CON, locks={}),
    case("set_plugin_enabled",
         set_base(read_home_selection={"plugins": {"enabled": ["web/exa"], "disabled": []}},
                  apply_activation={"state": {"enabled": [], "disabled": ["web/exa"]}}),
         name="web/exa", enable=False, aliases=["exa"], console=CON, locks={}),
    case("set_plugin_enabled", set_base(refuse_recalled_entry=raiser("PluginOperationError", "recalled")),
         name="web/exa", enable=True, aliases=["exa"], console=CON, locks={}),
    case("set_plugin_enabled", set_base(read_home_selection="notdict"),
         name="web/exa", enable=False, aliases=[], console=CON, locks={}),
    case("set_plugin_enabled", set_base(apply_activation=None),
         name="web/exa", enable=False, aliases=[], console=CON, locks={}),   # apply_activation fail-closed? no: None scripted
    case("set_plugin_enabled",
         set_base(admit_plugin_set_change=raiser("AdmissionRefused", "refused"), console_print=None),
         name="web/exa", enable=False, aliases=["exa"], console=CON, locks={}),
    case("set_plugin_enabled",
         set_base(discover_all_plugins=[[entry("web-exa", "web/exa")]],
                  refuse_recalled_entry=None),
         name="web/exa", enable=True, aliases=["exa"], console=CON, locks={}),
    case("set_plugin_enabled", set_base(admit_plugin_set_change=raiser("KeyboardInterrupt")),
         name="web/exa", enable=False, aliases=[], console=CON, locks={}),
    case("set_plugin_enabled", set_base(read_home_selection=None),
         name="web/exa", enable=False, aliases=[], console=None, locks={"/h": {"lock": 9}}),
    case("set_plugin_enabled",
         {k: v for k, v in set_base().items() if k != "apply_activation"},
         name="web/exa", enable=False, aliases=[], console=CON, locks={}),   # apply_activation MISSING -> fail-closed
]

# ── _catalog_installed_dir ──
CASES += [
    case("catalog_installed_dir",
         {"read_install_metadata": {"k1": {}}, "_plugins_dir": "/user", "is_dir": True,
          "catalog_install_record": {"catalog_name": "target"}}, name="target"),
    case("catalog_installed_dir",
         {"read_install_metadata": {"k1": {}}, "_plugins_dir": "/user", "is_dir": True,
          "catalog_install_record": {"catalog_name": "other"}}, name="target"),
    case("catalog_installed_dir",
         {"read_install_metadata": {"k1": {}, "k2": {}}, "_plugins_dir": "/user", "is_dir": True,
          "catalog_install_record": {"catalog_name": "target"}}, name="target"),
    case("catalog_installed_dir", {"read_install_metadata": raiser("PluginOperationError", "bad")}, name="target"),
    case("catalog_installed_dir",
         {"read_install_metadata": {"k1": {}}, "_plugins_dir": "/user", "is_dir": False}, name="target"),
    case("catalog_installed_dir",
         {"read_install_metadata": {"k1": {}}, "_plugins_dir": "/user", "is_dir": True,
          "catalog_install_record": {"no_name": 1}}, name="target"),   # KeyError catalog_name
    case("catalog_installed_dir",
         {"read_install_metadata": {"k1": {}}, "_plugins_dir": "/user", "is_dir": True,
          "catalog_install_record": None}, name="target"),
    case("catalog_installed_dir",
         {"read_install_metadata": {"k1": {}}, "_plugins_dir": "/user", "is_dir": True,
          "catalog_install_record": "notdict"}, name="target"),   # TypeError subscript
    case("catalog_installed_dir", {"read_install_metadata": None}, name="target"),   # TypeError not iterable
    pure("catalog_installed_dir", name="target"),   # fail-closed read_install_metadata
]

# ── _find_plugin_entry ──
CASES += [
    case("find_plugin_entry", {"discover_all_plugins": [[entry("web-exa", "web/exa")]], "legacy_plugin_alias": None},
         name="web/exa"),
    case("find_plugin_entry",
         {"discover_all_plugins": [[entry("web-exa", "web/exa")]], "legacy_plugin_alias": "web/exa"}, name="old"),
    case("find_plugin_entry", {"discover_all_plugins": [[entry("web-exa", "web/exa")]], "legacy_plugin_alias": None},
         name="exa"),
    case("find_plugin_entry",
         {"discover_all_plugins": [[entry("a", "web/exa"), entry("b", "other/exa")]], "legacy_plugin_alias": None},
         name="exa"),
    case("find_plugin_entry",
         {"discover_all_plugins": [[entry("mw", "memory-wiki", "user", "/user/memory-wiki")]],
          "legacy_plugin_alias": None, "read_install_metadata": {"memory-wiki": {}},
          "_plugins_dir": "/user", "is_dir": True, "catalog_install_record": {"catalog_name": "memory-wiki"}},
         name="memory-wiki"),
    case("find_plugin_entry", {"discover_all_plugins": [[]], "legacy_plugin_alias": None, "read_install_metadata": {}},
         name="nope"),
    case("find_plugin_entry", {"discover_all_plugins": raiser("RuntimeError")}, name="x"),
    case("find_plugin_entry",
         {"discover_all_plugins": [[["n", "1", "d", "user", "/u", 5]]], "legacy_plugin_alias": None}, name="x"),
    pure("find_plugin_entry", name="x"),   # fail-closed discover_all_plugins
]

# ── _enable_consent ──
CASES += [
    case("enable_consent",
         {"declared_capabilities_for_key": [["c1"]], "run_capability_consent": None, "resolve_tool_override_grant": None},
         console=CON, key="web/exa", allow_tool_override=True),
    case("enable_consent", {"declared_capabilities_for_key": [[]]}, console=CON, key="web/exa", allow_tool_override=None),
    case("enable_consent",
         {"declared_capabilities_for_key": [["c1"]], "run_capability_consent": None, "resolve_tool_override_grant": None},
         console=CON, key="web/exa", allow_tool_override=False),
    case("enable_consent",
         {"declared_capabilities_for_key": [[]], "resolve_tool_override_grant": None},
         console=None, key="web/exa", allow_tool_override=True),
    case("enable_consent", {"declared_capabilities_for_key": raiser("RuntimeError")}, console=CON, key="k", allow_tool_override=True),
    case("enable_consent",
         {"declared_capabilities_for_key": [["c1"]], "run_capability_consent": raiser("RuntimeError")},
         console=CON, key="k", allow_tool_override=None),
    case("enable_consent", {"declared_capabilities_for_key": [["c1"]], "run_capability_consent": None},
         console=CON, key="k", allow_tool_override=None),   # present-null skip resolve
    pure("enable_consent", console=CON, key="k", allow_tool_override=True),   # fail-closed declared
]

# ── _resolved_user_plugin_dir ──
CASES += [
    case("resolved_user_plugin_dir",
         {"discover_all_plugins": [[entry("x", "web/x", "user", "/u/x")]], "legacy_plugin_alias": None, "is_dir": True},
         name="web/x"),
    case("resolved_user_plugin_dir",
         {"discover_all_plugins": [[entry("x", "web/x", "bundled", "/b/x")]], "legacy_plugin_alias": None},
         name="web/x"),
    case("resolved_user_plugin_dir",
         {"discover_all_plugins": [[entry("x", "web/x", "user", "/u/x")]], "legacy_plugin_alias": None, "is_dir": False},
         name="web/x"),
    case("resolved_user_plugin_dir",
         {"discover_all_plugins": [[]], "legacy_plugin_alias": None, "read_install_metadata": {}}, name="nope"),
    case("resolved_user_plugin_dir",
         {"discover_all_plugins": [[entry("x", "web/x", "git", "/u/x")]], "legacy_plugin_alias": None, "is_dir": True},
         name="web/x"),
    case("resolved_user_plugin_dir",
         {"discover_all_plugins": [[entry("x", "web/x", "user", "/u/x")]], "legacy_plugin_alias": None,
          "is_dir": raiser("OSError")}, name="web/x"),
    pure("resolved_user_plugin_dir", name="x"),   # fail-closed discover
]

# ── _catalog ──
CASES += [
    case("catalog", {"catalog_module": {"mod": 1}}),
    case("catalog", {"catalog_module": "catalog"}),
    case("catalog", {"catalog_module": None}),
    case("catalog", {"catalog_module": raiser("RuntimeError")}),
    pure("catalog"),   # fail-closed
]

# ── _action_update ──
CASES += [
    case("action_update", {"cmd_update": None}, args={"name": "x"}),
    case("action_update", {"Console": CON, "fail": FAIL_EXIT}, args={"name": "x", "yes": True}),
    case("action_update", {"Console": CON, "fail": FAIL_EXIT}, args={"name": "", "force": True}),
    case("action_update", {"cmd_update_all": None}, args={"name": ""}),
    case("action_update", {"cmd_update_all": None}, args={"name": "", "yes": True}),
    case("action_update", {"cmd_update": None}, args={"name": "x", "force": True}),
    case("action_update", {"cmd_update": raiser("RuntimeError")}, args={"name": "x"}),
    case("action_update", {}, args={}),   # missing name -> AttributeError
    case("action_update", {"Console": CON, "fail": None, "cmd_update": None}, args={"name": "x", "yes": True}),
    pure("action_update", args={"name": ""}),   # fail-closed cmd_update_all
]

# ── _action_auto_update ──
CASES += [
    case("action_auto_update",
         {"Console": CON, "auto_update_state": {"auto_apply": True, "check_hours": 24}, "console_print": None},
         args={}),
    case("action_auto_update",
         {"Console": CON, "set_auto_update": {"auto_apply": False, "check_hours": 0}, "console_print": None},
         args={"state": "on"}),
    case("action_auto_update",
         {"Console": CON, "set_auto_update": {"auto_apply": True, "check_hours": 6}, "console_print": None},
         args={"state": "off"}),
    case("action_auto_update",
         {"Console": CON, "auto_update_state": raiser("PluginOperationError", "bad"), "fail": FAIL_EXIT}, args={}),
    case("action_auto_update",
         {"Console": CON, "auto_update_state": {"check_hours": 24}, "console_print": None}, args={}),   # KeyError auto_apply
    case("action_auto_update",
         {"Console": CON, "auto_update_state": {"auto_apply": False, "check_hours": 0}, "console_print": None}, args={}),
    case("action_auto_update",
         {"Console": CON, "auto_update_state": {"auto_apply": True, "check_hours": 24}, "console_print": raiser("RuntimeError")},
         args={}),
    case("action_auto_update", {"Console": CON, "auto_update_state": "notdict"}, args={}),   # TypeError subscript
    case("action_auto_update",
         {"Console": CON, "auto_update_state": {"auto_apply": True, "check_hours": "x"}}, args={}),   # TypeError >
    case("action_auto_update",
         {"Console": CON, "auto_update_state": raiser("PluginOperationError", "bad"), "fail": None}, args={}),   # UnboundLocalError
    pure("action_auto_update", args={}),   # fail-closed Console
]

# ── _action_pack ──
CASES += [
    case("action_pack", {"pack_command": None}, args={"a": 1}),
    case("action_pack", {"pack_command": None}, args={}),
    case("action_pack", {"pack_command": raiser("RuntimeError")}, args={"a": 1}),
    case("action_pack", {"pack_command": {"result": 1}}, args={"a": 1}),   # statement position -> ret null
    pure("action_pack", args={"a": 1}),   # fail-closed
]

# ── _action_idle_tasks ──
CASES += [
    case("action_idle_tasks", {"cmd_idle_tasks": None}, args={"name": "x", "pause": True, "resume": False}),
    case("action_idle_tasks", {"cmd_idle_tasks": None}, args={"name": "x", "pause": None, "resume": None}),
    case("action_idle_tasks", {}, args={"pause": True, "resume": False}),   # missing name
    case("action_idle_tasks", {}, args={"name": "x", "resume": False}),   # missing pause
    case("action_idle_tasks", {"cmd_idle_tasks": raiser("RuntimeError")}, args={"name": "x", "pause": True, "resume": False}),
    case("action_idle_tasks", {"cmd_idle_tasks": {"r": 1}}, args={"name": "x", "pause": False, "resume": True}),
    pure("action_idle_tasks", args={"name": "x", "pause": True, "resume": False}),   # fail-closed
]

# ── _refuse_legacy_relay (nested in cmd_enable) ──
LK = ["nemo_relay", "observability/nemo_relay"]
ENV = "HERMES_NEMO_RELAY_PLUGINS_TOML"
CASES += [
    case("refuse_legacy_relay", {"fail": FAIL_EXIT}, console=CON, plugin="nemo_relay", legacy_keys=LK, env=ENV),
    case("refuse_legacy_relay", {}, console=CON, plugin="other", legacy_keys=LK, env=ENV),
    case("refuse_legacy_relay", {"fail": raiser("RuntimeError")}, console=CON, plugin="nemo_relay", legacy_keys=LK, env=ENV),
    case("refuse_legacy_relay", {"fail": FAIL_EXIT}, console=None, plugin="observability/nemo_relay", legacy_keys=LK, env=ENV),
    case("refuse_legacy_relay", {}, console=CON, plugin="nemo_relay", legacy_keys=[], env=ENV),
    pure("refuse_legacy_relay", console=CON, plugin="nemo_relay", legacy_keys=LK, env=ENV),   # fail-closed fail
]

# ── _first_toolset (nested in _get_plugin_toolset_key) ──
CASES += [
    case("first_toolset", {"registry_get_entry": [{"toolset": "ts1"}]}, tool_names=["t1", "t2"]),
    case("first_toolset", {"registry_get_entry": [None, {"toolset": "ts2"}]}, tool_names=["t1", "t2"]),
    case("first_toolset", {"registry_get_entry": [{"toolset": ""}, {"toolset": "ts"}]}, tool_names=["t1", "t2"]),
    case("first_toolset", {"registry_get_entry": [None]}, tool_names=["t1", "t2"]),
    case("first_toolset", {"registry_get_entry": [{"no_ts": 1}]}, tool_names=["t1"]),   # AttributeError toolset
    case("first_toolset", {"registry_get_entry": raiser("RuntimeError")}, tool_names=["t1"]),
    case("first_toolset", {}, tool_names=[]),
    case("first_toolset", {"registry_get_entry": ["str"]}, tool_names=["t1"]),   # AttributeError str toolset
    pure("first_toolset", tool_names=["t1"]),   # fail-closed registry_get_entry
]

# ── _from_loaded_plugin (nested) ──
MGR = {"_plugins": {"web/exa": {"manifest": {"name": "web-exa"}, "tools_registered": ["t1"]}}}
CASES += [
    case("from_loaded_plugin",
         {"discover_plugins": None, "get_plugin_manager": MGR, "registry_get_entry": [{"toolset": "ts"}]}, name="web/exa"),
    case("from_loaded_plugin",
         {"discover_plugins": None, "get_plugin_manager": MGR, "registry_get_entry": [{"toolset": "ts"}]}, name="web-exa"),
    case("from_loaded_plugin",
         {"discover_plugins": None, "get_plugin_manager": MGR, "registry_get_entry": [{"toolset": "ts"}]}, name="nope"),
    case("from_loaded_plugin", {"discover_plugins": raiser("RuntimeError")}, name="x"),
    case("from_loaded_plugin", {"discover_plugins": None, "get_plugin_manager": None}, name="x"),   # NoneType _plugins
    case("from_loaded_plugin", {"discover_plugins": None, "get_plugin_manager": {"no_plugins": 1}}, name="x"),
    case("from_loaded_plugin",
         {"discover_plugins": None, "get_plugin_manager": {"_plugins": {"k": {"no_manifest": 1}}}}, name="k"),
    case("from_loaded_plugin",
         {"discover_plugins": None, "get_plugin_manager": {"_plugins": {"k": {"manifest": None}}}}, name="k"),
    case("from_loaded_plugin",
         {"discover_plugins": None, "get_plugin_manager": {"_plugins": {"k": {"manifest": {"name": "k"}}}}}, name="k"),
    case("from_loaded_plugin", {"discover_plugins": None, "get_plugin_manager": {"_plugins": []}}, name="x"),   # items
    pure("from_loaded_plugin", name="x"),   # fail-closed discover_plugins
]

# ── _from_manifest_on_disk (nested) ──
CASES += [
    case("from_manifest_on_disk",
         {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user", "is_dir": [True, True],
          "read_manifest": {"provides_tools": ["t1"]}, "registry_get_entry": [{"toolset": "ts"}]}, name="x"),
    case("from_manifest_on_disk",
         {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user", "is_dir": [False, True, True],
          "read_manifest": {"provides_tools": ["t1"]}, "registry_get_entry": [{"toolset": "ts"}]}, name="x"),
    case("from_manifest_on_disk",
         {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user", "is_dir": [False, False]}, name="x"),
    case("from_manifest_on_disk",
         {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user", "is_dir": [True, True],
          "read_manifest": {}}, name="x"),
    case("from_manifest_on_disk",
         {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user", "is_dir": [True, True],
          "read_manifest": raiser("RuntimeError")}, name="x"),
    case("from_manifest_on_disk", {"get_bundled_plugins_dir": raiser("RuntimeError")}, name="x"),
    case("from_manifest_on_disk", {"get_bundled_plugins_dir": None, "_plugins_dir": "/user"}, name="x"),   # NoneType is_dir
    case("from_manifest_on_disk",
         {"get_bundled_plugins_dir": "/bundled", "_plugins_dir": "/user", "is_dir": [True, True],
          "read_manifest": {"provides_tools": []}}, name="x"),
    pure("from_manifest_on_disk", name="x"),   # fail-closed get_bundled_plugins_dir
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