#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_flow4.jsonl (>=5 cases per FLOW func).

Deterministic, scripted-seam cases for the UNCLAIMED-remainder FLOW cores of
hermes_cli/gateway.py ported in src/cli/gateway_flow4.c. A scripted seam value
is either a JSON value (one result for every call) or an array used as a
per-call queue (last repeats). A {"__raise__": cls, "msg": ...} scripts a live
raise of that exact class; BaseException-family classes verify the full
builtin exception hierarchy (except Exception never catches them).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "gateway_flow4.jsonl"
CASES = []

def add(func, inp): CASES.append({"func": func, "in": inp})
def exc(name, msg="boom"): return {"__raise__": name, "msg": msg}

# ---------------- cmd_run ----------------
add("cmd_run", {"args": {"verbose": 1, "quiet": True, "replace": False, "force": False,
                         "idle_exit": True, "external_supervisor": False},
                "seams": {"redirect_s6": True}})
add("cmd_run", {"args": {"verbose": 0, "quiet": False, "replace": True, "force": False,
                         "idle_exit": False, "external_supervisor": True},
                "seams": {"redirect_s6": False, "setenv": None, "run_gateway": None}})
add("cmd_run", {"args": {"verbose": 2, "quiet": True, "replace": False, "force": True,
                         "idle_exit": False, "external_supervisor": False},
                "seams": {"redirect_s6": False, "run_gateway": None}})
add("cmd_run", {"args": {}, "seams": {"redirect_s6": exc("OSError")}})
add("cmd_run", {"args": {"external_supervisor": True},
                "seams": {"redirect_s6": False, "setenv": exc("ValueError")}})
add("cmd_run", {"args": {}, "seams": {"redirect_s6": False, "run_gateway": exc("KeyboardInterrupt")}})
add("cmd_run", {"args": {}, "seams": {"redirect_s6": False, "run_gateway": exc("RuntimeError")}})
add("cmd_run", {"args": {"verbose": 3, "external_supervisor": True},
                "seams": {"redirect_s6": False, "setenv": None, "run_gateway": exc("SystemExit")}})

# ---------------- wait_for_api_server_port_free ----------------
add("wait_for_api_server_port_free", {"seams": {"load_config": {"platforms": {}}}})
add("wait_for_api_server_port_free", {"seams": {"load_config": {"platforms": {
    "api_server": {"enabled": False}}}}})
add("wait_for_api_server_port_free", {"seams": {
    "load_config": {"platforms": {"api_server": {"enabled": True, "extra": {"port": 9000}}}},
    "listen_address": {"host": "127.0.0.1", "port": 9000}, "wait_port": True}})
add("wait_for_api_server_port_free", {"seams": {
    "load_config": {"platforms": {"api_server": {"enabled": True, "extra": {}}}},
    "listen_address": {"host": "0.0.0.0", "port": 8080}, "wait_port": False, "print": None}})
add("wait_for_api_server_port_free", {"seams": {"load_config": exc("OSError")}})
add("wait_for_api_server_port_free", {"seams": {
    "load_config": {"platforms": {"api_server": {"enabled": True, "extra": {}}}},
    "listen_address": exc("ValueError")}})
add("wait_for_api_server_port_free", {"seams": {
    "load_config": {"platforms": {"api_server": {"enabled": True, "extra": {}}}},
    "listen_address": ["h", 1], "wait_port": exc("KeyboardInterrupt")}})
add("wait_for_api_server_port_free", {"seams": {
    "load_config": {"platforms": {"api_server": {"enabled": True, "extra": {}}}},
    "listen_address": {"host": "h", "port": 2}, "wait_port": False, "print": exc("OSError")}})

# ---------------- pm_managed_node_dirs ----------------
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {
    "Facts_new": None, "env_for": {"PATH": ["/n1", "/n2"]}, "is_dir": True}})
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {
    "Facts_new": None, "env_for": {"PATH": ["/a", "/b", "/c"]},
    "is_dir": [True, False, True]}})
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {
    "Facts_new": None, "env_for": {"PATH": []}, "is_dir": True}})
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {
    "Facts_new": None, "env_for": {"PATH": None}, "is_dir": True}})
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {
    "Facts_new": None, "env_for": {"PATH": ["", "/x"]}, "is_dir": True}})
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {"Facts_new": exc("OSError")}})
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {
    "Facts_new": None, "env_for": exc("ValueError")}})
