#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_migrate_flow2.jsonl — >=5 cases per ported
func for the gateway_migrate_flow2 differential oracle.

Seam scripting convention (matches the harness script_call / oracle seam):
- a Python LIST script is a QUEUE: one result per call (last repeats);
- a LIST-returning seam therefore wraps each list result one level deeper:
  L([a, b]) == [[a, b]] (a queue whose first result is the list [a, b]);
- a scalar / dict / None script repeats verbatim every call;
- {"__raise__": cls, "msg": ...} scripts an exception.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_migrate_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def L(x):
    """Wrap a list result as a single-element queue (repeats every call)."""
    return [x]


def raise_spec(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def pg(name, home, pid=None, services=None, run_as_user=None, uid=None, runtime_home=None):
    return {"name": name, "home": home, "pid": pid, "services": services or [],
            "run_as_user": run_as_user, "uid": uid, "runtime_home": runtime_home}


def plan(profiles, default_home="/h", multiplex_flag_on=False, live_served=None,
         manifest=None, interrupted=False, blockers=None, notices=None, sbc=None):
    return {"default_home": default_home, "profiles": profiles,
            "multiplex_flag_on": multiplex_flag_on, "live_served": live_served,
            "manifest": manifest, "interrupted": interrupted,
            "blockers": blockers or [], "notices": notices or [],
            "standalone_by_config": sbc or []}


def pc(enabled=True, token=None, api_key=None, extra=None):
    d = {"enabled": enabled}
    if token is not None:
        d["token"] = token
    if api_key is not None:
        d["api_key"] = api_key
    d["extra"] = extra
    return d


def cfg(platforms):
    return {"platforms": platforms}


# ── 1. profile_homes ──
case("profile_homes", seams={"profiles_to_serve": L([["default", "/h"], ["p1", "/h/p1"]])})
case("profile_homes", seams={"profiles_to_serve": L([])})
case("profile_homes", seams={"profiles_to_serve": L([["default", "/h"]])})
case("profile_homes", seams={"profiles_to_serve": None})  # list(None) -> TypeError
case("profile_homes", seams={"profiles_to_serve": raise_spec("RuntimeError", "boom")})
case("profile_homes", seams={"profiles_to_serve": L([["default", "/h//x/"], ["p2", "/h/p2"]])})

# ── 2. gateway_identity ──
case("gateway_identity", home="/h", pid=None, services=[], seams={"gateway_identity": L([None, None])})
case("gateway_identity", home="/h", pid=5, services=[["systemd", True]], seams={"gateway_identity": L([7, "/other"])})
case("gateway_identity", home="/h", pid=5, services=[], seams={"gateway_identity": L([7, None])})
case("gateway_identity", home="/h", pid=None, services=[], seams={"gateway_identity": L([None, "/h"])})
case("gateway_identity", home="/h", pid=None, services=[], seams={"gateway_identity": raise_spec("ValueError", "x")})

# ── 3. windows_task_installed ──
case("windows_task_installed", seams={"is_task_registered": True})
case("windows_task_installed", seams={"is_task_registered": False, "is_startup_entry_installed": True})
case("windows_task_installed", seams={"is_task_registered": False, "is_startup_entry_installed": False})
case("windows_task_installed", seams={"is_task_registered": raise_spec("OSError", "x")})
case("windows_task_installed", seams={"is_task_registered": raise_spec("SystemExit", "1")})
case("windows_task_installed", seams={"is_task_registered": False, "is_startup_entry_installed": raise_spec("RuntimeError", "x")})
case("windows_task_installed", seams={"is_task_registered": "yes"})  # truthy non-bool -> True

# ── 4. systemd_service_user ──
case("systemd_service_user", home="/h", services=[["systemd", False]], seams={})
case("systemd_service_user", home="/h", services=[["systemd", True]],
     seams={"home_env_enter": None, "home_env_exit": None, "get_systemd_unit_path": "/u", "read_systemd_user_from_unit": "alice"})
case("systemd_service_user", home="/h", services=[["systemd", True]],
     seams={"home_env_enter": None, "home_env_exit": None, "get_systemd_unit_path": "/u", "read_systemd_user_from_unit": None})
case("systemd_service_user", home="/h", services=[["systemd", True]], seams={"home_env_enter": raise_spec("RuntimeError", "x")})
case("systemd_service_user", home="/h", services=[["systemd", True]],
     seams={"home_env_enter": None, "home_env_exit": None, "get_systemd_unit_path": "/u", "read_systemd_user_from_unit": raise_spec("OSError", "x")})
case("systemd_service_user", home="/h", services=[["systemd", 1]],  # True == 1
     seams={"home_env_enter": None, "home_env_exit": None, "get_systemd_unit_path": "/u", "read_systemd_user_from_unit": "bob"})
case("systemd_service_user", home="/h", services=[], seams={})

# ── 5. s6_slot_op ──
case("s6_slot_op", verb="start", home="/h", seams={"profile_name_for_home": None, "bring_root_slot_up": None})
case("s6_slot_op", verb="restart", home="/h", seams={"profile_name_for_home": "default", "bring_root_slot_up": None})
case("s6_slot_op", verb="stop", home="/h", seams={"profile_name_for_home": "default"})
case("s6_slot_op", verb="stop", home="/h/p1", seams={"profile_name_for_home": "p1", "park_named_slot": None})
case("s6_slot_op", verb="start", home="/h/p1", seams={"profile_name_for_home": "p1"})
case("s6_slot_op", verb="stop", home="/h/p1", seams={"profile_name_for_home": raise_spec("RuntimeError", "x")})
case("s6_slot_op", verb="stop", home="/h/p1", seams={"profile_name_for_home": "", "park_named_slot": None})  # falsy -> default

# ── 6. service_op ──
case("service_op", kind="s6", system=False, verb="stop", home="/h/p1",
     seams={"profile_name_for_home": "p1", "park_named_slot": None})
case("service_op", kind="launchd", system=False, verb="install", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None, "launchd_install": None})
case("service_op", kind="windows", system=False, verb="install", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None, "gww_install": None})
case("service_op", kind="systemd", system=True, verb="install", home="/h", run_as_user="alice",
     seams={"home_env_enter": None, "home_env_exit": None, "systemd_install": None})
