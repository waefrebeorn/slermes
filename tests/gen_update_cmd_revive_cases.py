#!/usr/bin/env python3
"""Generate tests/fixtures/update_cmd_revive.jsonl for the update_cmd wave-2
revival oracle (>=5 branch-covering cases per revived function).

Case shape: {"func", "in": {"args", "ctx", "io"}} — see the harness header.
io scripts: single value reused; bare array consumed per call (clamped);
{"__const__": v} constant list return; {"__raise__": kind, "msg"} exception.
"""
import json
from datetime import datetime, timedelta, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "update_cmd_revive.jsonl"

NOW = int(datetime(2026, 11, 18, 12, 0, 0, tzinfo=timezone.utc).timestamp())
DAY = 86400


def stamp(epoch):
    return datetime.fromtimestamp(epoch, timezone.utc).strftime("%Y%m%d-%H%M%S")


OLD = stamp(NOW - 8 * DAY)
NEW = stamp(NOW - 1 * DAY)
EDGE = stamp(NOW - 7 * DAY)          # exactly the cutoff: NOT stale (strict <)
LEAP = "20240229-000000"             # valid leap day, ancient
BAD_MONTH = "20251332-000000"        # month 13 / day 32
BAD_FEB = "20250229-000000"          # 2025 is not a leap year
PREFIX = "hermes-update-autostash-"

CASES = []


def add(func, args=None, ctx=None, io=None):
    CASES.append({"func": func,
                  "in": {"args": args or {}, "ctx": ctx or {}, "io": io or {}}})


def const(v):
    return {"__const__": v}


def raise_(kind, msg):
    return {"__raise__": kind, "msg": msg}


# ── receipt_looks_unfinished ──
F = "receipt_looks_unfinished"
add(F, {"receipt": {"exit_code": 0, "outcome": "success"}})
add(F, {"receipt": {"exit_code": 1}})
add(F, {"receipt": {"exit_code": 0, "outcome": "failed"}})
add(F, {"receipt": {"outcome": "running"}})
add(F, {"receipt": {"exit_code": 0, "gateway_restart": {"incomplete": True}}})
add(F, {"receipt": {"exit_code": 0, "gateway_restart": {"incomplete": False},
                    "stop_reason": "completed at command boundary"}})
add(F, {"receipt": {"outcome": "refused", "stop_reason": "update_contract refusal"}})
add(F, {"receipt": {"outcome": "success", "stop_reason": "sys.exit(0)"}})
add(F, {"receipt": {"exit_code": False}})                     # False == 0
add(F, {"receipt": {"exit_code": "0"}})                       # "0" != 0
add(F, {"receipt": {"exit_code": 0, "gateway_restart": {"incomplete": "yes"}}})
add(F, {"receipt": {}})
add(F, {"receipt": None})                                     # AttributeError

# ── receipt_reports_stale_runtime ──
F = "receipt_reports_stale_runtime"
SHA = {"current_checkout_sha": "aaa"}
add(F, {"expected_sha": "aaa"}, {"receipt": None})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"fleet": [{"state": "stale", "code_sha": "aaa"}]}})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"fleet": [{"state": "current", "code_sha": "bbb"}]}})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"fleet": [{"state": "current", "code_sha": "aaa"}]}})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"fleet": [{"state": "external", "code_sha": "bbb"}]}})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"fleet": ["x", 5, {"code_sha": "bbb"}]}})
add(F, {"expected_sha": ""},
    {"receipt": {"exit_code": 1, "plan": {"runtimes": [{"code_sha": "ddd"}]}}},
    {"current_checkout_sha": "ccc"})
add(F, {"expected_sha": None}, {"receipt": {"exit_code": 1}},
    {"current_checkout_sha": None})
add(F, {"expected_sha": "aaa"}, {"receipt": {"exit_code": 0, "fleet": []}})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"outcome": "partial",
                 "plan": {"runtimes": [{"code_sha": "aaa"}, {"code_sha": "bbb"}]}}})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"outcome": "partial", "plan": {"runtimes": [{"code_sha": "aaa"}]}}})
add(F, {"expected_sha": "aaa"}, {"receipt": {"exit_code": 1, "plan": "x"}})
add(F, {"expected_sha": "aaa"}, {"receipt": {"exit_code": 1, "plan": {"runtimes": None}}})
add(F, {"expected_sha": "123"}, {"receipt": {"fleet": [{"code_sha": 123}]}})
add(F, {"expected_sha": "123"}, {"receipt": {"fleet": [{"code_sha": 124}]}})
add(F, {"expected_sha": "True"}, {"receipt": {"fleet": [{"code_sha": True}]}})
add(F, {"expected_sha": None}, {"receipt": {}},
    {"current_checkout_sha": raise_("RuntimeError", "head capture failed")})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"fleet": {"a": 1}, "exit_code": 1,
                 "plan": {"runtimes": [{"code_sha": "bbb"}]}}})
