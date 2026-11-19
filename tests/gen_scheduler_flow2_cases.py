#!/usr/bin/env python3
"""Generate tests/fixtures/scheduler_flow2.jsonl for the cron/scheduler.py
UNCLAIMED-remainder FLOW port. >=5 cases per ported function, exercising normal
paths, branch decisions, scripted raises, fail-closed (missing script),
BaseException passthrough, import-failure gates, and state mutation.

Case shape: {"func": <live_name>, "in": {..., "io": {seam: script}, "state": {...}}}.
A script is a plain value (constant), a list (per-call queue), or
{"__script__": true, "ret": v} / {"__script__": true, "raise": {type,message,errno?}}.
"""
import json
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "scheduler_flow2.jsonl"

CASES = []


def case(func, io=None, state=None, **inp):
    d = dict(inp)
    io = dict(io or {})
    # The live logger always succeeds; script every level so a reached logger
    # call never fail-closes (fail-closed tests target the real I/O seams).
    for lvl in ("logger.warning", "logger.debug", "logger.info", "logger.error"):
        io.setdefault(lvl, {"__script__": True, "ret": None})
    d["io"] = io
    if state is not None:
        d["state"] = state
    CASES.append({"func": func, "in": d})


def sc(ret):
    return {"__script__": True, "ret": ret}


def rz(type, message="", errno=None):
    r = {"type": type, "message": message}
    if errno is not None:
        r["errno"] = errno
    return {"__script__": True, "raise": r}


def key(h, j):
    return [h, j]


def since(entries):
    return [{"key": [h, j], "started": t} for h, j, t in entries]


def allow(entries):
    return [{"key": [h, j], "allowance": a} for h, j, a in entries]


def pids(entries):
    return [{"key": [h, j], "pid": p} for h, j, p in entries]


def futs(entries):
    return [{"key": [h, j], "future": f} for h, j, f in entries]


def owners(keys):
    return [{"key": [h, j], "owners": {}} for h, j in keys]


# ── _job_fallback_chain ───────────────────────────────────────────────────
CHAIN = [{"provider": "openrouter", "model": "m1"}]
case("_job_fallback_chain", job={}, cfg={},
     io={"get_fallback_chain": sc(CHAIN), "_job_route_pinned": sc(False),
         "scoped_fallback_chain": sc(CHAIN)})
case("_job_fallback_chain", job={"provider": "x"}, cfg={},
     io={"get_fallback_chain": sc(CHAIN), "_job_route_pinned": sc(True),
         "scoped_fallback_chain": sc(None)})
case("_job_fallback_chain", job={}, cfg={},
     io={"get_fallback_chain": rz("RuntimeError", "cfg boom")})
case("_job_fallback_chain", job={}, cfg={},
     io={"get_fallback_chain": sc(CHAIN), "_job_route_pinned": rz("ValueError", "pin boom")})
case("_job_fallback_chain", job={}, cfg={},
     io={"get_fallback_chain": sc(CHAIN), "_job_route_pinned": sc(False),
         "scoped_fallback_chain": rz("RuntimeError", "scoped boom")})
case("_job_fallback_chain", job={}, cfg={}, io={})  # fail-closed
case("_job_fallback_chain", job={}, cfg={},
     io={"get_fallback_chain": rz("SystemExit", "bye")})

# ── _current_gateway_code_sha ─────────────────────────────────────────────
case("_current_gateway_code_sha", io={"current_code_sha": sc("abc123def")})
case("_current_gateway_code_sha", io={"current_code_sha": sc(None)})
case("_current_gateway_code_sha", io={"current_code_sha": rz("RuntimeError", "git boom")})
case("_current_gateway_code_sha", io={}, import_fail=["gateway.code_skew"])
case("_current_gateway_code_sha", io={"current_code_sha": rz("KeyboardInterrupt", "int")})
case("_current_gateway_code_sha", io={})  # fail-closed -> swallowed -> null

# ── _remember_inflight_home ───────────────────────────────────────────────
case("_remember_inflight_home", home="/h/p1", state={"_inflight_home_paths": {}},
     io={"hermes_home_key": sc("p1")})
case("_remember_inflight_home", home="/h/p2", state={"_inflight_home_paths": {"p2": "/old"}},
     io={"hermes_home_key": sc("p2")})
case("_remember_inflight_home", home="/h/p1", state={"_inflight_home_paths": {}},
     io={"hermes_home_key": rz("RuntimeError", "key boom")})
case("_remember_inflight_home", home="/h/p1", state={"_inflight_home_paths": {}}, io={})
case("_remember_inflight_home", home="/h/p1", state={"_inflight_home_paths": {}},
     io={"hermes_home_key": rz("GeneratorExit", "ge")})
