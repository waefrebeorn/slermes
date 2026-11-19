#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_cli_flow.jsonl for the gateway_cli_flow
FLOW oracle (hermes_cli/gateway.py). >=5 cases per ported function, exercising
decision branches, error paths, BaseException propagation and fail-closed
missing scripts. Script semantics match the harness/oracle: array = per-call
queue (last repeats), single value = constant, missing = None,
{"__raise__": cls, "msg": ...} = exception. ASCII / POSIX-lexical domain.
"""
import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_cli_flow.jsonl")

HOME = "/home/u/.hermes"
UNIT = "/etc/systemd/system/hermes-gateway.service"
DROPIN = "/etc/systemd/system/hermes-gateway.service.d/20-replace.conf"
G = {"home": "/home/u/.hermes", "pid": 100, "profile_label": "default"}
G2 = {"home": "/home/other/.hermes", "pid": 200, "profile_label": "alice"}


def cpe(rc=1, cmd="systemctl start hermes-gateway"):
    e = subprocess.CalledProcessError(rc, cmd)
    return {"__raise__": "CalledProcessError", "msg": str(e), "returncode": rc, "cmd": cmd}


def toe(cmd="systemctl", timeout=90):
    e = subprocess.TimeoutExpired(cmd, timeout)
    return {"__raise__": "TimeoutExpired", "msg": str(e), "cmd": cmd, "timeout": timeout}


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


CASES = []


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# 1. _parse_kv_pairs (pure)
add("parse_kv_pairs", {"items": ["A=1", "B=2"]})
add("parse_kv_pairs", {"items": ["A=1", "A=2"]})
add("parse_kv_pairs", {"items": ["NOEQ", "X= y "]})
add("parse_kv_pairs", {"items": []})
add("parse_kv_pairs", {"items": ["K="]})
add("parse_kv_pairs", {"items": ["A=b=c"]})
add("parse_kv_pairs", {"items": ["=v"]})

# 2. _systemd_cli_bits (pure)
add("systemd_cli_bits", {"system": True})
add("systemd_cli_bits", {"system": False})
add("systemd_cli_bits", {"system": True})
add("systemd_cli_bits", {"system": False})
add("systemd_cli_bits", {"system": True})

# 3. _runtime_state_pid (seam positive_pid)
add("runtime_state_pid", {"state": {"pid": 5}}, {"positive_pid": 5})
add("runtime_state_pid", {"state": {"pid": "7"}}, {"positive_pid": 7})
add("runtime_state_pid", {"state": {}}, {"positive_pid": None})
add("runtime_state_pid", {"state": None}, {"positive_pid": None})
add("runtime_state_pid", {"state": {"pid": 0}}, {"positive_pid": None})
add("runtime_state_pid", {"state": {"pid": -3}}, {"positive_pid": None})
add("runtime_state_pid", {"state": {"pid": "abc"}}, {"positive_pid": None})

# 4. _host_decision_exit_code (pure)
add("host_decision_exit_code", {"decision": {"transient": True}})
add("host_decision_exit_code", {"decision": {"transient": False}})
add("host_decision_exit_code", {"decision": {}})
add("host_decision_exit_code", {"decision": None})
add("host_decision_exit_code", {"decision": {"transient": True, "outcome": "refuse"}})
add("host_decision_exit_code", {"decision": {"outcome": "attach"}})

# 5. _print_info_lines (seam print_info)
add("print_info_lines", {"lines": ["a", "b"]})
add("print_info_lines", {"lines": []})
add("print_info_lines", {"lines": ["one"]})
add("print_info_lines", {"lines": ["x", "y", "z"]})
add("print_info_lines", {"lines": [""]})
add("print_info_lines", {"lines": ["a"]}, {"print_info": R("RuntimeError")})

# 6. _print_indented (seam emit)
add("print_indented", {"text": "a\nb"})
add("print_indented", {"text": ""})
add("print_indented", {"text": "single"})
add("print_indented", {"text": "a\n"})
add("print_indented", {"text": "a\n\nb"})
add("print_indented", {"text": "a\r\nb"})

# 7. _current_profile_name
add("current_profile_name", {}, {"get_hermes_home": HOME, "profile_name_for_home": "alice"})
add("current_profile_name", {}, {"get_hermes_home": HOME, "profile_name_for_home": None, "profile_suffix": "hash"})
add("current_profile_name", {}, {"get_hermes_home": HOME, "profile_name_for_home": "default"})
add("current_profile_name", {}, {"get_hermes_home": HOME, "profile_name_for_home": "", "profile_suffix": "sfx"})
add("current_profile_name", {}, {"get_hermes_home": R("RuntimeError")})
add("current_profile_name", {}, {"get_hermes_home": HOME, "profile_name_for_home": R("ValueError")})

# 8. _systemd_unit_is_active
BASE8 = {"get_systemd_unit_path": UNIT, "get_service_name": "hermes-gateway"}
add("systemd_unit_is_active", {"system": True}, {**BASE8, "path_exists": False})
add("systemd_unit_is_active", {"system": True}, {**BASE8, "path_exists": True, "run_systemctl": {"stdout": "active\n", "returncode": 0}})
add("systemd_unit_is_active", {"system": False}, {**BASE8, "path_exists": True, "run_systemctl": {"stdout": "inactive\n", "returncode": 0}})
add("systemd_unit_is_active", {"system": True}, {**BASE8, "path_exists": True, "run_systemctl": {"stdout": "  active  ", "returncode": 0}})
add("systemd_unit_is_active", {"system": True}, {**BASE8, "path_exists": True, "run_systemctl": R("RuntimeError")})
add("systemd_unit_is_active", {"system": True}, {**BASE8, "path_exists": True, "run_systemctl": toe()})
add("systemd_unit_is_active", {"system": True}, {**BASE8, "path_exists": True, "run_systemctl": R("ValueError")})

# 9. _systemctl_show
BASE9 = {"get_service_name": "hermes-gateway", "select_systemd_scope": False}
add("systemctl_show", {"properties": ["ActiveState", "MainPID"], "system": False}, {**BASE9, "run_systemctl": {"stdout": "A=1\nB=2\n", "returncode": 0}})
add("systemctl_show", {"properties": ["A"], "system": False}, {**BASE9, "run_systemctl": {"stdout": "A=1\n", "returncode": 1}})
add("systemctl_show", {"properties": ["A"], "system": True}, {**BASE9, "select_systemd_scope": True, "run_systemctl": R("OSError")})
add("systemctl_show", {"properties": ["A"], "system": False}, {**BASE9, "run_systemctl": R("RuntimeError")})
add("systemctl_show", {"properties": [], "system": False}, {**BASE9, "run_systemctl": {"stdout": "", "returncode": 0}})
add("systemctl_show", {"properties": ["A"], "system": False}, {**BASE9, "get_service_name": R("RuntimeError")})
add("systemctl_show", {"properties": ["A", "B"], "system": False}, {**BASE9, "run_systemctl": {"stdout": "X= y \nZ=3", "returncode": 0}})

# 10. _systemd_unit_installed
add("systemd_unit_installed", {}, {"supports_systemd_services": False})
add("systemd_unit_installed", {}, {"supports_systemd_services": True, "get_systemd_unit_path": UNIT, "path_exists": [False, False]})
add("systemd_unit_installed", {}, {"supports_systemd_services": True, "get_systemd_unit_path": UNIT, "path_exists": [True], "foreign_systemd_unit_home": [None]})
add("systemd_unit_installed", {}, {"supports_systemd_services": True, "get_systemd_unit_path": UNIT, "path_exists": [True, True], "foreign_systemd_unit_home": ["other", None]})
add("systemd_unit_installed", {}, {"supports_systemd_services": True, "get_systemd_unit_path": UNIT, "path_exists": [False, True], "foreign_systemd_unit_home": [None]})
add("systemd_unit_installed", {}, {"supports_systemd_services": R("RuntimeError")})

# 11. _agent_timeout_setting
AV = {"env_var": "HERMES_CRON_DRAIN_TIMEOUT", "key": "cron_drain_timeout"}
add("agent_timeout_setting", AV, {"os_getenv": "5", "parse": 5})
add("agent_timeout_setting", AV, {"os_getenv": None, "read_raw_config": {"agent": {"cron_drain_timeout": 7}}, "parse": 7})
add("agent_timeout_setting", AV, {"os_getenv": None, "read_raw_config": {"agent": {}}, "parse": 0})
add("agent_timeout_setting", AV, {"os_getenv": None, "read_raw_config": None, "parse": 0})
add("agent_timeout_setting", AV, {"os_getenv": "  ", "read_raw_config": {"agent": {"cron_drain_timeout": 9}}, "parse": 9})
add("agent_timeout_setting", AV, {"os_getenv": None, "read_raw_config": {"agent": "notdict"}, "parse": 0})
add("agent_timeout_setting", AV, {"os_getenv": None, "read_raw_config": {"agent": {"cron_drain_timeout": None}}, "parse": 0})

# 12. _systemd_scope_preamble
add("systemd_scope_preamble", {"action": "start", "system": True, "require_installed": True}, {"select_systemd_scope": True})
add("systemd_scope_preamble", {"action": "start", "system": False, "require_installed": True, "preflight_user": True}, {"select_systemd_scope": False})
add("systemd_scope_preamble", {"action": "stop", "system": False, "require_installed": False, "preflight_user": False}, {"select_systemd_scope": False})
add("systemd_scope_preamble", {"action": "start", "system": True, "require_installed": False}, {"select_systemd_scope": True})
add("systemd_scope_preamble", {"action": "start", "system": True}, {"select_systemd_scope": True, "require_root_for_system_service": R("RuntimeError")})
add("systemd_scope_preamble", {"action": "start", "system": False, "preflight_user": True}, {"select_systemd_scope": False, "preflight_user_systemd": R("RuntimeError")})
add("systemd_scope_preamble", {"action": "start", "system": False}, {"select_systemd_scope": False, "require_service_installed": R("SystemExit", "1")})

# 13. _retire_hermes_replace_dropin
FULL = "Added to end the gateway respawn storm\nExecStart=x --replace\n"
add("retire_hermes_replace_dropin", {"system": True}, {"get_systemd_unit_path": UNIT, "read_text": R("OSError")})
add("retire_hermes_replace_dropin", {"system": True}, {"get_systemd_unit_path": UNIT, "read_text": "no tokens here"})
add("retire_hermes_replace_dropin", {"system": True}, {"get_systemd_unit_path": UNIT, "read_text": FULL})
add("retire_hermes_replace_dropin", {"system": False}, {"get_systemd_unit_path": UNIT, "read_text": "Added to end the gateway respawn storm --replace"})
add("retire_hermes_replace_dropin", {"system": True}, {"get_systemd_unit_path": UNIT, "read_text": FULL, "unlink": R("OSError")})
add("retire_hermes_replace_dropin", {"system": True}, {"get_systemd_unit_path": UNIT, "read_text": FULL, "unlink": None})

# 14. host_multiplexer_serving
add("host_multiplexer_serving", {"profile_name": "alice"}, {"host_gateway_serving": None})
add("host_multiplexer_serving", {"profile_name": "alice"}, {"host_gateway_serving": G, "get_hermes_home": HOME, "launched_by_other_tenant": False})
add("host_multiplexer_serving", {"profile_name": "alice"}, {"host_gateway_serving": G, "get_hermes_home": HOME, "launched_by_other_tenant": True, "gateway.describe": "PID 100"})
add("host_multiplexer_serving", {"profile_name": None}, {"get_hermes_home": HOME, "profile_name_for_home": "bob", "host_gateway_serving": G, "launched_by_other_tenant": False})
add("host_multiplexer_serving", {"profile_name": "alice"}, {"host_gateway_serving": R("RuntimeError")})
add("host_multiplexer_serving", {"profile_name": "alice"}, {"host_gateway_serving": R("SystemExit", "1")})

# 15. _served_by_another_host_gateway
BASE15 = {"host_gateway_serving": G, "get_hermes_home": HOME, "launched_by_other_tenant": False}
add("served_by_another_host_gateway", {"profile_name": "alice"}, {"host_gateway_serving": None})
add("served_by_another_host_gateway", {"profile_name": "alice"}, {**BASE15, "get_process_hermes_home": HOME, "same_hermes_home": True})
add("served_by_another_host_gateway", {"profile_name": "alice"}, {**BASE15, "get_process_hermes_home": "/other", "same_hermes_home": False})
add("served_by_another_host_gateway", {"profile_name": "alice"}, {**BASE15, "get_process_hermes_home": R("RuntimeError")})
add("served_by_another_host_gateway", {"profile_name": "alice"}, {**BASE15, "get_process_hermes_home": HOME, "same_hermes_home": R("SystemExit", "1")})

# 16. _host_multiplexer_for_all_verb
add("host_multiplexer_for_all_verb", {}, {"host_gateway": G})
add("host_multiplexer_for_all_verb", {}, {"host_gateway": None})
add("host_multiplexer_for_all_verb", {}, {"host_gateway": R("RuntimeError")})
add("host_multiplexer_for_all_verb", {}, {"host_gateway": R("SystemExit", "1")})
add("host_multiplexer_for_all_verb", {}, {"host_gateway": G2})

# 17. _host_multiplexer_is_ours
add("host_multiplexer_is_ours", {"owner": G}, {"get_process_hermes_home": HOME, "same_hermes_home": True})
add("host_multiplexer_is_ours", {"owner": G}, {"get_process_hermes_home": "/other", "same_hermes_home": False})
add("host_multiplexer_is_ours", {"owner": None}, {})
add("host_multiplexer_is_ours", {"owner": G}, {"get_process_hermes_home": R("RuntimeError")})
add("host_multiplexer_is_ours", {"owner": G}, {"get_process_hermes_home": HOME, "same_hermes_home": R("SystemExit", "1")})
add("host_multiplexer_is_ours", {"owner": G}, {"get_process_hermes_home": HOME, "same_hermes_home": R("ValueError")})

# 18. _print_unfolded_gateway_note
add("print_unfolded_gateway_note", {"owner": {"pid": 100}}, {"find_gateway_pids": [[100]]})
add("print_unfolded_gateway_note", {"owner": {"pid": 100}}, {"find_gateway_pids": [[100, 200, 300]]})
add("print_unfolded_gateway_note", {"owner": {"pid": 100}}, {"find_gateway_pids": [[]]})
add("print_unfolded_gateway_note", {"owner": None}, {})
add("print_unfolded_gateway_note", {"owner": {"pid": 100}}, {"find_gateway_pids": R("RuntimeError")})
add("print_unfolded_gateway_note", {"owner": {"pid": 100}}, {"find_gateway_pids": R("SystemExit", "1")})
add("print_unfolded_gateway_note", {"owner": {"pid": 100}}, {"find_gateway_pids": [[200]]})

# 19. _stop_host_multiplexer
add("stop_host_multiplexer", {"owner": G}, {"kill_gateway_processes": 3})
add("stop_host_multiplexer", {"owner": G}, {"kill_gateway_processes": 0, "terminate_pid": None})
add("stop_host_multiplexer", {"owner": G}, {"kill_gateway_processes": 0, "terminate_pid": R("ProcessLookupError")})
add("stop_host_multiplexer", {"owner": G}, {"kill_gateway_processes": 0, "terminate_pid": R("PermissionError")})
add("stop_host_multiplexer", {"owner": G}, {"kill_gateway_processes": 0, "terminate_pid": R("OSError")})
add("stop_host_multiplexer", {"owner": None}, {"kill_gateway_processes": 0})
add("stop_host_multiplexer", {"owner": G}, {"kill_gateway_processes": 0, "terminate_pid": R("ValueError")})
add("stop_host_multiplexer", {"owner": G}, {"kill_gateway_processes": R("RuntimeError")})

# 20. _print_multiplex_standalone_reason
add("print_multiplex_standalone_reason", {}, {"recorded_standalone_warning_lines": [["a", "b"]]})
add("print_multiplex_standalone_reason", {}, {"recorded_standalone_warning_lines": [[]]})
add("print_multiplex_standalone_reason", {}, {"recorded_standalone_warning_lines": [["one"]]})
add("print_multiplex_standalone_reason", {}, {"recorded_standalone_warning_lines": R("RuntimeError")})
add("print_multiplex_standalone_reason", {}, {"recorded_standalone_warning_lines": R("SystemExit", "1")})
add("print_multiplex_standalone_reason", {}, {"recorded_standalone_warning_lines": [["a"]], "print": R("RuntimeError")})

# 21. _print_served_ingress_urls
add("print_served_ingress_urls", {"profile": None}, {"served_profile_ingress_urls": None})
add("print_served_ingress_urls", {"profile": None}, {"served_profile_ingress_urls": {}})
add("print_served_ingress_urls", {"profile": None}, {"served_profile_ingress_urls": {"telegram": {"x": 1}}, "format_ingress_url_lines": [["line1"]]})
add("print_served_ingress_urls", {"profile": "p"}, {"served_profile_ingress_urls": {"b": {}, "a": {}}, "format_ingress_url_lines": [["L"]]})
add("print_served_ingress_urls", {"profile": None}, {"served_profile_ingress_urls": R("RuntimeError")})
add("print_served_ingress_urls", {"profile": None}, {"served_profile_ingress_urls": R("SystemExit", "1")})
add("print_served_ingress_urls", {"profile": None}, {"served_profile_ingress_urls": {"t": {}}, "format_ingress_url_lines": R("ValueError")})

# 22. _served_profile_needs_no_service
# served=false -> composes _named_profile_refused (returns false: default profile, no host)
add("served_profile_needs_no_service", {}, {
    "named_profile_served": False, "get_hermes_home": HOME, "profile_name_for_home": "default",
    "host_gateway_serving": None})
# served=true, standalone=true -> rescan message + print_info
add("served_profile_needs_no_service", {}, {
    "named_profile_served": True, "get_hermes_home": HOME, "profile_name_for_home": "alice",
    "profile_is_standalone": True, "standalone_rescan_message": "rescan now"})
# served=true, standalone=false -> print_success + 2 print_info
add("served_profile_needs_no_service", {}, {
    "named_profile_served": True, "get_hermes_home": HOME, "profile_name_for_home": "alice",
    "profile_is_standalone": False})
add("served_profile_needs_no_service", {}, {"named_profile_served": R("RuntimeError")})
add("served_profile_needs_no_service", {}, {
    "named_profile_served": True, "get_hermes_home": HOME, "profile_is_standalone": R("ValueError")})

# 23. _named_profile_refused_under_multiplexer
add("named_profile_refused_under_multiplexer", {"force": True}, {})
add("named_profile_refused_under_multiplexer", {"force": False}, {
    "get_hermes_home": HOME, "profile_name_for_home": "default", "host_gateway_serving": None,
    "named_profile_served": False})
# standalone true + served true -> rescan refusal
add("named_profile_refused_under_multiplexer", {"force": False}, {
    "get_hermes_home": HOME, "profile_name_for_home": "alice", "profile_is_standalone": True,
    "host_gateway_serving": G, "launched_by_other_tenant": False, "get_process_hermes_home": "/other",
    "same_hermes_home": False, "standalone_rescan_message": "rescan now"})
# new_standalone true (not served, no service) -> install refusal prints
add("named_profile_refused_under_multiplexer", {"force": False}, {
    "get_hermes_home": HOME, "profile_name_for_home": "alice", "profile_is_standalone": False,
    "is_service_installed": False, "host_gateway_serving": None, "named_profile_served": False,
    "display_hermes_home": "~/.hermes"})
# not served, service installed -> false
add("named_profile_refused_under_multiplexer", {"force": False}, {
    "get_hermes_home": HOME, "profile_name_for_home": "alice", "profile_is_standalone": False,
    "is_service_installed": True, "host_gateway_serving": None, "named_profile_served": False})
# served via host (standalone false) -> served refusal prints
add("named_profile_refused_under_multiplexer", {"force": False}, {
    "get_hermes_home": HOME, "profile_name_for_home": "alice", "profile_is_standalone": False,
    "is_service_installed": True, "host_gateway_serving": G, "launched_by_other_tenant": False,
    "get_process_hermes_home": "/other", "same_hermes_home": False, "gateway.describe": "PID 100",
    "display_hermes_home": "~/.hermes"})
add("named_profile_refused_under_multiplexer", {"force": False}, {"get_hermes_home": R("RuntimeError")})
add("named_profile_refused_under_multiplexer", {"force": False}, {
    "get_hermes_home": HOME, "profile_name_for_home": R("SystemExit", "1")})

# 24. _attach_to_host_gateway_or_guard
add("attach_to_host_gateway_or_guard", {"force": True}, {})
add("attach_to_host_gateway_or_guard", {"force": False, "replace": True}, {
    "get_hermes_home": HOME, "decide": {"outcome": "replace-host", "message": "", "transient": False}})
add("attach_to_host_gateway_or_guard", {"force": False}, {
    "get_hermes_home": HOME, "decide": {"outcome": "attach", "message": "attached", "transient": True},
    "running_under_gateway_supervisor": False})
add("attach_to_host_gateway_or_guard", {"force": False}, {
    "get_hermes_home": HOME, "decide": {"outcome": "attach", "message": "attached", "transient": True},
    "running_under_gateway_supervisor": True})
add("attach_to_host_gateway_or_guard", {"force": False}, {
    "get_hermes_home": HOME, "decide": {"outcome": "refuse", "message": "refused", "transient": False}})
add("attach_to_host_gateway_or_guard", {"force": False}, {
    "get_hermes_home": HOME, "decide": {"outcome": "refuse", "message": "refused", "transient": True}})
add("attach_to_host_gateway_or_guard", {"force": False}, {"get_hermes_home": HOME, "decide": None})
add("attach_to_host_gateway_or_guard", {"force": False}, {"get_hermes_home": HOME, "decide": R("RuntimeError")})
add("attach_to_host_gateway_or_guard", {"force": False}, {"get_hermes_home": HOME, "decide": R("SystemExit", "1")})
add("attach_to_host_gateway_or_guard", {"force": False}, {
    "get_hermes_home": HOME, "decide": None, "guard_named_profile_under_multiplexer": R("SystemExit", "78")})

# 25. _systemd_reset_and_run
BASE25 = {"get_service_name": "hermes-gateway"}
add("systemd_reset_and_run", {"action": "start", "system": False, "previous_pid": 100}, {
    **BASE25, "run_systemctl": [{"returncode": 0}, {"returncode": 0}]})
add("systemd_reset_and_run", {"action": "start", "system": False, "previous_pid": 100}, {
    **BASE25, "run_systemctl": [{"returncode": 0}, cpe()], "systemd_error_indicates_start_limit": True})
add("systemd_reset_and_run", {"action": "restart", "system": True, "previous_pid": None}, {
    **BASE25, "run_systemctl": [{"returncode": 0}, cpe()], "systemd_error_indicates_start_limit": False,
    "systemd_service_is_start_limited": True})
add("systemd_reset_and_run", {"action": "start", "system": False, "previous_pid": 100}, {
    **BASE25, "run_systemctl": [{"returncode": 0}, cpe()], "systemd_error_indicates_start_limit": False,
    "systemd_service_is_start_limited": False})
add("systemd_reset_and_run", {"action": "start", "system": False, "previous_pid": 100}, {
    **BASE25, "run_systemctl": [{"returncode": 0}, toe()], "service_scope_label": "user"})
add("systemd_reset_and_run", {"action": "start", "system": False}, {
    **BASE25, "run_systemctl": R("RuntimeError")})
add("systemd_reset_and_run", {"action": "start", "system": False}, {
    **BASE25, "run_systemctl": [{"returncode": 0}, R("ValueError")]})
add("systemd_reset_and_run", {"action": "start", "system": False, "previous_pid": 100}, {
    **BASE25, "run_systemctl": [{"returncode": 0}, {"returncode": 0}],
    "wait_for_systemd_service_restart": R("RuntimeError")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func count audit
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    short = {k: v for k, v in cnt.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(cnt), OUT))
    if short:
        print("WARNING funcs with <5 cases: %s" % short)
    else:
        print("all funcs have >=5 cases")


if __name__ == "__main__":
    main()