add("pm_managed_node_dirs", {"home": "/data/h", "seams": {
    "Facts_new": None, "env_for": {"PATH": ["/x"]}, "is_dir": exc("KeyboardInterrupt")}})

# ---------------- native_service_homes ----------------
add("native_service_homes", {"seams": {"account_home": "/h/account", "sudo_home": "/h/sudo"}})
add("native_service_homes", {"seams": {"account_home": "/h/account", "sudo_home": None}})
add("native_service_homes", {"seams": {"account_home": exc("OSError")}})
add("native_service_homes", {"seams": {"account_home": "/h/a", "sudo_home": exc("ValueError")}})
add("native_service_homes", {"seams": {"account_home": "/h/a", "sudo_home": exc("KeyboardInterrupt")}})

# ---------------- windows_process_listing ----------------
add("windows_process_listing", {"seams": {
    "which": "/bin/wmic", "bounded_probe_run": {"returncode": 0, "stdout": "CommandLine=x\nProcessId=1\n"}}})
add("windows_process_listing", {"seams": {
    "which": "/bin/wmic",
    "bounded_probe_run": [{"returncode": 1, "stdout": ""}, {"returncode": 0, "stdout": "ProcessId=2\n"}],
}})
add("windows_process_listing", {"seams": {
    "which": [None, "/bin/powershell"],
    "bounded_probe_run": {"returncode": 0, "stdout": "ok"}}})
add("windows_process_listing", {"seams": {
    "which": [None, None, "/bin/pwsh"],
    "bounded_probe_run": {"returncode": 0, "stdout": "pwsh-ok"}}})
add("windows_process_listing", {"seams": {"which": None}})
add("windows_process_listing", {"seams": {"which": "/bin/wmic", "bounded_probe_run": exc("OSError")}})
add("windows_process_listing", {"seams": {
    "which": [None, "/bin/powershell"], "bounded_probe_run": exc("KeyboardInterrupt")}})
add("windows_process_listing", {"seams": {
    "which": "/bin/wmic", "bounded_probe_run": {"returncode": 0, "stdout": None}}})

# ---------------- s6_gateway_snapshot ----------------
add("s6_gateway_snapshot", {"gateway_pids": [1, 2], "seams": {"detect_manager": "systemd"}})
add("s6_gateway_snapshot", {"gateway_pids": [], "seams": {
    "detect_manager": "s6", "profile_name": "default", "get_manager": None, "mgr_scandir": None}})
add("s6_gateway_snapshot", {"gateway_pids": [7], "seams": {
    "detect_manager": "s6", "profile_name": "work", "get_manager": None,
    "mgr_scandir": "/etc/s6", "dir_isdir": True, "mgr_is_running": True}})
add("s6_gateway_snapshot", {"gateway_pids": [], "seams": {
    "detect_manager": "s6", "profile_name": "work", "get_manager": None,
    "mgr_scandir": "/etc/s6", "dir_isdir": False}})
add("s6_gateway_snapshot", {"gateway_pids": [], "seams": {
    "detect_manager": "s6", "profile_name": "w", "get_manager": None, "mgr_scandir": exc("OSError")}})
add("s6_gateway_snapshot", {"gateway_pids": [], "seams": {
    "detect_manager": "s6", "profile_name": "w", "get_manager": None,
    "mgr_scandir": "/s6", "dir_isdir": True, "mgr_is_running": exc("ValueError")}})
add("s6_gateway_snapshot", {"gateway_pids": [9], "seams": {
    "detect_manager": "s6", "profile_name": "w", "get_manager": None,
    "mgr_scandir": "/s6", "dir_isdir": True, "mgr_is_running": exc("KeyboardInterrupt")}})

# ---------------- reaper_exclusion_pids ----------------
add("reaper_exclusion_pids", {"extra_exclude": [100], "seams": {
    "getpid": 1, "service_pids": [200], "read_pid_record": "r1", "read_lock_record": "r2",
    "pid_from_record": [300, 0], "running_pid": 0,
    "psutil_parent": [600, None]}})
add("reaper_exclusion_pids", {"seams": {
    "getpid": 1, "service_pids": exc("OSError"), "read_pid_record": "r1",
    "read_lock_record": "r2", "pid_from_record": 300, "running_pid": None}})
add("reaper_exclusion_pids", {"seams": {
    "getpid": 1, "service_pids": None, "read_pid_record": exc("OSError")}})
