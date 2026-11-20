#!/usr/bin/env python3
"""Generate tests/fixtures/cron_flow4.jsonl — differential cases for the
hermes_cli/cron.py remainder FLOW port (src/cli/cron_flow4.c).

Deterministic, >=5 cases per ported func: branch boundaries, scripted
sequences, fail-closed seams (missing/empty script -> RuntimeError), the full
builtin exception hierarchy (Exception subclasses swallowed where live
swallows, BaseException trio passes through), and the documented color/arg/
constant projection domains.

Run: python tests/gen_cron_flow4_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "cron_flow4.jsonl")
CASES = []


def C(fn, io=None, **inp):
    io = dict(io or {})
    # These seams return a LIST; a bare array script is a per-call sequence,
    # so wrap list-valued results. `print` keeps bare arrays as sequences.
    for key in ("find_gateway_pids", "list_jobs", "list_incidents", "doctor_issues"):
        if isinstance(io.get(key), list):
            io[key] = {"__script__": True, "ret": io[key]}
    CASES.append({"func": fn, "in": dict(inp, io=io)})


def L(x):
    return {"__script__": True, "ret": x}


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


# ── builtin_gateway_liveness (tri-state; except Exception + suppress) ──
C("builtin_gateway_liveness", {"active_provider_name": "chronos"})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": True})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": ["g1"]})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": [], "ticker_heartbeat_age": 5,
   "multiplexer_serves": True}, ticker_interval=60)
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": [], "ticker_heartbeat_age": 5,
   "multiplexer_serves": False, "ticker_writer_alive": True})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": [], "ticker_heartbeat_age": 5,
   "multiplexer_serves": False, "ticker_writer_alive": False})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": [], "ticker_heartbeat_age": 400})
C("builtin_gateway_liveness", {"active_provider_name": R("RuntimeError", "resolve fail")})
C("builtin_gateway_liveness", {"active_provider_name": R("SystemExit", "bye")})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": R("ValueError", "suppressed"),
   "find_gateway_pids": ["g"]})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": R("PermissionError", "EACCES")})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": [], "ticker_heartbeat_age": R("KeyboardInterrupt")})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": [], "ticker_heartbeat_age": 1,
   "multiplexer_serves": R("OSError", "probe dead")})
C("builtin_gateway_liveness",
  {"active_provider_name": "builtin", "runtime_lock_active": False,
   "find_gateway_pids": [], "ticker_heartbeat_age": 1,
   "multiplexer_serves": False, "ticker_writer_alive": R("SystemExit", "halt")})
C("builtin_gateway_liveness", {})  # fail-closed active_provider_name -> RuntimeError -> None

# ── ticker_age_is_fresh (pure helper, default + custom interval) ──
C("ticker_age_is_fresh", {}, age=None)
C("ticker_age_is_fresh", {}, age=0)
C("ticker_age_is_fresh", {}, age=200)
C("ticker_age_is_fresh", {}, age=201)
C("ticker_age_is_fresh", {}, age=50, ticker_interval=10)
C("ticker_age_is_fresh", {}, age=51, ticker_interval=10)
C("ticker_age_is_fresh", {}, age=-1)

# ── store_unwritable_report (seam composition) ──
REP = {"store": "/h/.hermes/cron", "error": "EACCES", "since": "2026-01-01", "fix": "chown -R u:g /h/.hermes"}
C("store_unwritable_report", {"current_cron_store": "/h/.hermes/cron", "probe_report": REP})
C("store_unwritable_report", {"current_cron_store": "/h/.hermes/cron", "probe_report": None})
C("store_unwritable_report", {"current_cron_store": R("OSError", "stat fail")})
C("store_unwritable_report", {"current_cron_store": "/h/.hermes/cron", "probe_report": R("ValueError", "bad")})
C("store_unwritable_report", {"current_cron_store": "/h/.hermes/cron", "probe_report": R("SystemExit", "halt")})
C("store_unwritable_report", {})  # fail-closed current_cron_store
C("store_unwritable_report", {"current_cron_store": "/h/.hermes/cron"})  # fail-closed probe_report

# ── print_store_unwritable (brief one-line vs verbose three-line) ──
C("print_store_unwritable", {"print": None}, report=REP, brief=True)
C("print_store_unwritable", {"print": None}, report=REP, brief=False)
C("print_store_unwritable", {"print": R("ValueError", "print failed")}, report=REP, brief=True)
C("print_store_unwritable", {"print": [None, R("SystemExit", "halt")]}, report=REP, brief=False)
C("print_store_unwritable", {}, report=REP, brief=False)  # fail-closed print
C("print_store_unwritable", {"print": None},
  report={"store": "/x/y", "error": "readonly", "fix": "chmod", "since": "never"}, brief=True)

# ── probe_then_list_jobs (probe, print, try/except OSError re-raise) ──
JOBS = [{"id": "j1", "name": "nightly"}, {"id": "j2", "name": "cleanup"}]
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": None,
   "list_jobs": JOBS}, include_disabled=True, brief=False)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": REP,
   "print": None, "list_jobs": JOBS}, include_disabled=True, brief=True)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": REP,
   "print": None, "list_jobs": R("OSError", "read fail")}, include_disabled=False, brief=True)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": None,
   "list_jobs": R("OSError", "read fail")}, include_disabled=True)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": None,
   "list_jobs": R("ValueError", "bad job")}, include_disabled=True)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": None,
   "list_jobs": R("SystemExit", "halt")}, include_disabled=True)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": REP,
   "print": R("OSError", "print fail"), "list_jobs": JOBS}, include_disabled=True, brief=False)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": REP,
   "print": None, "list_jobs": R("PermissionError", "locked")}, include_disabled=True, brief=True)
C("probe_then_list_jobs",
  {"current_cron_store": "/h/.hermes/cron", "probe_report": None,
   "list_jobs": R("FileNotFoundError", "no store")}, include_disabled=True)
C("probe_then_list_jobs", {}, include_disabled=True)  # fail-closed current_cron_store

# ── incidents (ack/list orchestration, whitespace + 160-char clip) ──
C("incidents", {"print": None}, args={})
C("incidents", {"print": None, "ack_incident": True},
  args={"incident_action": "ack", "incident_id": "inc-1"})
C("incidents", {"print": None, "ack_incident": False},
  args={"incident_action": "ack", "incident_id": "inc-2"})
C("incidents", {"ack_incident": R("ValueError", "bad id")},
  args={"incident_action": "ack", "incident_id": "inc-3"})
C("incidents", {"ack_incident": R("SystemExit", "halt")},
  args={"incident_action": "ack", "incident_id": "inc-4"})
C("incidents", {"print": None, "list_incidents": []}, args={})
C("incidents", {"print": None, "list_incidents": []}, args={"state": "detected"})
C("incidents", {"print": None, "print_banner": None, "list_incidents": [
    {"id": "i1", "state": "detected", "job_id": "j1", "failure_type": "timeout",
     "first_seen_at": "t0", "last_seen_at": "t1", "error": "  boom \n  again  ",
     "output_file": "/o/1"}]}, args={})
C("incidents", {"print": None, "print_banner": None, "list_incidents": [
    {"id": "i1", "state": "alerted", "job_id": "j1", "error": "x" * 200},
    {"id": "i2", "state": "closed", "job_id": "j2", "error": None, "output_file": None}]},
  args={"state": "closed"})
C("incidents", {"print": R("SystemExit", "halt"), "print_banner": None,
                "list_incidents": [{"id": "i1", "state": "detected", "job_id": "j1"}]},
  args={})
C("incidents", {}, args={})  # fail-closed list_incidents

# ── doctor (orchestrator; seamed claimed sibling) ──
C("doctor", {"list_jobs": []})
C("doctor", {"list_jobs": JOBS, "doctor_issues": []})
C("doctor", {"list_jobs": JOBS, "doctor_issues": ["not firing", "bad script"], "print": None})
C("doctor", {"list_jobs": JOBS, "doctor_issues": ["single issue"], "print": None})
C("doctor", {"list_jobs": R("OSError", "read fail")})
C("doctor", {"list_jobs": JOBS, "doctor_issues": R("ValueError", "bad job")})
C("doctor", {"list_jobs": JOBS, "doctor_issues": R("SystemExit", "halt")})
C("doctor", {"list_jobs": JOBS, "doctor_issues": ["issue"], "print": R("OSError", "print fail")})
C("doctor", {})  # fail-closed list_jobs


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for case in CASES:
            fh.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    per = {}
    for case in CASES:
        per[case["func"]] = per.get(case["func"], 0) + 1
    print("wrote %s (%d cases)" % (OUT, len(CASES)))
    for fn in sorted(per):
        print("  %-28s %d" % (fn, per[fn]))
    assert all(v >= 5 for v in per.values()), "every func needs >=5 cases"
    assert len(per) == 7, "expected 7 funcs"


if __name__ == "__main__":
    main()