case("_remember_inflight_home", home="/H/P1", state={"_inflight_home_paths": {}},
     io={"hermes_home_key": sc("h/p1")})

# ── _inflight_home_path ───────────────────────────────────────────────────
case("_inflight_home_path", home_key="p1", state={"_inflight_home_paths": {"p1": "/h/p1"}})
case("_inflight_home_path", home_key="p2", state={"_inflight_home_paths": {"p1": "/h/p1"}})
case("_inflight_home_path", home_key="p1", state={"_inflight_home_paths": {"p1": None}})
case("_inflight_home_path", home_key="p1", state={"_inflight_home_paths": {}})
case("_inflight_home_path", home_key="", state={"_inflight_home_paths": {}})

# ── get_running_job_details ───────────────────────────────────────────────
case("get_running_job_details",
     state={"_running_job_ids": [key("h1", "j1"), key("h1", "j2")],
            "_running_fire_owners": [],
            "_running_since": since([("h1", "j1", 90.0), ("h1", "j2", 95.0)]),
            "_running_worker_pids": pids([("h1", "j1", 111)]),
            "_scope_isolated_job_ids": [key("h1", "j2")]},
     io={"time.time": sc(100.0)})
case("get_running_job_details",
     state={"_running_job_ids": [key("h1", "j1")], "_running_fire_owners": [],
            "_running_since": [], "_running_worker_pids": [],
            "_scope_isolated_job_ids": []},
     io={"time.time": sc(100.0)})
case("get_running_job_details",
     state={"_running_job_ids": [], "_running_fire_owners": [], "_running_since": [],
            "_running_worker_pids": [], "_scope_isolated_job_ids": []},
     io={"time.time": sc(100.0)})
case("get_running_job_details",
     state={"_running_job_ids": [], "_running_fire_owners": [], "_running_since": [],
            "_running_worker_pids": [], "_scope_isolated_job_ids": []},
     io={"time.time": rz("RuntimeError", "clock boom")})
case("get_running_job_details",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_fire_owners": owners([("h2", "j9")]),
            "_running_since": since([("h1", "j1", 80.0)]),
            "_running_worker_pids": [], "_scope_isolated_job_ids": []},
     io={"time.time": sc(100.0)})
case("get_running_job_details",
     state={"_running_job_ids": [], "_running_fire_owners": [], "_running_since": [],
            "_running_worker_pids": [], "_scope_isolated_job_ids": []}, io={})

# ── _wedged_inflight_keys ─────────────────────────────────────────────────
WSTATE = {"_running_job_ids": [key("h1", "j1")],
          "_running_since": since([("h1", "j1", 0.0)]),
          "_running_allowance_s": allow([("h1", "j1", 100.0)])}
case("_wedged_inflight_keys",
     state={"_running_job_ids": [], "_running_since": [], "_running_allowance_s": []},
     io={"time.time": sc(5000.0)})
case("_wedged_inflight_keys", state=dict(WSTATE),
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0)})
case("_wedged_inflight_keys",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_since": since([("h1", "j1", 0.0)]), "_running_allowance_s": []},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0),
         "load_jobs": sc([{"id": "j1", "schedule": {"kind": "interval", "minutes": 60}}]),
         "_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "_job_interval_minutes": sc(60.0)})
case("_wedged_inflight_keys",
     state={"_running_job_ids": [key("h2", "j1")],
            "_running_since": since([("h2", "j1", 0.0)]), "_running_allowance_s": []},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0),
         "load_jobs": sc([{"id": "j1"}]),
         "_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "_job_interval_minutes": sc(None)})
case("_wedged_inflight_keys",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_since": since([("h1", "j1", 0.0)]), "_running_allowance_s": []},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0),
         "load_jobs": rz("RuntimeError", "jobs boom"),
         "_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "_job_interval_minutes": sc(None)})
case("_wedged_inflight_keys",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_since": since([("h1", "j1", 0.0)]), "_running_allowance_s": []},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0),
         "_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "_job_interval_minutes": sc(None)}, import_fail=["cron.jobs"])
case("_wedged_inflight_keys", state=dict(WSTATE),
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": rz("RuntimeError", "floor boom")})
case("_wedged_inflight_keys",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_since": since([("h1", "j1", 4000.0)]), "_running_allowance_s": []},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0),
         "load_jobs": sc([]), "_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "_job_interval_minutes": sc(None)})

# ── get_wedged_job_ids ────────────────────────────────────────────────────
case("get_wedged_job_ids", state=dict(WSTATE),
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0)})
case("get_wedged_job_ids",
     state={"_running_job_ids": [], "_running_since": [], "_running_allowance_s": []},
     io={"time.time": sc(5000.0)})
case("get_wedged_job_ids",
     state={"_running_job_ids": [key("h1", "j1"), key("h2", "j1")],
            "_running_since": since([("h1", "j1", 0.0), ("h2", "j1", 0.0)]),
            "_running_allowance_s": allow([("h1", "j1", 100.0), ("h2", "j1", 100.0)])},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0)})
case("get_wedged_job_ids",
     state={"_running_job_ids": [], "_running_since": [], "_running_allowance_s": []}, io={})
case("get_wedged_job_ids",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_since": since([("h1", "j1", 0.0)]),
            "_running_allowance_s": allow([("h1", "j1", 100.0)])},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": rz("SystemExit", "bye")})

# ── get_restart_wait_cron_counts ──────────────────────────────────────────
case("get_restart_wait_cron_counts",
     state={"_running_job_ids": [key("h1", "j1"), key("h1", "j2"), key("h1", "j3")],
            "_running_fire_owners": [],
            "_running_since": since([("h1", "j1", 0.0), ("h1", "j2", 4900.0), ("h1", "j3", 4900.0)]),
            "_running_allowance_s": allow([("h1", "j1", 100.0), ("h1", "j2", 100.0), ("h1", "j3", 100.0)]),
            "_scope_isolated_job_ids": [key("h1", "j2"), key("h1", "j3")]},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0)})
