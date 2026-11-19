#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_windows_flow.jsonl — {func,in} cases for
t_port_gateway_windows_flow.c / oracle_gateway_windows_flow.py. >=4 cases per
port. List-returning seams are wrapped ([[...]]); per-call sequences are plain
arrays; object/scalar results are given directly. Paths are POSIX strings."""
import json
from pathlib import Path

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


SENT = "/h/sent.json"


def sentinel(pid=10, ct=100.0, phase="exited"):
    return json.dumps({"pid": pid, "create_time": ct, "phase": phase})


# ── start_or_report_running ──
add("start_or_report_running", running_pids=None, seams={"gateway_pids": [[]], "spawn_detached": 99, "report_gateway_start": None})
add("start_or_report_running", running_pids=None, seams={"gateway_pids": [[10]], "report_already_running": None})
add("start_or_report_running", running_pids=[7], seams={"report_already_running": None})
add("start_or_report_running", running_pids=[], seams={"spawn_detached": 5, "report_gateway_start": None})

# ── offer_elevated_install ──
add("offer_elevated_install", headline="H", force=False, start_now=True, start_on_login=True,
    seams={"prompt_yes_no": True, "launch_elevated_install": True})
add("offer_elevated_install", headline="H", force=True, start_now=False, start_on_login=True,
    seams={"prompt_yes_no": True, "launch_elevated_install": True})
add("offer_elevated_install", headline="H", force=False, start_now=True, start_on_login=False,
    seams={"prompt_yes_no": True, "launch_elevated_install": False})
add("offer_elevated_install", headline="H", force=False, start_now=False, start_on_login=False,
    seams={"prompt_yes_no": False})

# ── reconcile_autostart_launchers ──
add("reconcile_autostart_launchers", seams={"is_task_registered": True, "get_startup_entry_path": "/h/e.vbs",
    "legacy_startup_entry_path": "/h/l.cmd", "path_unlink": [None, None]})
add("reconcile_autostart_launchers", seams={"is_task_registered": False, "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [True, False], "write_task_script": "/h/g.cmd", "install_startup_entry": "/h/e.vbs"})
add("reconcile_autostart_launchers", seams={"is_task_registered": False, "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [True, True], "write_task_script": "/h/g.cmd", "install_startup_entry": "/h/e.vbs"})
add("reconcile_autostart_launchers", seams={"is_task_registered": False, "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [False, True], "get_startup_entry_path": "/h/e.vbs", "get_task_script_path": "/h/g.cmd",
    "install_startup_entry": "/h/e.vbs"})
add("reconcile_autostart_launchers", seams={"is_task_registered": False, "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [False, False], "get_startup_entry_path": "/h/e.vbs"})

# ── redundant_autostart_entries ──
add("redundant_autostart_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [True, True], "is_task_registered": True})
add("redundant_autostart_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [True, True], "is_task_registered": False})
add("redundant_autostart_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [True, False], "is_task_registered": False})
add("redundant_autostart_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_exists": [False, False], "is_task_registered": True})

# ── remove_startup_entries ──
add("remove_startup_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_unlink": [None, None]})
add("remove_startup_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_unlink": [{"__raise__": "FileNotFoundError"}, None]})
add("remove_startup_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_unlink": [{"__raise__": "OSError", "msg": "locked"}, None]})
add("remove_startup_entries", seams={"get_startup_entry_path": "/h/e.vbs", "legacy_startup_entry_path": "/h/l.cmd",
    "path_unlink": [{"__raise__": "FileNotFoundError"}, {"__raise__": "FileNotFoundError"}]})

# ── attested_death_generation ──
add("attested_death_generation", current_pids=[], home="/h", seams={
    "read_start_attestation": {"pids": [10], "generation": "g1", "ts": "x", "create_times": {"10": 100.0}},
    "within_horizon": True, "get_lifecycle_sentinel_path": SENT, "read_text": {"__raise__": "OSError"}})
add("attested_death_generation", current_pids=[], home="/h", seams={
    "read_start_attestation": {"pids": [], "generation": "g1"}, "within_horizon": True})
add("attested_death_generation", current_pids=[], home="/h", seams={
    "read_start_attestation": {"pids": [10], "generation": "g1"}, "within_horizon": False})
add("attested_death_generation", current_pids=[10], home="/h", seams={
    "read_start_attestation": {"pids": [10], "generation": "g1"}, "within_horizon": True})
add("attested_death_generation", current_pids=[], home="/h", seams={"read_start_attestation": None})

# ── attested_dead ──
add("attested_dead", attested=[10], current_pids=[], data={"create_times": {"10": 100.0}}, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": sentinel()})
add("attested_dead", attested=[10], current_pids=[], data={"create_times": {"10": 100.0}}, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": {"__raise__": "OSError"}})
add("attested_dead", attested=[10], current_pids=[10], data={}, home="/h", seams={})
add("attested_dead", attested=[], current_pids=[], data=None, home="/h", seams={})

# ── attested_pid_exited_cleanly ──
add("attested_pid_exited_cleanly", pid=10, create_time=100.0, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": sentinel(10, 100.0, "exited")})
add("attested_pid_exited_cleanly", pid=10, create_time=100.0, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": json.dumps({"pid": 99, "phase": "exited"})})
add("attested_pid_exited_cleanly", pid=10, create_time=100.0, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": json.dumps({"pid": 10, "create_time": 200.0, "phase": "exited"})})
add("attested_pid_exited_cleanly", pid=10, create_time=None, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": "not json"})
add("attested_pid_exited_cleanly", pid=10, create_time=None, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": json.dumps({"pid": 10, "phase": "exited"})})
add("attested_pid_exited_cleanly", pid=10, create_time=100.0, home="/h",
    seams={"get_lifecycle_sentinel_path": SENT, "read_text": {"__raise__": "OSError"}})

# ── consume_start_attestation ──
add("consume_start_attestation", generation="g1", home="/h",
    seams={"read_start_attestation": {"generation": "g1"}, "clear_start_attestation": None})
add("consume_start_attestation", generation="g1", home="/h", seams={"read_start_attestation": {"generation": "g2"}})
add("consume_start_attestation", generation="g1", home="/h", seams={"read_start_attestation": None})
add("consume_start_attestation", generation="g1", home=None,
    seams={"read_start_attestation": {"generation": "g1"}, "clear_start_attestation": None})

# ── reconcile_scheduled_task ──
add("reconcile_scheduled_task", task_name="T", seams={"scheduled_task_drift": [["LogonTrigger Delay", "version"]],
    "write_task_script": "/h/g.cmd", "install_scheduled_task": {"ok": True, "detail": "Created Scheduled Task 'T'"}})
add("reconcile_scheduled_task", task_name="T", seams={"scheduled_task_drift": [["LogonTrigger Delay"]],
    "write_task_script": "/h/g.cmd", "install_scheduled_task": {"ok": False, "detail": "denied"}})
add("reconcile_scheduled_task", task_name="T", seams={"scheduled_task_drift": [[]]})
add("reconcile_scheduled_task", task_name="T", seams={"scheduled_task_drift": [["a", "b", "c"]],
    "write_task_script": "/h/g.cmd", "install_scheduled_task": {"ok": True, "detail": "ok"}})

# ── live_gateway_pids ──
add("live_gateway_pids", all_profiles=False, home="/h", pid_filter_present=False, seams={"get_running_pid": 42})
add("live_gateway_pids", all_profiles=False, home="/h", pid_filter_present=False, seams={"get_running_pid": None})
add("live_gateway_pids", all_profiles=True, home=None, pid_filter_present=False, seams={"find_gateway_pids": [[1, 2]]})
add("live_gateway_pids", all_profiles=False, home=None, pid_filter_present=True,
    seams={"find_gateway_pids": [[1, 2, 3]], "pid_filter": [[2, 3]]})
add("live_gateway_pids", all_profiles=False, home="/h", pid_filter_present=False, seams={"get_running_pid": 0})

# ── gateway_pid_identities ──
add("gateway_pid_identities", pids=[10, 20], seams={"import_start_time": None, "get_process_start_time": [1.5, 2.5]})
add("gateway_pid_identities", pids=[10], seams={"import_start_time": {"__raise__": "ImportError"}})
add("gateway_pid_identities", pids=[], seams={"import_start_time": None})
add("gateway_pid_identities", pids=[10], seams={"import_start_time": None, "get_process_start_time": [None]})

out = Path(__file__).resolve().parent / "fixtures" / "gateway_windows_flow.jsonl"
out.parent.mkdir(parents=True, exist_ok=True)
with open(out, "w", encoding="utf-8", newline="\n") as fh:
    for c in CASES:
        fh.write(json.dumps(c, ensure_ascii=False) + "\n")
print("wrote %d cases -> %s" % (len(CASES), out))