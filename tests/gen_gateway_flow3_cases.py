#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_flow3.jsonl (>=5 cases per FLOW func).

Deterministic, scripted-seam cases for the UNCLAIMED-remainder FLOW cores of
hermes_cli/gateway.py ported in src/cli/gateway_flow3.c. A scripted seam value
is either a JSON value (one result for every call) or an array used as a
per-call queue (last repeats). A {"__raise__": cls, "msg": ...} scripts a live
raise of that exact class; BaseException-family classes verify the full
builtin exception hierarchy (except Exception never catches them).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "gateway_flow3.jsonl"
CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


def exc(name, msg="boom"):
    return {"__raise__": name, "msg": msg}


# ---------------- pure string parsers ----------------
for i, line in enumerate([
    "  1234  my cmd  ",
    "grep something",
    "   ",
    "notanint cmd",
    "root 123 0.0 0.1 100 200 ? S 10:00 0:00 /usr/bin/python -m gateway",
    "1_234 cmd",
    "-5 cmd",
    "\t42\t\ttabbed value",
]):
    add("parse_ps_line", {"line": line})

for i, listing in enumerate([
    "CommandLine=/usr/bin/python -m gateway\nProcessId=101\n",
    "ProcessId=1\nCommandLine=first\nProcessId=2\nCommandLine=second\n",
    "ProcessId= 55 \nCommandLine= spaced \n",
    "CommandLine=x\nProcessId=abc\nCommandLine=y\nProcessId=7\n",
    "CommandLine=a=1\nProcessId=9\r\n\nProcessId=10\n",
    "",
]):
    add("iter_windows_list_processes", {"listing": listing})

# ---------------- scm_service_field ----------------
add("scm_service_field", {"service": "svc", "field": "name",
                          "seams": {"scm_getattr": {"callable": True},
                                    "scm_call": "Hermes Gateway"}})
add("scm_service_field", {"service": "svc", "field": "status",
                          "seams": {"scm_getattr": {"callable": False},
                                    "scm_as_dict": {"status": "running", "pid": 7}}})
add("scm_service_field", {"service": "svc", "field": "name",
                          "seams": {"scm_getattr": None,
                                    "scm_as_dict": {"name": "fallback", "pid": 7}}})
add("scm_service_field", {"service": "svc", "field": "missing",
                          "seams": {"scm_getattr": None, "scm_as_dict": {"name": "x"}}})
add("scm_service_field", {"service": "svc", "field": "name",
                          "seams": {"scm_getattr": exc("RuntimeError")}})
add("scm_service_field", {"service": "svc", "field": "name",
                          "seams": {"scm_getattr": {"callable": True},
                                    "scm_call": exc("KeyboardInterrupt")}})

# ---------------- gateway_detached_env ----------------
for i, (v, truth) in enumerate([("1", True), ("0", False), ("yes", True), (None, False)]):
    add("gateway_detached_env", {"seams": {"getenv": v, "truthy_env": truth}})
add("gateway_detached_env", {"seams": {"getenv": exc("OSError"), "truthy_env": True}})
add("gateway_detached_env", {"seams": {"getenv": "1", "truthy_env": exc("ValueError")}})

# ---------------- stdin_is_tty ----------------
add("stdin_is_tty", {"stdin_present": True, "seams": {"stdin_isatty": True}})
add("stdin_is_tty", {"stdin_present": True, "seams": {"stdin_isatty": False}})
add("stdin_is_tty", {"stdin_present": False})
add("stdin_is_tty", {"stdin_present": True, "seams": {"stdin_isatty": exc("ValueError")}})
add("stdin_is_tty", {"stdin_present": True, "seams": {"stdin_isatty": exc("OSError")}})
add("stdin_is_tty", {"stdin_present": True, "seams": {"stdin_isatty": exc("KeyboardInterrupt")}})

# ---------------- home_owns_bare_service_name ----------------
add("home_owns_bare_service_name", {"home": "/home/u/.hermes",
    "seams": {"native_service_homes": [["/home/u/.hermes", "/root/.hermes"]]}})
add("home_owns_bare_service_name", {"home": "/data/h",
    "seams": {"native_service_homes": [["/home/u/.hermes"]],
              "bare_unit_pinned_home": "/data/h"}})
add("home_owns_bare_service_name", {"home": "/elsewhere",
    "seams": {"native_service_homes": [["/home/u/.hermes"]],
              "bare_unit_pinned_home": "/root/.hermes"}})
add("home_owns_bare_service_name", {"home": "/x",
    "seams": {"native_service_homes": exc("RuntimeError")}})
add("home_owns_bare_service_name", {"home": "/x",
    "seams": {"native_service_homes": [], "bare_unit_pinned_home": exc("OSError")}})

