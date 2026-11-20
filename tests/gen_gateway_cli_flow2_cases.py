#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_cli_flow2.jsonl for the gateway_cli_flow2
FLOW oracle (hermes_cli/gateway.py UNCLAIMED remainder). >=5 cases per ported
function, exercising decision branches, error paths, BaseException propagation
and FAIL-CLOSED missing scripts. Script semantics match the harness/oracle:
array = per-call queue (last repeats), single value = constant, MISSING =
RuntimeError raise spec (fail-closed), scripted null = None,
{"__raise__": cls, "msg": ...} = exception. ASCII / POSIX-lexical domain.
"""
import json
import os
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_cli_flow2.jsonl")

UNIT = "/etc/systemd/system/hermes-gateway.service"
HOME = "/home/u/.hermes"


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


CASES = []


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# 1. _systemd_command (pure)
add("systemd_command", {"argv": ["ls", "-la"]})
add("systemd_command", {"argv": ["a b", "c"]})
add("systemd_command", {"argv": ["a\\b"]})
add("systemd_command", {"argv": ['a"b']})
add("systemd_command", {"argv": ["100%", "$x"]})
add("systemd_command", {"argv": []})
add("systemd_command", {"argv": ['mix\\%"$']})
add("systemd_command", {"argv": None})
add("systemd_command", {"argv": 5})
add("systemd_command", {"argv": ["ok", 7]})

# 2. _completed_process_detail (pure leaf, result double)
add("completed_process_detail", {"result": {"stderr": "err\n", "stdout": "out", "returncode": 1}})
add("completed_process_detail", {"result": {"stderr": "", "stdout": "out\n", "returncode": 0}})
add("completed_process_detail", {"result": {"stderr": "", "stdout": "", "returncode": 3}})
add("completed_process_detail", {"result": {"stderr": "  ", "stdout": "x", "returncode": 0}})
add("completed_process_detail", {"result": {"stderr": None, "stdout": "out", "returncode": 0}})
add("completed_process_detail", {"result": {"stderr": None, "stdout": None, "returncode": 0}})
add("completed_process_detail", {"result": None})
add("completed_process_detail", {"result": 5})

# 3. _hermes_home_pinned_by_unit (seam unit_environment_value)
add("hermes_home_pinned_by_unit", {"unit_path": UNIT}, {"unit_environment_value": HOME})
add("hermes_home_pinned_by_unit", {"unit_path": UNIT}, {"unit_environment_value": None})
add("hermes_home_pinned_by_unit", {"unit_path": UNIT}, {"unit_environment_value": ""})
add("hermes_home_pinned_by_unit", {"unit_path": UNIT}, {"unit_environment_value": R("RuntimeError")})
add("hermes_home_pinned_by_unit", {"unit_path": UNIT}, {"unit_environment_value": R("SystemExit", "1")})
add("hermes_home_pinned_by_unit", {"unit_path": UNIT}, {})  # fail-closed

# 4. _installed_unit_ld_library_path (seams get_systemd_unit_path + unit_environment_value)
B4 = {"get_systemd_unit_path": UNIT}
add("installed_unit_ld_library_path", {"system": False}, {**B4, "unit_environment_value": "/lib"})
add("installed_unit_ld_library_path", {"system": True}, {**B4, "unit_environment_value": None})
add("installed_unit_ld_library_path", {"system": False}, {**B4, "unit_environment_value": ""})
add("installed_unit_ld_library_path", {"system": False}, {"get_systemd_unit_path": R("RuntimeError")})
add("installed_unit_ld_library_path", {"system": False}, {**B4, "unit_environment_value": R("SystemExit", "1")})
add("installed_unit_ld_library_path", {"system": False}, {})  # fail-closed

# 5. _ld_library_path_line (environ_get + composed sibling + remap + systemd_env_line)
add("ld_library_path_line", {"system": False}, {"environ_get": "/a:/b", "systemd_env_line": "Environment=LD_LIBRARY_PATH=/a:/b"})
add("ld_library_path_line", {"system": False}, {"environ_get": "", "get_systemd_unit_path": UNIT, "unit_environment_value": "/lib", "systemd_env_line": "ENV"})
add("ld_library_path_line", {"system": False}, {"environ_get": "", "get_systemd_unit_path": UNIT, "unit_environment_value": None})
add("ld_library_path_line", {"system": False}, {"environ_get": "/a::b:", "systemd_env_line": "ENV"})
add("ld_library_path_line", {"system": True, "target_home_dir": "/home/t"}, {"environ_get": "/a:/b", "remap_path_for_user": ["/home/t/a", "/home/t/b"], "systemd_env_line": "ENV"})
add("ld_library_path_line", {"system": False}, {"environ_get": R("RuntimeError")})
add("ld_library_path_line", {"system": False, "target_home_dir": "/home/t"}, {"environ_get": "/a", "remap_path_for_user": R("ValueError")})
add("ld_library_path_line", {"system": False}, {"environ_get": "/a", "systemd_env_line": R("SystemExit", "1")})
add("ld_library_path_line", {"system": False}, {})  # fail-closed

# 6. _loginctl_enable_linger (seam subprocess_run)
add("loginctl_enable_linger", {"username": "u"}, {"subprocess_run": {"stdout": "", "stderr": "", "returncode": 0}})
add("loginctl_enable_linger", {"username": "alice"}, {"subprocess_run": None})
add("loginctl_enable_linger", {"username": "u"}, {"subprocess_run": R("RuntimeError")})
add("loginctl_enable_linger", {"username": "u"}, {"subprocess_run": R("SystemExit", "1")})
add("loginctl_enable_linger", {"username": "u"}, {})  # fail-closed
add("loginctl_enable_linger", {"username": "bob"}, {"subprocess_run": {"returncode": 1}})

# 7. _wait_for_target_user_bus (seam poll_until)
add("wait_for_target_user_bus", {"uid": 1000}, {"poll_until": True})
add("wait_for_target_user_bus", {"uid": 1000}, {"poll_until": False})
add("wait_for_target_user_bus", {"uid": 0, "timeout": 3.0}, {"poll_until": True})
add("wait_for_target_user_bus", {"uid": 1000}, {"poll_until": R("RuntimeError")})
add("wait_for_target_user_bus", {"uid": 1000}, {"poll_until": R("SystemExit", "1")})
add("wait_for_target_user_bus", {"uid": 1000}, {})  # fail-closed

# 8. user_systemd_unit_dir (environ_get + get_real_home)
add("user_systemd_unit_dir", {}, {"environ_get": "/custom/config"})
add("user_systemd_unit_dir", {}, {"environ_get": "", "get_real_home": "/home/u"})
add("user_systemd_unit_dir", {}, {"environ_get": "  ", "get_real_home": "/home/u"})
add("user_systemd_unit_dir", {}, {"environ_get": None})
add("user_systemd_unit_dir", {}, {"environ_get": R("RuntimeError")})
add("user_systemd_unit_dir", {}, {"environ_get": "", "get_real_home": R("SystemExit", "1")})
add("user_systemd_unit_dir", {}, {})  # fail-closed

# 9. _user_runtime_dir (environ_get + getuid)
add("user_runtime_dir", {}, {"environ_get": "/run/user/1000"})
add("user_runtime_dir", {}, {"environ_get": None, "getuid": 1000})
add("user_runtime_dir", {}, {"environ_get": "", "getuid": 0})
add("user_runtime_dir", {}, {"environ_get": R("RuntimeError")})
add("user_runtime_dir", {}, {"environ_get": None, "getuid": R("SystemExit", "1")})
add("user_runtime_dir", {}, {})  # fail-closed

# 10. _ensure_system_service_linger (ensure_linger_enabled + pwd + poll_until + unit_is_active + get_service_name + print)
add("ensure_system_service_linger", {"username": "u"}, {"ensure_linger_enabled": False})
add("ensure_system_service_linger", {"username": "u"}, {"ensure_linger_enabled": True, "pwd_getpwnam": {"pw_uid": 1000}, "poll_until": True, "systemd_unit_is_active": False, "print": None})
add("ensure_system_service_linger", {"username": "u"}, {"ensure_linger_enabled": True, "pwd_getpwnam": {"pw_uid": 1000}, "poll_until": False, "systemd_unit_is_active": False, "print": None})
add("ensure_system_service_linger", {"username": "root"}, {"ensure_linger_enabled": True, "pwd_getpwnam": {"pw_uid": 0}, "poll_until": True, "systemd_unit_is_active": True, "get_service_name": "hermes-gateway", "print": None})
add("ensure_system_service_linger", {"username": "u"}, {"ensure_linger_enabled": R("RuntimeError")})
add("ensure_system_service_linger", {"username": "u"}, {"ensure_linger_enabled": True, "pwd_getpwnam": R("SystemExit", "1")})
add("ensure_system_service_linger", {"username": "u"}, {"ensure_linger_enabled": True, "pwd_getpwnam": {"pw_uid": 1000}, "poll_until": True, "systemd_unit_is_active": False, "print": R("ValueError")})
add("ensure_system_service_linger", {"username": "u"}, {})  # fail-closed

# 11. refuses_container_user_scope_install (is_container + print_error + print_info_lines)
add("refuses_container_user_scope_install", {"system": True}, {})
add("refuses_container_user_scope_install", {"system": False}, {"is_container": False})
add("refuses_container_user_scope_install", {"system": False}, {"is_container": True, "print_error": None, "print_info_lines": None})
add("refuses_container_user_scope_install", {"system": False}, {"is_container": R("RuntimeError")})
add("refuses_container_user_scope_install", {"system": False}, {"is_container": True, "print_error": R("SystemExit", "1")})
add("refuses_container_user_scope_install", {"system": False}, {})  # fail-closed

# 12. _is_apt_termux_install (seam sealed_steward)
add("is_apt_termux_install", {}, {"sealed_steward": "apt-termux"})
add("is_apt_termux_install", {}, {"sealed_steward": "other"})
add("is_apt_termux_install", {}, {"sealed_steward": None})
add("is_apt_termux_install", {}, {"sealed_steward": R("RuntimeError")})
add("is_apt_termux_install", {}, {"sealed_steward": R("SystemExit", "1")})
add("is_apt_termux_install", {}, {})  # fail-closed

# 13. _print_service_not_installed (systemd_cli_bits + print)
add("print_service_not_installed", {"system": True}, {"systemd_cli_bits": [["sudo ", " --system", ""]], "print": None})
add("print_service_not_installed", {"system": False}, {"systemd_cli_bits": [["", "", "--user "]], "print": None})
add("print_service_not_installed", {"system": False}, {"systemd_cli_bits": R("RuntimeError")})
add("print_service_not_installed", {"system": False}, {"systemd_cli_bits": [["", "", "--user "]], "print": R("SystemExit", "1")})
add("print_service_not_installed", {"system": False}, {})  # fail-closed
add("print_service_not_installed", {"system": True}, {"systemd_cli_bits": [["sudo ", " --system", ""]], "print": [None, None]})

# 14. _print_runtime_health (runtime_health_lines + print)
add("print_runtime_health", {}, {"runtime_health_lines": [["a", "b"]], "print": None})
add("print_runtime_health", {}, {"runtime_health_lines": [[]]})
add("print_runtime_health", {}, {"runtime_health_lines": [["one"]], "print": None})
add("print_runtime_health", {}, {"runtime_health_lines": R("RuntimeError")})
add("print_runtime_health", {}, {"runtime_health_lines": [["a"]], "print": R("SystemExit", "1")})
add("print_runtime_health", {}, {})  # fail-closed

# 15. _systemd_unit_belongs_to_current_home (foreign + unit_path + home + sync + unit_file + expanduser/resolve + print_warning)
add("systemd_unit_belongs_to_current_home", {"system": True}, {"foreign_systemd_unit_home": "/other", "get_systemd_unit_path": UNIT, "get_hermes_home": HOME, "print_warning": None})
add("systemd_unit_belongs_to_current_home", {"system": False}, {"foreign_systemd_unit_home": None, "sync_hermes_home_from_systemd_unit": None, "hermes_home_from_systemd_unit_file": None})
add("systemd_unit_belongs_to_current_home", {"system": False}, {"foreign_systemd_unit_home": None, "sync_hermes_home_from_systemd_unit": None, "hermes_home_from_systemd_unit_file": HOME, "path_expanduser": HOME, "path_resolve": [HOME, HOME], "get_hermes_home": HOME})
add("systemd_unit_belongs_to_current_home", {"system": False}, {"foreign_systemd_unit_home": None, "sync_hermes_home_from_systemd_unit": None, "hermes_home_from_systemd_unit_file": "/other", "path_expanduser": "/other", "path_resolve": ["/other", HOME], "get_hermes_home": [HOME, HOME], "get_systemd_unit_path": UNIT, "print_warning": None})
add("systemd_unit_belongs_to_current_home", {"system": False}, {"foreign_systemd_unit_home": R("RuntimeError")})
add("systemd_unit_belongs_to_current_home", {"system": False}, {"foreign_systemd_unit_home": None, "sync_hermes_home_from_systemd_unit": None, "hermes_home_from_systemd_unit_file": HOME, "path_expanduser": R("SystemExit", "1")})
add("systemd_unit_belongs_to_current_home", {"system": False}, {})  # fail-closed

# 16. _bare_unit_pinned_home (is_linux + geteuid + composed unit_dir + composed pinned + expanduser/resolve)
add("bare_unit_pinned_home", {}, {"is_linux": False})
add("bare_unit_pinned_home", {}, {"is_linux": True, "geteuid": 0, "unit_environment_value": None})
add("bare_unit_pinned_home", {}, {"is_linux": True, "geteuid": 0, "unit_environment_value": HOME, "path_expanduser": HOME, "path_resolve": HOME})
add("bare_unit_pinned_home", {}, {"is_linux": True, "geteuid": 1000, "environ_get": "/cfg", "unit_environment_value": HOME, "path_expanduser": HOME, "path_resolve": HOME})
add("bare_unit_pinned_home", {}, {"is_linux": True, "geteuid": 0, "unit_environment_value": "~/x", "path_expanduser": R("RuntimeError")})
add("bare_unit_pinned_home", {}, {"is_linux": True, "geteuid": 0, "unit_environment_value": "~/x", "path_expanduser": R("ValueError")})
add("bare_unit_pinned_home", {}, {"is_linux": True, "geteuid": 0, "unit_environment_value": "~/x", "path_expanduser": R("SystemExit", "1")})
add("bare_unit_pinned_home", {}, {"is_linux": True, "geteuid": 0, "unit_environment_value": "~/x", "path_expanduser": R("TypeError")})
add("bare_unit_pinned_home", {}, {"is_linux": R("RuntimeError")})
add("bare_unit_pinned_home", {}, {})  # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    cnt = Counter(c["func"] for c in CASES)
    short = {k: v for k, v in cnt.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(cnt), OUT))
    if short:
        print("WARNING funcs with <5 cases: %s" % short)
    else:
        print("all funcs have >=5 cases")


if __name__ == "__main__":
    main()