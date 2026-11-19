#!/usr/bin/env python3
"""gen_rest_revive2_cases.py — regenerate tests/fixtures/rest_revive2.jsonl.

Case shape: {"func": <port name>, "in": {<args>, "io": {seam scripts}}}.
Script semantics (both sides): plain value reused; bare array = per-call
queue; {"__script__": true, "ret": v} wraps list-valued returns;
{"__script__": true, "raise": {type, message}} scripts an exception.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "rest_revive2.jsonl"
CASES = []


def add(func, inp, io=None):
    if io is not None:
        inp = {**inp, "io": io}
    CASES.append({"func": func, "in": inp})


def ret(v):
    return {"__script__": True, "ret": v}


def raise_(ty, msg=""):
    return {"__script__": True, "raise": {"type": ty, "message": msg}}


# ── split_monitor_arg ──
add("split_monitor_arg", {"monitor": None, "monitor_script": "s.sh", "monitor_url": "http://u"})
add("split_monitor_arg", {})
add("split_monitor_arg", {"monitor": ""})
add("split_monitor_arg", {"monitor": "   "})
add("split_monitor_arg", {"monitor": "https://x.example/a"})
add("split_monitor_arg", {"monitor": "HTTP://X"})
add("split_monitor_arg", {"monitor": " /path/m.sh "})
add("split_monitor_arg", {"monitor": "ftp://x"})
add("split_monitor_arg", {"monitor": 5})

# ── mode_guidance_notes ──
add("mode_guidance_notes", {"job": {}, "user_deliver": None})
add("mode_guidance_notes", {"job": {"monitor_script": "m.sh"}})
add("mode_guidance_notes", {"job": {"monitor_url": "http://u"}})
add("mode_guidance_notes", {"job": {"no_agent": True}})
add("mode_guidance_notes", {"job": {"monitor_script": "m", "no_agent": True}})
add("mode_guidance_notes", {"job": {}, "user_deliver": "all"})
add("mode_guidance_notes", {"job": {}, "user_deliver": "bot-chat:prof"})
add("mode_guidance_notes", {"job": {}, "user_deliver": "telegram:123"})
add("mode_guidance_notes", {"job": {}, "user_deliver": "telegram:123#topic"})
add("mode_guidance_notes", {"job": {}, "user_deliver": "room:abc"})
add("mode_guidance_notes", {"job": {}, "user_deliver": "sms:123"})
add("mode_guidance_notes", {"job": {}, "user_deliver": "telegram:"})
add("mode_guidance_notes", {"job": {}, "user_deliver": " ALL , bot-chat:p , telegram:1 "})
add("mode_guidance_notes", {"job": {}, "user_deliver": "telegram:1,discord:2"})
add("mode_guidance_notes", {"job": {}, "user_deliver": "telegram:1:extra"})
add("mode_guidance_notes", {"job": None})
add("mode_guidance_notes", {"job": {}, "user_deliver": 5})
add("mode_guidance_notes", {"job": {}, "user_deliver": "\x1call\x1f"})

# ── manual_run_delivery_note ──
add("manual_run_delivery_note", {"deliver": "", "refreshed": {}})
add("manual_run_delivery_note", {"deliver": None, "refreshed": {}})
add("manual_run_delivery_note", {"deliver": "local", "refreshed": {}})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": {}})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": {"last_delivery_queued": True}})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": {"last_delivery_error": "boom"}})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": {"last_delivery_error": "e" * 250}})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": {"last_delivery_error": "   "}})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": {"last_delivery_error": None, "last_delivery_queued": False}})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": None})
add("manual_run_delivery_note", {"deliver": "telegram", "refreshed": {"last_delivery_error": "⚠" * 205}})
add("manual_run_delivery_note", {"deliver": 0, "refreshed": {}})

# ── gateway_liveness_notice ──
add("gateway_liveness_notice", {"plural": False}, {"builtin_gateway_liveness": True})
add("gateway_liveness_notice", {"plural": False}, {"builtin_gateway_liveness": False})
add("gateway_liveness_notice", {"plural": True}, {"builtin_gateway_liveness": False})
add("gateway_liveness_notice", {}, {"builtin_gateway_liveness": None})
add("gateway_liveness_notice", {}, {"builtin_gateway_liveness": raise_("RuntimeError", "probe failed")})
add("gateway_liveness_notice", {}, {"builtin_gateway_liveness": raise_("SystemExit", "bye")})
add("gateway_liveness_notice", {}, {"builtin_gateway_liveness": 0})

# ── resolve_cron_context_deliver ──
add("resolve_cron_context_deliver", {"deliver": "telegram:1"}, {"get_session_env": ["0"]})
add("resolve_cron_context_deliver", {"deliver": None}, {"get_session_env": ["true", "plat", "chat", "thr"]})
add("resolve_cron_context_deliver", {"deliver": None}, {"get_session_env": ["1", "", "chat"]})
add("resolve_cron_context_deliver", {"deliver": None}, {"get_session_env": ["yes", "plat", ""]})
add("resolve_cron_context_deliver", {"deliver": "origin"}, {"get_session_env": ["on", "p", "c", ""]})
add("resolve_cron_context_deliver", {"deliver": "origin,local"}, {"get_session_env": ["1", "p", "c", "", "p", "c", ""]})
add("resolve_cron_context_deliver", {"deliver": "telegram:1,origin"}, {"get_session_env": ["1", "p", "c", "t"]})
add("resolve_cron_context_deliver", {"deliver": "origin,origin"}, {"get_session_env": ["1", "p", "c", "t", "p", "c", "t"]})
add("resolve_cron_context_deliver", {"deliver": ""}, {"get_session_env": ["1"]})
add("resolve_cron_context_deliver", {"deliver": "Origin"}, {"get_session_env": ["1", "p", "c", "t"]})
add("resolve_cron_context_deliver", {"deliver": "telegram:1, telegram:1 "}, {"get_session_env": ["1"]})
add("resolve_cron_context_deliver", {"deliver": "x"}, {"get_session_env": raise_("RuntimeError", "env down")})

# ── forward_relay_fronted_run ──
FR_OK = {"relay_fronted_delivery_platforms": ret(["telegram"]),
         "get_secret": "K", "api_server_base_url": "http://127.0.0.1:8642",
         "httpx_post": {"status_code": 200}}
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {"relay_fronted_delivery_platforms": ret([])})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, FR_OK)
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "httpx_post": {"status_code": 500}})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "httpx_post": raise_("RuntimeError", "conn refused")})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "httpx_post": None})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "get_secret": raise_("ValueError", "no scope")})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "relay_fronted_delivery_platforms": raise_("RuntimeError", "relay down")})
add("forward_relay_fronted_run", {"job": {"id": "j1"}, "extra_prompt": "go"}, {**FR_OK, "httpx_post": {"status_code": 201}})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "httpx_post": {"status_code": "200"}})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "httpx_post": {}})
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "relay_fronted_delivery_platforms": None})
add("forward_relay_fronted_run", {"job": {}}, FR_OK)
add("forward_relay_fronted_run", {"job": {"id": "j1"}}, {**FR_OK, "httpx_post": raise_("SystemExit", "halt")})

# ── latest_job_output_excerpt ──
LJ_BASE = {"get_cron_output_dir": "/h/out"}
add("latest_job_output_excerpt", {"job_id": "j1"}, {**LJ_BASE, "glob": ret([])})
add("latest_job_output_excerpt", {"job_id": "j1"}, {**LJ_BASE, "glob": ret(["/h/out/j1/a.md"]), "read_text": "hello"})
add("latest_job_output_excerpt", {"job_id": "j1"}, {**LJ_BASE, "glob": ret(["/h/out/j1/a.md"]), "read_text": "  "})
add("latest_job_output_excerpt", {"job_id": "j1", "max_chars": 10}, {**LJ_BASE, "glob": ret(["/h/out/j1/a.md"]), "read_text": "x" * 30})
add("latest_job_output_excerpt", {"job_id": "j1"}, {**LJ_BASE, "glob": ret(["/h/out/j1/a.md"]), "read_text": raise_("OSError", "disk")})
add("latest_job_output_excerpt", {"job_id": "j1"}, {**LJ_BASE, "glob": raise_("OSError", "perm")})
add("latest_job_output_excerpt", {"job_id": "j1"}, {"get_cron_output_dir": raise_("RuntimeError", "no home")})
add("latest_job_output_excerpt", {"job_id": "j1"}, {**LJ_BASE, "glob": ret(["/h/out/j1/B.md", "/h/out/j1/a.md"]), "read_text": "x"})
add("latest_job_output_excerpt", {"job_id": "j1", "max_chars": 0}, {**LJ_BASE, "glob": ret(["/h/out/j1/a.md"]), "read_text": "abc"})
add("latest_job_output_excerpt", {"job_id": "j1"}, {**LJ_BASE, "get_cron_output_dir": raise_("SystemExit", "halt")})
add("latest_job_output_excerpt", {"job_id": 5}, LJ_BASE)
add("latest_job_output_excerpt", {"job_id": None}, LJ_BASE)
add("latest_job_output_excerpt", {"job_id": "j1", "max_chars": 200}, {**LJ_BASE, "glob": ret(["/h/out/j1/a.md"]), "read_text": "é" * 205})

# ── _cronjob_handler ──
add("_cronjob_handler", {"args": {}, "kw": {}}, {"cronjob": "ok"})
add("_cronjob_handler", {"args": {"monitor": "https://u", "action": "create", "prompt": "p"}, "kw": {"session_id": "s1"}}, {"cronjob": "ok"})
add("_cronjob_handler", {"args": {"monitor": "m.sh", "monitor_script": "old.sh", "monitor_url": "http://old"}}, {"cronjob": "ok"})
add("_cronjob_handler", {"args": {"monitor": None, "monitor_script": "s", "monitor_url": "u"}}, {"cronjob": "ok"})
add("_cronjob_handler", {"args": {}}, {"cronjob": raise_("ValueError", "bad action")})
add("_cronjob_handler", {"args": None}, {})
add("_cronjob_handler", {"args": {"paused": True, "no_agent": True, "grace_minutes": 5, "deliver": "telegram"}}, {"cronjob": "ok"})

# ── run_claimed_job ──
RC_BASE = {"try_register_running_job": True, "get_activity_callback": None,
           "run_one_job": True, "release_running_job": None,
           "get_job": {"last_status": "ok"}}
add("run_claimed_job", {"job": {"id": "j1", "name": "Job"}}, RC_BASE)
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "try_register_running_job": False})
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "run_one_job": raise_("RuntimeError", "boom"), "mark_job_run": None, "logger.error": None})
add("run_claimed_job", {"job": {"id": "j1", "fire_claim": {"by": "owner-abc"}}}, {**RC_BASE, "run_one_job": raise_("RuntimeError", "boom"), "mark_job_run": None, "logger.error": None})
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "get_job": {"last_status": "delivery_failed", "last_delivery_error": "derr"}})
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "get_job": {"last_status": "delivery_failed", "last_error": "e", "last_delivery_error": "d"}})
add("run_claimed_job", {"job": {"id": "j1", "execution_id": "e1"}}, {**RC_BASE, "get_execution": {"status": "failed", "error": "ex"}})
add("run_claimed_job", {"job": {"id": "j1", "execution_id": "e1"}}, {**RC_BASE, "get_execution": {"status": "running"}})
add("run_claimed_job", {"job": {"id": "j1", "execution_id": "e1"}}, {**RC_BASE, "get_execution": {"status": "completed"}})
add("run_claimed_job", {"job": {"id": "j1"}, "runner": {"ref_callable": True}},
    {**RC_BASE, "runner_ref_call": {"adapters": {"telegram": "ad"}, "has_adapters_for_profile": True,
                                   "has_is_shared_bot_satellite": True, "gateway_loop": {"id": "loop1"}},
     "get_hermes_home": "/h", "profile_name_for_home": "default",
     "adapters_for_profile": {"telegram": "ad2"}, "is_shared_bot_satellite": True,
     "shared_route_adapters": {"telegram": "ad3"}})
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "run_one_job": raise_("SystemExit", "halt")})
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "get_job": None})
add("run_claimed_job", {"job": {}}, {})
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "try_register_running_job": raise_("RuntimeError", "reg down"), "mark_job_run": None, "logger.error": None})
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "get_activity_callback": raise_("RuntimeError", "no cb")})
add("run_claimed_job", {"job": {"id": "j1"}, "runner": {"ref_callable": False}}, RC_BASE)
add("run_claimed_job", {"job": {"id": "j1"}}, {**RC_BASE, "run_one_job": raise_("RuntimeError", "boom"), "mark_job_run": raise_("ValueError", "mark down"), "logger.error": None})
add("run_claimed_job", {"job": {"id": "j1", "execution_id": "e1"}}, {**RC_BASE, "get_execution": raise_("RuntimeError", "exec down"), "mark_job_run": None, "logger.error": None})
add("run_claimed_job", {"job": {"id": "j1"}, "extra_prompt": "extra"}, RC_BASE)

# ── try_dispatch_background_run ──
TD_CLAIMED = {"id": "j1", "deliver": "telegram"}
TD_BASE = {"reap_stale_executions": None, "async_delivery_supported": True,
           "get_current_session_key": "k", "is_job_running": False,
           "claim_job_for_fire": TD_CLAIMED, "get_session_env": "ui1",
           "current_origin_session_id": "o1", "get_max_async_children": 3,
           "time.time": 100.0,
           "dispatch_async_delegation": {"status": "dispatched", "delegation_id": "d1"}}
add("try_dispatch_background_run", {"job": {"id": "j1", "name": "J"}}, {**TD_BASE, "async_delivery_supported": False})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "get_current_session_key": ""})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "is_job_running": True})
add("try_dispatch_background_run", {"job": {"id": "j1", "name": "J"}, "session_id": "s1"}, TD_BASE)
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "dispatch_async_delegation": {"status": "rejected", "error": "full"}, "logger.info": None, "run_claimed_job": {"claimed": True, "success": True, "error": None}})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "claim_job_for_fire": False, "get_job": None})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "claim_job_for_fire": False, "get_job": {"paused": True}, "is_job_runnable": False})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "claim_job_for_fire": raise_("RuntimeError", "claimboom"), "logger.error": None, "mark_job_run": None})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "current_origin_session_id": raise_("ImportError", "no registry"), "logger.warning": None, "run_claimed_job": {"claimed": True, "success": False, "error": "x"}})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "async_delivery_supported": raise_("SystemExit", "halt")})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "get_max_async_children": raise_("ValueError", "cfg")})
add("try_dispatch_background_run", {"job": {"id": "j1"}, "session_id": "s9"}, {**TD_BASE, "get_current_session_key": ""})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "is_job_running": raise_("RuntimeError", "probe")})
add("try_dispatch_background_run", {"job": {"id": "j1"}}, {**TD_BASE, "dispatch_async_delegation": raise_("RuntimeError", "dispatch down")})
add("try_dispatch_background_run", {"job": {"id": "j1"}, "extra_prompt": "ep"}, TD_BASE)

# ── cron_doctor_issues_for_job ──
DOC_OK = {"parse_aware": "2026-01-01T00:00:00+00:00", "now": "2025-12-31T23:00:00+00:00"}
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "last_status": "ok", "enabled": True, "state": "active", "next_run_at": "2026-01-01T00:00:00+00:00"}}, DOC_OK)
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "last_status": "error", "last_error": "boom", "enabled": False}}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "last_delivery_error": "adapter down", "enabled": False}}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "last_delivery_unverified": ["telegram:1", "discord:2"], "enabled": False}}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "last_dispatch": {"kind": "late", "scheduled_at": "S", "lateness_seconds": 30}, "enabled": False}}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "last_fire_error": {"at": "T", "detail": "gateway unreachable"}, "enabled": False}}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "enabled": True, "next_run_at": ""}}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "no_agent": True, "script": "", "enabled": False}}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "script": "x.py", "enabled": False}},
    {"cron_dir": "/h/cron", "path_resolve": ["/h/scripts", "/h/scripts/x.py"], "path_expanduser": "x.py", "path_exists": True, "path_is_file": True})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "workdir": "/missing", "enabled": False}},
    {"path_expanduser": "/missing", "path_exists": False})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "timezone": "Bad/Zone", "enabled": False}}, {"zone_for_job": None})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "grace_minutes": True, "enabled": False}},
    {"normalize_grace_minutes": raise_("ValueError", "grace_minutes must be a whole number of minutes (0 = never catch up), not a boolean.")})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "grace_minutes": 5, "schedule": {"kind": "once"}, "enabled": False}}, {"normalize_grace_minutes": None})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "grace_minutes": 5, "schedule": {"kind": "cron"}, "enabled": False}}, {"normalize_grace_minutes": None})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "enabled": True, "next_run_at": "2026-01-01T00:00:00+00:00"}},
    {"parse_aware": "2026-01-01T00:00:00+00:00", "now": "2026-01-01T02:30:00+00:00"})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "grace_minutes": 5, "enabled": False}}, {"normalize_grace_minutes": raise_("TypeError", "bad type")})
add("cron_doctor_issues_for_job", {"job": None}, {})
add("cron_doctor_issues_for_job", {"job": {"id": "j1", "last_status": "delivery_failed", "enabled": False}}, {})

# ── dispatch_display ──
add("dispatch_display", {"dispatch": None}, {})
add("dispatch_display", {"dispatch": {"scheduled_at": "S"}}, {})
add("dispatch_display", {"dispatch": {"scheduled_at": "S", "dispatched_at": "A", "kind": "on_time"}}, {"color": "on time (scheduled S)"})
add("dispatch_display", {"dispatch": {"scheduled_at": "S", "dispatched_at": "A", "kind": "late", "lateness_seconds": 45}}, {"color": ["⚠ late: ", "(45s late)"]})
add("dispatch_display", {"dispatch": {"scheduled_at": "S", "dispatched_at": "A", "kind": "catch_up", "lateness_seconds": 3600}}, {"color": ["⚠ catch-up after missed fire: ", "(1h late)"]})
add("dispatch_display", {"dispatch": {"scheduled_at": "S", "dispatched_at": "A", "kind": "weird"}}, {"color": ["⚠ late: ", "(0s late)"]})
add("dispatch_display", {"dispatch": {"scheduled_at": "S", "dispatched_at": "A", "kind": "late", "lateness_seconds": "abc"}}, {"color": ["⚠ late: ", "(? late)"]})
add("dispatch_display", {"dispatch": {"scheduled_at": "S", "dispatched_at": "A", "kind": "on_time"}}, {"color": raise_("RuntimeError", "no color")})
add("dispatch_display", {"dispatch": {"scheduled_at": 0, "dispatched_at": "A", "kind": "late"}}, {})

# ── script_health_issue ──
SH_BASE = {"cron_dir": "/h/cron", "path_expanduser": "x.py",
           "path_resolve": ["/h/scripts", "/h/scripts/x.py"],
           "path_exists": True, "path_is_file": True}
add("script_health_issue", {"script": "x.py"}, SH_BASE)
add("script_health_issue", {"script": "x.py"}, {**SH_BASE, "path_exists": False})
add("script_health_issue", {"script": "x.py"}, {**SH_BASE, "path_is_file": False})
add("script_health_issue", {"script": "C:/other/x.py"},
    {"cron_dir": "/h/cron", "path_expanduser": "C:/other/x.py", "path_resolve": ["/h/scripts", "C:/other/x.py"]})
add("script_health_issue", {"script": "sub/x.py"},
    {"cron_dir": "/h/cron", "path_expanduser": "sub/x.py", "path_resolve": ["/h/scripts", "/h/scripts/sub/x.py"], "path_exists": True, "path_is_file": True})
add("script_health_issue", {"script": "x.py"}, {**SH_BASE, "cron_dir": raise_("RuntimeError", "no cron dir")})
add("script_health_issue", {"script": "x.py"}, {**SH_BASE, "path_resolve": raise_("OSError", "resolve down")})
add("script_health_issue", {"script": 5}, {})
add("script_health_issue", {"script": 5},
    {"cron_dir": "/h/cron", "path_resolve": "/h/scripts"})
add("script_health_issue", {"script": None},
    {"cron_dir": "/h/cron", "path_resolve": "/h/scripts"})
add("script_health_issue", {"script": "~/x.py"},
    {"cron_dir": "/h/cron", "path_expanduser": "/h/x.py", "path_resolve": ["/h/scripts", "/h/x.py"]})

# ── format_lateness ──
for v in (0, 45, 60, 3600, 5400, 90000, 90061, -5, None, "abc", "30", 12.9, "  +1_0  ", 59, 86399):
    add("format_lateness", {"seconds": v})

# ── next_run_overdue_issue ──
add("next_run_overdue_issue", {"next_run": "bad"}, {"parse_aware": None})
add("next_run_overdue_issue", {"next_run": "x"}, {"parse_aware": "2026-01-01T00:00:00+00:00", "now": "2025-12-31T23:00:00+00:00"})
add("next_run_overdue_issue", {"next_run": "x"}, {"parse_aware": "2026-01-01T00:00:00+00:00", "now": "2026-01-01T00:15:00+00:00"})
add("next_run_overdue_issue", {"next_run": "x"}, {"parse_aware": "2026-01-01T00:00:00+00:00", "now": "2026-01-01T00:15:01+00:00"})
add("next_run_overdue_issue", {"next_run": "x"}, {"parse_aware": "2026-01-01T00:00:00+00:00", "now": "2026-01-01T02:30:00+00:00"})
add("next_run_overdue_issue", {"next_run": "x"}, {"parse_aware": "2026-01-01T00:00:00+00:00", "now": "2026-01-01T01:00:00+00:00"})
add("next_run_overdue_issue", {"next_run": "x"}, {"parse_aware": "2026-01-01T05:00:00+05:00", "now": "2026-01-01T02:30:00Z"})
add("next_run_overdue_issue", {"next_run": "x"}, {"parse_aware": raise_("RuntimeError", "parse down")})
add("next_run_overdue_issue", {"next_run": 5}, {"parse_aware": None})

# ── scripts_dir_for_cron ──
add("scripts_dir_for_cron", {}, {"cron_dir": "/h/cron"})
add("scripts_dir_for_cron", {}, {"cron_dir": "/h/deep/nest/cron"})
add("scripts_dir_for_cron", {}, {"cron_dir": "cron"})
add("scripts_dir_for_cron", {}, {"cron_dir": "/"})
add("scripts_dir_for_cron", {}, {"cron_dir": raise_("RuntimeError", "no dir")})
add("scripts_dir_for_cron", {}, {"cron_dir": "C:/h/cron"})

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print("total cases:", len(CASES))
for k, v in counts.items():
    assert v >= 5, f"{k} has only {v} cases"
    print(f"  {k}: {v}")