case("service_op", kind="systemd", system=False, verb="enable", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None, "get_service_name": "hermes-gateway", "run_systemctl": {"returncode": 0}})
case("service_op", kind="systemd", system=True, verb="enable", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None, "get_service_name": "hermes-gateway", "run_systemctl": {"returncode": 1}})
case("service_op", kind="launchd", system=False, verb="enable", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None})
case("service_op", kind="systemd", system=False, verb="stop", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None, "service_call": None})
case("service_op", kind="systemd", system=False, verb="restart", home="/h",
     seams={"home_env_enter": raise_spec("RuntimeError", "x")})
case("service_op", kind="systemd", system=False, verb="enable", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None, "get_service_name": "hermes-gateway", "run_systemctl": raise_spec("OSError", "x")})
case("service_op", kind="systemd", system=False, verb="enable", home="/h",
     seams={"home_env_enter": None, "home_env_exit": None, "get_service_name": "hermes-gateway", "run_systemctl": {}})  # absent returncode -> 0

# ── 7. stop_gateway_process ──
case("stop_gateway_process", home="/h", seams={"profiles_stop_gateway_process": None})
case("stop_gateway_process", home="/h", seams={"profiles_stop_gateway_process": "ignored"})  # discarded -> None
case("stop_gateway_process", home="/h", seams={"profiles_stop_gateway_process": raise_spec("RuntimeError", "x")})
case("stop_gateway_process", home="/h", seams={"profiles_stop_gateway_process": raise_spec("SystemExit", "1")})
case("stop_gateway_process", home="/h/p1", seams={"profiles_stop_gateway_process": None})