case("get_restart_wait_cron_counts",
     state={"_running_job_ids": [], "_running_fire_owners": [], "_running_since": [],
            "_running_allowance_s": [], "_scope_isolated_job_ids": []},
     io={"time.time": sc(5000.0)})
case("get_restart_wait_cron_counts",
     state={"_running_job_ids": [key("h1", "j1")], "_running_fire_owners": [],
            "_running_since": since([("h1", "j1", 4900.0)]),
            "_running_allowance_s": allow([("h1", "j1", 100.0)]),
            "_scope_isolated_job_ids": [key("h1", "j1")]},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0)})
case("get_restart_wait_cron_counts",
     state={"_running_job_ids": [], "_running_fire_owners": [], "_running_since": [],
            "_running_allowance_s": [], "_scope_isolated_job_ids": []}, io={})
case("get_restart_wait_cron_counts",
     state={"_running_job_ids": [key("h1", "j1")], "_running_fire_owners": [],
            "_running_since": since([("h1", "j1", 4900.0)]),
            "_running_allowance_s": allow([("h1", "j1", 100.0)]),
            "_scope_isolated_job_ids": [key("h1", "j1")]},
     io={"time.time": sc(5000.0), "_inflight_min_allowance_minutes": sc(30.0)})

# ── _record_external_cron_worker ──────────────────────────────────────────
RS = {"_running_worker_pids": [], "_scope_isolated_job_ids": []}
case("_record_external_cron_worker", job_id="j1", pid=123, scope_isolated=True,
     state=dict(RS), io={"_inflight_key": sc(key("h1", "j1"))})
case("_record_external_cron_worker", job_id="j1", pid="456", scope_isolated=False,
     state=dict(RS), io={"_inflight_key": sc(key("h1", "j1"))})
case("_record_external_cron_worker", job_id="j1", pid="abc", scope_isolated=True,
     state=dict(RS), io={"_inflight_key": sc(key("h1", "j1"))})
case("_record_external_cron_worker", job_id="j1", pid=None, scope_isolated=False,
     state=dict(RS), io={"_inflight_key": sc(key("h1", "j1"))})
case("_record_external_cron_worker", job_id="j1", pid=3.9, scope_isolated=True,
     state=dict(RS), io={"_inflight_key": sc(key("h1", "j1"))})
case("_record_external_cron_worker", job_id="j1", pid={}, scope_isolated=True,
     state=dict(RS), io={"_inflight_key": sc(key("h1", "j1"))})
case("_record_external_cron_worker", job_id="j1", pid=1, scope_isolated=True,
     state=dict(RS), io={"_inflight_key": rz("RuntimeError", "key boom")})
case("_record_external_cron_worker", job_id="j1", pid="1_0", scope_isolated=False,
     state=dict(RS), io={"_inflight_key": sc(key("h1", "j1"))})

# ── _cron_interval_minutes ────────────────────────────────────────────────
ISO1 = "2026-11-18T12:00:00+00:00"
ISO2 = "2026-11-18T13:00:00+00:00"
case("_cron_interval_minutes", expr="0 * * * *", state={"_cron_interval_cache": {}},
     io={"_ensure_croniter": sc(True), "datetime.now": sc(ISO1), "croniter": sc("it"),
         "croniter.get_next": [sc(ISO1), sc(ISO2)]})