add(F, {"expected_sha": "aaa"},
    {"receipt": {"exit_code": 1, "plan": {"runtimes": "abc"}}})

# ── warn_orphaned_update_autostashes ──
F = "warn_orphaned_update_autostashes"
GC = ["git"]
CWD = "/repo"


def gitio(stdout, rc=0):
    return {"git_run": {"returncode": rc, "stdout": stdout}, "now_utc": NOW}


add(F, {"git_cmd": GC, "cwd": CWD},
    io={"git_run": {"returncode": 1, "stdout": ""}})
add(F, {"git_cmd": GC, "cwd": CWD}, io=gitio(""))
add(F, {"git_cmd": GC, "cwd": CWD},
    io=gitio(f"stash@{{0}}: On main: {PREFIX}{OLD}\n"))
add(F, {"git_cmd": GC, "cwd": CWD},
    io=gitio(f"stash@{{0}}: On main: {PREFIX}{OLD}\n"
             f"stash@{{1}}: WIP on dev: {PREFIX}{stamp(NOW - 9 * DAY)}\n"))
add(F, {"git_cmd": GC, "cwd": CWD},
    io=gitio(f"stash@{{0}}: On main: {PREFIX}{OLD}\n"
             f"stash@{{1}}: On main: {PREFIX}{NEW}\n"
             f"stash@{{2}}: WIP on main: manual stash\n"
             f"stash@{{3}}: On main: {PREFIX}notastamp\n"
             f"stash@{{4}}: On main: {PREFIX}2025111\n"))
add(F, {"git_cmd": GC, "cwd": CWD}, io=gitio(f"stash@{{0}}: On main: {PREFIX}{EDGE}\n"))
add(F, {"git_cmd": GC, "cwd": CWD},
    io={"git_run": raise_("OSError", "disk gone"), "now_utc": NOW})
add(F, {"git_cmd": GC, "cwd": CWD},
    io={"git_run": raise_("SystemExit", "1"), "now_utc": NOW})
add(F, {"git_cmd": GC, "cwd": CWD},
    io={"git_run": {"returncode": False, "stdout": f"stash@{{0}}: {PREFIX}{OLD}"},
        "now_utc": NOW})
add(F, {"git_cmd": GC, "cwd": CWD},
    io=gitio(f"  stash@{{0}}: On main: {PREFIX}{OLD}  \r\n"
             f"stash@{{1}}: On main: {PREFIX}{LEAP}\r\n"
             f"stash@{{2}}: On main: {PREFIX}{BAD_MONTH}\r\n"
             f"stash@{{3}}: On main: {PREFIX}{BAD_FEB}\r\n"))
add(F, {"git_cmd": GC, "cwd": CWD}, io={"git_run": None, "now_utc": NOW})
add(F, {"git_cmd": GC, "cwd": None}, io={"now_utc": NOW})
# lenient strptime fields (1-digit m/d/H/M/S) and Unicode strip
add(F, {"git_cmd": GC, "cwd": CWD}, io=gitio(f"stash@{{0}}: On main: {PREFIX}2026118-120000\n"))
add(F, {"git_cmd": GC, "cwd": CWD}, io=gitio(f"stash@{{0}}: On main: {PREFIX}20261118-12000\n"))
add(F, {"git_cmd": GC, "cwd": CWD}, io=gitio(f"stash@{{0}}: On main: {PREFIX}202611-111\n"))
add(F, {"git_cmd": GC, "cwd": CWD},
    io=gitio(f"\xa0stash@{{0}}: On main: {PREFIX}{OLD}\xa0\n"))

# ── desktop_owns_gateway_lifecycle ──
F = "desktop_owns_gateway_lifecycle"
CP = "python -m hermes_cli.main serve"
SCAN = {"psutil": None, "detect_venv_python_processes": const([])}
add(F, io={"ledger_entries": const([{"purpose": "serve", "pid": 1}]),
           "spawner_is_dead": False})
add(F, io=dict({"ledger_entries": const([{"purpose": "serve"}]),
                "spawner_is_dead": True}, **SCAN))