# ── 8. spawn_detached_gateway ──
case("spawn_detached_gateway", home="/h", seams={"home_env_enter": None, "home_env_exit": None, "gw_spawn_detached": True})
case("spawn_detached_gateway", home="/h", seams={"home_env_enter": None, "home_env_exit": None, "gw_spawn_detached": False})
case("spawn_detached_gateway", home="/h", seams={"home_env_enter": None, "home_env_exit": None, "gw_spawn_detached": None})
case("spawn_detached_gateway", home="/h", seams={"home_env_enter": None, "home_env_exit": None, "gw_spawn_detached": raise_spec("RuntimeError", "x")})
case("spawn_detached_gateway", home="/h", seams={"home_env_enter": raise_spec("RuntimeError", "x")})

# ── 9. read_multiplex_flag ──
case("read_multiplex_flag", default_home="/h", seams={"explicit_multiplex_flag": True})
case("read_multiplex_flag", default_home="/h", seams={"explicit_multiplex_flag": False})
case("read_multiplex_flag", default_home="/h", seams={"explicit_multiplex_flag": 1})  # is True -> False
case("read_multiplex_flag", default_home="/h", seams={"explicit_multiplex_flag": None})
case("read_multiplex_flag", default_home="/h", seams={"explicit_multiplex_flag": "yes"})
case("read_multiplex_flag", default_home="/h", seams={"explicit_multiplex_flag": raise_spec("OSError", "x")})

# ── 10. write_multiplex_flag ──
case("write_multiplex_flag", default_home="/h", value=True,
     seams={"require_readable_config": {}, "set_nested": None, "write_user_config": None})
case("write_multiplex_flag", default_home="/h", value=False,
     seams={"require_readable_config": {"gateway": {"other": 1}}, "set_nested": None, "write_user_config": None})
case("write_multiplex_flag", default_home="/h", value=True,
     seams={"require_readable_config": {"multiplex_profiles": "stale", "gateway": {}}, "set_nested": None, "write_user_config": None})
case("write_multiplex_flag", default_home="/h", value=True, seams={"require_readable_config": None})  # .pop AttributeError
case("write_multiplex_flag", default_home="/h", value=True, seams={"require_readable_config": raise_spec("ValueError", "x")})
case("write_multiplex_flag", default_home="/h", value=True,
     seams={"require_readable_config": {}, "set_nested": raise_spec("ValueError", "x")})
case("write_multiplex_flag", default_home="/h", value=True,
     seams={"require_readable_config": {}, "set_nested": None, "write_user_config": raise_spec("OSError", "x")})

# ── 11. credential_probe ──
case("credential_probe", platform_config=pc(token="tok"))
case("credential_probe", platform_config=pc(api_key="key"))
case("credential_probe", platform_config=pc(token="tok", api_key="key", extra={"app_id": "a", "client_id": "c", "bot_id": "b", "project_secret": "s"}))
case("credential_probe", platform_config=None)
case("credential_probe", platform_config=pc(token="", api_key="fallback"))  # falsy token -> api_key
case("credential_probe", platform_config={"enabled": True, "extra": [1, 2]})  # extra non-dict -> AttributeError

# ── 12. credential_claims ──
case("credential_claims", config=cfg({"telegram": pc(token="t")}), seams={"adapter_credential_fingerprint": "fp1"})
case("credential_claims", config=cfg({"telegram": pc(enabled=False, token="t")}), seams={"adapter_credential_fingerprint": "fp1"})
case("credential_claims", config=cfg({"telegram": pc(token="t")}), seams={"adapter_credential_fingerprint": None})
case("credential_claims", config=cfg({"telegram": pc(token="t"), "discord": pc(token="d")}), seams={"adapter_credential_fingerprint": ["fp1", "fp2"]})
case("credential_claims", config=None, seams={})  # AttributeError platforms
case("credential_claims", config=cfg({"telegram": pc(token="t")}), seams={"adapter_credential_fingerprint": raise_spec("RuntimeError", "x")})
case("credential_claims", config=cfg({}), seams={})

