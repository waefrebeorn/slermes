#!/usr/bin/env python3
"""Generate tests/fixtures/config_migration_flow.jsonl for the
config_migration_flow oracle (t_port_config_migration_flow.c).

Each line: {"case": n, "func": <name>, "in": {...}}. Scripted I/O lives in
in.io: seam-name -> {"ret":...} / {"raise":{type,message,...}} /
{"import_raise":{...}} / {"state":{...}} or a queue LIST consumed per call.
A single (non-list) entry is reused on every call.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "config_migration_flow.jsonl"

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def ret(v):
    return {"ret": v}


def raise_(t, msg, **extra):
    return {"raise": dict({"type": t, "message": msg}, **extra)}


# ── _marked ────────────────────────────────────────────────────────────────
add("marked", config={}, key="_left_core_installed", feature="spotify")
add("marked", config={"_left_core_installed": ["spotify"]}, key="_left_core_installed", feature="spotify")
add("marked", config={"_left_core_installed": ["langfuse"]}, key="_left_core_installed", feature="spotify")
add("marked", config={"_left_core_installed": "spotify"}, key="_left_core_installed", feature="spotify")
add("marked", config={"_left_core_installed": [1, True, None]}, key="_left_core_installed", feature="spotify")
add("marked", config={"_left_core_installed": ["camofox", "spotify"]}, key="_left_core_installed", feature="camofox")
add("marked", config={"_left_core_scoped": ["yuanbao"]}, key="_left_core_scoped", feature="yuanbao")

# ── _mark (untagged helper, exercised directly) ───────────────────────────
add("mark", config={}, key="_left_core_used", feature="spotify")
add("mark", config={"_left_core_used": ["yuanbao", "spotify", "spotify"]}, key="_left_core_used", feature="spotify")
add("mark", config={"_left_core_used": "x"}, key="_left_core_used", feature="camofox")
add("mark", config={"_left_core_used": [1, "camofox"]}, key="_left_core_used", feature="camofox")
add("mark", config={"_left_core_used": ["spotify"]}, key="_left_core_used", feature="spotify")
add("mark", config={}, key="_left_core_scoped", feature="yuanbao")

# ── _core_carried ──────────────────────────────────────────────────────────
add("core_carried", feature="camofox", selection=["camofox"])
add("core_carried", feature="camofox", selection=["browser"])
add("core_carried", feature="camofox", selection=["hermes"])
add("core_carried", feature="camofox", selection=["hermes-webhook"])
add("core_carried", feature="camofox", selection=["hermes-cli"])
add("core_carried", feature="camofox", selection=["other"])
add("core_carried", feature="camofox", selection=[])
add("core_carried", feature="spotify", selection=["hermes-cli"])
add("core_carried", feature="yuanbao", selection=["hermes-homeassistant"])
add("core_carried", feature="yuanbao", selection=["hermes-yuanbao"])
add("core_carried", feature="camofox", selection=[1, None])

# ── _predates_split ────────────────────────────────────────────────────────
add("predates_split", home="h", feature="spotify")
add("predates_split", home="h", feature="camofox",
    io={"read_config_doc": raise_("OSError", "unreadable")})
add("predates_split", home="h", feature="camofox",
    io={"read_config_doc": ret({"_left_core_used": ["camofox"]})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": 51})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": 52})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": 53})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": "51"})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": "abc"})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": None})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": True})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret({"_config_version": " 4_2 "})})
add("predates_split", home="h", feature="camofox", io={"read_config_doc": ret([1, 2])})
add("predates_split", home="h", feature="camofox",
    io={"read_config_doc": raise_("SystemExit", "boom", code="boom")})

# ── record_core_use ────────────────────────────────────────────────────────
add("record_core_use", home="h", config={}, version=52, io={"feature_in_use": ret(True)})
add("record_core_use", home="h", config={}, version=52, io={"feature_in_use": ret(False)})
add("record_core_use", home="h", config={"_left_core_used": ["camofox"]}, version=52)
add("record_core_use", home="h", config={}, version=0,
    io={"feature_in_use": [ret(True), ret(False), ret(True), ret(False),
                           ret(False), ret(False), ret(False), ret(False)]})
add("record_core_use", home="h", config={}, version=99)
add("record_core_use", home="h", config={"_left_core_used": ["spotify"]}, version=52,
    io={"feature_in_use": ret(True)})
add("record_core_use", home="h", config={"_left_core_used": ["homeassistant"]}, version=0,
    io={"feature_in_use": [ret(False), ret(True), ret(False), ret(False),
                           ret(False), ret(False), ret(False)]})

# ── _turn_off_replaced ─────────────────────────────────────────────────────
add("turn_off_replaced", feature="camofox", config={"platform_toolsets": {"gui": ["browser", "ha"]}},
    io={"parse_platform_toolsets_value": ret(["browser", "ha"])})
add("turn_off_replaced", feature="camofox", config={"platform_toolsets": {"gui": ["ha"]}},
    io={"parse_platform_toolsets_value": ret(["ha"])})
add("turn_off_replaced", feature="camofox", config={"platform_toolsets": {"gui": None}},
    io={"parse_platform_toolsets_value": ret(None)})
add("turn_off_replaced", feature="camofox", config={"agent": {"disabled_toolsets": "browser,other"}},
    io={"parse_config_string_list": ret(["browser", "other"])})
add("turn_off_replaced", feature="camofox", config={"agent": {"disabled_toolsets": ["other"]}},
    io={"parse_config_string_list": ret(["other"])})
add("turn_off_replaced", feature="camofox", config={"agent": "x"})
add("turn_off_replaced", feature="spotify", config={"platform_toolsets": {"gui": ["browser"]}},
    io={"parse_platform_toolsets_value": ret(["browser"])})
add("turn_off_replaced", feature="camofox",
    config={"platform_toolsets": {"gui": ["browser"], "cli": ["browser"]}},
    io={"parse_platform_toolsets_value": [ret(["browser"]), ret(["browser"])]})
add("turn_off_replaced", feature="camofox", config={})
add("turn_off_replaced", feature="camofox", config={"platform_toolsets": {"gui": ["browser"]}},
    io={"parse_platform_toolsets_value": raise_("ValueError", "bad list")})

# ── _keep_legacy_disable ───────────────────────────────────────────────────
add("keep_legacy_disable", feature="yuanbao", config={"agent": {"disabled_toolsets": ["hermes-yuanbao"]}},
    io={"parse_config_string_list": ret(["hermes-yuanbao"])})
add("keep_legacy_disable", feature="yuanbao", config={"agent": {"disabled_toolsets": ["hermes-gateway"]}},
    io={"parse_config_string_list": ret(["hermes-gateway"])})
add("keep_legacy_disable", feature="yuanbao", config={"agent": {"disabled_toolsets": ["other"]}},
    io={"parse_config_string_list": ret(["other"])})
add("keep_legacy_disable", feature="yuanbao", config={})
add("keep_legacy_disable", feature="yuanbao",
    config={"agent": {"disabled_toolsets": ["hermes-yuanbao", "yuanbao"]}},
    io={"parse_config_string_list": ret(["hermes-yuanbao", "yuanbao"])})
add("keep_legacy_disable", feature="camofox", config={"agent": {"disabled_toolsets": ["hermes-yuanbao"]}},
    io={"parse_config_string_list": ret(["hermes-yuanbao"])})
add("keep_legacy_disable", feature="yuanbao", config={"agent": {"disabled_toolsets": ""}})
add("keep_legacy_disable", feature="yuanbao",
    config={"agent": {"disabled_toolsets": ["hermes-gateway", "hermes-yuanbao"]}},
    io={"parse_config_string_list": ret(["hermes-gateway", "hermes-yuanbao"])})

# ── _record_migration ──────────────────────────────────────────────────────
add("record_migration", home="h", feature="spotify",
    io={"config_exists": ret(True), "read_user_config_raw": ret({}), "plugin_present": ret(False)})
add("record_migration", home="h", feature="spotify",
    io={"config_exists": ret(True), "read_user_config_raw": ret({}), "plugin_present": ret(True),
        "atomic_config_write": ret(None)})
add("record_migration", home="h", feature="camofox",
    io={"config_exists": ret(True),
        "read_user_config_raw": ret({"platform_toolsets": {"gui": ["ha"]}}),
        "plugin_present": ret(False),
        "parse_platform_toolsets_value": [ret(["ha"]), ret(["ha"])],
        "parse_config_string_list": ret(["browser"]),
        "atomic_config_write": ret(None)})
add("record_migration", home="h", feature="camofox",
    io={"config_exists": ret(False), "read_user_config_raw": ret({}), "plugin_present": ret(False),
        "parse_config_string_list": ret(["browser"]),
        "default_config_version": ret(54),
        "note_first_stamp": ret({"_left_core_used": ["camofox"]}),
        "atomic_config_write": ret(None)})
add("record_migration", home="h", feature="camofox",
    io={"config_exists": ret(True), "read_user_config_raw": raise_("OSError", "locked")})
add("record_migration", home="h", feature="camofox",
    io={"config_exists": ret(True), "read_user_config_raw": ret({"_left_core_scoped": ["camofox"]}),
        "plugin_present": ret(False)})
add("record_migration", home="h", feature="camofox",
    io={"config_exists": ret(True),
        "read_user_config_raw": ret({"_left_core_installed": ["camofox"], "_left_core_scoped": ["camofox"]}),
        "plugin_present": ret(True)})
add("record_migration", home="h", feature="camofox",
    io={"config_exists": ret(True),
        "read_user_config_raw": ret({"known_plugin_toolsets": {"gui": ["x"], "webhook": ["camofox"]},
                                     "platform_toolsets": {"gui": ["ha"]}}),
        "plugin_present": ret(False),
        "parse_platform_toolsets_value": [ret(["ha"]), ret(["ha"])],
        "parse_config_string_list": ret(["browser"]),
        "atomic_config_write": ret(None)})
add("record_migration", home="h", feature="camofox",
    io={"config_exists": ret(True),
        "read_user_config_raw": ret({"platform_toolsets": {"gui": ["hermes-cli"]}}),
        "plugin_present": ret(False),
        "parse_platform_toolsets_value": [ret(["hermes-cli"]), ret(["hermes-cli"])],
        "parse_config_string_list": ret(["browser"]),
        "atomic_config_write": ret(None)})
add("record_migration", home="h", feature="yuanbao",
    io={"config_exists": ret(True),
        "read_user_config_raw": ret({"agent": {"disabled_toolsets": ["hermes-yuanbao"]},
                                     "platform_toolsets": {"gui": ["other"]}}),
        "plugin_present": ret(False),
        "parse_platform_toolsets_value": ret(["other"]),
        "parse_config_string_list": ret(["hermes-yuanbao"]),
        "atomic_config_write": ret(None)})
add("record_migration", home="h", feature="spotify",
    io={"config_exists": ret(True), "read_user_config_raw": ret({}),
        "plugin_present": raise_("OSError", "stat failed")})
add("record_migration", home="h", feature="spotify",
    io={"config_exists": ret(True), "read_user_config_raw": ret({}), "plugin_present": ret(True),
        "atomic_config_write": raise_("OSError", "disk full")})

# ── _first_cause ───────────────────────────────────────────────────────────
add("first_cause", error="fatal: boom")
add("first_cause", error="warning: x\nerror: bad thing\nfatal: y")
add("first_cause", error="")
add("first_cause", error="  \n\t\n")
add("first_cause", error="fatal:")
add("first_cause", error="error: x.y.")
add("first_cause", error="plain failure.")
add("first_cause", error="a\r\nb\r\nc")
add("first_cause", error="  fatal:   spaced cause   ")
add("first_cause", error="error: trailing dots...   ")
add("first_cause", error="\v\f\x1c")
add("first_cause", error="fatal: a\nerror: b")

# ── _install_one ───────────────────────────────────────────────────────────
add("install_one", home="h", feature="spotify",
    io={"install": ret({"ok": True}), "refresh_bound_home": ret(None),
        "config_exists": ret(True), "read_user_config_raw": ret({}), "plugin_present": ret(True),
        "atomic_config_write": ret(None), "feature_notice": ret(""), "say": ret(None)})
add("install_one", home="h", feature="spotify",
    io={"install": ret({"ok": False, "error": "fatal: clone failed\nmore"}),
        "note_failure": ret(None), "install_command": ret("hermes plugins install spotify"),
        "feature_notice": ret(""), "say": ret(None)})
add("install_one", home="h", feature="spotify",
    io={"install": raise_("RuntimeError", "net down"), "note_failure": ret(None),
        "install_command": ret("hermes plugins install spotify"),
        "feature_notice": ret(""), "say": ret(None)})
add("install_one", home="h", feature="spotify",
    io={"install": raise_("SystemExit", "1", code=1)})
add("install_one", home="h", feature="spotify",
    io={"install": ret({"ok": True}), "refresh_bound_home": ret(None),
        "config_exists": ret(True), "read_user_config_raw": raise_("OSError", "locked"),
        "logger_debug": ret(None), "feature_notice": ret(""), "say": ret(None)})
add("install_one", home="h", feature="spotify",
    io={"install": ret({"ok": False, "error": "boom"}), "note_failure": ret(None),
        "install_command": ret("hermes plugins install spotify"),
        "feature_notice": ret("Extra sentence."), "say": ret(None)})
add("install_one", home="h", feature="spotify", io={"install": ret(None)})
add("install_one", home="h", feature="spotify",
    io={"install": ret({"ok": "yes"}), "refresh_bound_home": ret(None),
        "config_exists": ret(True), "read_user_config_raw": ret({}), "plugin_present": ret(False),
        "feature_notice": ret(""), "say": ret(None)})
add("install_one", home="h", feature="camofox",
    io={"install": ret({"ok": True}), "refresh_bound_home": ret(None),
        "config_exists": ret(True),
        "read_user_config_raw": ret({"platform_toolsets": {"gui": ["ha"]}}),
        "plugin_present": ret(True),
        "parse_platform_toolsets_value": [ret(["ha"]), ret(["ha"])],
        "parse_config_string_list": ret(["browser"]),
        "atomic_config_write": ret(None), "feature_notice": ret(""), "say": ret(None)})
add("install_one", home="h", feature="spotify",
    io={"install": ret({"ok": False, "error": "boom"}), "note_failure": ret(None),
        "install_command": raise_("RuntimeError", "no home")})

# ── migrate_home ───────────────────────────────────────────────────────────
add("migrate_home", home="h", io={"pending": ret([])})
add("migrate_home", home="h",
    io={"pending": ret(["spotify", "langfuse"]),
        "install": [ret({"ok": True}), ret({"ok": True})],
        "refresh_bound_home": ret(None), "config_exists": ret(True),
        "read_user_config_raw": ret({}), "plugin_present": ret(True),
        "atomic_config_write": ret(None), "feature_notice": ret(""), "say": ret(None)})
add("migrate_home", home="h",
    io={"pending": ret(["spotify", "langfuse"]),
        "install": [ret({"ok": False, "error": "boom"}), ret({"ok": True})],
        "note_failure": ret(None), "install_command": ret("hermes plugins install spotify"),
        "refresh_bound_home": ret(None), "config_exists": ret(True),
        "read_user_config_raw": ret({}), "plugin_present": ret(True),
        "atomic_config_write": ret(None), "feature_notice": ret(""), "say": ret(None)})
add("migrate_home", home="h",
    io={"pending": ret(["spotify", "spotify"]),
        "install": ret({"ok": True}), "refresh_bound_home": ret(None),
        "config_exists": ret(True), "read_user_config_raw": ret({}), "plugin_present": ret(True),
        "atomic_config_write": ret(None), "feature_notice": ret(""), "say": ret(None)})
add("migrate_home", home="h", io={"pending": raise_("RuntimeError", "catalog down")})
add("migrate_home", home="h",
    io={"pending": ret(["spotify"]), "install": raise_("SystemExit", "1", code=1)})

# ── version_token ──────────────────────────────────────────────────────────
add("version_token", version=None)
add("version_token", version=[])
add("version_token", version=[{"int": 1700000000000000000}])
add("version_token", version=[{"int": 0}])
add("version_token", version=[{"bool": True}])
add("version_token", version=[{"str": "abc"}])
add("version_token", version=[{"float": 1.5}, {"str": "x"}])
add("version_token", version=[{"tuple": [{"int": 1}, {"str": "a"}]}])
add("version_token", version=[{"int": 5}, {"int": 6}])
add("version_token", version=[{"null": True}])
add("version_token", version=[{"str": "a'b"}, {"str": 'c"d'}])
add("version_token", version=[{"float": 0.1}])
add("version_token", version=[{"str": "é"}, {"str": "\u2028"}])
add("version_token", version=[{"float": 1e16}, {"float": 0.0001}])
add("version_token", version=[{"bool": False}])

# ── _backend_for ───────────────────────────────────────────────────────────
add("backend_for", kind="file")
add("backend_for", kind="")
add("backend_for", kind="Remote")
add("backend_for", kind="a.b")
add("backend_for", kind="remote",
    io={"sys_modules_get": ret({"get_backend": "fn"}), "get_backend_call": ret({"name": "remote"})})
add("backend_for", kind="remote", io={"sys_modules_get": ret({"get_backend": 42})})
add("backend_for", kind="remote",
    io={"sys_modules_get": ret(None), "import_module": ret({"get_backend": "fn"}),
        "get_backend_call": ret({"name": "remote"})})
add("backend_for", kind="remote",
    io={"sys_modules_get": ret(None),
        "import_module": raise_("ImportError", "No module named 'plugins.config_backends.remote'",
                                name="plugins.config_backends.remote")})
add("backend_for", kind="remote",
    io={"sys_modules_get": ret(None),
        "import_module": raise_("ImportError", "No module named 'plugins'", name="plugins")})
add("backend_for", kind="remote",
    io={"sys_modules_get": ret(None),
        "import_module": raise_("ImportError", "broken dep", name="other.module")})
add("backend_for", kind="remote", io={"sys_modules_get": ret({})})
add("backend_for", kind="remote",
    io={"sys_modules_get": ret({"get_backend": "fn"}),
        "get_backend_call": raise_("RuntimeError", "broken backend")})
add("backend_for", kind="a" * 64,
    io={"sys_modules_get": ret({"get_backend": "fn"}), "get_backend_call": ret({"name": "x"})})
add("backend_for", kind="a" * 65)
add("backend_for", kind="remote",
    io={"sys_modules_get": ret(None), "import_module": raise_("ValueError", "not an import error")})
add("backend_for", kind="remote",
    io={"sys_modules_get": ret({"get_backend": "fn"}), "get_backend_call": ret(None)})

# ── get_config_backend ─────────────────────────────────────────────────────
add("get_config_backend", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})
add("get_config_backend", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("")})
add("get_config_backend", state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("  REMOTE  "), "sys_modules_get": ret({"get_backend": "fn"}),
        "get_backend_call": ret({"name": "remote"})})
add("get_config_backend", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret(None)})
add("get_config_backend", state={"_BOOTSTRAPPED": False},
    io={"threading_get_ident": [ret(11), ret(11)], "apply_config_bootstrap_env": ret(None),
        "os_environ_get": ret("file")})
add("get_config_backend", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("bad.kind")})

# ── _bootstrap ─────────────────────────────────────────────────────────────
add("bootstrap", state={"_BOOTSTRAPPED": True}, once=True)
add("bootstrap", state={"_BOOTSTRAPPED": True, "_BOOTSTRAP_OWNER": None}, once=False,
    io={"threading_get_ident": [ret(7), ret(7)], "apply_config_bootstrap_env": ret(None)})
add("bootstrap", state={"_BOOTSTRAP_OWNER": 9}, once=False, io={"threading_get_ident": ret(9)})
add("bootstrap", home="h/p", project_env="proj/.env", once=True,
    io={"threading_get_ident": [ret(1), ret(2)], "apply_config_bootstrap_env": ret(None)})
add("bootstrap", once=True,
    io={"threading_get_ident": [ret(1), ret(1)],
        "apply_config_bootstrap_env": raise_("ImportError", "no dotenv", name="hermes_cli.env_loader")})
add("bootstrap", once=True,
    io={"threading_get_ident": [ret(1), ret(1)],
        "apply_config_bootstrap_env": raise_("RuntimeError", "boom")})
add("bootstrap", once=True,
    io={"threading_get_ident": [ret(1), ret(1)],
        "apply_config_bootstrap_env": {"import_raise": {"type": "ImportError", "message": "no module",
                                                        "name": "hermes_cli.env_loader"}}})
add("bootstrap", state={"_BOOTSTRAPPED": False, "_BOOTSTRAP_OWNER": None}, once=True,
    io={"threading_get_ident": [ret(5), raise_("RuntimeError", "ident fail")]})
add("bootstrap", state={"_BOOTSTRAPPED": False, "_BOOTSTRAP_OWNER": None}, once=True,
    io={"threading_get_ident": [ret(3), ret(3)], "apply_config_bootstrap_env": ret(None)})

# ── _ensure_bootstrap_env ──────────────────────────────────────────────────
add("ensure_bootstrap_env", state={"_BOOTSTRAPPED": True})
add("ensure_bootstrap_env", state={"_BOOTSTRAPPED": False},
    io={"threading_get_ident": [ret(1), ret(1)], "apply_config_bootstrap_env": ret(None)})
add("ensure_bootstrap_env",
    io={"threading_get_ident": [ret(2), ret(2)], "apply_config_bootstrap_env": ret(None)})
add("ensure_bootstrap_env", state={"_BOOTSTRAP_OWNER": 4}, io={"threading_get_ident": ret(4)})
add("ensure_bootstrap_env",
    io={"threading_get_ident": [ret(1), ret(1)],
        "apply_config_bootstrap_env": raise_("RuntimeError", "publish failed")})

# ── _route ─────────────────────────────────────────────────────────────────
add("route", config_path="h/config.yaml", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})
add("route", config_path="config.yaml", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})
add("route", config_path="h/other.yaml")
add("route", config_path="./config.yaml", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})
add("route", config_path="C:/h/config.yaml", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})
add("route", config_path="/config.yaml", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})
add("route", config_path="h/config.yaml/", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})
add("route", config_path="")
add("route", config_path="h/x/other.yaml")
add("route", config_path="h/config.yaml", state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("remote"), "sys_modules_get": ret({"get_backend": "fn"}),
        "get_backend_call": ret({"name": "remote"})})
add("route", config_path="a/./b/config.yaml", state={"_BOOTSTRAPPED": True}, io={"os_environ_get": ret("file")})

# ── read_config_doc ────────────────────────────────────────────────────────
add("read_config_doc", config_path="h/config.yaml", state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "backend_read_user_doc": ret({"model": "x"})})
add("read_config_doc", config_path="h/other.yaml", io={"backend_read_user_doc": ret(None)})
add("read_config_doc", config_path="h/config.yaml", state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"),
        "backend_read_user_doc": raise_("FileNotFoundError", "no file")})
add("read_config_doc", config_path="config.yaml", state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "backend_read_user_doc": ret([1, 2])})
add("read_config_doc", config_path="h/config.yaml", state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("remote"), "sys_modules_get": ret({"get_backend": "fn"}),
        "get_backend_call": ret({"name": "remote"}), "backend_read_user_doc": ret({"a": 1})})
add("read_config_doc", config_path="C:/h/config.yaml", state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "backend_read_user_doc": ret({"a": 1})})

# ── FileBackend.write_path ─────────────────────────────────────────────────
add("write_path", path="h/config.yaml",
    changes={"document": {"a": 1}, "set": {"k": "v"}, "unset": ["z"], "extra_content_on_create": "# c"},
    io={"atomic_roundtrip_yaml_save": ret(None), "atomic_roundtrip_yaml_update": ret(None),
        "drop_other_access": ret(None)})
add("write_path", path="h/config.yaml", changes={"document": None, "set": {}, "unset": []},
    io={"drop_other_access": ret(None)})
add("write_path", path="h/config.yaml", changes={"set": {"b": 2, "a": 1}},
    io={"atomic_roundtrip_yaml_update": ret(None), "drop_other_access": ret(None)})
add("write_path", path="h/config.yaml", changes={"document": {"a": 1}},
    io={"atomic_roundtrip_yaml_save": raise_("OSError", "read-only")})
add("write_path", path="h/config.yaml", changes={"set": {"a": 1, "b": 2}},
    io={"atomic_roundtrip_yaml_update": [ret(None), raise_("ValueError", "bad value")]})
add("write_path", path="h/config.yaml", changes={"unset": ["x", "y"]},
    io={"atomic_roundtrip_yaml_update": ret(None), "drop_other_access": ret(None)})
add("write_path", path="h/config.yaml", changes={"document": {}, "set": {}},
    io={"atomic_roundtrip_yaml_save": ret(None), "drop_other_access": ret(None)})
add("write_path", path="h/config.yaml", changes={"document": {"a": 1}, "extra_content_on_create": None},
    io={"atomic_roundtrip_yaml_save": ret(None), "drop_other_access": ret(None)})

# ── config_cache_version_or_none ───────────────────────────────────────────
add("config_cache_version_or_none", config_path="h/config.yaml",
    io={"config_cache_version": ret([1, 2, 3])})
add("config_cache_version_or_none", config_path="h/config.yaml",
    io={"config_cache_version": raise_("FileNotFoundError", "absent")})
add("config_cache_version_or_none", config_path="h/config.yaml",
    io={"config_cache_version": raise_("PermissionError", "denied")})
add("config_cache_version_or_none", config_path="h/config.yaml",
    io={"config_cache_version": raise_("OSError", "io")})
add("config_cache_version_or_none", config_path="h/config.yaml",
    io={"config_cache_version": raise_("ValueError", "not oserror")})
add("config_cache_version_or_none", config_path="h/config.yaml",
    io={"config_cache_version": ret(None)})

# ── write_config_keys ──────────────────────────────────────────────────────
add("write_config_keys", config_path="h/config.yaml", updates={"a": 1, "b": None, "c": "x"},
    state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "refuse_redacted_sets": ret(None),
        "backend_write_changes": ret(None)})
add("write_config_keys", config_path="h/config.yaml", updates={"a": None, "b": None},
    state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "refuse_redacted_sets": ret(None),
        "backend_write_changes": ret(None)})
add("write_config_keys", config_path="h/config.yaml", updates={"a": 1},
    io={"refuse_redacted_sets": raise_("ValueError", "redacted mask")})
add("write_config_keys", config_path="h/other.yaml", updates={"a": 1},
    io={"refuse_redacted_sets": ret(None), "backend_write_changes": ret(None)})
add("write_config_keys", config_path="h/config.yaml", updates={},
    state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "refuse_redacted_sets": ret(None),
        "backend_write_changes": ret(None)})
add("write_config_keys", config_path="h/config.yaml", updates={"k": {"nested": True}},
    state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "refuse_redacted_sets": ret(None),
        "backend_write_changes": ret(None)})
add("write_config_keys", config_path="h/config.yaml", updates={"a": 1},
    state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("remote"), "sys_modules_get": ret({"get_backend": "fn"}),
        "get_backend_call": ret({"name": "remote"}),
        "refuse_redacted_sets": ret(None), "backend_write_changes": ret(None)})
add("write_config_keys", config_path="h/config.yaml", updates={"z": 0, "y": False, "x": ""},
    state={"_BOOTSTRAPPED": True},
    io={"os_environ_get": ret("file"), "refuse_redacted_sets": ret(None),
        "backend_write_changes": ret(None)})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for i, c in enumerate(CASES, 1):
            c = dict(c, case=i)
            fh.write(json.dumps(c, ensure_ascii=True, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for f, n in sorted(counts.items()):
        print(f"  {f}: {n}")
    assert all(n >= 5 for n in counts.values()), "every port needs >=5 cases"


if __name__ == "__main__":
    main()