case("_cron_interval_minutes", expr="0 * * * *", state={"_cron_interval_cache": {"0 * * * *": 60.0}},
     io={})
case("_cron_interval_minutes", expr="bad", state={"_cron_interval_cache": {}},
     io={"_ensure_croniter": sc(False)})
case("_cron_interval_minutes", expr="bad", state={"_cron_interval_cache": {}},
     io={"_ensure_croniter": sc(True), "datetime.now": sc(ISO1), "croniter": sc("it"),
         "croniter.get_next": rz("ValueError", "bad expr")})
case("_cron_interval_minutes", expr="0 * * * *", state={"_cron_interval_cache": {}},
     io={}, import_fail=["cron.jobs"])
case("_cron_interval_minutes", expr="* * * * *", state={"_cron_interval_cache": {}},
     io={"_ensure_croniter": sc(True), "datetime.now": sc(ISO1), "croniter": sc("it"),
         "croniter.get_next": [sc(ISO1), sc(ISO1)]})  # gap 0 -> null
case("_cron_interval_minutes", expr="0 * * * *", state={"_cron_interval_cache": {}},
     io={"_ensure_croniter": rz("RuntimeError", "croniter boom")})

# ── _latest_executions_for_releasable_claims ──────────────────────────────
LS = {"_running_job_ids": [key("h1", "j1"), key("h1", "j2")],
      "_running_futures": futs([("h1", "j1", None), ("h1", "j2", {"id": "f2"})])}