# ---------------- cmd_setup / cmd_list ----------------
for i, v in enumerate([None, True, "ok", exc("OSError"), exc("KeyboardInterrupt")]):
    add("cmd_setup", {"args": {"quiet": True}, "seams": {"gateway_setup": v}})
    add("cmd_list", {"args": {}, "seams": {"gateway_list": v}})

# ---------------- cmd_migrate_legacy ----------------
add("cmd_migrate_legacy", {"args": {"dry_run": True, "yes": True},
    "seams": {"supports_systemd_services": True,
              "remove_legacy_hermes_units": None}})
add("cmd_migrate_legacy", {"args": {"dry_run": False, "yes": False},
    "seams": {"supports_systemd_services": False, "is_macos": True,
              "remove_legacy_hermes_units": None}})
add("cmd_migrate_legacy", {"args": {},
    "seams": {"supports_systemd_services": False, "is_macos": False,
              "print": None}})
add("cmd_migrate_legacy", {"args": {"dry_run": True, "yes": True},
    "seams": {"supports_systemd_services": exc("OSError")}})
add("cmd_migrate_legacy", {"args": {"dry_run": True, "yes": True},
    "seams": {"supports_systemd_services": False, "is_macos": exc("ValueError")}})

# ---------------- print_unserved_shared_ingress ----------------
add("print_unserved_shared_ingress", {"profile": "work",
    "seams": {"served_unserved_platforms": {}, "print": None}})
add("print_unserved_shared_ingress", {"profile": "work",
    "seams": {"served_unserved_platforms": {"whatsapp": "not served under multiplex"}, "print": None}})
add("print_unserved_shared_ingress", {"profile": None,
    "seams": {"served_unserved_platforms": {"relay": "shared only on default",
                                            "whatsapp": "disabled here"},
              "print": None}})
add("print_unserved_shared_ingress", {"profile": "work",
    "seams": {"served_unserved_platforms": exc("RuntimeError"), "print": None}})
add("print_unserved_shared_ingress", {"profile": "work",
    "seams": {"served_unserved_platforms": exc("KeyboardInterrupt"), "print": None}})

# ---------------- print_duplicate_credential_warnings ----------------
add("print_duplicate_credential_warnings", {"seams": {"duplicate_credential_findings": []}})
add("print_duplicate_credential_warnings", {"seams": {
    "duplicate_credential_findings": [["telegram token reused", "discord token reused"]], "print": None}})
add("print_duplicate_credential_warnings", {"seams": {
    "duplicate_credential_findings": [["single ⚠ line"]], "print": None}})
add("print_duplicate_credential_warnings", {"seams": {
    "duplicate_credential_findings": exc("ValueError")}})
add("print_duplicate_credential_warnings", {"seams": {
    "duplicate_credential_findings": exc("KeyboardInterrupt")}})

# ---------------- absorb_windows_console_controls ----------------
add("absorb_windows_console_controls", {"seams": {
    "signal": None, "hasattr_sigbreak": True, "set_console_ctrl_handler": None}})
add("absorb_windows_console_controls", {"seams": {
    "signal": None, "hasattr_sigbreak": False, "set_console_ctrl_handler": None}})
add("absorb_windows_console_controls", {"seams": {
    "signal": exc("OSError"), "set_console_ctrl_handler": None}})
add("absorb_windows_console_controls", {"seams": {
    "signal": [None, exc("ValueError")], "hasattr_sigbreak": True,
    "set_console_ctrl_handler": None}})
add("absorb_windows_console_controls", {"seams": {
    "signal": None, "hasattr_sigbreak": True,
    "set_console_ctrl_handler": exc("AttributeError")}})
add("absorb_windows_console_controls", {"seams": {
    "signal": exc("KeyboardInterrupt")}})

# ---------------- ping_loop_tick_witness ----------------
add("ping_loop_tick_witness", {"family": 2, "address": ["127.0.0.1", 9000], "timeout": 0.5,
    "seams": {"socket_new": None, "settimeout": None, "connect": None,
              "recv": [[49]], "close": None}})
add("ping_loop_tick_witness", {"family": 2, "address": ["127.0.0.1", 9000], "timeout": -1.0,
    "seams": {"socket_new": None, "settimeout": None, "connect": None,
              "recv": [[48]], "close": None}})
add("ping_loop_tick_witness", {"family": 2, "address": ["127.0.0.1", 9000], "timeout": 1.0,
    "seams": {"socket_new": None, "settimeout": None,
              "connect": exc("ConnectionRefusedError"), "close": None}})
add("ping_loop_tick_witness", {"family": 2, "address": ["::1", 9000], "timeout": 2.0,
    "seams": {"socket_new": None, "settimeout": exc("OSError"), "close": None}})
