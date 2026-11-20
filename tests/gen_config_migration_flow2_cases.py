#!/usr/bin/env python3
"""Generate tests/fixtures/config_migration_flow2.jsonl for the
config_migration_flow2 FLOW oracle. Emits fixture INPUTS only (scripts +
inputs); the oracle computes expected values from the live quarry defs.
>= 5 cases per ported function, covering happy / skip / boundary / fail-closed
/ BaseException-propagation / import-failure / real-exception paths. ASCII
fixture domain (posix paths; no drive letters / backslashes / '~').
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_migration_flow2.jsonl")

SE = {"type": "SystemExit", "message": "1", "code": 1}
KI = {"type": "KeyboardInterrupt", "message": "ki"}
GE = {"type": "GeneratorExit", "message": "ge"}
OSERR = {"type": "OSError", "message": "disk error"}
VALERR = {"type": "ValueError", "message": "bad value"}
RTE = {"type": "RuntimeError", "message": "runtime boom"}
FNFE = {"type": "FileNotFoundError", "message": "no file"}


def imp_fail(mod):
    return {"type": "ModuleNotFoundError",
            "message": f"import of {mod} halted; None in sys.modules", "name": mod}


CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


def R(config=None, quiet=True, results=None):
    d = {"results": results if results is not None else {"config_added": [], "warnings": []},
         "quiet": quiet}
    if config is not None:
        d["state"] = {"raw_config": config}
    return d


# ── migrate_to_34 ───────────────────────────────────────────────────────────
P34 = {"display": {"personality": "stoic"}, "agent": {"system_prompt": "txt"}}
IO34 = {"normalize_personality_name": {"ret": "stoic"},
        "prompt_text": {"ret": "txt"},
        "available_personalities": {"ret": {"stoic": {"name": "stoic"}}},
        "render_personality_prompt": {"ret": "txt"}}
add("migrate_to_34", {**R(P34, quiet=False), "io": IO34})
add("migrate_to_34", {**R(P34, quiet=True), "io": IO34})
add("migrate_to_34", {**R({"agent": {"system_prompt": "txt"}}, quiet=False),
                      "io": {"prompt_text": {"ret": "txt"},
                             "available_personalities": {"ret": {"a": {}}},
                             "render_personality_prompt": {"ret": "txt"}}})
add("migrate_to_34", {**R({"display": {"personality": "x"}, "agent": {"system_prompt": "mine"}}),
                      "io": {"normalize_personality_name": {"ret": ""},
                             "prompt_text": {"ret": "mine"},
                             "available_personalities": {"ret": {}}}})
add("migrate_to_34", {**R({"display": {"personality": "x"}}), "io": {}})   # fail closed normalize
add("migrate_to_34", {**R({"agent": {"system_prompt": "t"}}),
                      "io": {"prompt_text": {"raise": SE}}})
add("migrate_to_34", {**R(P34), "io": {"available_personalities": {"import_raise": imp_fail("hermes_cli.personality")}}})
add("migrate_to_34", {**R(P34), "io": {**IO34, "render_personality_prompt": {"raise": KI}}})
add("migrate_to_34", {**R({"display": {"personality": "stoic"}}, quiet=True, results={}),
                      "io": {"normalize_personality_name": {"ret": "stoic"}}})   # commit KeyError
add("migrate_to_34", {**R({"display": 5, "agent": "x"}), "io": {}})   # no touch

# ── migrate_to_38 ───────────────────────────────────────────────────────────
P38 = {"plugins": {"enabled": ["nemo_relay", "other"]}}
add("migrate_to_38", {**R(P38, quiet=False), "io": {"legacy_relay_plugin_keys": {"ret": ["nemo_relay"]}}})
add("migrate_to_38", {**R(P38, quiet=True), "io": {"legacy_relay_plugin_keys": {"ret": ["nemo_relay"]}}})
add("migrate_to_38", {**R({"plugins": 5}), "io": {}})   # plugins non-dict
add("migrate_to_38", {**R({"plugins": {"enabled": ["a"]}}), "io": {"legacy_relay_plugin_keys": {"ret": []}}})
add("migrate_to_38", {**R({"plugins": {"enabled": "x"}}), "io": {"legacy_relay_plugin_keys": {"ret": ["nemo_relay"]}}})
add("migrate_to_38", {**R({"plugins": {"enabled": ["a"]}}), "io": {}})   # fail closed legacy
add("migrate_to_38", {**R(P38), "io": {"legacy_relay_plugin_keys": {"ret": ["nemo_relay"]},
                                       "persist_migration": {"raise": SE}}})
add("migrate_to_38", {**R(P38), "io": {"legacy_relay_plugin_keys": {"import_raise": imp_fail("hermes_cli.relay_plugin_cutover")}}})
add("migrate_to_38", {**R(P38, quiet=True, results={}), "io": {"legacy_relay_plugin_keys": {"ret": ["nemo_relay"]}}})  # warnings KeyError
add("migrate_to_38", {**R(P38), "io": {"legacy_relay_plugin_keys": {"ret": [5]}}})   # join TypeError

# ── migrate_to_39 ───────────────────────────────────────────────────────────
P39 = {"platform_toolsets": {"cli": ["bfl", "web"], "gateway": ["web"]}}
add("migrate_to_39", {**R(P39, quiet=False), "io": {}})
add("migrate_to_39", {**R(P39, quiet=True), "io": {}})
add("migrate_to_39", {**R({"platform_toolsets": {"cli": ["bfl"]},
                           "known_builtin_toolsets": {"x": ["bfl", "y"]}}, quiet=False), "io": {}})
add("migrate_to_39", {**R({"platform_toolsets": {"cli": ["web"]}}), "io": {}})   # no bfl
add("migrate_to_39", {**R({"platform_toolsets": 5, "known_builtin_toolsets": "x"}), "io": {}})
add("migrate_to_39", {**R(P39), "io": {"read_raw_config": {"raise": OSERR}}})
add("migrate_to_39", {**R(P39), "io": {"commit": {"raise": SE}}})
add("migrate_to_39", {**R(P39), "io": {"commit": {"raise": GE}}})
add("migrate_to_39", {**R(P39, quiet=True, results={}), "io": {}})   # commit KeyError
add("migrate_to_39", {**R({"platform_toolsets": 5, "known_builtin_toolsets": {"x": ["bfl"]}}, quiet=False), "io": {}})

# ── agent_browser_env ───────────────────────────────────────────────────────
def ABE(io, home="/h/p1"):
    return {"home": home, "io": io}

add("agent_browser_env", ABE({"resolve_home_dotenv": {"ret": {"AGENT_BROWSER_PROFILE": "/data", "AGENT_BROWSER_HEADED": "true"}},
                              "hermes_home_key": ["k1", "k2"],
                              "get_routing_process_hermes_home": {"ret": "/h/other"}}))
add("agent_browser_env", ABE({"resolve_home_dotenv": {"ret": {}},
                              "hermes_home_key": ["k1", "k1"],
                              "get_routing_process_hermes_home": {"ret": "/h/p1"},
                              "os.environ.get": ["/exe", None, " args ", "1", "auto"]}))
add("agent_browser_env", ABE({"resolve_home_dotenv": {"ret": {"AGENT_BROWSER_HEADED": "yes"}},
                              "hermes_home_key": ["k", "k"],
                              "get_routing_process_hermes_home": {"ret": "/h"},
                              "os.environ.get": {"ret": "no"}}))
add("agent_browser_env", ABE({}))   # fail closed resolve
add("agent_browser_env", ABE({"resolve_home_dotenv": {"ret": {}}, "hermes_home_key": {"raise": SE}}))
add("agent_browser_env", ABE({"resolve_home_dotenv": {"import_raise": imp_fail("hermes_cli.env_loader")}}))
add("agent_browser_env", ABE({"resolve_home_dotenv": {"ret": 5},
                              "hermes_home_key": ["k", "k"],
                              "get_routing_process_hermes_home": {"ret": "/h"}}))   # dotenv.get AttributeError
add("agent_browser_env", ABE({"resolve_home_dotenv": {"ret": {"AGENT_BROWSER_ARGS": "  a , b  "}},
                              "hermes_home_key": ["k", "k"],
                              "get_routing_process_hermes_home": {"ret": "/h"},
                              "os.environ.get": {"ret": None}}))

# ── inside_pm_store ─────────────────────────────────────────────────────────
def IPS(value, io):
    return {"value": value, "io": io}

add("inside_pm_store", IPS("/store/chromium/123/chrome",
                           {"path.expanduser": {"ret": "/store/chromium/123/chrome"},
                            "path.resolve": ["/store/chromium/123/chrome", "/store", "/wstore"],
                            "pm.store_root": {"ret": "/store"},
                            "pm.writable_store_root": {"ret": "/wstore"}}))
add("inside_pm_store", IPS("/other/chrome",
                           {"path.expanduser": {"ret": "/other/chrome"},
                            "path.resolve": ["/other/chrome", "/store", "/wstore"],
                            "pm.store_root": {"ret": "/store"},
                            "pm.writable_store_root": {"ret": "/wstore"}}))
add("inside_pm_store", IPS("/wstore/x",
                           {"path.expanduser": {"ret": "/wstore/x"},
                            "path.resolve": ["/wstore/x", "/store", "/wstore"],
                            "pm.store_root": {"ret": "/store"},
                            "pm.writable_store_root": {"ret": "/wstore"}}))
add("inside_pm_store", IPS("/x", {"path.expanduser": {"raise": OSERR}}))   # caught -> False
add("inside_pm_store", IPS("/x", {"path.expanduser": {"ret": "/x"},
                                  "path.resolve": {"ret": "/x"},
                                  "pm.store_root": {"raise": RTE}}))   # caught -> False
add("inside_pm_store", IPS("/x", {"path.expanduser": {"raise": VALERR}}))   # not caught
add("inside_pm_store", IPS("/x", {"path.expanduser": {"raise": SE}}))
add("inside_pm_store", IPS("/x", {"pm.paths": {"import_raise": imp_fail("pm")}}))
add("inside_pm_store", IPS("/x", {}))   # fail closed expanduser (RuntimeError caught -> False)
add("inside_pm_store", IPS("/x", {"path.expanduser": {"ret": None}}))   # AttributeError resolve

# ── browser_value_from_env ──────────────────────────────────────────────────
def BVE(key, value, io=None):
    return {"key": key, "value": value, "io": io or {}}

add("browser_value_from_env", BVE("chromium_args", "a, b\nc ,, d"))
add("browser_value_from_env", BVE("chromium_args", " , \n , "))
add("browser_value_from_env", BVE("headed", "YES"))
add("browser_value_from_env", BVE("headed", "no"))
add("browser_value_from_env", BVE("engine", "Chrome"))
add("browser_value_from_env", BVE("engine", "firefox"))
add("browser_value_from_env", BVE("user_data_dir", "rel/profile",
                                  {"os.path.expanduser": ["rel/profile", "rel/profile"],
                                   "os.path.abspath": {"ret": "/cwd/rel/profile"}}))
add("browser_value_from_env", BVE("user_data_dir", "/abs/profile",
                                  {"os.path.expanduser": {"ret": "/abs/profile"}}))
add("browser_value_from_env", BVE("executable_path", "/store/chrome",
                                  {"path.expanduser": {"ret": "/store/chrome"},
                                   "path.resolve": ["/store/chrome", "/store", "/w"],
                                   "pm.store_root": {"ret": "/store"},
                                   "pm.writable_store_root": {"ret": "/w"}}))
add("browser_value_from_env", BVE("executable_path", "/other/chrome",
                                  {"path.expanduser": {"ret": "/other/chrome"},
                                   "path.resolve": ["/other/chrome", "/store", "/w"],
                                   "pm.store_root": {"ret": "/store"},
                                   "pm.writable_store_root": {"ret": "/w"}}))
add("browser_value_from_env", BVE("other", "v"))
add("browser_value_from_env", BVE("user_data_dir", "rel",
                                  {"os.path.expanduser": {"ret": "rel"}}))   # fail closed abspath

# ── config_decided ──────────────────────────────────────────────────────────
def CD(key, current):
    return {"key": key, "current": current}

add("config_decided", CD("engine", "auto"))
add("config_decided", CD("engine", "chrome"))
add("config_decided", CD("engine", ""))
add("config_decided", CD("engine", None))
add("config_decided", CD("headed", "true"))
add("config_decided", CD("headed", "on"))
add("config_decided", CD("headed", None))
add("config_decided", CD("headed", "1"))
add("config_decided", CD("user_data_dir", None))
add("config_decided", CD("user_data_dir", ""))
add("config_decided", CD("user_data_dir", []))
add("config_decided", CD("user_data_dir", "x"))
add("config_decided", CD("user_data_dir", False))
add("config_decided", CD("engine", " AUTO "))
add("config_decided", CD("headed", " TRUE "))

# ── backend_removed_note ────────────────────────────────────────────────────
def BN(backend):
    return {"backend": backend}

add("backend_removed_note", BN(None))
add("backend_removed_note", BN(True))
add("backend_removed_note", BN(False))
add("backend_removed_note", BN("browser-use"))
add("backend_removed_note", BN("agent-browser"))
add("backend_removed_note", BN("camofox"))
add("backend_removed_note", BN("local"))
add("backend_removed_note", BN("browserbase"))
add("backend_removed_note", BN("nous"))
add("backend_removed_note", BN("unknown"))
add("backend_removed_note", BN("  Browser-Use  "))
add("backend_removed_note", BN(""))
add("backend_removed_note", BN(5))
add("backend_removed_note", BN({"v": "1"}))          # container repr quotes strings
add("backend_removed_note", BN([""]))
add("backend_removed_note", BN({"a": [1, "x"], "b": None}))
add("backend_removed_note", BN([True, "off", None]))

# ── move_agent_browser_env ──────────────────────────────────────────────────
def MAB(browser, io, home="/h/p1"):
    return {"home": home, "browser": browser, "io": io}

ENV_BASE = {"hermes_home_key": ["k", "k"],
            "get_routing_process_hermes_home": {"ret": "/h"},
            "os.environ.get": {"ret": None}}
add("move_agent_browser_env", MAB({}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_PROFILE": "/data", "AGENT_BROWSER_HEADED": "true"}},
    "os.path.expanduser": {"ret": "/data"}}))
add("move_agent_browser_env", MAB({"engine": "chrome"}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_ENGINE": "auto"}}}))
add("move_agent_browser_env", MAB({}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_PROFILE": "rel"}},
    "os.path.expanduser": ["rel", "rel"],
    "os.path.abspath": {"ret": "/cwd/rel"}}))
add("move_agent_browser_env", MAB({}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_ARGS": " , "}}}))
add("move_agent_browser_env", MAB({}, {}))   # fail closed resolve
add("move_agent_browser_env", MAB({}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_EXECUTABLE_PATH": "/x"}},
    "path.expanduser": {"raise": SE}}))
add("move_agent_browser_env", MAB({}, {"resolve_home_dotenv": {"import_raise": imp_fail("hermes_cli.env_loader")}}))
add("move_agent_browser_env", MAB({}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_EXECUTABLE_PATH": "/store/c"}},
    "path.expanduser": {"ret": "/store/c"},
    "path.resolve": ["/store/c", "/store", "/w"],
    "pm.store_root": {"ret": "/store"},
    "pm.writable_store_root": {"ret": "/w"}}))

# ── agent_browser_env_pending ───────────────────────────────────────────────
def PEND(config, io):
    return {"state": {"raw_config": config}, "io": io}

add("agent_browser_env_pending", PEND({"browser": {}}, {**ENV_BASE,
    "config.read_raw_config": {"ret": {"browser": {}}},
    "get_hermes_home": {"ret": "/h/p1"},
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_HEADED": "true"}}}))
add("agent_browser_env_pending", PEND({"browser": {}}, {**ENV_BASE,
    "get_hermes_home": {"ret": "/h"},
    "resolve_home_dotenv": {"ret": {}}}))
add("agent_browser_env_pending", PEND({"browser": 5}, {**ENV_BASE,
    "get_hermes_home": {"ret": "/h"},
    "resolve_home_dotenv": {"ret": {}}}))
add("agent_browser_env_pending", PEND({"browser": {}}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {}}}))   # fail closed get_hermes_home
add("agent_browser_env_pending", PEND({"browser": {}}, {"config.read_raw_config": {"raise": SE}}))
add("agent_browser_env_pending", PEND({"browser": {}}, {"config.read_raw_config": {"import_raise": imp_fail("hermes_cli.config")}}))
add("agent_browser_env_pending", PEND({"browser": {}}, {"config.read_raw_config": {"ret": 5}}))   # AttributeError get

# ── adopt_agent_browser_env ─────────────────────────────────────────────────
def ADOPT(config, io, home="/h/p1"):
    return {"home": home, "config": config, "io": io}

add("adopt_agent_browser_env", ADOPT({"browser": {}}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_HEADED": "true"}}}))
add("adopt_agent_browser_env", ADOPT({"browser": {}}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_PROFILE": "rel"}},
    "os.path.expanduser": ["rel", "rel"],
    "os.path.abspath": {"ret": "/cwd/rel"},
    "defer_startup_notice": {"ret": None}}))
add("adopt_agent_browser_env", ADOPT({"browser": {"engine": "chrome"}}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_ENGINE": "auto"}}}))
add("adopt_agent_browser_env", ADOPT({}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_HEADED": "1"}}}))
add("adopt_agent_browser_env", ADOPT({"browser": {}}, {}))   # fail closed resolve
add("adopt_agent_browser_env", ADOPT({"browser": {}}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_PROFILE": "rel"}},
    "os.path.expanduser": ["rel", "rel"],
    "os.path.abspath": {"ret": "/cwd/rel"},
    "defer_startup_notice": {"import_raise": imp_fail("hermes_cli.left_core_migration")}}))
add("adopt_agent_browser_env", ADOPT({"browser": {}}, {**ENV_BASE,
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_PROFILE": "rel"}},
    "os.path.expanduser": ["rel", "rel"],
    "os.path.abspath": {"ret": "/cwd/rel"},
    "defer_startup_notice": {"raise": SE}}))
add("adopt_agent_browser_env", ADOPT({"browser": {}}, {
    "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_HEADED": "true"}},
    "hermes_home_key": {"raise": KI}}))

# ── migrate_to_52 ───────────────────────────────────────────────────────────
M52_BASE = {"get_hermes_home": ["/h", "/h"],
            "hermes_home_key": ["k", "k"],
            "get_routing_process_hermes_home": {"ret": "/h"},
            "os.environ.get": {"ret": None},
            "record_core_use": {"ret": False}}
add("migrate_to_52", {**R({"browser": {"backend": "off", "extension_control": True}}, quiet=False),
                      "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_HEADED": "true"}}}})
add("migrate_to_52", {**R({"browser": {"backend": "browser-use"}}, quiet=True),
                      "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}}}})
add("migrate_to_52", {**R({"browser": {}}), "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}}}})   # early return
add("migrate_to_52", {**R({"browser": {}}), "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}},
                                                   "record_core_use": {"ret": True}}})   # used True
add("migrate_to_52", {**R({"browser": {"backend": False}}), "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}}}})
add("migrate_to_52", {**R({"browser": {"backend": "off"}}),
                      "io": {k: v for k, v in {**M52_BASE, "resolve_home_dotenv": {"ret": {}}}.items()
                             if k != "record_core_use"}})   # fail closed record_core_use
add("migrate_to_52", {**R({"browser": {"backend": "off"}}),
                      "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}}, "persist_migration": {"raise": SE}}})
add("migrate_to_52", {**R({"browser": {}}), "io": {"persist_migration": {"import_raise": imp_fail("hermes_cli.config_migrations")}}})
add("migrate_to_52", {**R({"browser": {"backend": "off"}}, quiet=True, results={}),
                      "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}}}})   # config_added KeyError
add("migrate_to_52", {**R({"browser": {"backend": "off"}}, quiet=False),
                      "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}}, "print": {"raise": GE}}})
add("migrate_to_52", {**R({"browser": {"backend": 0}}), "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {}}}})
add("migrate_to_52", {**R({"browser": {}}, quiet=False),
                      "io": {**M52_BASE, "resolve_home_dotenv": {"ret": {"AGENT_BROWSER_PROFILE": "rel"}},
                             "os.path.expanduser": ["rel", "rel"],
                             "os.path.abspath": {"ret": "/cwd/rel"}}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {len(CASES)} cases to {OUT}")
    for k in sorted(counts):
        flag = "" if counts[k] >= 5 else "  (<5!)"
        print(f"  {k}: {counts[k]}{flag}")
    assert all(v >= 5 for v in counts.values()), "a function has <5 cases"
    assert len(counts) == 12, f"expected 12 funcs, got {len(counts)}"


if __name__ == "__main__":
    main()