add(F, io=dict({"ledger_entries": const([{"purpose": "chat"}])}, **SCAN))
add(F, io={"ledger_entries": const([]),
           "psutil": None,
           "detect_venv_python_processes": const([[10, "python", CP]]),
           "holder_subcommand": "serve"})
add(F, io={"ledger_entries": const([]),
           "psutil": {},
           "detect_venv_python_processes": const([[10, "python", CP]]),
           "holder_subcommand": "serve",
           "psutil.Process": {},
           "parent_is_live": True})
add(F, io={"ledger_entries": const([]),
           "psutil": {},
           "detect_venv_python_processes": const([[10, "python", CP]]),
           "holder_subcommand": "serve",
           "psutil.Process": {},
           "parent_is_live": False})
add(F, io={"ledger_entries": const([]),
           "psutil": None,
           "detect_venv_python_processes": const([[10, "python", "kanban --preserve-cache"]])})
add(F, io=dict({"ledger_entries": raise_("RuntimeError", "ledger corrupt")}, **SCAN))
add(F, io=dict({"ledger_entries": const([{"purpose": "dashboard"}]),
                "spawner_is_dead": raise_("ValueError", "bad row")}, **SCAN))
add(F, io={"ledger_entries": const([]),
           "psutil": None,
           "detect_venv_python_processes": raise_("OSError", "scan failed")})
add(F, io={"ledger_entries": const([]),
           "psutil": {},
           "detect_venv_python_processes": const([[10, "python", CP]]),
           "holder_subcommand": "serve",
           "psutil.Process": raise_("OSError", "no such process")})
add(F, io=dict({"ledger_entries": const([5])}, **SCAN))
add(F, io=dict({"ledger_entries": const([{"purpose": "serve"}]),
                "spawner_is_dead": None}, **SCAN))
add(F, io={"ledger_entries": const([{"purpose": "serve"}, {"purpose": "serve"}]),
           "spawner_is_dead": [False, raise_("RuntimeError", "must not run")]})
add(F, io={"ledger_entries": const([]),
           "psutil": None,
           "detect_venv_python_processes": const([[10, "python", CP]]),
           "holder_subcommand": "unknown"})
add(F, io={"ledger_entries": raise_("SystemExit", "1")})

# ── pending_fleet_restart_needed ──
F = "pending_fleet_restart_needed"
add(F, ctx={"receipt_kw": {}},
    io={"retain_receipt_manual_serves": const([]),
        "fleet_restart_obligation_armed": True,
        "marker_only_restart_obsolete": True})
add(F, ctx={"receipt_kw": {}},
    io={"retain_receipt_manual_serves": const([]),
        "fleet_restart_obligation_armed": True,
        "marker_only_restart_obsolete": False})
add(F, ctx={"receipt_kw": {"exit_code": 0}},
    io={"retain_receipt_manual_serves": const([]),
        "fleet_restart_obligation_armed": False,
        "receipt_owed_gateways": const([]),
        "current_checkout_sha": "aaa"})
add(F, ctx={"receipt_kw": {"fleet": [{"state": "stale"}]}},
    io={"retain_receipt_manual_serves": const([]),
        "fleet_restart_obligation_armed": False,
        "receipt_owed_gateways": const([]),
        "current_checkout_sha": "aaa",
        "live_fleet_covers_receipt": True})
add(F, ctx={"receipt_kw": {"fleet": [{"state": "stale"}]}},
    io={"retain_receipt_manual_serves": const([]),
        "fleet_restart_obligation_armed": False,
        "receipt_owed_gateways": const([["gateway", "default"]]),
        "current_checkout_sha": "aaa",
        "live_fleet_covers_receipt": False})
add(F, io={"read_latest_receipt": {"exit_code": 1, "fleet": [{"state": "stale"}]},
           "retain_receipt_manual_serves": const([]),
           "fleet_restart_obligation_armed": False,
           "receipt_owed_gateways": None,
           "current_checkout_sha": "aaa",
           "live_fleet_covers_receipt": False})
add(F, io={"read_latest_receipt": None,
           "retain_receipt_manual_serves": const([]),
           "fleet_restart_obligation_armed": False,
           "receipt_owed_gateways": const([]),
           "current_checkout_sha": "aaa"})
add(F, ctx={"receipt_kw": {"exit_code": 0}, "pending_manual_kw": [{"profile": "p"}]},
    io={"fleet_restart_obligation_armed": False,
        "receipt_owed_gateways": const([]),
        "current_checkout_sha": "aaa"})
add(F, ctx={"receipt_kw": {}},
    io={"retain_receipt_manual_serves": const([]),
        "fleet_restart_obligation_armed": raise_("SystemExit", "1")})