add("reaper_exclusion_pids", {"seams": {
    "getpid": 1, "service_pids": None, "read_pid_record": "r1", "read_lock_record": "r2",
    "pid_from_record": exc("ValueError")}})
add("reaper_exclusion_pids", {"seams": {
    "getpid": 1, "service_pids": None, "read_pid_record": "r1", "read_lock_record": "r2",
    "pid_from_record": 300, "running_pid": exc("OSError")}})
add("reaper_exclusion_pids", {"seams": {
    "getpid": 1, "service_pids": None, "read_pid_record": "r1", "read_lock_record": "r2",
    "pid_from_record": 300, "running_pid": None, "psutil_parent": exc("OSError")}})
add("reaper_exclusion_pids", {"seams": {
    "getpid": 1, "service_pids": None, "read_pid_record": "r1", "read_lock_record": "r2",
    "pid_from_record": 300, "running_pid": None, "psutil_parent": exc("KeyboardInterrupt")}})

# ---------------- apply_startup_watchdog_config ----------------
add("apply_startup_watchdog_config", {"env": {}, "seams": {
    "load_config": {"gateway": {"startup_watchdog": False, "startup_watchdog_timeout_seconds": 45}},
    "setenv": None, "sw_disabled": False, "sw_disarm": None, "sw_arm": None}})
add("apply_startup_watchdog_config", {"env": {}, "seams": {
    "load_config": {"gateway": {"startup_watchdog_timeout_seconds": 30}},
    "setenv": None, "sw_disabled": False, "sw_disarm": None, "sw_arm": None}})
add("apply_startup_watchdog_config", {"env": {}, "seams": {
    "load_config": {"gateway": {}}, "sw_disabled": True, "sw_disarm": None}})
add("apply_startup_watchdog_config", {"env": {}, "seams": {
    "load_config": exc("OSError"), "sw_disabled": False, "sw_arm": None}})
add("apply_startup_watchdog_config", {"env": {}, "seams": {
    "load_config": {"gateway": {"startup_watchdog": False}},
    "setenv": exc("ValueError"), "sw_disabled": False, "sw_arm": None}})
add("apply_startup_watchdog_config", {"env": {}, "seams": {
    "load_config": {"gateway": {}}, "sw_disabled": exc("OSError")}})
add("apply_startup_watchdog_config", {"env": {}, "seams": {
    "load_config": {"gateway": {}}, "sw_disabled": False, "sw_arm": exc("KeyboardInterrupt")}})

# ---------------- exit_diag ----------------
add("exit_diag", {"env": {"HERMES_GATEWAY_EXIT_DIAG": "0"}, "tag": "start", "seams": {}})
add("exit_diag", {"env": {}, "tag": "run", "python": "3.14.9", "platform": "win32",
                  "extra": {"phase": "boot"}, "seams": {
    "get_hermes_home": "/h", "mkdir": None, "timestamp": "2026-01-01T00:00:00+00:00",
    "getpid": 42, "json_dumps": "LINEJSON", "write": None}})
add("exit_diag", {"env": {}, "tag": "run", "python": "3.14.9", "platform": "win32", "seams": {"get_hermes_home": exc("OSError")}})
add("exit_diag", {"env": {}, "tag": "run", "python": "3.14.9", "platform": "win32", "seams": {
    "get_hermes_home": "/h", "mkdir": exc("ValueError")}})
add("exit_diag", {"env": {}, "tag": "run", "python": "3.14.9", "platform": "win32", "seams": {
    "get_hermes_home": "/h", "mkdir": None, "timestamp": exc("OSError")}})
add("exit_diag", {"env": {}, "tag": "run", "python": "3.14.9", "platform": "win32", "seams": {
    "get_hermes_home": "/h", "mkdir": None, "timestamp": "T", "getpid": exc("KeyboardInterrupt")}})
add("exit_diag", {"env": {}, "tag": "run", "python": "3.14.9", "platform": "win32", "seams": {
    "get_hermes_home": "/h", "mkdir": None, "timestamp": "T", "getpid": 1,
    "json_dumps": exc("OSError")}})
add("exit_diag", {"env": {}, "tag": "run", "python": "3.14.9", "platform": "win32", "seams": {
    "get_hermes_home": "/h", "mkdir": None, "timestamp": "T", "getpid": 1,
    "json_dumps": "L", "write": exc("OSError")}})