case("_latest_executions_for_releasable_claims", state=dict(LS),
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "future.done": sc(True), "latest_executions": sc({"j1": {"status": "ok"}, "j2": {"status": "ok"}})})
case("_latest_executions_for_releasable_claims",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_futures": futs([("h1", "j1", {"__pending__": True})])},
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "latest_executions": sc({"j1": {"status": "running"}})})
case("_latest_executions_for_releasable_claims",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_futures": futs([("h1", "j1", {"id": "f1"})])},
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "future.done": sc(False)})  # no candidates -> {}
case("_latest_executions_for_releasable_claims",
     state={"_running_job_ids": [], "_running_futures": []},
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1")})
case("_latest_executions_for_releasable_claims", state=dict(LS),
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "future.done": sc(True), "latest_executions": rz("RuntimeError", "ledger boom")})
case("_latest_executions_for_releasable_claims", state=dict(LS),
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "future.done": sc(True)}, import_fail=["cron.executions"])
case("_latest_executions_for_releasable_claims",
     state={"_running_job_ids": [key("h2", "j1")],
            "_running_futures": futs([("h2", "j1", None)])},
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1")})  # foreign home filtered
case("_latest_executions_for_releasable_claims",
     state={"_running_job_ids": [key("h1", "j1")],
            "_running_futures": futs([("h1", "j1", {"id": "f1"})])},
     io={"_get_hermes_home": sc("/h1"), "hermes_home_key": sc("h1"),
         "future.done": rz("RuntimeError", "done boom")})

# ── _row_belongs_to_claim ─────────────────────────────────────────────────
case("_row_belongs_to_claim", row={"claimed_at": "2026-11-18T12:00:00+00:00"}, claim_started=100.0,
     io={"_ensure_aware": sc("2026-11-18T12:00:00+00:00")})
case("_row_belongs_to_claim", row={"claimed_at": "2026-11-18T12:00:00+00:00"}, claim_started=9e9,
     io={"_ensure_aware": sc("2026-11-18T12:00:00+00:00")})
case("_row_belongs_to_claim", row={"claimed_at": None}, claim_started=100.0, io={})
case("_row_belongs_to_claim", row={"claimed_at": "not-a-date"}, claim_started=100.0, io={})
case("_row_belongs_to_claim", row={"claimed_at": 5}, claim_started=100.0, io={})
case("_row_belongs_to_claim", row={"claimed_at": "2026-11-18T12:00:00+00:00"}, claim_started=100.0,
     io={"_ensure_aware": rz("ValueError", "aware boom")})
case("_row_belongs_to_claim", row={"claimed_at": "2026-11-18T12:00:00+00:00"}, claim_started=100.0,
     io={"_ensure_aware": rz("RuntimeError", "aware boom")})
case("_row_belongs_to_claim", row={"claimed_at": "2026-11-18T12:00:00+00:00"}, claim_started=100.0,
     io={}, import_fail=["cron.jobs"])

# ── _reclaim_fds_best_effort ──────────────────────────────────────────────
case("_reclaim_fds_best_effort", io={"gc.collect": sc(None), "apply_nofile_soft_limit": sc(None)})
case("_reclaim_fds_best_effort", io={"gc.collect": rz("RuntimeError", "gc boom"),
                                     "apply_nofile_soft_limit": sc(None)})
case("_reclaim_fds_best_effort", io={"gc.collect": sc(None),
                                     "apply_nofile_soft_limit": rz("OSError", "limit boom", 24)})
case("_reclaim_fds_best_effort", io={"apply_nofile_soft_limit": sc(None)}, import_fail=["gc"])
case("_reclaim_fds_best_effort", io={}, import_fail=["gc", "hermes_cli.resource_limits"])
case("_reclaim_fds_best_effort", io={"gc.collect": rz("SystemExit", "bye")})

# ── _load_prefill_messages ────────────────────────────────────────────────
case("_load_prefill_messages", cfg={}, job_id="j1",
     io={"cron_env_setting": sc("/abs/pf.json"), "path.expanduser": sc("/abs/pf.json"),
         "path.exists": sc(True), "prefill.load": sc([{"role": "user", "content": "hi"}])})
case("_load_prefill_messages", cfg={"prefill_messages_file": "pf.json"}, job_id="j1",
     io={"cron_env_setting": sc(""), "path.expanduser": sc("pf.json"),
         "_get_hermes_home": sc("/h1"), "path.exists": sc(True),
         "prefill.load": sc([{"role": "user"}])})
case("_load_prefill_messages", cfg={}, job_id="j1",
     io={"cron_env_setting": sc("/abs/pf.json"), "path.expanduser": sc("/abs/pf.json"),
         "path.exists": sc(False)})
case("_load_prefill_messages", cfg={}, job_id="j1",
     io={"cron_env_setting": sc("/abs/pf.json"), "path.expanduser": sc("/abs/pf.json"),
         "path.exists": sc(True), "prefill.load": rz("ValueError", "Expecting value")})
case("_load_prefill_messages", cfg={}, job_id="j1",
     io={"cron_env_setting": sc("/abs/pf.json"), "path.expanduser": sc("/abs/pf.json"),
         "path.exists": sc(True), "prefill.load": sc({"not": "a list"})})
case("_load_prefill_messages", cfg={}, job_id="j1", io={"cron_env_setting": sc("")})
case("_load_prefill_messages", cfg={}, job_id="j1",
     io={"cron_env_setting": sc("/abs/pf.json"), "path.expanduser": sc("/abs/pf.json"),
         "path.exists": sc(True), "prefill.load": rz("SystemExit", "bye")})
case("_load_prefill_messages", cfg={"agent": {"prefill_messages_file": "legacy.json"}}, job_id="j1",
     io={"cron_env_setting": sc(""), "path.expanduser": sc("legacy.json"),
         "_get_hermes_home": sc("/h1"), "path.exists": sc(True), "prefill.load": sc([])})

# ── _preflight_or_block ───────────────────────────────────────────────────
NOWISO = "2026-11-18T12:00:00+00:00"
case("_preflight_or_block", job={}, job_id="j1", job_name="Job", cfg={},
     io={"_cron_preflight_enabled": sc(False)})
case("_preflight_or_block", job={}, job_id="j1", job_name="Job", cfg={},
     io={"_cron_preflight_enabled": sc(True), "_preflight_job_config": sc(None)})
case("_preflight_or_block", job={"preflight_alerted": True}, job_id="j1", job_name="Job", cfg={},
     io={"_cron_preflight_enabled": sc(True), "_preflight_job_config": sc(None),
         "clear_preflight_alerted": sc(None)})
case("_preflight_or_block", job={}, job_id="j1", job_name="Job", cfg={},
     io={"_cron_preflight_enabled": sc(True), "_preflight_job_config": sc("missing key"),
         "mark_preflight_alerted": sc(False), "_hermes_now": sc(NOWISO)})
case("_preflight_or_block", job={}, job_id="j1", job_name="Job", cfg={},
     io={"_cron_preflight_enabled": sc(True), "_preflight_job_config": rz("RuntimeError", "pf boom")})
case("_preflight_or_block", job={"preflight_alerted": True}, job_id="j1", job_name="Job", cfg={},
     io={"_cron_preflight_enabled": sc(True), "_preflight_job_config": sc(None),
         "clear_preflight_alerted": rz("RuntimeError", "clear boom")})
case("_preflight_or_block", job={}, job_id="j1", job_name="Job", cfg={},
     io={"_cron_preflight_enabled": rz("SystemExit", "bye")})

# ── _blocked_config_result ────────────────────────────────────────────────
case("_blocked_config_result", job_id="j1", job_name="Job", _pf_reason="missing key",
     io={"mark_preflight_alerted": sc(False), "_hermes_now": sc(NOWISO)})
case("_blocked_config_result", job_id="j1", job_name="Job", _pf_reason="missing key",
     io={"mark_preflight_alerted": sc(True), "_hermes_now": sc(NOWISO)})
case("_blocked_config_result", job_id="j1", job_name="Job", _pf_reason="missing key",
     io={"mark_preflight_alerted": rz("RuntimeError", "mark boom"), "_hermes_now": sc(NOWISO)})
case("_blocked_config_result", job_id="j1", job_name="Job", _pf_reason="missing key",
     io={"_hermes_now": sc(NOWISO)}, import_fail=["cron.jobs"])
case("_blocked_config_result", job_id="j1", job_name="Job", _pf_reason="missing key",
     io={"mark_preflight_alerted": sc(False), "_hermes_now": rz("RuntimeError", "now boom")})
case("_blocked_config_result", job_id="j1", job_name="Job", _pf_reason="missing key",
     io={"mark_preflight_alerted": rz("KeyboardInterrupt", "int")})

# ── _load_credential_pool ─────────────────────────────────────────────────
POOL = {"has_credentials": True, "entries": [{"k": 1}, {"k": 2}]}
case("_load_credential_pool", runtime={"provider": "OpenAI"}, job_id="j1",
     io={"load_route_pool": sc(POOL)})
case("_load_credential_pool", runtime={"provider": ""}, job_id="j1", io={})
case("_load_credential_pool", runtime={"provider": "openai"}, job_id="j1",
     io={"load_route_pool": sc(None)})
case("_load_credential_pool", runtime={"provider": "openai"}, job_id="j1",
     io={"load_route_pool": sc({"has_credentials": False, "entries": []})})
case("_load_credential_pool", runtime={"provider": "openai"}, job_id="j1",
     io={"load_route_pool": rz("RuntimeError", "pool boom")})
case("_load_credential_pool", runtime={"provider": "openai"}, job_id="j1",
     io={}, import_fail=["agent.custom_entry_pool"])
case("_load_credential_pool", runtime={"provider": "  OpenAI  ", "base_url": "http://x",
                                       "requested_provider": "rp"}, job_id="j1",
     io={"load_route_pool": sc(POOL)})

# ── _init_cron_mcp_tools ──────────────────────────────────────────────────
case("_init_cron_mcp_tools", job_id="j1", io={"discover_mcp_tools": sc(["t1", "t2"])})
case("_init_cron_mcp_tools", job_id="j1", io={"discover_mcp_tools": sc(None)})
case("_init_cron_mcp_tools", job_id="j1", io={"discover_mcp_tools": sc([])})
case("_init_cron_mcp_tools", job_id="j1", io={"discover_mcp_tools": rz("RuntimeError", "mcp boom")})
case("_init_cron_mcp_tools", job_id="j1", io={}, import_fail=["tools.mcp_tool_discovery"])
case("_init_cron_mcp_tools", job_id="j1", io={"discover_mcp_tools": rz("SystemExit", "bye")})

# ── _raise_inactivity_timeout ─────────────────────────────────────────────
SUMM = {"last_activity_desc": "tool call", "seconds_since_activity": 120.0,
        "api_call_count": 3, "max_iterations": 50, "current_tool": "bash"}
case("_raise_inactivity_timeout", agent={"id": "a1", "has_activity": True},
     job_name="Job", limit_s=300.0, io={"agent.get_activity_summary": sc(SUMM),
                                        "request_hard_interrupt": sc(None)})
case("_raise_inactivity_timeout", agent={"id": "a1", "has_activity": False},
     job_name="Job", limit_s=300.0, io={"request_hard_interrupt": sc(None)})
case("_raise_inactivity_timeout", agent={"id": "a1", "has_activity": True},
     job_name="Job", limit_s=300.0, io={"agent.get_activity_summary": rz("RuntimeError", "summ boom"),
                                        "request_hard_interrupt": sc(None)})
case("_raise_inactivity_timeout", agent={"id": "a1", "has_activity": True},
     job_name="Job", limit_s=300.0, io={"agent.get_activity_summary": sc(None)})
case("_raise_inactivity_timeout", agent={"id": "a1", "has_activity": False},
     job_name="Job", limit_s=300.0, io={"request_hard_interrupt": rz("RuntimeError", "int boom")})
case("_raise_inactivity_timeout", agent={"id": "a1", "has_activity": True},
     job_name="Job", limit_s=300.0, io={"agent.get_activity_summary": rz("SystemExit", "bye")})
case("_raise_inactivity_timeout", agent={"id": "a1", "has_activity": True},
     job_name="Job", limit_s=300.0, io={"agent.get_activity_summary": sc({"current_tool": None}),
                                        "request_hard_interrupt": sc(None)})

# ── _run_doc_header ───────────────────────────────────────────────────────
case("_run_doc_header", job={"schedule_display": "every hour"}, title="T", job_id="j1",
     prompt="do it", io={"_hermes_now": sc(NOWISO)})
case("_run_doc_header", job={}, title="T", job_id="j1", prompt="do it",
     io={"_hermes_now": sc(NOWISO)})
case("_run_doc_header", job={"schedule_display": "daily"}, title="T", job_id="j1",
     prompt="p", prompt_stamp="STAMP", io={"_hermes_now": sc(NOWISO)})
case("_run_doc_header", job={}, title="T", job_id="j1", prompt="p",
     io={"_hermes_now": rz("RuntimeError", "now boom")})
case("_run_doc_header", job={}, title="T", job_id="j1", prompt="p", io={})
case("_run_doc_header", job={"schedule_display": 5}, title="T", job_id="j1", prompt="p",
     io={"_hermes_now": sc(NOWISO)})

# ── _reload_dotenv_and_publish_delivery_target ────────────────────────────
TARGET = {"platform": "telegram", "chat_id": 123, "thread_id": 9}
case("_reload_dotenv_and_publish_delivery_target", job={"id": "j1"},
     io={"_get_hermes_home": sc("/h1"), "reset_secret_source_cache": sc(None),
         "load_hermes_dotenv": sc(None), "_resolve_delivery_target": sc(TARGET),
         "var_map.set": sc(None)})
case("_reload_dotenv_and_publish_delivery_target", job={"id": "j1"},
     io={"_get_hermes_home": sc("/h1"), "reset_secret_source_cache": sc(None),
         "load_hermes_dotenv": sc(None), "_resolve_delivery_target": sc(None)})
case("_reload_dotenv_and_publish_delivery_target", job={"id": "j1"},
     io={"_get_hermes_home": sc("/h1"), "reset_secret_source_cache": sc(None),
         "load_hermes_dotenv": sc(None), "_resolve_delivery_target": sc({"chat_id": 1})})
case("_reload_dotenv_and_publish_delivery_target", job={"id": "j1"},
     io={"_get_hermes_home": sc("/h1"), "reset_secret_source_cache": sc(None),
         "load_hermes_dotenv": sc(None),
         "_resolve_delivery_target": sc({"platform": "telegram", "chat_id": "c1", "thread_id": None}),
         "var_map.set": sc(None)})
case("_reload_dotenv_and_publish_delivery_target", job={"id": "j1"},
     io={"_get_hermes_home": sc("/h1"), "reset_secret_source_cache": sc(None),
         "load_hermes_dotenv": sc(None), "_resolve_delivery_target": rz("RuntimeError", "dt boom")})
case("_reload_dotenv_and_publish_delivery_target", job={"id": "j1"},
     io={}, import_fail=["hermes_cli.env_loader"])
case("_reload_dotenv_and_publish_delivery_target", job={"id": "j1"},
     io={"_get_hermes_home": sc("/h1"), "reset_secret_source_cache": sc(None),
         "load_hermes_dotenv": sc(None), "_resolve_delivery_target": sc(TARGET),
         "var_map.set": rz("RuntimeError", "set boom")})

# ── _install_fire_secret_scope ────────────────────────────────────────────
case("_install_fire_secret_scope",
     io={"_get_hermes_home": sc("/h1"), "hydrate_profile_secret_sources": sc(None),
         "owner_secret_scope": sc("scope1"), "set_secret_scope": sc("tokS"),
         "routed_profile_fire": sc(True), "set_multiplex_context": sc("tokC")})
case("_install_fire_secret_scope",
     io={"_get_hermes_home": sc("/h1"), "hydrate_profile_secret_sources": sc(None),
         "owner_secret_scope": sc("scope1"), "set_secret_scope": sc("tokS"),
         "routed_profile_fire": sc(False)})
case("_install_fire_secret_scope",
     io={"_get_hermes_home": sc("/h1"), "hydrate_profile_secret_sources": rz("RuntimeError", "hyd boom")})
case("_install_fire_secret_scope", io={}, import_fail=["agent.secret_scope"])
case("_install_fire_secret_scope",
     io={"_get_hermes_home": sc("/h1"), "hydrate_profile_secret_sources": sc(None),
         "owner_secret_scope": sc("scope1"), "set_secret_scope": rz("RuntimeError", "set boom")})
case("_install_fire_secret_scope",
     io={"_get_hermes_home": sc("/h1"), "hydrate_profile_secret_sources": sc(None),
         "owner_secret_scope": sc("scope1"), "set_secret_scope": sc("tokS"),
         "routed_profile_fire": rz("SystemExit", "bye")})

# ── _reset_fire_secret_scope ──────────────────────────────────────────────
case("_reset_fire_secret_scope", tokens=["tokS", "tokC"],
     io={"reset_multiplex_context": sc(None), "reset_secret_scope": sc(None)})
case("_reset_fire_secret_scope", tokens=["tokS", None], io={"reset_secret_scope": sc(None)})
case("_reset_fire_secret_scope", tokens=None, io={})
case("_reset_fire_secret_scope", tokens=["tokS"], io={})
case("_reset_fire_secret_scope", tokens=["a", "b", "c"], io={})
case("_reset_fire_secret_scope", tokens=["tokS", "tokC"],
     io={"reset_multiplex_context": rz("RuntimeError", "reset boom")})
case("_reset_fire_secret_scope", tokens=["tokS", "tokC"], io={}, import_fail=["agent.secret_scope"])

# ── _start_owned_run ──────────────────────────────────────────────────────
case("_start_owned_run", job={"id": "j1"}, execution_id="e1",
     io={"os.environ.get": sc("e1"), "get_execution": sc({"id": "e1", "status": "running"}),
         "enter_cron_execution": sc("tok")})
case("_start_owned_run", job={"id": "j1"}, execution_id="e1",
     io={"os.environ.get": sc("other"), "mark_execution_running": sc({"id": "e1"}),
         "enter_cron_execution": sc("tok")})
case("_start_owned_run", job={"id": "j1"}, execution_id="e1",
     io={"os.environ.get": sc("other"), "mark_execution_running": sc(None)})
case("_start_owned_run", job={"id": "j1"}, execution_id="e1",
     io={"os.environ.get": sc("e1"), "get_execution": rz("RuntimeError", "get boom")})
case("_start_owned_run", job={"id": "j1"}, execution_id="e1",
     io={"os.environ.get": sc(None), "mark_execution_running": sc({"id": "e1"}),
         "enter_cron_execution": sc("tok")})
case("_start_owned_run", job={"id": "j1"}, execution_id="e1",
     io={"os.environ.get": sc("other"), "mark_execution_running": sc({"id": "e1"}),
         "enter_cron_execution": rz("RuntimeError", "enter boom")})
case("_start_owned_run", job={"id": "j1"}, execution_id="e1",
     io={"os.environ.get": sc("other"), "mark_execution_running": sc({}),
         "enter_cron_execution": sc("tok")})

# ── _maybe_run_worktree_maintenance ───────────────────────────────────────
case("_maybe_run_worktree_maintenance", state={"_last_worktree_maintenance_at": None},
     io={"get_hermes_home": sc("/h1"), "time.monotonic": sc(1000.0),
         "thread.start": sc(None), "run_sweep": sc(None)})
case("_maybe_run_worktree_maintenance", state={"_last_worktree_maintenance_at": {"/h1": 900.0}},
     io={"get_hermes_home": sc("/h1"), "time.monotonic": sc(1000.0)})
case("_maybe_run_worktree_maintenance", state={"_last_worktree_maintenance_at": {"/h1": 0.0}},
     io={"get_hermes_home": sc("/h1"), "time.monotonic": sc(100000.0),
         "thread.start": sc(None), "run_sweep": sc(None)})
case("_maybe_run_worktree_maintenance", state={"_last_worktree_maintenance_at": None},
     io={}, import_fail=["hermes_constants"])
case("_maybe_run_worktree_maintenance", state={"_last_worktree_maintenance_at": None},
     io={"get_hermes_home": sc("/h1"), "time.monotonic": sc(1000.0), "thread.start": sc(None)},
     import_fail=["cron.scheduler_worktree_maintenance"])
case("_maybe_run_worktree_maintenance", state={"_last_worktree_maintenance_at": None},
     io={"get_hermes_home": sc("/h1"), "time.monotonic": sc(1000.0),
         "thread.start": sc(None), "run_sweep": rz("RuntimeError", "sweep boom")})
case("_maybe_run_worktree_maintenance", state={"_last_worktree_maintenance_at": None},
     io={"get_hermes_home": sc("/h1"), "time.monotonic": rz("RuntimeError", "clock boom")})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    # per-function coverage assertion (>=5 each)
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        raise SystemExit(f"FATAL: functions with <5 cases: {short}")
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} functions -> {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()
