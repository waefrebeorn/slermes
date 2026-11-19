#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_status_flow.jsonl for the
gateway/status.py liveness/status FLOW oracle (t_port_gateway_status_flow.c /
oracle_gateway_status_flow.py). Pure fixture construction — no quarry import.

Case shape: {"func": <harness name>, "in": {..., "seams": {name: script}}}
where a script is a single JSON value (reused per call) or an array (consumed
per call, clamped at the last entry); {"__raise__": cls, "msg": ...} scripts an
exception; a scripted null models Python None.

Domain rules (mirrored by the C port and the oracle):
- str()-coerced numbers use int literals or non-integral floats;
- heartbeat epochs/timestamps use whole or .5 seconds;
- platform keys / path scripts are strings or null;
- scripted raises never use KeyError (live KeyError paths run through missing
  record keys instead).
"""
import json
import os
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_status_flow.jsonl")

BASE = int(datetime(2026, 11, 18, tzinfo=timezone.utc).timestamp())
STAMP = "2026-11-18T00:00:00+00:00"

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_spec(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


PID_REC = {"kind": "hermes-gateway", "pid": 10, "argv": ["gateway", "run"], "start_time": 900}
IDENT = {"code_sha": "abc", "code_version": "1.0"}
PATH_SCRIPT = "/h/gateway_state.json"

# ── live_pid_from_record ──
add("live_pid_from_record", record={"pid": 42, "start_time": 1000},
    seams={"pid_from_record": 42, "pid_exists": True, "get_process_start_time": 1000})
add("live_pid_from_record", record=None, seams={"pid_from_record": None})
add("live_pid_from_record", record={"pid": 7},
    seams={"pid_from_record": 7, "pid_exists": False})
add("live_pid_from_record", record={"pid": 7, "start_time": 100},
    seams={"pid_from_record": 7, "pid_exists": True, "get_process_start_time": 200})
add("live_pid_from_record", record={"pid": 7},
    seams={"pid_from_record": 7, "pid_exists": True, "get_process_start_time": None})
add("live_pid_from_record", record={"pid": 7, "start_time": None},
    seams={"pid_from_record": 7, "pid_exists": True, "get_process_start_time": 500})
add("live_pid_from_record", record={"pid": 7},
    seams={"pid_from_record": 7, "pid_exists": raise_spec("OSError", "probe failed")})
add("live_pid_from_record", record=None,
    seams={"pid_from_record": 42, "pid_exists": True})
add("live_pid_from_record", record={"pid": 7, "start_time": True},
    seams={"pid_from_record": 7, "pid_exists": True, "get_process_start_time": 1})

# ── read_live_pid_marker ──
MARK = {"target_pid": 9, "written_at": STAMP, "target_start_time": 55}
add("read_live_pid_marker", path="/h/.gateway-takeover.json", ttl_s=60,
    seams={"read_json_file": MARK, "pid_from_record": 9, "marker_is_stale": False})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60, seams={"read_json_file": {}})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60, seams={"read_json_file": None})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60,
    seams={"read_json_file": {"written_at": STAMP}, "pid_from_record": None,
           "unlink_quietly": None})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60,
    seams={"read_json_file": MARK, "pid_from_record": 9, "marker_is_stale": True,
           "unlink_quietly": None})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60,
    seams={"read_json_file": {"accepted": True, "written_at": STAMP},
           "pid_from_record": None, "update_pause_on_disk": True})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60,
    seams={"read_json_file": {"accepted": True, "written_at": STAMP},
           "pid_from_record": None, "update_pause_on_disk": False, "unlink_quietly": None})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60,
    seams={"read_json_file": {"accepted": 1, "written_at": STAMP},
           "pid_from_record": None, "unlink_quietly": None})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60,
    seams={"read_json_file": MARK, "pid_from_record": 9, "marker_is_stale": True,
           "unlink_quietly": raise_spec("OSError", "busy")})
add("read_live_pid_marker", path="/h/m.json", ttl_s=60,
    seams={"read_json_file": {"target_pid": 9, "written_at": 0},
           "pid_from_record": 9, "marker_is_stale": True, "unlink_quietly": None})

# ── multiplexer_liveness ──
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": None})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": ""})
add("multiplexer_liveness", profile_dir="/elsewhere/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": False})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": True,
           "host_gateway_topology": {"pid": 100, "home": None},
           "topology.serves": True,
           "read_runtime_status": {"gateway_state": "running"}})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": True,
           "host_gateway_topology": {"pid": 100, "home": "/launch"},
           "topology.serves": True, "read_runtime_status": None})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": True, "host_gateway_topology": None,
           "named_profile_served_by_running_multiplexer": True,
           "live_default_gateway_pid": 77, "read_runtime_status": None})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": True, "host_gateway_topology": {"pid": 100},
           "topology.serves": False,
           "named_profile_served_by_running_multiplexer": False})
add("multiplexer_liveness", profile_dir="/root/.hermes",
    seams={"profile_name_for_home": "default",
           "host_gateway_topology": {"pid": 5}, "topology.serves": True,
           "get_default_hermes_root": "/root/.hermes",
           "read_runtime_status": {"gateway_state": "running"}})
add("multiplexer_liveness", profile_dir="/root/.hermes",
    seams={"profile_name_for_home": "default", "host_gateway_topology": None,
           "get_default_hermes_root": "/root/.hermes"})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": True, "host_gateway_topology": {"pid": None},
           "topology.serves": True})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": True, "host_gateway_topology": {"pid": 100},
           "topology.serves": raise_spec("RuntimeError", "roster wedged")})
add("multiplexer_liveness", profile_dir="/root/profiles/ops",
    seams={"profile_name_for_home": "ops", "get_default_hermes_root": "/root",
           "same_hermes_home": True, "host_gateway_topology": None,
           "named_profile_served_by_running_multiplexer": True,
           "live_default_gateway_pid": None})

# ── live_gateway_pid_for_home ──
add("live_gateway_pid_for_home", home="/h", seams={"get_running_pid_cached": 42})
add("live_gateway_pid_for_home", home="/h",
    seams={"get_running_pid_cached": None, "read_runtime_status": {"pid": 9},
           "get_runtime_status_running_pid": 9})
add("live_gateway_pid_for_home", home="/h",
    seams={"get_running_pid_cached": None, "read_runtime_status": None,
           "get_runtime_status_running_pid": None})
add("live_gateway_pid_for_home", home="/h", seams={"get_running_pid_cached": 0})
add("live_gateway_pid_for_home", home="/h",
    seams={"get_running_pid_cached": None,
           "read_runtime_status": raise_spec("OSError", "gone")})
add("live_gateway_pid_for_home", home="/h",
    seams={"get_running_pid_cached": None, "read_runtime_status": {"pid": 9},
           "get_runtime_status_running_pid": raise_spec("RuntimeError", "bad")})
add("live_gateway_pid_for_home", home="/h/", seams={"get_running_pid_cached": 42})

# ── prepare_runtime_status ──
PREP_SEAMS = {
    "get_runtime_status_path": PATH_SCRIPT,
    "build_pid_record": PID_REC,
    "utc_now_iso": STAMP,
    "get_code_identity_fields": IDENT,
}


def prep(fields=None, st=None, extra_seams=None, **kw):
    seams = dict(PREP_SEAMS)
    seams.update(extra_seams or {})
    inp = {"fields": fields or {}, "seams": seams}
    if st is not None:
        inp["st"] = st
    inp.update(kw)
    add("prepare_runtime_status", **inp)


BUILD_REC = {"gateway_state": "starting", "platforms": {}}
prep(extra_seams={"read_json_file": None, "build_runtime_status_record": BUILD_REC})
prep(fields={"gateway_state": "degraded"},
     st={"state_path": PATH_SCRIPT,
         "state": {"gateway_state": "running",
                   "platforms": {"telegram": {"state": "connected"}}}})
prep(fields={"load_existing": False},
     extra_seams={"build_runtime_status_record": BUILD_REC})
prep(fields={"reload_existing": True},
     st={"state_path": PATH_SCRIPT, "state": {"gateway_state": "starting"}},
     extra_seams={"read_json_file": {"gateway_state": "stopped"}})
prep(extra_seams={"read_json_file": {}, "build_runtime_status_record": BUILD_REC})
prep(fields={"platform": "telegram", "platform_state": "connected"},
     st={"state_path": PATH_SCRIPT,
         "state": {"platforms": {"telegram": {"state": "connecting",
                                              "needs_attention": True,
                                              "retrying_since": 5}}}})
prep(fields={"platform": "discord", "platform_state": "error",
             "error_code": "E1", "needs_attention": True},
     st={"state_path": PATH_SCRIPT, "state": {"platforms": {}}})
prep(fields={"clear_profile_platforms": True},
     st={"state_path": PATH_SCRIPT,
         "state": {"platforms": {"telegram": {"s": 1}, "ops:telegram": {"s": 2},
                                 "webhook": {"s": 3}}}})
prep(fields={"drop_profile_platforms": "ops", "keep_platforms": ["ops:webhook"]},
     st={"state_path": PATH_SCRIPT,
         "state": {"platforms": {"ops:telegram": {}, "ops:webhook": {},
                                 "other:telegram": {}, "flat": {}}}})
prep(fields={"active_agents": "5"}, extra_seams={"parse_active_agents": 5},
     st={"state_path": PATH_SCRIPT, "state": {}})
prep(fields={"session_store": {"status": "ok"}},
     st={"state_path": PATH_SCRIPT, "state": {}})
prep(fields={"served_profiles": ["ops", "default"], "parked_profiles": {"ops": 5},
             "active_work": ["a"], "multiplex_standalone_reason": 0},
     st={"state_path": PATH_SCRIPT, "state": {}})
prep(extra_seams={"read_json_file": None, "build_runtime_status_record": BUILD_REC,
                  "build_pid_record": {"pid": 10, "argv": [], "start_time": 1}})
prep(fields={"restart_requested": "yes"},
     st={"state_path": PATH_SCRIPT, "state": {}})
prep(fields={"exit_reason": None},
     st={"state_path": PATH_SCRIPT, "state": {"exit_reason": "old"}})
prep(st={"state_path": PATH_SCRIPT, "state": {"platforms": [1]}})

# ── publish_runtime_status ──
PUB_SEAMS = dict(PREP_SEAMS)
PUB_SEAMS.update({"read_json_file": None, "build_runtime_status_record": BUILD_REC,
                  "get_runtime_status_writer": {"id": "w1"}, "writer.submit": 1,
                  "emit_runtime_status_transition": None})


def pub(fields=None, st=None, extra_seams=None):
    seams = dict(PUB_SEAMS)
    seams.update(extra_seams or {})
    inp = {"fields": fields or {}, "seams": seams}
    if st is not None:
        inp["st"] = st
    add("publish_runtime_status", **inp)


pub(fields={"gateway_state": "running"})
pub(fields={"gateway_state": "running"},
    extra_seams={"writer.submit": raise_spec("OSError", "disk full")})
pub(fields={"gateway_state": "running"},
    extra_seams={"emit_runtime_status_transition": raise_spec("ValueError", "bad")})
pub(fields={"gateway_state": "running"},
    extra_seams={"emit_runtime_status_transition": raise_spec("SystemExit", "1")})
pub(fields={"gateway_state": "running"},
    extra_seams={"get_runtime_status_writer": raise_spec("RuntimeError", "no writer")})
pub(fields={"platform": "telegram", "platform_state": "connected"},
    st={"state_path": PATH_SCRIPT, "state": {"platforms": {}}})
pub(fields={"gateway_state": "starting"},
    st={"state_path": "/other/gateway_state.json", "state": {"gateway_state": "old"}})

# ── probe_lock_file ──
add("probe_lock_file", handle="h1",
    seams={"try_acquire_file_lock": True, "release_file_lock": None, "handle.close": None})
add("probe_lock_file", handle="h1",
    seams={"try_acquire_file_lock": False, "handle.close": None})
add("probe_lock_file", handle="h1",
    seams={"try_acquire_file_lock": raise_spec("BlockingIOError", "held"),
           "handle.close": None})
add("probe_lock_file", handle="h1",
    seams={"try_acquire_file_lock": False,
           "handle.close": raise_spec("OSError", "close failed")})
add("probe_lock_file", handle="h1",
    seams={"try_acquire_file_lock": False,
           "handle.close": raise_spec("ValueError", "closed twice")})
add("probe_lock_file", handle="h1",
    seams={"try_acquire_file_lock": raise_spec("OSError", "acquire failed"),
           "handle.close": raise_spec("ValueError", "closed twice")})
add("probe_lock_file", handle="h1",
    seams={"try_acquire_file_lock": True,
           "release_file_lock": raise_spec("PermissionError", "denied"),
           "handle.close": None})

# ── terminate_verified_owner ──
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "exited"})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "unknown"})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "same", "terminate_pid": None,
           "wait_for_scoped_lock_owner_exit": [True, False]})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "same", "terminate_pid": None,
           "wait_for_scoped_lock_owner_exit": [[False, True], [True, False]]})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "same", "terminate_pid": None,
           "wait_for_scoped_lock_owner_exit": [False, False]})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "same",
           "terminate_pid": raise_spec("ProcessLookupError", "gone")})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "same",
           "terminate_pid": raise_spec("PermissionError", "denied")})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "same",
           "terminate_pid": raise_spec("ValueError", "junk")})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    graceful_attempts=3, force_attempts=2,
    seams={"scoped_lock_owner_state": "same", "terminate_pid": None,
           "wait_for_scoped_lock_owner_exit": [False, True]})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    seams={"scoped_lock_owner_state": "same", "terminate_pid": None,
           "wait_for_scoped_lock_owner_exit": None})
add("terminate_verified_owner", owner_pid=5, owner_start_time=100,
    graceful_attempts=0,
    seams={"scoped_lock_owner_state": "same", "terminate_pid": None,
           "wait_for_scoped_lock_owner_exit": [False, True]})

# ── host_gateway_serves_home ──
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": None})
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": {"pid": 1}})
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": {"pid": 2}, "host_profile_name_for_home": "ops",
           "owner.serves": True})
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": {"pid": 2}, "host_profile_name_for_home": "ops",
           "owner.serves": False})
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": raise_spec("ImportError", "no host_attach")})
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": raise_spec("SystemExit", "1")})
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": {"pid": 2}, "host_profile_name_for_home": "ops",
           "owner.serves": raise_spec("RuntimeError", "wedged")})
add("host_gateway_serves_home", pid=1, profile_home="/root/profiles/ops",
    seams={"host_gateway": {"pid": True}, "host_profile_name_for_home": "ops",
           "owner.serves": True})
add("host_gateway_serves_home", pid=2, profile_home="/root/profiles/ops",
    seams={"host_gateway": {"pid": 2},
           "host_profile_name_for_home": raise_spec("RuntimeError", "bad home")})

# ── merge_over_on_disk ──
add("merge_over_on_disk", path="/h/gateway_state.json", payload={"b": 9, "c": 3},
    seams={"read_json_file": {"a": 1, "b": 2}})
add("merge_over_on_disk", path="/h/gateway_state.json", payload={"a": 1},
    seams={"read_json_file": None})
add("merge_over_on_disk", path="/h/gateway_state.json", payload={"a": 1},
    seams={"read_json_file": [1, 2]})
add("merge_over_on_disk", path="/h/gateway_state.json", payload={"a": 1},
    seams={"read_json_file": {}})
add("merge_over_on_disk", path="/h/gateway_state.json", payload={"a": 1},
    seams={"read_json_file": raise_spec("OSError", "gone")})
add("merge_over_on_disk", path="/h/gateway_state.json", payload=None,
    seams={"read_json_file": {"a": 1}})
add("merge_over_on_disk", path="/h/gateway_state.json", payload={},
    seams={"read_json_file": {"a": 1, "b": 2}})

# ── start_times_conflict ──
add("start_times_conflict", recorded=100, current=100)
add("start_times_conflict", recorded=100, current=200)
add("start_times_conflict", recorded=None, current=100)
add("start_times_conflict", recorded=100, current=None)
add("start_times_conflict", recorded=None, current=None)
add("start_times_conflict", recorded=True, current=1)
add("start_times_conflict", recorded="100", current=100)
add("start_times_conflict", recorded=100.5, current=100.5)
add("start_times_conflict", recorded=0, current=0)
add("start_times_conflict", recorded=False, current=0)

# ── pid_marker_names_self ──
add("pid_marker_names_self", target_pid=1, target_start_time=500,
    seams={"getpid": 2})
add("pid_marker_names_self", target_pid=2, target_start_time=500,
    seams={"getpid": 2, "get_process_start_time": 500})
add("pid_marker_names_self", target_pid=2, target_start_time=None,
    seams={"getpid": 2, "get_process_start_time": 500})
add("pid_marker_names_self", target_pid=2, target_start_time=500,
    seams={"getpid": 2, "get_process_start_time": None})
add("pid_marker_names_self", target_pid=2, target_start_time=500,
    seams={"getpid": 2, "get_process_start_time": 600})
add("pid_marker_names_self", target_pid=2, target_start_time=500,
    seams={"getpid": raise_spec("OSError", "no pid")})
add("pid_marker_names_self", target_pid=2, target_start_time=True,
    seams={"getpid": 2, "get_process_start_time": 1})

# ── heartbeat_age_s ──
add("heartbeat_age_s", record=None)
add("heartbeat_age_s", record=[1])
add("heartbeat_age_s", record={}, seams={"normalize_updated_at": None})
add("heartbeat_age_s", record={"updated_at": STAMP},
    seams={"normalize_updated_at": STAMP, "datetime_now": BASE + 30})
add("heartbeat_age_s", record={"updated_at": STAMP},
    seams={"normalize_updated_at": STAMP, "datetime_now": BASE - 10})
add("heartbeat_age_s", record={"updated_at": "x"},
    seams={"normalize_updated_at": ""})
add("heartbeat_age_s", record={"updated_at": "x"},
    seams={"normalize_updated_at": "garbage", "datetime_now": BASE})
add("heartbeat_age_s", record={"updated_at": "x"},
    seams={"normalize_updated_at": "2026-11-18T00:00:00", "datetime_now": BASE})
add("heartbeat_age_s", record={"updated_at": "x"},
    seams={"normalize_updated_at": "2026-11-18T00:00:00.500000+00:00",
           "datetime_now": BASE + 10.5})
add("heartbeat_age_s", record={"updated_at": "x"},
    seams={"normalize_updated_at": "2026-11-18T05:30:00+05:30",
           "datetime_now": BASE + 7})
add("heartbeat_age_s", record={"updated_at": "x"},
    seams={"normalize_updated_at": raise_spec("ValueError", "bad stamp")})
add("heartbeat_age_s", record={"updated_at": "x"},
    seams={"normalize_updated_at": 5, "datetime_now": BASE})

# ── coerce_session_store ──
add("coerce_session_store", session_store={"status": "ok"})
add("coerce_session_store", session_store={"status": "unavailable"})
add("coerce_session_store", session_store={"status": "retrying"})
add("coerce_session_store", session_store={"status": "bogus"})
add("coerce_session_store", session_store={"status": None})
add("coerce_session_store", session_store={"status": 0})
add("coerce_session_store", session_store={"status": True})
add("coerce_session_store", session_store={"status": 5})
add("coerce_session_store", session_store=None)
add("coerce_session_store", session_store={})
add("coerce_session_store", session_store={"status": "ok", "extra": 1})
add("coerce_session_store", session_store={"status": "OK"})

# ── profile_platforms ──
add("profile_platforms", runtime=None, profile="ops")
add("profile_platforms", runtime={}, profile="ops")
add("profile_platforms", runtime={"platforms": [1]}, profile="ops")
add("profile_platforms", profile="ops",
    runtime={"platforms": {"ops:telegram": {"state": "connected"},
                           "ops:discord": {"state": "error"},
                           "default:telegram": {"state": "connected"},
                           "flat": {"state": "connected"}}},
    seams={"shared_listener_mirror_platforms": {}})
add("profile_platforms", profile="default",
    runtime={"platforms": {"default:telegram": {"v": 1}, "telegram": {"v": 2},
                           "webhook": {"v": 3}}},
    seams={"shared_listener_mirror_platforms": {}})
add("profile_platforms", profile="ops",
    runtime={"platforms": {"ops:telegram": {"v": 2}}},
    seams={"shared_listener_mirror_platforms": {"api_server": {"m": 1}}})
add("profile_platforms", profile="ops",
    runtime={"platforms": {"ops:telegram": {"v": 2}}},
    seams={"shared_listener_mirror_platforms": {"telegram": {"m": 1}}})
add("profile_platforms", profile="ops",
    runtime={"platforms": {"ops:telegram": [1], "ops:discord": {}}},
    seams={"shared_listener_mirror_platforms": {}})
add("profile_platforms", runtime=[1], profile="ops")
add("profile_platforms", profile="ops", runtime={"platforms": {}},
    seams={"shared_listener_mirror_platforms": raise_spec("OSError", "no")})
add("profile_platforms", profile="ops", runtime={"platforms": {}},
    seams={"shared_listener_mirror_platforms": None})

# ── profile_name_for_home ──
add("profile_name_for_home", profile_home="/root/profiles/ops",
    seams={"profile_name_for_home_priv": "ops"})
add("profile_name_for_home", profile_home="/root/.hermes",
    seams={"profile_name_for_home_priv": None,
           "path_resolve": ["/root/.hermes", "/root/.hermes"],
           "get_default_hermes_root": "/root/.hermes"})
add("profile_name_for_home", profile_home="/elsewhere",
    seams={"profile_name_for_home_priv": None,
           "path_resolve": ["/elsewhere", "/root/.hermes"],
           "get_default_hermes_root": "/root/.hermes"})
add("profile_name_for_home", profile_home="/root/.hermes",
    seams={"profile_name_for_home_priv": "",
           "path_resolve": ["/root/.hermes", "/root/.hermes"],
           "get_default_hermes_root": "/root/.hermes"})
add("profile_name_for_home", profile_home="/root/profiles/ops",
    seams={"profile_name_for_home_priv": raise_spec("RuntimeError", "bad")})
add("profile_name_for_home", profile_home="/root/.hermes",
    seams={"profile_name_for_home_priv": None,
           "path_resolve": raise_spec("OSError", "EACCES")})
add("profile_name_for_home", profile_home="/root/.hermes",
    seams={"profile_name_for_home_priv": None, "path_resolve": "/root/.hermes",
           "get_default_hermes_root": raise_spec("ImportError", "no constants")})
add("profile_name_for_home", profile_home="/root/.hermes",
    seams={"profile_name_for_home_priv": None, "path_resolve": "/root/.hermes",
           "get_default_hermes_root": None})
add("profile_name_for_home", profile_home="/Root/.Hermes",
    seams={"profile_name_for_home_priv": None,
           "path_resolve": ["/root/.hermes", "/root/.hermes"],
           "get_default_hermes_root": "/root/.hermes"})
add("profile_name_for_home", profile_home="/root/.hermes",
    seams={"profile_name_for_home_priv": None,
           "path_resolve": ["/root/.hermes", raise_spec("OSError", "EACCES")],
           "get_default_hermes_root": "/root/.hermes"})

# ── apply_set_fields ──
add("apply_set_fields", target={"a": 1}, fields=[{"key": "b", "value": 2}])
add("apply_set_fields", target={"a": 1}, fields=[{"key": "b"}])
add("apply_set_fields", target={"a": 1}, fields=[{"key": "b", "value": None}])
add("apply_set_fields", target={},
    fields=[{"key": "x", "value": "yes", "coerce": "bool"},
            {"key": "y", "value": 0, "coerce": "bool"},
            {"key": "z", "value": [], "coerce": "bool"}])
add("apply_set_fields", target={}, fields=[{"key": "n", "value": "7",
                                            "coerce": "parse_active_agents"}],
    seams={"parse_active_agents": 7})
add("apply_set_fields", target={},
    fields=[{"key": "w", "value": ["x", "y"], "coerce": "active_work"},
            {"key": "w2", "value": None, "coerce": "active_work"},
            {"key": "w3", "value": [], "coerce": "active_work"}])
add("apply_set_fields", target={},
    fields=[{"key": "s", "value": None, "coerce": "served_profiles"},
            {"key": "s2", "value": ["a"], "coerce": "served_profiles"}])
add("apply_set_fields", target={},
    fields=[{"key": "p", "value": {"ops": 5, "x": None}, "coerce": "parked_profiles"},
            {"key": "p2", "value": None, "coerce": "parked_profiles"}])
add("apply_set_fields", target={},
    fields=[{"key": "r", "value": "r", "coerce": "multiplex_reason"},
            {"key": "r2", "value": 0, "coerce": "multiplex_reason"},
            {"key": "r3", "value": 5, "coerce": "multiplex_reason"}])
add("apply_set_fields", target={},
    fields=[{"key": "ss", "value": {"status": "ok"}, "coerce": "session_store"}])
add("apply_set_fields", target={"a": 1},
    fields=[{"key": "a", "value": 9},
            {"key": "n", "value": "7", "coerce": "parse_active_agents"}],
    seams={"parse_active_agents": raise_spec("ValueError", "junk")})
add("apply_set_fields", target={"a": 1, "b": 2}, fields=[{"key": "a", "value": 9}])

# ── emit_transition ──
add("emit_transition", previous={"gateway_state": "starting"},
    payload={"gateway_state": "running"},
    seams={"emit_runtime_status_transition": None})
add("emit_transition", previous={}, payload={},
    seams={"emit_runtime_status_transition": {"ok": True}})
add("emit_transition", previous={}, payload={},
    seams={"emit_runtime_status_transition": raise_spec("ValueError", "bad")})
add("emit_transition", previous={}, payload={},
    seams={"emit_runtime_status_transition": raise_spec("OSError", "io")})
add("emit_transition", previous={}, payload={},
    seams={"emit_runtime_status_transition": raise_spec("SystemExit", "1")})
add("emit_transition", previous={}, payload={},
    seams={"emit_runtime_status_transition": raise_spec("KeyboardInterrupt", "")})
add("emit_transition", previous=None, payload={"gateway_state": "running"},
    seams={"emit_runtime_status_transition": None})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    if short:
        raise SystemExit("FATAL: funcs with <5 cases: %s" % short)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print("wrote %s: %d cases across %d funcs" % (OUT, len(CASES), len(counts)))
    for k in sorted(counts):
        print("  %-28s %d" % (k, counts[k]))


if __name__ == "__main__":
    main()