# ---------------- respawn_storm_backoff ----------------
add("respawn_storm_backoff", {"env": {}, "seams": {"load_config": {}, "record_storm": None}})
add("respawn_storm_backoff", {"env": {}, "seams": {
    "load_config": {}, "record_storm": {"count": 6, "window_s": 120.0, "backoff_s": 30.0},
    "logger_warning": None, "kick_watchdog": None, "sleep": None}})
add("respawn_storm_backoff", {"env": {}, "seams": {
    "load_config": {"gateway": {"respawn_storm": {"max_starts": 0}}}}})
add("respawn_storm_backoff", {"env": {"HERMES_GATEWAY_MAX_STARTS": "3", "HERMES_GATEWAY_START_WINDOW_S": "60.5"},
    "seams": {"load_config": {}, "record_storm": {"count": 4, "window_s": 60.5, "backoff_s": 10.0},
              "logger_warning": None, "kick_watchdog": None, "sleep": None}})
add("respawn_storm_backoff", {"env": {"HERMES_GATEWAY_MAX_STARTS": "abc"},
    "seams": {"load_config": {}, "record_storm": None}})
add("respawn_storm_backoff", {"env": {}, "seams": {"load_config": {}, "record_storm": exc("ValueError"),
    "logger_debug": None}})
add("respawn_storm_backoff", {"env": {}, "seams": {"load_config": {}, "record_storm": exc("KeyboardInterrupt")}})
add("respawn_storm_backoff", {"env": {}, "seams": {
    "load_config": {}, "record_storm": {"count": 6, "window_s": 120.0, "backoff_s": 30.0},
    "logger_warning": None, "kick_watchdog": exc("OSError"), "sleep": None}})

# ---------------- install_systemd_from_cli ----------------
add("install_systemd_from_cli", {"force": False, "system": False, "run_as_user": None,
    "info_lines": ["  a", "  b"], "stdin_kind": "none", "args": {"start_now": True, "start_on_login": True},
    "seams": {"is_wsl": True, "print_warning": None, "info_lines": None, "print": None,
              "systemd_install": None, "systemd_start": None}})
add("install_systemd_from_cli", {"force": True, "system": False, "run_as_user": None,
    "stdin_kind": "tty", "stdin_isatty": True,
    "args": {"start_now": None, "start_on_login": None, "force_unit_path": True},
    "seams": {"is_wsl": False, "prompt_yes_no": True, "systemd_install": None, "systemd_start": None}})
add("install_systemd_from_cli", {"force": False, "system": True, "run_as_user": "bob",
    "stdin_kind": "none", "args": {"start_now": False, "start_on_login": True},
    "seams": {"is_wsl": False, "systemd_install": None}})
add("install_systemd_from_cli", {"force": False, "system": False, "run_as_user": None,
    "stdin_kind": "no_isatty", "args": {},
    "seams": {"is_wsl": False, "systemd_install": None, "systemd_start": None}})
add("install_systemd_from_cli", {"force": False, "system": False,
    "seams": {"is_wsl": exc("OSError")}})
add("install_systemd_from_cli", {"force": False, "system": False, "stdin_kind": "tty", "stdin_isatty": True,
    "args": {"start_now": None}, "seams": {"is_wsl": False, "prompt_yes_no": exc("ValueError")}})
add("install_systemd_from_cli", {"force": False, "system": False, "stdin_kind": "none",
    "args": {"start_now": True, "start_on_login": True},
    "seams": {"is_wsl": False, "systemd_install": exc("KeyboardInterrupt")}})
add("install_systemd_from_cli", {"force": False, "system": False, "stdin_kind": "none",
    "args": {"start_now": True, "start_on_login": True},
    "seams": {"is_wsl": False, "systemd_install": None, "systemd_start": exc("OSError")}})

# ---------------- legacy_launchd_labels_for_install ----------------
add("legacy_launchd_labels_for_install", {"exclude": [], "seams": {
    "pw_dir": "/home/u", "root": "/hermes", "agents_isdir": False}})
add("legacy_launchd_labels_for_install", {"exclude": [], "seams": {
    "pw_dir": "/home/u", "root": "/hermes", "agents_isdir": True,
    "glob": {"paths": ["/home/u/Library/LaunchAgents/ai.hermes.gateway.plist"]},
    "read_plist": {"Label": "ai.hermes.gateway", "EnvironmentVariables": {"HERMES_HOME": "/hermes"}},
    "relative_to": {"parts": []}}})
