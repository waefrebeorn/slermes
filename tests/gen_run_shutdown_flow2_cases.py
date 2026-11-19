#!/usr/bin/env python3
"""Generate tests/fixtures/run_shutdown_flow2.jsonl for the run_shutdown_flow2
oracle. Each case is {"func": <name>, "in": {...}} with scripted seams in in.io
(plain value reused; {"__seq__": [...]} per-call; {"__raise__": kind, "msg": text}
raises; scripted null -> None; ABSENT key -> fail-closed RuntimeError). >=5 cases
per ported function, exercising decision branches, short-circuits, error paths,
fail-closed seams and BaseException passthrough. Integral floats are avoided in
str()-computed positions (chat_id/thread_id) per the port header domain.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "run_shutdown_flow2.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


# ── send_failed ──
add("send_failed", result=None)
add("send_failed", result={"success": False})
add("send_failed", result={"success": True})
add("send_failed", result={})
add("send_failed", result={"success": 0})
add("send_failed", result={"success": False, "error": "x"})
add("send_failed", result="a string")

# ── send_error ──
add("send_error", result=None)
add("send_error", result={"error": "boom"})
add("send_error", result={})
add("send_error", result={"success": False})
add("send_error", result={"error": None})
add("send_error", result={"error": "x", "success": False})

# ── notice_target_key ──
add("notice_target_key", platform="telegram", chat_id=123, thread_id=None)
add("notice_target_key", platform="telegram", chat_id="c1", thread_id="t1")
add("notice_target_key", platform="telegram", chat_id=123, thread_id=0)
add("notice_target_key", platform="telegram", chat_id=123, thread_id="")
add("notice_target_key", platform="telegram", chat_id=None, thread_id="t")
add("notice_target_key", platform="telegram", chat_id=True, thread_id=False)
add("notice_target_key", platform="telegram", chat_id=123, thread_id="0")

# ── delivery_target_key ──
add("delivery_target_key", platform="telegram", chat_id=123, thread_id=None, profile="p1",
    io={"looks_like_telegram_private_chat_id": True})
add("delivery_target_key", platform="telegram", chat_id=123, thread_id=None, profile="default", io={})
add("delivery_target_key", platform="telegram", chat_id=123, thread_id=None, profile=None, io={})
add("delivery_target_key", platform="discord", chat_id=123, thread_id=None, profile="p1", io={})
add("delivery_target_key", platform="telegram", chat_id=123, thread_id=None, profile="p1",
    io={"looks_like_telegram_private_chat_id": False})
add("delivery_target_key", platform="telegram", chat_id=-100, thread_id=None, profile="p1",
    io={"looks_like_telegram_private_chat_id": False})
add("delivery_target_key", platform="telegram", chat_id=123, thread_id="t", profile="p1",
    io={"looks_like_telegram_private_chat_id": True})
add("delivery_target_key", platform="telegram", chat_id=123, thread_id=None, profile="p1", io={})
add("delivery_target_key", platform="telegram", chat_id=123, thread_id=None, profile="p1",
    io={"looks_like_telegram_private_chat_id": R("SystemExit", "bye")})

# ── restart_notification_allowed ──
def cfg(flag):
    return {"config": {"platforms": {"telegram": {"gateway_restart_notification": flag}}}}
add("restart_notification_allowed", platform="telegram", self=cfg(True))
add("restart_notification_allowed", platform="telegram", self=cfg(False))
add("restart_notification_allowed", platform="discord", self=cfg(True))
add("restart_notification_allowed", platform="telegram", self=cfg(0))
add("restart_notification_allowed", platform="telegram", self=cfg("yes"))
add("restart_notification_allowed", platform="telegram",
    self={"config": {"platforms": {"telegram": {}}}})
add("restart_notification_allowed", platform="telegram",
    self={"config": {"platforms": {"telegram": None}}})

# ── log_suppressed ──
add("log_suppressed", level=10, msg="m", args=[], exc_info=False, io={"step.fn": None})
add("log_suppressed", level=10, msg="m %s", args=["x"], exc_info=False,
    io={"step.fn": R("ValueError", "bad"), "logger.log": None})
add("log_suppressed", level=30, msg="m", args=[], exc_info=True,
    io={"step.fn": R("RuntimeError", "e"), "logger.log": None})
add("log_suppressed", level=10, msg="m", args=[], exc_info=False,
    io={"step.fn": R("SystemExit", "bye")})
add("log_suppressed", level=10, msg="m", args=[], exc_info=False,
    io={"step.fn": R("KeyboardInterrupt", "ki")})
add("log_suppressed", level=10, msg="m", args=[], exc_info=False, io={"step.fn": "ok"})
add("log_suppressed", level=10, msg="m", args=[], exc_info=False, io={"logger.log": None})
add("log_suppressed", level=10, msg="m", args=["x"], exc_info=False,
    io={"step.fn": R("ValueError", "b"), "logger.log": R("OSError", "log")})

# ── effective_watchdog_leash ──
add("effective_watchdog_leash", self={},
    io={"effective_stop_drain_timeout": 10, "resolve_shutdown_watchdog_delay": 15,
        "effective_stop_watchdog_delay": 20})
add("effective_watchdog_leash", self={}, io={"effective_stop_drain_timeout": R("ValueError", "d")})
add("effective_watchdog_leash", self={},
    io={"effective_stop_drain_timeout": 10, "resolve_shutdown_watchdog_delay": R("OSError", "r")})
add("effective_watchdog_leash", self={},
    io={"effective_stop_drain_timeout": 10, "resolve_shutdown_watchdog_delay": 15,
        "effective_stop_watchdog_delay": R("RuntimeError", "w")})
add("effective_watchdog_leash", self={},
    io={"effective_stop_drain_timeout": 0, "resolve_shutdown_watchdog_delay": 0,
        "effective_stop_watchdog_delay": 0})
add("effective_watchdog_leash", self={}, io={})
add("effective_watchdog_leash", self={}, io={"effective_stop_drain_timeout": R("SystemExit", "x")})

# ── active_async_delegation_count ──
add("active_async_delegation_count", io={"async_delegation.active_count": 3})
add("active_async_delegation_count", io={"async_delegation.active_count": 0})
add("active_async_delegation_count", io={"async_delegation.active_count": R("RuntimeError", "x")})
add("active_async_delegation_count", io={"async_delegation.active_count": R("SystemExit", "x")})
add("active_async_delegation_count", io={})
add("active_async_delegation_count", io={"async_delegation.active_count": R("KeyboardInterrupt", "x")})

# ── running_cron_job_count ──
add("running_cron_job_count", io={"cron.get_running_job_ids": ["j1", "j2"]})
add("running_cron_job_count", io={"cron.get_running_job_ids": []})
add("running_cron_job_count", io={"cron.get_running_job_ids": R("RuntimeError", "x")})
add("running_cron_job_count", io={"cron.get_running_job_ids": None})
add("running_cron_job_count", io={"cron.get_running_job_ids": 5})
add("running_cron_job_count", io={"cron.get_running_job_ids": {"a": 1, "b": 2}})
add("running_cron_job_count", io={})
add("running_cron_job_count", io={"cron.get_running_job_ids": R("SystemExit", "x")})

# ── active_cron_job_count ──
add("active_cron_job_count", self={}, io={"cron.get_running_job_ids": ["j1"]})
add("active_cron_job_count", self={}, io={"cron.get_running_job_ids": R("RuntimeError", "x")})
add("active_cron_job_count", self={}, io={"cron.get_running_job_ids": None})
add("active_cron_job_count", self={}, io={"cron.get_running_job_ids": R("SystemExit", "x")})
add("active_cron_job_count", self={}, io={})
add("active_cron_job_count", self={}, io={"cron.get_running_job_ids": []})

# ── api_server_hook ──
API = {"adapters": {"api_server": {"active_agent_work_count": True}}}
add("api_server_hook", self=API, name="active_agent_work_count", args=[], io={"api_server_hook": 5})
add("api_server_hook", self=API, name="active_agent_work_count", args=[], io={"api_server_hook": -3})
add("api_server_hook", self=API, name="active_agent_work_count", args=[], io={"api_server_hook": "7"})
add("api_server_hook", self=API, name="active_agent_work_count", args=[],
    io={"api_server_hook": R("RuntimeError", "x")})
add("api_server_hook", self={"adapters": {}}, name="active_agent_work_count", args=[], io={})
add("api_server_hook", self={}, name="x", args=[], io={})
add("api_server_hook", self={"adapters": {"api_server": {}}}, name="active_agent_work_count", args=[], io={})
add("api_server_hook", self={"adapters": {"api_server": {"interrupt_active_runs": True}}},
    name="interrupt_active_runs", args=["reason"], io={"api_server_hook": 2})
add("api_server_hook", self=API, name="active_agent_work_count", args=[], io={"api_server_hook": None})
add("api_server_hook", self=API, name="active_agent_work_count", args=[], io={"api_server_hook": "abc"})
add("api_server_hook", self=API, name="active_agent_work_count", args=[],
    io={"api_server_hook": R("SystemExit", "x")})

# ── active_api_run_count ──
add("active_api_run_count", self=API, io={"api_server_hook": 4})
add("active_api_run_count", self=API, io={"api_server_hook": R("RuntimeError", "x")})
add("active_api_run_count", self=API, io={"api_server_hook": None})
add("active_api_run_count", self=API, io={"api_server_hook": R("SystemExit", "x")})
add("active_api_run_count", self={"adapters": {}}, io={})
add("active_api_run_count", self=API, io={})

# ── active_api_worker_count ──
add("active_api_worker_count", self={}, io={"api_worker_live_count": 3})
add("active_api_worker_count", self={}, io={"api_worker_live_count": -2})
add("active_api_worker_count", self={}, io={"api_worker_live_count": "5"})
add("active_api_worker_count", self={}, io={"api_worker_live_count": R("RuntimeError", "x")})
add("active_api_worker_count", self={}, io={"api_worker_live_count": None})
add("active_api_worker_count", self={}, io={"api_worker_live_count": R("SystemExit", "x")})
add("active_api_worker_count", self={}, io={})

# ── mark_api_runs_shutdown_requested ──
MARK = {"adapters": {"api_server": {"mark_shutdown_requested": True}}}
add("mark_api_runs_shutdown_requested", self=MARK, io={"api_server_hook": 2})
add("mark_api_runs_shutdown_requested", self=MARK,
    io={"api_server_hook": R("RuntimeError", "x"), "logger.debug": None})
add("mark_api_runs_shutdown_requested", self=MARK, io={"api_server_hook": None, "logger.debug": None})
add("mark_api_runs_shutdown_requested", self=MARK, io={"api_server_hook": R("SystemExit", "x")})
add("mark_api_runs_shutdown_requested", self={"adapters": {}}, io={})
add("mark_api_runs_shutdown_requested", self=MARK,
    io={"api_server_hook": R("ValueError", "v"), "logger.debug": R("OSError", "log")})

# ── active_work_count ──
AW = {"adapters": {}}
add("active_work_count", self=AW,
    io={"running_agent_count": 2, "managed_turn_count": 1, "cron.get_running_job_ids": ["j"],
        "active_deferred_agent_worker_count": 0, "async_delegation.active_count": 3})
add("active_work_count", self=AW,
    io={"running_agent_count": 0, "managed_turn_count": 0, "cron.get_running_job_ids": [],
        "active_deferred_agent_worker_count": 0, "async_delegation.active_count": 0})
add("active_work_count", self=AW, io={"running_agent_count": R("RuntimeError", "x")})
add("active_work_count", self=AW, io={"running_agent_count": 2, "managed_turn_count": None})
add("active_work_count", self=AW, io={"running_agent_count": R("SystemExit", "x")})
add("active_work_count", self=AW, io={})
add("active_work_count", self=AW,
    io={"running_agent_count": 1, "managed_turn_count": 0,
        "cron.get_running_job_ids": R("RuntimeError", "c"),
        "active_deferred_agent_worker_count": 0, "async_delegation.active_count": 0})

# ── drain_work_counts ──
add("drain_work_counts", self=AW,
    io={"running_agent_count": 2, "managed_turn_count": 1, "cron.get_running_job_ids": ["j1", "j2"],
        "active_deferred_agent_worker_count": 3, "async_delegation.active_count": 4})
add("drain_work_counts", self=AW,
    io={"running_agent_count": 0, "managed_turn_count": 0, "cron.get_running_job_ids": [],
        "active_deferred_agent_worker_count": 0, "async_delegation.active_count": 0})
add("drain_work_counts", self=AW, io={"running_agent_count": R("RuntimeError", "x")})
add("drain_work_counts", self=AW, io={"running_agent_count": 2, "managed_turn_count": None})
add("drain_work_counts", self=AW,
    io={"running_agent_count": 0, "managed_turn_count": 0,
        "cron.get_running_job_ids": R("RuntimeError", "c"),
        "active_deferred_agent_worker_count": 0, "async_delegation.active_count": 0})
add("drain_work_counts", self=AW, io={"running_agent_count": R("SystemExit", "x")})

# ── gateway_cfg_section ──
add("gateway_cfg_section", name="scale_to_zero",
    io={"_load_gateway_config": {"gateway": {"scale_to_zero": {"idle_timeout_minutes": 5}}}})
add("gateway_cfg_section", name="scale_to_zero",
    io={"_load_gateway_config": {"gateway": {"scale_to_zero": "notdict"}}})
add("gateway_cfg_section", name="scale_to_zero", io={"_load_gateway_config": {"gateway": {}}})
add("gateway_cfg_section", name="scale_to_zero", io={"_load_gateway_config": {"other": 1}})
add("gateway_cfg_section", name="scale_to_zero", io={"_load_gateway_config": "notdict"})
add("gateway_cfg_section", name="scale_to_zero", io={"_load_gateway_config": R("RuntimeError", "x")})
add("gateway_cfg_section", name="scale_to_zero", io={"_load_gateway_config": R("SystemExit", "x")})
add("gateway_cfg_section", name="scale_to_zero", io={})
add("gateway_cfg_section", name="scale_to_zero", io={"_load_gateway_config": None})

# ── scale_to_zero_idle_timeout_seconds ──
add("scale_to_zero_idle_timeout_seconds", self={},
    io={"_load_gateway_config": {"gateway": {"scale_to_zero": {"idle_timeout_minutes": 5}}},
        "parse_idle_timeout_seconds": 300})
add("scale_to_zero_idle_timeout_seconds", self={},
    io={"_load_gateway_config": {"gateway": {"scale_to_zero": {}}}, "parse_idle_timeout_seconds": 60})
add("scale_to_zero_idle_timeout_seconds", self={},
    io={"_load_gateway_config": {"gateway": {}}, "parse_idle_timeout_seconds": 60})
add("scale_to_zero_idle_timeout_seconds", self={},
    io={"_load_gateway_config": {"gateway": {"scale_to_zero": {"other": 1}}},
        "parse_idle_timeout_seconds": 60})
add("scale_to_zero_idle_timeout_seconds", self={},
    io={"_load_gateway_config": R("RuntimeError", "x"), "parse_idle_timeout_seconds": 60})
add("scale_to_zero_idle_timeout_seconds", self={},
    io={"_load_gateway_config": {"gateway": {"scale_to_zero": {"idle_timeout_minutes": 5}}},
        "parse_idle_timeout_seconds": R("ValueError", "v")})
add("scale_to_zero_idle_timeout_seconds", self={},
    io={"_load_gateway_config": {"gateway": {"scale_to_zero": {"idle_timeout_minutes": None}}},
        "parse_idle_timeout_seconds": 60})

# ── relay_wake_url_or_none ──
add("relay_wake_url_or_none", io={"relay_wake_url": "https://x"})
add("relay_wake_url_or_none", io={"relay_wake_url": None})
add("relay_wake_url_or_none", io={"relay_wake_url": R("RuntimeError", "x")})
add("relay_wake_url_or_none", io={"relay_wake_url": R("SystemExit", "x")})
add("relay_wake_url_or_none", io={})

# ── scale_to_zero_hold_redial ──
add("scale_to_zero_hold_redial", self={}, held=True,
    io={"relay_adapter_for_dormancy": {"hold_redial": True}, "adapter.hold_redial": True})
add("scale_to_zero_hold_redial", self={}, held=True,
    io={"relay_adapter_for_dormancy": {"hold_redial": True}, "adapter.hold_redial": False})
add("scale_to_zero_hold_redial", self={}, held=True,
    io={"relay_adapter_for_dormancy": {"hold_redial": True}, "adapter.hold_redial": 1})
add("scale_to_zero_hold_redial", self={}, held=False,
    io={"relay_adapter_for_dormancy": {"release_redial": True}, "adapter.release_redial": True})
add("scale_to_zero_hold_redial", self={}, held=True, io={"relay_adapter_for_dormancy": None})
add("scale_to_zero_hold_redial", self={}, held=True, io={"relay_adapter_for_dormancy": {}})
add("scale_to_zero_hold_redial", self={}, held=True,
    io={"relay_adapter_for_dormancy": R("RuntimeError", "x"), "logger.debug": None})
add("scale_to_zero_hold_redial", self={}, held=True,
    io={"relay_adapter_for_dormancy": {"hold_redial": True},
        "adapter.hold_redial": R("RuntimeError", "m"), "logger.debug": None})
add("scale_to_zero_hold_redial", self={}, held=True,
    io={"relay_adapter_for_dormancy": R("SystemExit", "x")})
add("scale_to_zero_hold_redial", self={}, held=True,
    io={"relay_adapter_for_dormancy": {"hold_redial": True},
        "adapter.hold_redial": R("KeyboardInterrupt", "k")})

# ── scale_to_zero_abandon_suspend ──
def stz_self(draining, running, cooldown):
    return {"_draining": draining, "_running": running, "_scale_to_zero_cooldown_until": cooldown}
add("scale_to_zero_abandon_suspend", self=stz_self(False, True, 0),
    io={"relay_adapter_for_dormancy": None, "serving_state": "running",
        "scale_to_zero_status": None, "time.time": 100})
add("scale_to_zero_abandon_suspend", self=stz_self(True, True, 0),
    io={"relay_adapter_for_dormancy": None, "time.time": 100})
add("scale_to_zero_abandon_suspend", self=stz_self(False, False, 0),
    io={"relay_adapter_for_dormancy": None, "time.time": 100})
add("scale_to_zero_abandon_suspend", self=stz_self(False, True, 500),
    io={"relay_adapter_for_dormancy": None, "serving_state": "running",
        "scale_to_zero_status": None, "time.time": 100})
add("scale_to_zero_abandon_suspend", self=stz_self(False, True, 0),
    io={"relay_adapter_for_dormancy": None, "serving_state": R("RuntimeError", "s")})
add("scale_to_zero_abandon_suspend", self=stz_self(False, True, 0),
    io={"relay_adapter_for_dormancy": None, "serving_state": "running",
        "scale_to_zero_status": None, "time.time": R("OSError", "t")})
add("scale_to_zero_abandon_suspend", self=stz_self(False, True, 0),
    io={"relay_adapter_for_dormancy": R("SystemExit", "x")})
add("scale_to_zero_abandon_suspend", self=stz_self(False, True, 0),
    io={"relay_adapter_for_dormancy": None, "serving_state": "running",
        "scale_to_zero_status": R("RuntimeError", "st")})

# ── restart_wait_cron_counts ──
add("restart_wait_cron_counts", self={},
    io={"cron.get_restart_wait_cron_counts": {"awaitable": 5, "wedged": 2, "restart_safe": 1}})
add("restart_wait_cron_counts", self={},
    io={"cron.get_restart_wait_cron_counts": R("RuntimeError", "x"),
        "cron.get_running_job_ids": ["j1", "j2"]})
add("restart_wait_cron_counts", self={}, io={"cron.get_restart_wait_cron_counts": R("SystemExit", "x")})
add("restart_wait_cron_counts", self={}, io={})
add("restart_wait_cron_counts", self={},
    io={"cron.get_restart_wait_cron_counts": R("RuntimeError", "x"),
        "cron.get_running_job_ids": R("KeyboardInterrupt", "k")})
add("restart_wait_cron_counts", self={},
    io={"cron.get_restart_wait_cron_counts": {"awaitable": 0, "wedged": 0, "restart_safe": 0}})

# ── wedged_cron_job_count ──
add("wedged_cron_job_count", self={},
    io={"cron.get_restart_wait_cron_counts": {"awaitable": 5, "wedged": 2, "restart_safe": 1}})
add("wedged_cron_job_count", self={}, io={"cron.get_restart_wait_cron_counts": {"awaitable": 5}})
add("wedged_cron_job_count", self={}, io={"cron.get_restart_wait_cron_counts": None})
add("wedged_cron_job_count", self={}, io={"cron.get_restart_wait_cron_counts": 5})
add("wedged_cron_job_count", self={},
    io={"cron.get_restart_wait_cron_counts": R("RuntimeError", "x"), "cron.get_running_job_ids": []})
add("wedged_cron_job_count", self={}, io={"cron.get_restart_wait_cron_counts": ["a"]})
add("wedged_cron_job_count", self={}, io={"cron.get_restart_wait_cron_counts": R("SystemExit", "x")})

# ── restart_safe_cron_count ──
add("restart_safe_cron_count", self={},
    io={"cron.get_restart_wait_cron_counts": {"awaitable": 5, "wedged": 2, "restart_safe": 1}})
add("restart_safe_cron_count", self={}, io={"cron.get_restart_wait_cron_counts": {"awaitable": 5}})
add("restart_safe_cron_count", self={}, io={"cron.get_restart_wait_cron_counts": None})
add("restart_safe_cron_count", self={},
    io={"cron.get_restart_wait_cron_counts": R("RuntimeError", "x"), "cron.get_running_job_ids": ["j"]})
add("restart_safe_cron_count", self={}, io={"cron.get_restart_wait_cron_counts": R("SystemExit", "x")})
add("restart_safe_cron_count", self={}, io={"cron.get_restart_wait_cron_counts": {"restart_safe": 0}})

# ── wedged_agent_count ──
add("wedged_agent_count", self={},
    io={"wedged_chat_agent_count": 3,
        "cron.get_restart_wait_cron_counts": {"awaitable": 5, "wedged": 2, "restart_safe": 1}})
add("wedged_agent_count", self={},
    io={"wedged_chat_agent_count": 0, "cron.get_restart_wait_cron_counts": {"wedged": 0}})
add("wedged_agent_count", self={}, io={"wedged_chat_agent_count": R("RuntimeError", "x")})
add("wedged_agent_count", self={},
    io={"wedged_chat_agent_count": 2, "cron.get_restart_wait_cron_counts": {"awaitable": 1}})
add("wedged_agent_count", self={},
    io={"wedged_chat_agent_count": None, "cron.get_restart_wait_cron_counts": {"wedged": 2}})
add("wedged_agent_count", self={}, io={"wedged_chat_agent_count": R("SystemExit", "x")})

# ── awaitable_work_count ──
add("awaitable_work_count", self=AW,
    io={"running_agent_count": 5, "managed_turn_count": 2, "active_deferred_agent_worker_count": 1,
        "wedged_chat_agent_count": 3, "async_delegation.active_count": 4,
        "cron.get_restart_wait_cron_counts": {"awaitable": 6, "wedged": 0, "restart_safe": 0}})
add("awaitable_work_count", self=AW,
    io={"running_agent_count": 0, "managed_turn_count": 0, "active_deferred_agent_worker_count": 0,
        "wedged_chat_agent_count": 0, "async_delegation.active_count": 0,
        "cron.get_restart_wait_cron_counts": {"awaitable": 0}})
add("awaitable_work_count", self=AW, io={"running_agent_count": R("RuntimeError", "x")})
add("awaitable_work_count", self=AW,
    io={"running_agent_count": 1, "managed_turn_count": 0, "active_deferred_agent_worker_count": 0,
        "wedged_chat_agent_count": 5, "async_delegation.active_count": 0,
        "cron.get_restart_wait_cron_counts": {"awaitable": 0}})
add("awaitable_work_count", self=AW, io={"running_agent_count": 2, "managed_turn_count": None})
add("awaitable_work_count", self=AW,
    io={"running_agent_count": 0, "managed_turn_count": 0, "active_deferred_agent_worker_count": 0,
        "wedged_chat_agent_count": 0, "async_delegation.active_count": 0,
        "cron.get_restart_wait_cron_counts": {"wedged": 0}})
add("awaitable_work_count", self=AW, io={"running_agent_count": R("SystemExit", "x")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases -> %s" % (len(CASES), OUT))
    low = {k: v for k, v in counts.items() if v < 5}
    print("funcs:", len(counts), "under-5:", low)
    if low:
        raise SystemExit("some functions have <5 cases: %s" % low)


if __name__ == "__main__":
    main()