add(F, ctx={"receipt_kw": {"fleet": [{"state": "stale"}]}},
    io={"retain_receipt_manual_serves": const([]),
        "fleet_restart_obligation_armed": False,
        "receipt_owed_gateways": const([]),
        "current_checkout_sha": "aaa",
        "live_fleet_covers_receipt": raise_("RuntimeError", "probe died")})

# ── refuse_gateway_ancestor_tree_kill ──
F = "refuse_gateway_ancestor_tree_kill"
ANC = "is_pid_ancestor_of_current_process"
add(F, {"gateway_pids": [1]}, {"gateway_mode": True})
add(F, {"gateway_pids": []})
add(F, {"gateway_pids": [1, 2]}, io={ANC: [False, True]})
add(F, {"gateway_pids": [1, 2]}, io={ANC: False})
add(F, {"gateway_pids": [1]}, io={ANC: raise_("ValueError", "ancestry unknown")})
add(F, {"gateway_pids": ["7"]}, io={ANC: True})
add(F, {"gateway_pids": ["abc"]}, io={ANC: True})
add(F, {"gateway_pids": [1]}, io={ANC: raise_("SystemExit", "1")})
add(F, {"gateway_pids": None})
add(F, {"gateway_pids": [3, 3]}, io={ANC: True})

# ── restart_phase_failure_is_incomplete ──
F = "restart_phase_failure_is_incomplete"
add(F, ctx={"surviving": None, "pre_restart_pids": []})
add(F, ctx={"surviving": [1], "pre_restart_pids": []})
add(F, ctx={"surviving": [], "pre_restart_pids": []})
add(F, ctx={"surviving": [], "pre_restart_pids": [5]})
add(F, ctx={"surviving": [], "pre_restart_pids": None})
add(F, ctx={"surviving": []})
add(F, ctx={"surviving": "", "pre_restart_pids": []})
add(F, ctx={"surviving": 0, "pre_restart_pids": []})
add(F, ctx={"surviving": {"a": 1}, "pre_restart_pids": []})

# ── looks_like_desktop_control_plane ──
F = "looks_like_desktop_control_plane"
add(F, {"cmdline": "python -m hermes_cli.main serve"}, io={"holder_subcommand": "serve"})
add(F, {"cmdline": "python -m hermes_cli.main dashboard"}, io={"holder_subcommand": "dashboard"})
add(F, {"cmdline": "python -m hermes_cli.main gateway"}, io={"holder_subcommand": "unknown"})
add(F, {"cmdline": "python -m hermes_cli.main serve"}, io={"holder_subcommand": None})
add(F, {"cmdline": "kanban --preserve-cache"})
add(F, {"cmdline": None})
add(F, {"cmdline": ""})
add(F, {"cmdline": "PYTHON -M HERMES_CLI.MAIN SERVE"}, io={"holder_subcommand": "serve"})
add(F, {"cmdline": "python -m hermes_cli.main serve"},
    io={"holder_subcommand": raise_("RuntimeError", "parser died")})
add(F, {"cmdline": "python -m hermes_cli.main serve"}, io={"holder_subcommand": "gateway"})

# ── git_is_trampoline ──
F = "git_is_trampoline"
add(F, {"git_cmd": ["git"]}, io={"run_git": {"stdout": "git version 2.56.0", "stderr": ""}})
add(F, {"git_cmd": ["git"]}, io={"run_git": {"stdout": "fatal: fork bomb detected", "stderr": ""}})
add(F, {"git_cmd": ["git"]}, io={"run_git": {"stdout": "", "stderr": "FORK BOMB guard"}})
add(F, {"git_cmd": ["git"]}, io={"run_git": raise_("OSError", "spawn failed")})
add(F, {"git_cmd": ["git"]}, io={"run_git": raise_("SystemExit", "1")})
add(F, {"git_cmd": ["git"]}, io={"run_git": {"stdout": None, "stderr": "fork bomb"}})
add(F, {"git_cmd": ["git"]}, io={"run_git": {"stdout": "", "stderr": ""}})
add(F, {"git_cmd": ["git"]}, io={"run_git": {"stdout": "x"}})
add(F, {"git_cmd": ["git"]}, io={"run_git": None})
add(F, {"git_cmd": None}, io={"run_git": {"stdout": "fork bomb", "stderr": ""}})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert len(counts) == 9, counts
    for fn, n in counts.items():
        assert n >= 5, (fn, n)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn, n in sorted(counts.items()):
        print("  %-40s %d" % (fn, n))


if __name__ == "__main__":
    main()