add("legacy_launchd_labels_for_install", {"exclude": [], "seams": {
    "pw_dir": "/home/u", "root": "/hermes", "agents_isdir": True,
    "glob": {"paths": ["/home/u/Library/LaunchAgents/ai.hermes.gateway-work.plist"]},
    "read_plist": {"Label": "ai.hermes.gateway-work", "EnvironmentVariables": {"HERMES_HOME": "/hermes/profiles/work"}},
    "relative_to": {"parts": ["profiles", "work"]}}})
add("legacy_launchd_labels_for_install", {"exclude": ["ai.hermes.gateway"],
    "seams": {"pw_dir": "/home/u", "root": "/hermes", "agents_isdir": True,
    "glob": {"paths": ["/home/u/Library/LaunchAgents/ai.hermes.gateway.plist"]},
    "read_plist": {"Label": "ai.hermes.gateway", "EnvironmentVariables": {"HERMES_HOME": "/hermes"}},
    "relative_to": {"parts": []}}})
add("legacy_launchd_labels_for_install", {"exclude": [], "seams": {
    "pw_dir": "/home/u", "root": "/hermes", "agents_isdir": True,
    "glob": {"paths": ["/home/u/Library/LaunchAgents/ai.hermes.gateway-bad.plist"]},
    "read_plist": exc("OSError")}})
add("legacy_launchd_labels_for_install", {"exclude": [], "seams": {
    "pw_dir": "/home/u", "root": "/hermes", "agents_isdir": True,
    "glob": {"paths": ["/home/u/Library/LaunchAgents/ai.hermes.gateway-out.plist"]},
    "read_plist": {"Label": "ai.hermes.gateway-out", "EnvironmentVariables": {"HERMES_HOME": "/other"}},
    "relative_to": exc("ValueError")}})
add("legacy_launchd_labels_for_install", {"exclude": [], "seams": {"pw_dir": exc("OSError")}})

# ---------------- prepare_service_launcher ----------------
add("prepare_service_launcher", {"system": False, "seams": {
    "get_hermes_home": "/h", "set_override": "tok", "publishes": True,
    "ensure_launchers": {"paths": ["/p1", "/p2"]}, "reset_override": None}})
add("prepare_service_launcher", {"system": False, "seams": {
    "get_hermes_home": "/h", "set_override": "tok", "publishes": False, "reset_override": None}})
add("prepare_service_launcher", {"system": False, "seams": {
    "get_hermes_home": "/h", "set_override": "tok", "publishes": True,
    "ensure_launchers": {"paths": ["/p1"]}, "reset_override": None}})
add("prepare_service_launcher", {"system": True, "run_as_user": "bob", "seams": {
    "get_hermes_home": "/h", "sys_identity": {"values": ["bob", "bob", "/home/bob", 1000]},
    "remap_path": "/remapped", "target_home": "/home/bob/.hermes",
    "set_override": "tok", "publishes": True, "ensure_launchers": {"paths": ["/p1", "/p2"]},
    "pwnam_gid": 1000, "chown": None, "reset_override": None}})
add("prepare_service_launcher", {"system": False, "seams": {"get_hermes_home": "/h", "set_override": exc("OSError")}})
add("prepare_service_launcher", {"system": False, "seams": {
    "get_hermes_home": "/h", "set_override": "tok", "publishes": exc("OSError"), "reset_override": None}})
add("prepare_service_launcher", {"system": False, "seams": {
    "get_hermes_home": "/h", "set_override": "tok", "publishes": True,
    "ensure_launchers": exc("ValueError"), "reset_override": None}})
add("prepare_service_launcher", {"system": True, "run_as_user": "bob", "seams": {
    "get_hermes_home": "/h", "sys_identity": {"values": ["bob", "bob", "/home/bob", 1000]},
    "remap_path": "/r", "target_home": "/home/bob/.hermes", "set_override": "tok",
    "publishes": True, "ensure_launchers": {"paths": ["/p1", "/p2"]}, "pwnam_gid": 1000,
    "chown": exc("KeyboardInterrupt"), "reset_override": None}})

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(json.dumps(c, ensure_ascii=False) for c in CASES) + "\n", encoding="utf-8")
counts = {}
for c in CASES: counts[c["func"]] = counts.get(c["func"], 0) + 1
print("wrote %d cases across %d funcs" % (len(CASES), len(counts)))
for k in sorted(counts): print("  %-40s %d" % (k, counts[k]))