# ── 13. credential_key_names ──
case("credential_key_names", platform_value="telegram", seams={"credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram", "X": "discord"}})
case("credential_key_names", platform_value="telegram", seams={"credential_env_keys": {"Z_TOKEN": "telegram", "A_TOKEN": "telegram"}})
case("credential_key_names", platform_value="sms", seams={"credential_env_keys": {"A": "telegram"}})  # no match -> the sms token
case("credential_key_names", platform_value="telegram", seams={"credential_env_keys": {}})
case("credential_key_names", platform_value="telegram", seams={"credential_env_keys": raise_spec("RuntimeError", "x")})

# ── 14. duplicate_credential_lines ──
_TG = cfg({"telegram": pc(token="t")})
case("duplicate_credential_lines", configs=[["default", _TG], ["p1", _TG]],
     seams={"adapter_credential_fingerprint": "fp1", "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("duplicate_credential_lines", configs=[["default", _TG]],
     seams={"adapter_credential_fingerprint": "fp1", "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("duplicate_credential_lines", configs=[["default", cfg({"telegram": pc(token="a")})], ["p1", cfg({"telegram": pc(token="b")})]],
     seams={"adapter_credential_fingerprint": ["fa", "fb"], "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("duplicate_credential_lines", configs=[], seams={})
case("duplicate_credential_lines", configs=[["default", _TG], ["p1", _TG], ["p2", _TG]],
     seams={"adapter_credential_fingerprint": "fp1", "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("duplicate_credential_lines", configs=[["default", _TG]], seams={"adapter_credential_fingerprint": raise_spec("RuntimeError", "x")})

# ── 15. duplicate_credential_findings ──
case("duplicate_credential_findings", seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None,
    "profiles_to_serve": L([["default", "/h"], ["p1", "/h/p1"]]),
    "profile_gateway_config": _TG, "adapter_credential_fingerprint": "fp1",
    "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("duplicate_credential_findings", seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None,
    "profiles_to_serve": L([["default", "/h"], ["p1", "/h/p1"]]),
    "profile_gateway_config": [_TG, raise_spec("RuntimeError", "bad")],
    "adapter_credential_fingerprint": "fp1", "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("duplicate_credential_findings", seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None,
    "profiles_to_serve": L([["default", "/h"]]),
    "profile_gateway_config": raise_spec("SystemExit", "1")})
case("duplicate_credential_findings", seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None, "profiles_to_serve": L([])})
case("duplicate_credential_findings", seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None,
    "profiles_to_serve": raise_spec("RuntimeError", "x")})

# ── 16. check_duplicate_credentials ──
_P2 = plan([pg("default", "/h"), pg("p1", "/h/p1")])
case("check_duplicate_credentials", plan=_P2, configs={"default": _TG, "p1": _TG},
     seams={"adapter_credential_fingerprint": "fp1", "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("check_duplicate_credentials", plan=_P2, configs={"default": _TG, "p1": cfg({"telegram": pc(token="b")})},
     seams={"adapter_credential_fingerprint": ["fa", "fb"], "credential_env_keys": {"TELEGRAM_BOT_TOKEN": "telegram"}})
case("check_duplicate_credentials", plan=_P2, configs={"default": _TG},
     seams={"adapter_credential_fingerprint": "fp1", "credential_env_keys": {}})
case("check_duplicate_credentials", plan=plan([]), configs={}, seams={})
case("check_duplicate_credentials", plan=_P2, configs={"default": _TG, "p1": _TG},
     seams={"adapter_credential_fingerprint": raise_spec("RuntimeError", "x")})

# ── 17. listener_url ──
case("listener_url", default_cfg=None, platform_value="api_server", profile="p1", seams={"format_url_host": "127.0.0.1"})
case("listener_url", default_cfg=None, platform_value="webhook", profile="p1", seams={"format_url_host": "0.0.0.0"})
case("listener_url", default_cfg=None, platform_value="telegram", profile="p1", seams={"format_url_host": "<host>"})
case("listener_url", default_cfg=cfg({"api_server": pc(extra={"host": "0.0.0.0", "port": 9000})}),
     platform_value="api_server", profile="p1", seams={"format_url_host": "0.0.0.0"})
case("listener_url", default_cfg=cfg({"api_server": pc(extra={})}), platform_value="nope", profile="p1",
     seams={"format_url_host": "H"})  # invalid Platform -> suppressed -> defaults
case("listener_url", default_cfg=None, platform_value="api_server", profile="p1",
     seams={"format_url_host": raise_spec("ValueError", "x")})
case("listener_url", default_cfg=cfg({"webhook": pc(extra={"port": "8080"})}), platform_value="webhook", profile="p2",
     seams={"format_url_host": "H"})

# ── 18. check_secondary_port_binders ──
_SEC = [pg("p1", "/h/p1")]
_BIND_CFG = cfg({"webhook": pc(enabled=True, extra={})})
case("check_secondary_port_binders", plan=_P2, configs={"default": cfg({}), "p1": _BIND_CFG},
     seams={"plan_secondaries": L(_SEC), "platform_binds_port": True, "serves_profile_prefix": True, "format_url_host": "0.0.0.0"})
case("check_secondary_port_binders", plan=_P2, configs={"default": cfg({}), "p1": _BIND_CFG},
     seams={"plan_secondaries": L(_SEC), "platform_binds_port": True, "serves_profile_prefix": False})
case("check_secondary_port_binders", plan=_P2, configs={"default": cfg({}), "p1": _BIND_CFG},
     seams={"plan_secondaries": L(_SEC), "platform_binds_port": False})
case("check_secondary_port_binders", plan=_P2, configs={"default": cfg({}), "p1": cfg({"webhook": pc(enabled=False, extra={})})},
     seams={"plan_secondaries": L(_SEC)})
case("check_secondary_port_binders", plan=_P2, configs={"default": cfg({})},
     seams={"plan_secondaries": L(_SEC)})  # cfg None -> skip
case("check_secondary_port_binders", plan=_P2, configs={"default": cfg({}), "p1": _BIND_CFG},
     seams={"plan_secondaries": L(_SEC), "platform_binds_port": raise_spec("RuntimeError", "x")})

# ── 19. load_profile_configs ──
case("load_profile_configs", plan=_P2, seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None, "profile_gateway_config": cfg({})})
case("load_profile_configs", plan=_P2, seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None,
    "profile_gateway_config": [cfg({}), raise_spec("RuntimeError", "boom")]})
case("load_profile_configs", plan=_P2, seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None,
    "profile_gateway_config": raise_spec("SystemExit", "1")})
case("load_profile_configs", plan=plan([]), seams={"multiplex_read_enter": None, "multiplex_read_exit": None})
case("load_profile_configs", plan=_P2, seams={
    "multiplex_read_enter": None, "multiplex_read_exit": None,
    "profile_gateway_config": raise_spec("ValueError", "bad")})
case("load_profile_configs", plan=_P2, seams={"multiplex_read_enter": raise_spec("RuntimeError", "x")})

# ── 20. plan_tail ──
case("plan_tail", plan=plan([pg("default", "/h")]), seams={})
case("plan_tail", plan=plan([pg("default", "/h")], blockers=["b1", "b2"]), seams={})
case("plan_tail", plan=plan([pg("default", "/h")], blockers=["b1"], manifest={"version": 1}),
     seams={"manifest_path": "/h/gateway_migration.json"})
case("plan_tail", plan=plan([pg("default", "/h")], notices=["n1"]), seams={})
case("plan_tail", plan=plan([pg("default", "/h")], blockers=["b1"], notices=["n1"]), seams={})
case("plan_tail", plan=plan([pg("default", "/h")], blockers=["b1"], manifest={"version": 1}),
     seams={"manifest_path": raise_spec("RuntimeError", "x")})

# ── 21. format_update_warning ──
case("format_update_warning", plan=plan([pg("default", "/h")]), auto_blockers=[])
case("format_update_warning", plan=plan([pg("default", "/h")], blockers=["b1"]), auto_blockers=[])
case("format_update_warning", plan=plan([pg("default", "/h")]), auto_blockers=["a1", "a2"])
case("format_update_warning", plan=plan([pg("default", "/h")], blockers=["b1", "b2"]), auto_blockers=["a1"])
case("format_update_warning", plan=plan([pg("default", "/h")], blockers=["b1"]), auto_blockers=["a1", "a2", "a3"])

# ── 22. host_supports_migration ──
case("host_supports_migration", seams={"running_under_s6": False})
case("host_supports_migration", seams={"running_under_s6": True, "named_slot_name": "hermes-default", "s6_scandir": "/run/s6", "path_is_dir": True})
case("host_supports_migration", seams={"running_under_s6": True, "named_slot_name": "hermes-default", "s6_scandir": "/run/s6", "path_is_dir": False})
case("host_supports_migration", seams={"running_under_s6": True, "named_slot_name": raise_spec("RuntimeError", "x")})
case("host_supports_migration", seams={"running_under_s6": True, "named_slot_name": "hermes-default", "s6_scandir": "/run/s6", "path_is_dir": raise_spec("OSError", "x")})
case("host_supports_migration", seams={"running_under_s6": None})


# ── big-flow seam builders ──
def build_seams(profiles_pairs=None, own_pids=None, services=None, identities=None,
                flag=False, live_served=None, manifest=None, mnys=False,
                pgcfg=None, sec=None, binds=False, serves=False, auto_blockers=None,
                run_as_user=None, extra=None):
    pairs = profiles_pairs if profiles_pairs is not None else [["default", "/h"], ["p1", "/h/p1"]]
    n = len(pairs)
    own = own_pids if own_pids is not None else [None] * n
    svcs = services if services is not None else [[] for _ in range(n)]
    ids = identities if identities is not None else [[None, None] for _ in range(n)]
    s = {
        "default_home": "/h",
        "host_gateway_owner": None,
        "profiles_to_serve": [pairs, pairs, pairs],
        "own_gateway_pid": own,
        "installed_services": svcs,
        "gateway_identity": ids,
        "explicit_multiplex_flag": flag,
        "recorded_served_profiles": live_served,
        "read_manifest": manifest,
        "profile_gateway_config": pgcfg if pgcfg is not None else cfg({}),
        "plan_secondaries": L(sec if sec is not None else []),
        "platform_binds_port": binds,
        "serves_profile_prefix": serves,
        "auto_migration_blockers": L(auto_blockers if auto_blockers is not None else []),
        "multiplex_read_enter": None, "multiplex_read_exit": None,
        "home_env_enter": None, "home_env_exit": None,
    }
    if mnys:
        s["manifest_not_yet_served"] = True
    if run_as_user is not None:
        s["get_systemd_unit_path"] = "/u"
        s["read_systemd_user_from_unit"] = run_as_user
    if extra:
        s.update(extra)
    return s


# ── 23. build_migration_plan ──
case("build_migration_plan", seams=build_seams())
case("build_migration_plan", seams=build_seams(profiles_pairs=[["default", "/h"]]))
case("build_migration_plan", seams=build_seams(
    own_pids=[None, 123], services=[[], [["systemd", False]]], identities=[[None, None], [7, None]],
    sec=[pg("p1", "/h/p1", pid=123, services=[["systemd", False]])]))
case("build_migration_plan", seams=build_seams(flag=True, manifest={"version": 1}, mnys=True))
case("build_migration_plan", seams=build_seams(
    profiles_pairs=[["default", "/h"], ["p1", "/h/p1"]],
    extra={"profiles_to_serve": [[["default", "/h"], ["p1", "/h/p1"]], [["default", "/h"], ["p1", "/h/p1"]], [["default", "/h"], ["p1", "/h/p1"], ["p2", "/h/p2"]]]}))
case("build_migration_plan", seams=build_seams(own_pids=[raise_spec("RuntimeError", "x"), None]))
case("build_migration_plan", seams=build_seams(auto_blockers=["service-domain boundary"]))
case("build_migration_plan", seams=build_seams(
    services=[[], [["systemd", True]]], run_as_user="alice",
    sec=[pg("p1", "/h/p1", services=[["systemd", True]])]))

# ── 24. format_plan ──
_FP = plan([pg("default", "/h", services=[["systemd", True]]), pg("p1", "/h/p1", pid=123, services=[["systemd", False]])],
           multiplex_flag_on=True)
_FSEC = [pg("p1", "/h/p1", pid=123, services=[["systemd", False]])]


def format_seams(**over):
    s = {
        "pg_service_label": ["systemd (system)", "systemd (user)"],
        "plan_already_multiplexed": False,
        "plan_standalone_secondaries": L(_FSEC),
        "signalled_gateways": L(["p1 (pid 123)"]),
        "remove_verb": "uninstall",
        "service_label": "systemd (user)",
        "plan_target_service_kind": ["systemd", True],
        "plan_default": pg("default", "/h", services=[["systemd", True]]),
        "pg_has_gateway": True,
        "plan_expected_served": L(["default", "p1"]),
    }
    s.update(over)
    return s


case("format_plan", plan=_FP, dry_run=True, seams=format_seams())
case("format_plan", plan=_FP, dry_run=False, seams=format_seams())
case("format_plan", plan=plan([pg("default", "/h")], multiplex_flag_on=True, live_served=["default", "p1"]),
     dry_run=False, seams=format_seams(plan_already_multiplexed=True, pg_service_label="none"))
case("format_plan", plan=plan([pg("default", "/h"), pg("p1", "/h/p1")], interrupted=True, manifest={"version": 1}),
     dry_run=False, seams=format_seams(plan_standalone_secondaries=L([]), signalled_gateways=L([]),
                                       resume_target=[["systemd", True], "alice"], pg_service_label=["none", "none"]))
case("format_plan", plan=plan([pg("default", "/h")]), dry_run=False,
     seams=format_seams(pg_service_label="none", plan_standalone_secondaries=L([]), signalled_gateways=L([])))
case("format_plan", plan=plan([pg("default", "/h"), pg("p1", "/h/p1")], blockers=["b1"]),
     dry_run=False, seams=format_seams(plan_standalone_secondaries=L([]), signalled_gateways=L([]),
                                       pg_service_label=["none", "none"]))
case("format_plan", plan=plan([pg("default", "/h"), pg("p1", "/h/p1", pid=5, services=[["s6", False]])], multiplex_flag_on=True),
     dry_run=False, seams=format_seams(plan_standalone_secondaries=L([pg("p1", "/h/p1", pid=5, services=[["s6", False]])]),
                                       remove_verb="park", service_label="s6 slot", pg_service_label=["none", "s6 slot"]))
case("format_plan", plan=plan([pg("default", "/h"), pg("p1", "/h/p1")], manifest={"version": 1}, blockers=["b1"]),
     dry_run=False, seams=format_seams(plan_standalone_secondaries=L([]), signalled_gateways=L([]),
                                       resume_target=[None, None], pg_service_label=["none", "none"],
                                       manifest_path="/h/gateway_migration.json"))

# ── 25. cmd_migrate ──
case("cmd_migrate", args={"backends": True}, seams={"cmd_backends_report": None})
case("cmd_migrate", args={"json": True}, seams={})  # SystemExit 2
case("cmd_migrate", args={}, seams={"running_under_s6": True, "named_slot_name": "hermes-default", "s6_scandir": "/run/s6", "path_is_dir": False})  # SystemExit 1
case("cmd_migrate", args={"dry_run": True}, seams=dict(build_seams(), **format_seams()))
case("cmd_migrate", args={}, seams=dict(build_seams(flag=True, live_served=["default", "p1"]),
                                        plan_already_multiplexed=True, pg_service_label="none"))
case("cmd_migrate", args={"yes": True}, seams=dict(build_seams(), plan_blocked=True,
                                                   plan_already_multiplexed=False, pg_service_label=["none", "none"]))
case("cmd_migrate", args={"yes": True}, seams=dict(build_seams(profiles_pairs=[["default", "/h"]]),
                                                   plan_already_multiplexed=False, plan_blocked=False, pg_service_label="none"))
case("cmd_migrate", args={}, seams=dict(build_seams(), plan_already_multiplexed=False, plan_blocked=False,
                                        pg_service_label=["none", "none"], stdin_isatty=True, prompt_yes_no=False))
case("cmd_migrate", args={"yes": True}, seams=dict(build_seams(), plan_already_multiplexed=False, plan_blocked=False,
                                                   pg_service_label=["none", "none"], apply_migration=True,
                                                   plan_standalone_secondaries=L([]), signalled_gateways=L([])))
case("cmd_migrate", args={"yes": True}, seams=dict(build_seams(), plan_already_multiplexed=False, plan_blocked=False,
                                                   pg_service_label=["none", "none"], apply_migration=False,
                                                   plan_standalone_secondaries=L([]), signalled_gateways=L([])))

# ── 26. maybe_auto_migrate_after_update ──
case("maybe_auto_migrate_after_update", seams={"running_under_s6": True, "named_slot_name": "hermes-default", "s6_scandir": "/run/s6", "path_is_dir": False})
case("maybe_auto_migrate_after_update", seams={"running_under_s6": False, "default_home": "/h", "auto_migration_opted_out": True})
case("maybe_auto_migrate_after_update", seams=dict(build_seams(flag=True, live_served=["default", "p1"]),
                                                   running_under_s6=False, auto_migration_opted_out=False,
                                                   plan_already_multiplexed=True, pg_service_label="none"))
case("maybe_auto_migrate_after_update", seams=dict(build_seams(profiles_pairs=[["default", "/h"]]),
                                                   running_under_s6=False, auto_migration_opted_out=False,
                                                   plan_already_multiplexed=False, pg_service_label="none"))
case("maybe_auto_migrate_after_update", seams=dict(build_seams(), running_under_s6=False, auto_migration_opted_out=False,
                                                   plan_already_multiplexed=False, plan_standalone_secondaries=L([]),
                                                   pg_service_label=["none", "none"]))
case("maybe_auto_migrate_after_update", seams=dict(build_seams(), running_under_s6=False, auto_migration_opted_out=False,
                                                   plan_already_multiplexed=False, plan_standalone_secondaries=L(_FSEC),
                                                   masked_default_unit="hermes-gateway", pg_service_label=["none", "none"]))
case("maybe_auto_migrate_after_update", seams=dict(build_seams(), running_under_s6=False, auto_migration_opted_out=False,
                                                   plan_already_multiplexed=False, plan_standalone_secondaries=L(_FSEC),
                                                   masked_default_unit=None, plan_blocked=True, pg_service_label=["none", "none"]))
case("maybe_auto_migrate_after_update", seams=dict(build_seams(manifest={"version": 1}), running_under_s6=False,
                                                   auto_migration_opted_out=False, plan_already_multiplexed=False,
                                                   plan_standalone_secondaries=L([]), pg_service_label=["none", "none"],
                                                   resume_target=[None, None], apply_migration=True, signalled_gateways=L([])))
case("maybe_auto_migrate_after_update", seams=dict(build_seams(), running_under_s6=False, auto_migration_opted_out=False,
                                                   plan_already_multiplexed=False, plan_standalone_secondaries=L(_FSEC),
                                                   masked_default_unit=None, plan_blocked=False, pg_service_label=["none", "none"],
                                                   apply_migration=True, signalled_gateways=L(["p1 (pid 123)"]),
                                                   remove_verb="uninstall", service_label="systemd (user)",
                                                   plan_target_service_kind=["systemd", True],
                                                   plan_default=pg("default", "/h"), pg_has_gateway=False,
                                                   plan_expected_served=L(["default", "p1"])))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print("  %-32s %d%s" % (fn, counts[fn], flag))
    short = [fn for fn, n in counts.items() if n < 5]
    if short:
        raise SystemExit("funcs with <5 cases: %s" % short)


if __name__ == "__main__":
    main()