add("ping_loop_tick_witness", {"family": 2, "address": ["127.0.0.1", 9000], "timeout": 1.0,
    "seams": {"socket_new": exc("OSError")}})
add("ping_loop_tick_witness", {"family": 2, "address": ["127.0.0.1", 9000], "timeout": 1.0,
    "seams": {"socket_new": None, "settimeout": None, "connect": None,
              "recv": [[49]], "close": exc("KeyboardInterrupt")}})
add("ping_loop_tick_witness", {"family": 2, "address": ["127.0.0.1", 9000], "timeout": 1.0,
    "seams": {"socket_new": None, "settimeout": None, "connect": None,
              "recv": exc("BaseExceptionGroup", "boom (1 sub-exception)"), "close": None}})

# ---------------- iter_proc_cmdlines ----------------
add("iter_proc_cmdlines", {"exclude_pids": [], "seams": {
    "getpid": 500, "listdir": [["1", "2", "self", "500", "abc", "3"]],
    "read_cmdline": ["/bin/one\0--flag", "/bin/two", "/bin/three"]}})
add("iter_proc_cmdlines", {"exclude_pids": [1, 3], "seams": {
    "getpid": 500, "listdir": [["1", "2", "3"]],
    "read_cmdline": ["/bin/one", "/bin/two", "/bin/three"]}})
add("iter_proc_cmdlines", {"exclude_pids": [], "seams": {
    "getpid": 500, "listdir": [["1", "2", "3"]],
    "read_cmdline": [exc("FileNotFoundError"), "/bin/two", exc("PermissionError")]}})
add("iter_proc_cmdlines", {"exclude_pids": [], "seams": {
    "getpid": exc("OSError")}})
add("iter_proc_cmdlines", {"exclude_pids": [], "seams": {
    "getpid": 500, "listdir": exc("OSError")}})
add("iter_proc_cmdlines", {"exclude_pids": [], "seams": {
    "getpid": 500, "listdir": [["1"]],
    "read_cmdline": exc("KeyboardInterrupt")}})

# ---------------- wait_for_tcp_port_free ----------------
for i, v in enumerate([True, False, exc("OSError"), exc("KeyboardInterrupt"), True]):
    add("wait_for_tcp_port_free", {"host": "127.0.0.1", "port": 8765 + i,
        "timeout": 0.5 + i, "seams": {"poll_until": v}})

# ---------------- restart_argv_is_host_gateway ----------------
add("restart_argv_is_host_gateway", {"argv": []})
add("restart_argv_is_host_gateway", {"argv": ["hermes", "gateway", "run"]})
add("restart_argv_is_host_gateway", {"argv": ["gateway", "--profile", "default"]})
add("restart_argv_is_host_gateway", {"argv": ["gateway", "-p", "work"]})
add("restart_argv_is_host_gateway", {"argv": ["gateway", "--profile=default"]})
add("restart_argv_is_host_gateway", {"argv": ["gateway", "--profile=work"]})
add("restart_argv_is_host_gateway", {"argv": ["gateway"],
    "seams": {"is_multiplex_active": True}})
add("restart_argv_is_host_gateway", {"argv": ["gateway"],
    "seams": {"is_multiplex_active": exc("RuntimeError"),
              "hr_read_record": {"live_owner": True, "profiles": ["default", "work"]}}})
add("restart_argv_is_host_gateway", {"argv": ["gateway"],
    "seams": {"is_multiplex_active": False,
              "hr_read_record": {"live_owner": False, "profiles": ["default", "work"]},
              "get_hermes_home": "/h/.hermes", "get_default_hermes_root": "/h/.hermes"}})
add("restart_argv_is_host_gateway", {"argv": ["gateway"],
    "seams": {"is_multiplex_active": False, "hr_read_record": None,
              "get_hermes_home": "/h/.hermes", "get_default_hermes_root": "/other"}})
add("restart_argv_is_host_gateway", {"argv": ["gateway"],
    "seams": {"is_multiplex_active": False, "hr_read_record": None,
              "get_hermes_home": exc("OSError")}})
add("restart_argv_is_host_gateway", {"argv": ["gateway"],
    "seams": {"is_multiplex_active": exc("KeyboardInterrupt")}})

# ---------------- gw_windows ----------------
for i, v in enumerate(["<module gateway_windows>", "marker", None,
                       exc("ImportError"), exc("KeyboardInterrupt")]):
    add("gw_windows", {"seams": {"import_gateway_windows": v}})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    low = {k: v for k, v in counts.items() if v < 5}
    print("generated %d cases across %d funcs (%s)" % (len(CASES), len(counts), OUT))
    if low:
        print("BELOW-FLOOR funcs: %s" % low)
        raise SystemExit(2)


if __name__ == "__main__":
    main()
