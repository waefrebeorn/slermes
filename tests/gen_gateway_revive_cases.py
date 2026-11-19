#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_revive.jsonl for t_port_gateway_revive.

Each case: {"func": <C port name>, "in": {args..., "io": {seam scripts}}}.
Script shapes: plain value (reused), {"__seq__": [...]} (per-call, last
repeats), {"__raise__": kind, "msg": text} (exception), absent -> None.
Run: python tests/gen_gateway_revive_cases.py
"""
import json
import os

CASES = []


def C(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def SEQ(*vals):
    return {"__seq__": list(vals)}


# ── probe_loop_tick_socket ──
P = "probe_loop_tick_socket"
C(P, pid=100, home="/h", timeout=1.0,
  io={"get_loop_tick_socket_path": "/run/tick.sock", "path.is_socket": True,
      "_ping_loop_tick_witness": True})
C(P, pid=100, home="/h", io={"get_loop_tick_socket_path": "/run/tick.sock",
                            "path.is_socket": True, "_ping_loop_tick_witness": False})
C(P, pid=100, home="/h", io={"get_loop_tick_socket_path": "/run/tick.sock",
                            "path.is_socket": False})
C(P, pid=100, home="/h", io={"get_loop_tick_socket_path": R("RuntimeError", "no home")})
C(P, pid=100, home="/h", io={"get_loop_tick_socket_path": "/s",
                            "path.is_socket": R("OSError", "perm")})
C(P, pid=100, home="/h", io={"get_loop_tick_socket_path": "/s", "path.is_socket": True,
                            "_ping_loop_tick_witness": R("TimeoutError", "timed out")})
C(P, pid=100, home="/h", io={"get_loop_tick_socket_path": "/s", "path.is_socket": True,
                            "_ping_loop_tick_witness": 1})
C(P, pid=100, home=None, io={"get_loop_tick_socket_path": "/s", "path.is_socket": True,
                            "_ping_loop_tick_witness": True})
C(P, pid=100, home="/h", io={"get_loop_tick_socket_path": R("SystemExit", "1")})

# ── probe_loop_tick_tcp ──
P = "probe_loop_tick_tcp"
C(P, port=8080, io={"_ping_loop_tick_witness": True})
C(P, port="8080", io={"_ping_loop_tick_witness": True})
C(P, port=0)
C(P, port=65536)
C(P, port="abc")
C(P, port=None)
C(P, port=-5)
C(P, port=" 12 ", io={"_ping_loop_tick_witness": False})
C(P, port="1_0", io={"_ping_loop_tick_witness": True})
C(P, port=65535, timeout=0.5, io={"_ping_loop_tick_witness": True})

# ── probe_loop_tick_socket_sustained ──
P = "probe_loop_tick_socket_sustained"
SOCK_IO = {"get_loop_tick_socket_path": "/s", "path.is_socket": True}
C(P, pid=7, home="/h", strikes=3, gap_s=0.2,
  io={**SOCK_IO, "_ping_loop_tick_witness": SEQ(False, False, True), "time.sleep": None})
C(P, pid=7, home="/h", strikes=3, gap_s=0.2,
  io={**SOCK_IO, "_ping_loop_tick_witness": SEQ(False, False, False), "time.sleep": None})
C(P, pid=7, home="/h", strikes=3, gap_s=0.2,
  io={"get_loop_tick_socket_path": "/s", "path.is_socket": SEQ(True, False),
      "_ping_loop_tick_witness": SEQ(False), "time.sleep": None})
C(P, pid=7, home="/h", strikes=0)
C(P, pid=7, home="/h", strikes=-2)
C(P, pid=7, home="/h", strikes=2, gap_s=0.2, tcp_port=9000,
  io={"_ping_loop_tick_witness": SEQ(False, True), "time.sleep": None})
C(P, pid=7, home="/h", strikes=2, gap_s=0,
  io={**SOCK_IO, "_ping_loop_tick_witness": SEQ(False, False)})
C(P, pid=7, home="/h", strikes=2.9, gap_s=0.2,
  io={**SOCK_IO, "_ping_loop_tick_witness": SEQ(False, True), "time.sleep": None})
C(P, pid=7, home="/h", strikes=2, gap_s=0.2, tcp_port=9000,
  io={"_ping_loop_tick_witness": SEQ(1, True), "time.sleep": None})
C(P, pid=7, home="/h", strikes=None)

# ── parse_launchd_pid_from_print_output ──
P = "parse_launchd_pid_from_print_output"
C(P, output="pid = 123\n")
C(P, output="state = running\npid = 42\npid = 99")
C(P, output="  pid = 7  \r\nother")
C(P, output="pid = 0")
C(P, output="pid = -3")
C(P, output="pid = abc")
C(P, output="no pid here")
C(P, output="pid = \n")
C(P, output="pid = 1_0")
C(P, output="\x0bpid = 5\x0c")
C(P, output="pid = 9\rpid = 8")

# ── launchd_print_service_pid ──
P = "launchd_print_service_pid"
C(P, domain="gui/501", label="com.hermes.gw",
  io={"subprocess.run": {"returncode": 0, "stdout": "pid = 55\n"}})
C(P, domain="gui/501", label="com.hermes.gw",
  io={"subprocess.run": {"returncode": 1, "stdout": ""}})
C(P, domain="gui/501", label="com.hermes.gw",
  io={"subprocess.run": R("FileNotFoundError", "launchctl")})
C(P, domain="gui/501", label="com.hermes.gw",
  io={"subprocess.run": R("TimeoutExpired", "wedged")})
C(P, domain="gui/501", label="com.hermes.gw",
  io={"subprocess.run": {"returncode": 0, "stdout": "no pid here"}})
C(P, domain="gui/501", label="com.hermes.gw",
  io={"subprocess.run": R("SystemExit", "1")})
C(P, domain="gui/501", label="com.hermes.gw",
  io={"subprocess.run": {"returncode": 0, "stdout": "pid = 0"}})

# ── reaper_candidate_is_supervisor_owned ──
P = "reaper_candidate_is_supervisor_owned"
C(P, pid=100, io={"is_windows": False})
C(P, pid=100, io={"is_windows": True, "process.parent": SEQ(10, 20, None),
                  "process.name": SEQ("cmd.exe", "services.exe")})
C(P, pid=100, io={"is_windows": True, "process.parent": SEQ(None)})
C(P, pid=100, io={"is_windows": True,
                  "process.parent": SEQ(*range(10, 23)),
                  "process.name": SEQ(*(["x.exe"] * 12))})
C(P, pid=100, io={"is_windows": True, "process.parent": SEQ(10, 20, None),
                  "process.name": SEQ(R("RuntimeError", "denied"), "services.exe")})
C(P, pid=100, io={"is_windows": True, "process.parent": R("RuntimeError", "no proc")})
C(P, pid=100, io={"is_windows": True, "process.parent": SEQ(10, R("RuntimeError", "gone")),
                  "process.name": SEQ("x.exe")})
C(P, pid=100, io={"is_windows": True, "process.parent": SEQ(10, None),
                  "process.name": SEQ("SERVICES.EXE")})
C(P, pid=100, io={"is_windows": True, "process.parent": SEQ(10, None),
                  "process.name": SEQ(None)})
C(P, pid=100, io={"is_windows": True, "process.parent": R("SystemExit", "1")})

# ── windows_scheduled_task_supervises ──
P = "windows_scheduled_task_supervises"
for state in ("Running", "Ready", "Queued", "Disabled", "MISSING", None):
    C(P, task_name="HermesGateway", io={"_windows_scheduled_task_state": state})
C(P, task_name="HermesGateway", io={"_windows_scheduled_task_state": R("RuntimeError", "com")})

# ── windows_console_window_attached ──
P = "windows_console_window_attached"
C(P, io={"is_windows": False})
C(P, io={"is_windows": True, "GetConsoleWindow": 123})
C(P, io={"is_windows": True, "GetConsoleWindow": 0})
C(P, io={"is_windows": True, "GetConsoleWindow": R("AttributeError", "no windll")})
C(P, io={"is_windows": True, "GetConsoleWindow": R("OSError", "fail")})
C(P, io={"is_windows": True, "GetConsoleWindow": R("RuntimeError", "boom")})

# ── windows_gateway_breakaway_state ──
P = "windows_gateway_breakaway_state"
C(P, io={"is_windows": False})
C(P, io={"is_windows": True, "os.environ.pop": "1"})
C(P, io={"is_windows": True, "os.environ.pop": "0"})
C(P, io={"is_windows": True, "os.environ.pop": "2"})
C(P, io={"is_windows": True, "os.environ.pop": None})
C(P, io={"is_windows": True, "os.environ.pop": R("RuntimeError", "env")})

# ── launchctl_label_supervising_process ──
P = "launchctl_label_supervising_process"
C(P, label="com.hermes.gw", io={"_launchctl_supervised_pid": 7})
C(P, label="com.hermes.gw", io={"_launchctl_supervised_pid": None})
C(P, label="com.hermes.gw", io={"_launchctl_supervised_pid": 0})
C(P, label="com.hermes.gw", io={"_launchctl_supervised_pid": R("TimeoutExpired", "wedged")})
C(P, label="com.hermes.gw", io={"_launchctl_supervised_pid": R("OSError", "fail")})

# ── wait_for_launchd_service_pid ──
P = "wait_for_launchd_service_pid"
OK9 = {"returncode": 0, "stdout": "pid = 9"}
NOPID = {"returncode": 0, "stdout": "state = not running"}
C(P, label="com.hermes.gw", old_pid=5, timeout=10.0, domain="gui/501",
  io={"subprocess.run": OK9, "time.monotonic": SEQ(0)})
C(P, label="com.hermes.gw", old_pid=9, timeout=10.0, domain="gui/501",
  io={"subprocess.run": SEQ(OK9, OK9), "time.monotonic": SEQ(0, 0.6, 10.5),
      "time.sleep": None})
C(P, label="com.hermes.gw", old_pid=None, timeout=10.0, domain="gui/501",
  io={"subprocess.run": SEQ(NOPID, OK9), "time.monotonic": SEQ(0, 0.4),
      "time.sleep": None})
C(P, label="com.hermes.gw", old_pid=5, timeout=0.1, domain="gui/501",
  io={"subprocess.run": {"returncode": 0, "stdout": "pid = 5"},
      "time.monotonic": SEQ(0, 0.5)})
C(P, label="com.hermes.gw", old_pid=5, timeout=10.0, domain="gui/501",
  io={"subprocess.run": R("TimeoutExpired", "wedged"), "time.monotonic": SEQ(0)})
C(P, label="com.hermes.gw", old_pid=5, timeout=0.1, domain="gui/501",
  io={"subprocess.run": {"returncode": 0, "stdout": "pid = 0"},
      "time.monotonic": SEQ(0, 0.5)})
C(P, label="com.hermes.gw", old_pid=None, timeout=10.0, domain="gui/501",
  io={"subprocess.run": OK9, "time.monotonic": SEQ(0)})

# ── named_profile_served_by_running_multiplexer ──
P = "named_profile_served_by_running_multiplexer"
C(P, profile_name=None,
  io={"_current_profile_name": "work", "host_multiplexer_serving": {"profile_label": "default"},
      "normalize_profile_name": SEQ("default", "work")})
C(P, profile_name="default")
C(P, profile_name="")
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": None})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": 7, "recorded_served_profiles": ["Work", "other"],
      "normalize_profile_name": SEQ("work", "work", "other")})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": 7, "recorded_served_profiles": None,
      "explicit_multiplex_flag": True})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": 7, "recorded_served_profiles": None,
      "explicit_multiplex_flag": 1})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": R("RuntimeError", "probe"),
      "logger.debug": None})
C(P, profile_name=None, io={"_current_profile_name": R("RuntimeError", "no profile")})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": R("RuntimeError", "no root")})
C(P, profile_name="work", io={"host_multiplexer_serving": {}})
C(P, profile_name="work",
  io={"host_multiplexer_serving": {"profile_label": "x"},
      "normalize_profile_name": R("ValueError", "profile name cannot be empty")})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": 7, "recorded_served_profiles": ["bad", "work"],
      "normalize_profile_name": SEQ("work", R("ValueError", "profile name cannot be empty")),
      "logger.debug": None})
C(P, profile_name="work",
  io={"host_multiplexer_serving": {"profile_label": "Work"},
      "normalize_profile_name": SEQ("work", "work"),
      "get_default_hermes_root": "/h", "live_default_gateway_pid": None})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": 7, "recorded_served_profiles": [],
      "normalize_profile_name": SEQ("work")})
C(P, profile_name=None, io={"_current_profile_name": "default"})
C(P, profile_name="work",
  io={"host_multiplexer_serving": None, "get_default_hermes_root": "/h",
      "live_default_gateway_pid": R("SystemExit", "1")})

# ── gateway_plugin_keyless_inject_launched_by_classic_cli ──
P = "gateway_plugin_keyless_inject_launched_by_classic_cli"
C(P, runner={"id": "r1"}, live={"source": "telegram"},
  io={"policy_for_source": {"source": "cli"}})
C(P, runner={"id": "r1"}, live={"source": "telegram"},
  io={"policy_for_source": {"source": "gateway"}})
C(P, runner={"id": "r1"}, live={"source": "telegram"}, io={"policy_for_source": None})
C(P, runner={"id": "r1"}, live={"source": "telegram"},
  io={"policy_for_source": R("AuthorityRefusal", "denied")})
C(P, runner={"id": "r1"}, live={"source": "telegram"},
  io={"policy_for_source": R("RuntimeError", "boom")})
C(P, runner={"id": "r1"}, live={})
C(P, runner={"id": "r1"}, live={"source": "telegram"}, io={"policy_for_source": {}})

# ── gateway_session_command_sink_active_sink ──
P = "gateway_session_command_sink_active_sink"
C(P, io={"_SINK.get": {"id": 1}})
C(P, io={"_SINK.get": None})
C(P, io={"_SINK.get": "x"})
C(P, io={"_SINK.get": 0})
C(P, io={"_SINK.get": R("RuntimeError", "ctx")})
# ── gateway_status_scoped_locks_scoped_lock_record_is_stale ──
P = "gateway_status_scoped_locks_scoped_lock_record_is_stale"
C(P, existing={}, existing_pid=None)
C(P, existing={}, existing_pid=5, io={"scoped_lock_pid_is_ours": True, "_pid_exists": False})
C(P, existing={"start_time": 100}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": True, "_get_process_start_time": 200})
C(P, existing={}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": False, "_foreign_owner_is_live": False})
C(P, existing={"scope": "s", "pidns": "ns", "hermes_home": "/h"}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": False, "_foreign_owner_is_live": True, "logger.warning": None})
C(P, existing={"scope": "", "pidns": None, "hermes_home": ""}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": False, "_foreign_owner_is_live": True, "logger.warning": None})
C(P, existing={"start_time": 100, "metadata": {"owner_kind": "process"}}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": True, "_get_process_start_time": 100,
      "_process_is_stopped": True})
C(P, existing={"start_time": 100, "metadata": {}}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": True, "_get_process_start_time": 100,
      "_looks_like_gateway_process": False, "_read_process_cmdline": "python -m hermes"})
C(P, existing={"metadata": {}}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": True, "_get_process_start_time": 100,
      "_looks_like_gateway_process": False, "_read_process_cmdline": None,
      "_record_looks_like_gateway": False})
C(P, existing={"start_time": 100, "metadata": {}}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": True, "_get_process_start_time": 100,
      "_looks_like_gateway_process": False, "_read_process_cmdline": None,
      "_process_is_stopped": False})
C(P, existing={"start_time": 100, "metadata": {}}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": True, "_get_process_start_time": 100,
      "_looks_like_gateway_process": True, "_process_is_stopped": False})
C(P, existing={}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": R("RuntimeError", "probe")})
C(P, existing={"metadata": None}, existing_pid=5,
  io={"scoped_lock_pid_is_ours": True, "_pid_exists": True, "_get_process_start_time": None,
      "_looks_like_gateway_process": False, "_read_process_cmdline": None,
      "_record_looks_like_gateway": True, "_process_is_stopped": False})

# ── gateway_systemd_stop_mark_resolve_target_pid ──
P = "gateway_systemd_stop_mark_resolve_target_pid"
C(P, argv=["hermes", "123"])
C(P, argv=["hermes"], io={"os.environ.get": "456"})
C(P, argv=["hermes", ""], io={"os.environ.get": None})
C(P, argv=["hermes", "0"])
C(P, argv=["hermes", "-5"])
C(P, argv=["hermes", " 78 "])
C(P, argv=["hermes", "abc"])
C(P, argv=["hermes", 123])
C(P, argv=[], io={"os.environ.get": "9"})
C(P, argv=["hermes", "1_0"])
C(P, argv=["hermes"], io={"os.environ.get": R("RuntimeError", "env")})

# ── gateway_session_skin_resolved_skin ──
P = "gateway_session_skin_resolved_skin"
SKIN = {"name": "dark", "colors": {"bg": "#000"}, "light_colors": {},
        "dark_colors": {"bg": "#111"}, "branding": {"help_header": "H"},
        "banner_logo": "L", "banner_hero": "B", "tool_prefix": "T",
        "custom_css": "body{}"}
SKIN_IO = {"get_hermes_home": "/h", "load_user_config_effective": {"model": {}},
           "init_skin_from_config": None, "get_active_skin": SKIN}
C(P, io=dict(SKIN_IO))
C(P, io={**SKIN_IO, "get_active_skin": {k: v for k, v in SKIN.items() if k != "name"},
         "logger.debug": None})
C(P, io={**SKIN_IO, "get_active_skin": R("RuntimeError", "skin"), "logger.debug": None})
C(P, io={**SKIN_IO, "load_user_config_effective": R("RuntimeError", "cfg"), "logger.debug": None})
C(P, io={**SKIN_IO, "get_active_skin": {**SKIN, "branding": None}})
C(P, io={**SKIN_IO, "get_active_skin": {**SKIN, "branding": {}}})
C(P, io={**SKIN_IO, "get_active_skin": {**SKIN, "branding": "x"}, "logger.debug": None})
C(P, io={**SKIN_IO, "init_skin_from_config": R("ValueError", "bad"), "logger.debug": None})
C(P, io={**SKIN_IO, "get_hermes_home": "/h/"})
C(P, io={**SKIN_IO, "get_active_skin": None, "logger.debug": None})
C(P, io={**SKIN_IO, "load_user_config_effective": R("SystemExit", "1")})

# ── hermes_cli_gateway_side_connection_for ──
P = "hermes_cli_gateway_side_connection_for"
CONN = {"final": False, "id": "c1"}
C(P, cli={"_gateway_session_open": True}, io={"gateway_store": None})
C(P, cli={"_gateway_session_open": True}, io={"gateway_store": {"connection": CONN}})
C(P, cli={"_gateway_session_open": False}, io={"gateway_store": {"connection": CONN}})
C(P, cli={}, io={"gateway_store": {"connection": CONN}})
C(P, cli={"_gateway_session_open": True},
  io={"gateway_store": {"connection": {"final": True, "id": "c1"}}})
C(P, cli={"_gateway_session_open": True}, io={"gateway_store": {}})
C(P, cli={"_gateway_session_open": True}, io={"gateway_store": {"connection": {}}})
C(P, cli={"_gateway_session_open": True}, io={"gateway_store": R("RuntimeError", "store")})
C(P, cli=None, io={"gateway_store": {"connection": CONN}})

# ── warm_turn_machinery_sync ──
P = "warm_turn_machinery_sync"
CFG = {"agent": {"environment_probe": True}}
CTX = {"model": "m", "context_length": 1000, "context_source": "detected"}
W_IO = {"load_config_readonly": CFG, "_get_platform_tools": ["b", "a"],
        "get_tool_definitions": [{}, {}], "get_environment_probe_line": "line",
        "_resolve_gateway_model_context": CTX, "logger.info": None}
C(P, io=dict(W_IO))
C(P, io={**W_IO, "load_config_readonly": SEQ(R("RuntimeError", "cfg"), {"agent": {}}),
         "get_tool_definitions": [{}, {}, {}], "logger.debug": None})
C(P, io={**W_IO, "load_config_readonly": {}})
C(P, io={**W_IO, "load_config_readonly": {"agent": {"environment_probe": False}}})
C(P, io={**W_IO, "load_config_readonly": {"agent": {"environment_probe": None}}})
C(P, io={**W_IO, "load_config_readonly": {"agent": "x"}})
C(P, io={**W_IO, "_resolve_gateway_model_context": R("RuntimeError", "ctx"),
         "logger.debug": None})
C(P, io={**W_IO, "_resolve_gateway_model_context": {"context_length": 1, "context_source": "s"},
         "logger.debug": None})
C(P, io={**W_IO, "get_environment_probe_line": R("RuntimeError", "probe")})
C(P, io={**W_IO, "load_config_readonly": SEQ(CFG, R("RuntimeError", "cfg2"))})
C(P, io={**W_IO, "get_tool_definitions": None})
C(P, io={**W_IO, "_get_platform_tools": [1, "a"], "logger.debug": None})
C(P, io={**W_IO, "load_config_readonly": R("SystemExit", "1")})

# ── active_deferred_agent_worker_count ──
P = "active_deferred_agent_worker_count"
C(P, runner={"_deferred_agent_workers": []})
C(P, runner={})
C(P, runner={"_deferred_agent_workers": 5})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"},
                                          {"future": "f2", "agent": "a2"},
                                          {"future": "f3", "agent": "a3"}]},
  io={"future.done": SEQ(False, True, False)})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"},
                                          {"future": "f2", "agent": "a2"}]},
  io={"future.done": SEQ(True, True)})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"}]},
  io={"future.done": R("RuntimeError", "done")})
C(P, runner={"_deferred_agent_workers": None})

# ── interrupt_deferred_agent_workers ──
P = "interrupt_deferred_agent_workers"
C(P, runner={"_deferred_agent_workers": []}, reason="shutdown")
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"},
                                          {"future": "f2", "agent": "a2"}]},
  reason="shutdown", io={"future.done": SEQ(False, False),
                         "request_hard_interrupt": SEQ(None, None)})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"},
                                          {"future": "f2", "agent": "a2"}]},
  reason="shutdown", io={"future.done": SEQ(True, False),
                         "request_hard_interrupt": SEQ(None)})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": None}]},
  reason="shutdown", io={"future.done": SEQ(False)})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"},
                                          {"future": "f2", "agent": "a1"}]},
  reason="shutdown", io={"future.done": SEQ(False, False),
                         "request_hard_interrupt": SEQ(None)})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"},
                                          {"future": "f2", "agent": "a2"}]},
  reason="shutdown", io={"future.done": SEQ(False, False),
                         "request_hard_interrupt": SEQ(R("RuntimeError", "nope"), None),
                         "logger.debug": None})
C(P, runner={"_deferred_agent_workers": "x"}, reason="shutdown")
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"}]},
  reason="shutdown", io={"future.done": SEQ(False),
                         "request_hard_interrupt": R("SystemExit", "1")})
C(P, runner={"_deferred_agent_workers": [{"future": "f1", "agent": "a1"}]},
  reason="shutdown", io={"future.done": R("RuntimeError", "done")})

# ── redeliver_failed_obligations_for_platform ──
P = "redeliver_failed_obligations_for_platform"
R1 = {"obligation_id": "o1", "last_error": "timeout"}
R2 = {"obligation_id": "o2"}
C(P, platform="telegram", io={"ledger_enabled": False})
C(P, platform="telegram", io={"ledger_enabled": R("RuntimeError", "db"), "logger.debug": None})
C(P, platform="telegram", io={"ledger_enabled": True, "sweep_failed_for_runtime": []})
C(P, platform="telegram", profile="p1",
  io={"ledger_enabled": True, "sweep_failed_for_runtime": [R1, R2],
      "_clear_resume_pending_for_claimed_obligations": [R1],
      "_release_runtime_claim_quiet": None, "_redeliver_claimed_obligations": 1})
C(P, platform="telegram", io={"ledger_enabled": True,
                              "sweep_failed_for_runtime": R("RuntimeError", "sweep"),
                              "logger.debug": None})
C(P, platform="telegram",
  io={"ledger_enabled": True,
      "sweep_failed_for_runtime": {"__raise__": "CancelledError", "msg": "", "owned": [R1]},
      "release_runtime_claim": None})
C(P, platform="telegram", io={"ledger_enabled": R("CancelledError", "")})
C(P, platform="telegram",
  io={"ledger_enabled": True, "sweep_failed_for_runtime": [R1, R2],
      "_clear_resume_pending_for_claimed_obligations": R("CancelledError", ""),
      "release_runtime_claim": None})
C(P, platform="telegram",
  io={"ledger_enabled": True, "sweep_failed_for_runtime": [R1],
      "_clear_resume_pending_for_claimed_obligations": [R1],
      "_redeliver_claimed_obligations": R("ValueError", "send")})
C(P, platform="telegram",
  io={"ledger_enabled": True, "sweep_failed_for_runtime": [R1, R2],
      "_clear_resume_pending_for_claimed_obligations": [],
      "_release_runtime_claim_quiet": SEQ(None, None),
      "_redeliver_claimed_obligations": 0})
C(P, platform="telegram",
  io={"ledger_enabled": True, "sweep_failed_for_runtime": [{"last_error": "x"}],
      "_clear_resume_pending_for_claimed_obligations": []})
C(P, platform="telegram",
  io={"ledger_enabled": True,
      "sweep_failed_for_runtime": R("CancelledError", "")})
C(P, platform="telegram",
  io={"ledger_enabled": True,
      "sweep_failed_for_runtime": {"__raise__": "CancelledError", "msg": "", "owned": [R1]},
      "release_runtime_claim": R("RuntimeError", "release")})
C(P, platform="telegram", io={"ledger_enabled": R("SystemExit", "1")})

# ── instantiate_adapter ──
P = "instantiate_adapter"
SC = {"group_sessions_per_user": True, "thread_sessions_per_user": True}
PLAT = {"value": "telegram"}
A_IO = {"platform_registry.is_registered": True,
        "platform_registry.create_adapter": {"name": "a"},
        "_instantiate_builtin_adapter": {"name": "b"}}
C(P, self_config=SC, platform=PLAT, config={"extra": {}}, io=dict(A_IO))
C(P, self_config=SC, platform=PLAT, config={"extra": {}},
  io={**A_IO, "platform_registry.create_adapter": None, "logger.error": None})
C(P, self_config=SC, platform=PLAT, config={"extra": {}},
  io={**A_IO, "platform_registry.is_registered": False})
C(P, self_config=SC, platform=PLAT, config={"extra": {}},
  io={**A_IO, "platform_registry.is_registered": R("RuntimeError", "reg"),
      "logger.log": None})
C(P, self_config=SC, platform=PLAT, config={"extra": {}},
  io={**A_IO, "platform_registry.create_adapter": R("ValueError", "deps"),
      "logger.log": None})
C(P, self_config=SC, platform=PLAT, config={"extra": {}},
  io={**A_IO, "platform_registry.is_registered": R("SystemExit", "1")})
C(P, self_config=SC, platform=PLAT, config={}, io=dict(A_IO))
C(P, self_config=SC, platform=PLAT, config={"extra": "x"}, io=dict(A_IO))
C(P, self_config={}, platform=PLAT,
  config={"extra": {"group_sessions_per_user": False}}, io=dict(A_IO))
C(P, self_config={"group_sessions_per_user": True}, platform=PLAT,
  config={"extra": {}}, io=dict(A_IO))
C(P, self_config=SC, platform=PLAT, config={"extra": {}},
  io={**A_IO, "platform_registry.create_adapter": None,
      "logger.error": R("RuntimeError", "log"), "logger.log": None})
C(P, self_config=SC, platform=PLAT, config={"extra": {}},
  io={**A_IO, "platform_registry.is_registered": False,
      "_instantiate_builtin_adapter": R("RuntimeError", "builtin")})
C(P, self_config=SC, platform=PLAT,
  config={"extra": {"group_sessions_per_user": False, "thread_sessions_per_user": True}},
  io=dict(A_IO))

# ── drain_restart_safe_cron_deliveries ──
P = "drain_restart_safe_cron_deliveries"
C(P, adapters={"a": 1}, loop="L", runner=None, io={"drain_delivery_queue": None})
C(P, adapters=None, loop="L", runner=None)
C(P, adapters={"a": 1}, loop="L", runner={},
  io={"_handoff_watch_scopes": [[None, None]], "get_hermes_home": "/h",
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "drain_delivery_queue": None})
C(P, adapters={"a": 1}, loop="L",
  runner={"_profile_adapters": {"p1": {"b": 2}},
          "_profile_bot_platforms": {"p1": ["telegram"]}},
  io={"_handoff_watch_scopes": [[None, None], ["p1", "/h1"]], "get_hermes_home": "/h",
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "has_credential_platform": True, "drain_delivery_queue": None})
C(P, adapters={"a": 1}, loop="L", runner={"_profile_adapters": {}},
  io={"_handoff_watch_scopes": [[None, None], ["p1", "/h1"]], "get_hermes_home": "/h",
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "drain_delivery_queue": None})
C(P, adapters={"a": 1}, loop="L",
  runner={"_profile_adapters": {"p1": {"b": 2}},
          "_profile_bot_platforms": {"p1": ["telegram"]}},
  io={"_handoff_watch_scopes": [["p1", "/h1"]],
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "has_credential_platform": False,
      "shared_route_adapters_for_current_home": {"s": 1},
      "drain_delivery_queue": None})
C(P, adapters={"a": 1}, loop="L",
  runner={"_profile_adapters": {"p1": {"b": 2}}},
  io={"_handoff_watch_scopes": [["p1", "/h1"]],
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "has_credential_platform": False,
      "shared_route_adapters_for_current_home": None,
      "drain_delivery_queue": None})
C(P, adapters={"a": 1}, loop="L", runner={},
  io={"_handoff_watch_scopes": [[None, ""]], "get_hermes_home": "/h",
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "drain_delivery_queue": None})
C(P, adapters=None, loop="L", runner={"_profile_adapters": {"p1": {"b": 2}}},
  io={"_handoff_watch_scopes": [[None, None], ["p1", "/h1"]],
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "drain_delivery_queue": None})
C(P, adapters={"a": 1}, loop="L", runner={},
  io={"_handoff_watch_scopes": [[None, None]], "get_hermes_home": "/h",
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "drain_delivery_queue": R("RuntimeError", "drain")})
C(P, adapters={"a": 1}, loop="L",
  runner={"_profile_adapters": {"p1": {"b": 2}}, "_profile_bot_platforms": None},
  io={"_handoff_watch_scopes": [["p1", "/h1"]],
      "profile_runtime_scope.enter": None, "profile_runtime_scope.exit": None,
      "has_credential_platform": True, "drain_delivery_queue": None})
C(P, adapters={"a": 1}, loop="L", runner={},
  io={"_handoff_watch_scopes": R("RuntimeError", "scopes")})


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "fixtures", "gateway_revive.jsonl")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), out))
    low = {k: v for k, v in counts.items() if v < 5}
    if low:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % low)
    for k in sorted(counts):
        print("  %-60s %d" % (k, counts[k]))


if __name__ == "__main__":
    main()
