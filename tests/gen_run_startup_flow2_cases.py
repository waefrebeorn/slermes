#!/usr/bin/env python3
"""Generate tests/fixtures/run_startup_flow2.jsonl — >=5 cases per ported func.

Cases exercise happy paths, short-circuit order, scripted raises, BaseException
passthrough, fail-closed (missing/exhausted scripts), _log_suppressed swallow
vs pass, and state mutation. Seam scripts: bare array = per-call queue (raw
elements); {"__script__":true,"ret":v} = constant list/object; else constant.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "run_startup_flow2.jsonl")
MAXSIZE = 9223372036854775807


def sc(v):
    return {"__script__": True, "ret": v}


def rs(kind, msg):
    return {"__raise__": kind, "message": msg}


CASES = []


def C(func, io=None, state=None, **args):
    inp = dict(args)
    if io is not None:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    CASES.append({"func": func, "in": inp})


# ── 1. _own_policy_envs ──────────────────────────────────────────────────────
C("_own_policy_envs", platform="telegram",
  io={"platform_registry.get": None, "platform_env.authz_field": None})
C("_own_policy_envs", platform="telegram",
  io={"platform_registry.get": sc({"name": "tg"}),
      "platform_env.authz_field": [["TG_DM", "TG_GROUP"], "TG_ALL"]})
C("_own_policy_envs", platform="discord",
  io={"platform_registry.get": None, "platform_env.authz_field": [[]]})
C("_own_policy_envs", platform="telegram",
  io={"platform_registry.get": rs("RuntimeError", "reg down")})
C("_own_policy_envs", platform="telegram",
  io={"platform_registry.get": None, "platform_env.authz_field": rs("ValueError", "bad")})
C("_own_policy_envs", platform="telegram",
  io={"platform_registry.get": None,
      "platform_env.authz_field": [["A", "B"], rs("OSError", "disk")]})
C("_own_policy_envs", platform="telegram",
  io={"platform_registry.get": None, "platform_env.authz_field": [5]})
C("_own_policy_envs", platform="telegram", io={})
C("_own_policy_envs", platform="telegram",
  io={"platform_registry.get": rs("SystemExit", "bye")})

# ── 2. _log_agent_budget ─────────────────────────────────────────────────────
C("_log_agent_budget", io={"os.getenv": None, "config.resolve_turn_limit": 500,
                           "logger.info": None})
C("_log_agent_budget", io={"os.getenv": None, "config.resolve_turn_limit": MAXSIZE,
                           "logger.info": None})
C("_log_agent_budget", io={"os.getenv": "500", "config.resolve_turn_limit": 500,
                           "logger.info": None})
C("_log_agent_budget", io={"os.getenv": None,
                           "config.resolve_turn_limit": rs("ValueError", "nope")})
C("_log_agent_budget", io={"os.getenv": None, "config.resolve_turn_limit": 0,
                           "logger.info": rs("RuntimeError", "log down")})
C("_log_agent_budget", io={})
C("_log_agent_budget", io={"os.getenv": None, "config.resolve_turn_limit": 0,
                           "logger.info": None})

# ── 3. _startup_retry_entry ──────────────────────────────────────────────────
ENTRY = {"platform": "telegram", "attempts": 1, "delay": 30}
C("_startup_retry_entry", platform="telegram", adapter={"id": "a"}, platform_config={"e": 1},
  io={"self._reconnect_queue_entry": sc(ENTRY)})
C("_startup_retry_entry", platform="telegram", queued=False,
  io={"self._reconnect_queue_entry": sc(ENTRY)})
C("_startup_retry_entry", platform="telegram",
  io={"self._reconnect_queue_entry": rs("RuntimeError", "q")})
C("_startup_retry_entry", platform="no_such_xyz", io={})
C("_startup_retry_entry", platform="telegram",
  io={"self._reconnect_queue_entry": rs("KeyboardInterrupt", "stop")})
C("_startup_retry_entry", platform="telegram", io={})

# ── 4. _startup_queue_transient_failure ──────────────────────────────────────
C("_startup_queue_transient_failure", platform="telegram", message="boom",
  adapter={"id": "a"}, platform_config={},
  io={"self._update_platform_runtime_status": None,
      "self._reconnect_queue_entry": sc(ENTRY)})
C("_startup_queue_transient_failure", platform="telegram", message="boom",
  io={"self._update_platform_runtime_status": rs("RuntimeError", "status")})
C("_startup_queue_transient_failure", platform="telegram", message="boom",
  io={"self._update_platform_runtime_status": None,
      "self._reconnect_queue_entry": rs("ValueError", "entry")})
C("_startup_queue_transient_failure", platform="telegram", message="again",
  state={"_failed_platforms": {"telegram": {"old": True}}},
  io={"self._update_platform_runtime_status": None,
      "self._reconnect_queue_entry": sc(ENTRY)})
C("_startup_queue_transient_failure", platform="telegram", message="boom", io={})
C("_startup_queue_transient_failure", platform="telegram", message="boom",
  io={"self._update_platform_runtime_status": rs("GeneratorExit", "gen")})

# ── 5. _startup_teardown_adapter ─────────────────────────────────────────────
C("_startup_teardown_adapter", platform="telegram", adapter={"id": "a"},
  io={"log_suppressed": None, "adapter.cancel_background_tasks": None,
      "self._safe_adapter_disconnect": None})
C("_startup_teardown_adapter", platform="telegram", adapter={},
  io={"log_suppressed": None,
      "adapter.cancel_background_tasks": rs("RuntimeError", "cancel"),
      "logger.log": None, "self._safe_adapter_disconnect": None})
C("_startup_teardown_adapter", platform="telegram", adapter={},
  io={"log_suppressed": None,
      "adapter.cancel_background_tasks": rs("SystemExit", "bye")})
C("_startup_teardown_adapter", platform="telegram", adapter={},
  io={"log_suppressed": None, "adapter.cancel_background_tasks": None,
      "self._safe_adapter_disconnect": rs("OSError", "disc")})
C("_startup_teardown_adapter", platform="telegram", adapter={},
  io={"log_suppressed": rs("RuntimeError", "enter")})
C("_startup_teardown_adapter", platform="telegram", adapter={},
  io={"log_suppressed": None,
      "adapter.cancel_background_tasks": rs("ValueError", "v"),
      "logger.log": rs("RuntimeError", "loglog")})
C("_startup_teardown_adapter", platform="telegram", adapter={},
  io={"log_suppressed": None})

# ── 6. _resume_pending_candidates ────────────────────────────────────────────
C("_resume_pending_candidates", platform=None,
  io={"run_restored_delivery.resume_pending_candidates": sc([{"session_key": "k"}])})
C("_resume_pending_candidates", platform="telegram",
  io={"run_restored_delivery.resume_pending_candidates": None})
C("_resume_pending_candidates", platform=None,
  io={"run_restored_delivery.resume_pending_candidates": rs("RuntimeError", "r")})
C("_resume_pending_candidates", platform="no_such_xyz", io={})
C("_resume_pending_candidates", platform=None, io={})
C("_resume_pending_candidates", platform="discord",
  io={"run_restored_delivery.resume_pending_candidates": rs("CancelledError", "c")})

# ── 7. _release_runtime_claim_quiet ──────────────────────────────────────────
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="release %s failed",
  io={"delivery_ledger.release_runtime_claim": None})
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="release %s failed",
  io={"delivery_ledger.release_runtime_claim": rs("OSError", "db"),
      "logger.debug": None})
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="f",
  io={"delivery_ledger.release_runtime_claim": rs("SystemExit", "bye")})
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="f",
  io={"delivery_ledger.release_runtime_claim": rs("OSError", "db"),
      "logger.debug": rs("RuntimeError", "log")})
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="f",
  io={"delivery_ledger.release_runtime_claim": None})
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="f", error=None,
  io={"delivery_ledger.release_runtime_claim": None})
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="f", io={})

# ── 8. _obligation_adapter ───────────────────────────────────────────────────
C("_obligation_adapter", row={"platform": "telegram", "obligation_id": "o1"},
  state={"adapters": {"telegram": {"id": "a1"}}})
C("_obligation_adapter", row={"platform": "no_such_xyz", "obligation_id": "o1"},
  io={"logger.debug": None})
C("_obligation_adapter", row={"obligation_id": "o1"}, io={"logger.debug": None})
C("_obligation_adapter", row={"platform": "telegram", "profile": "p1", "obligation_id": "o1"},
  io={"self._authorization_adapter": sc({"id": "auth"})})
C("_obligation_adapter", row={"platform": "telegram", "obligation_id": "o1"},
  io={"delivery_ledger.is_flood_error": False,
      "delivery_ledger.release_runtime_claim": None})
C("_obligation_adapter",
  row={"platform": "telegram", "obligation_id": "o1", "last_error": "flood_control:30"},
  io={"delivery_ledger.is_flood_error": True,
      "delivery_ledger.release_runtime_claim": None})
C("_obligation_adapter", row={"platform": "telegram", "profile": None, "obligation_id": "o1"},
  io={"self._authorization_adapter": rs("RuntimeError", "auth")})
C("_obligation_adapter", row={"platform": "telegram", "obligation_id": "o1"},
  io={"delivery_ledger.is_flood_error": rs("ValueError", "flood")})
C("_obligation_adapter", row={"platform": "telegram", "obligation_id": "o1"},
  io={"delivery_ledger.is_flood_error": False,
      "delivery_ledger.release_runtime_claim": rs("KeyboardInterrupt", "stop")})
C("_obligation_adapter", row={"platform": "telegram", "obligation_id": "o1"}, io={})

# ── 9. _on_relay_provisioned ─────────────────────────────────────────────────
C("_on_relay_provisioned", state={"_failed_platforms": {"relay": {"next_retry": 99}}})
C("_on_relay_provisioned", state={"_failed_platforms": {}})
C("_on_relay_provisioned", state={"_failed_platforms": {"relay": None}})
C("_on_relay_provisioned", state={"_failed_platforms": {"relay": 5}})
C("_on_relay_provisioned", state={"_failed_platforms": {"telegram": {"x": 1}}})
C("_on_relay_provisioned",
  state={"_failed_platforms": {"relay": {"retry_now": False, "next_retry": 5}}})

# ── 10. _start_relay_provision_retry ─────────────────────────────────────────
C("_start_relay_provision_retry", io={"relay.relay_provision_pending": False})
C("_start_relay_provision_retry",
  io={"relay.relay_provision_pending": True, "relay.retry_self_provision": None,
      "asyncio.create_task": sc({"task": "t1"}), "self._retain_background_task": None})
C("_start_relay_provision_retry",
  io={"relay.relay_provision_pending": rs("RuntimeError", "pend"),
      "logger.warning": None})
C("_start_relay_provision_retry",
  io={"relay.relay_provision_pending": rs("SystemExit", "bye")})
C("_start_relay_provision_retry",
  io={"relay.relay_provision_pending": True, "relay.retry_self_provision": None,
      "asyncio.create_task": rs("RuntimeError", "task"), "logger.warning": None})
C("_start_relay_provision_retry",
  io={"relay.relay_provision_pending": True, "relay.retry_self_provision": None,
      "asyncio.create_task": None, "self._retain_background_task": rs("OSError", "ret"),
      "logger.warning": None})
C("_start_relay_provision_retry",
  io={"relay.relay_provision_pending": rs("ValueError", "v"),
      "logger.warning": rs("RuntimeError", "log")})
C("_start_relay_provision_retry", io={})

# ── 11. _register_config_hooks ───────────────────────────────────────────────
HOOK_IO = {"config.load_config": sc({"hooks": []}),
           "shell_hooks.register_from_config": None,
           "outbound_webhooks.register_from_config": None}
C("_register_config_hooks", fail_fmt="hooks failed", io=dict(HOOK_IO))
C("_register_config_hooks", fail_fmt="hooks failed",
  io={"config.load_config": rs("RuntimeError", "cfg"), "logger.log": None})
C("_register_config_hooks", fail_fmt="hooks failed",
  io={"config.load_config": None,
      "shell_hooks.register_from_config": rs("ValueError", "sh"), "logger.log": None})
C("_register_config_hooks", fail_fmt="hooks failed",
  io={"config.load_config": None, "shell_hooks.register_from_config": None,
      "outbound_webhooks.register_from_config": rs("OSError", "ow"), "logger.log": None})
C("_register_config_hooks", fail_fmt="hooks %s failed", fail_args=["x"], level=30,
  io={"config.load_config": rs("RuntimeError", "cfg"), "logger.log": None})
C("_register_config_hooks", fail_fmt="f",
  io={"config.load_config": rs("RuntimeError", "cfg"),
      "logger.log": rs("RuntimeError", "loglog")})
C("_register_config_hooks", fail_fmt="f",
  io={"config.load_config": rs("GeneratorExit", "gen")})
C("_register_config_hooks", fail_fmt="f", io={})

# ── 12. _register_launch_profile_config_hooks ────────────────────────────────
C("_register_launch_profile_config_hooks",
  io={"secret_scope.is_multiplex_active": False, **HOOK_IO})
C("_register_launch_profile_config_hooks",
  io={"secret_scope.is_multiplex_active": True,
      "hermes_constants.get_process_hermes_home": "/h/launch",
      "run._profile_runtime_scope": None, **HOOK_IO})
C("_register_launch_profile_config_hooks",
  io={"secret_scope.is_multiplex_active": rs("RuntimeError", "mp")})
C("_register_launch_profile_config_hooks",
  io={"secret_scope.is_multiplex_active": True,
      "hermes_constants.get_process_hermes_home": "/h",
      "run._profile_runtime_scope": rs("RuntimeError", "scope")})
C("_register_launch_profile_config_hooks",
  io={"secret_scope.is_multiplex_active": True,
      "hermes_constants.get_process_hermes_home": rs("OSError", "home")})
C("_register_launch_profile_config_hooks", io={})

# ── 13. _start_register_plugins_relay_hooks ──────────────────────────────────
RELAY_OK = {"plugins.sweep_pending_plugin_deletes": None,
            "plugins.discover_plugins": None, "relay.self_provision_relay": None,
            "relay.register_relay_adapter": True, "relay.relay_url": "http://r",
            "logger.info": None, "relay.send_relay_policy": None,
            "secret_scope.is_multiplex_active": False, **HOOK_IO}
C("_start_register_plugins_relay_hooks", io=dict(RELAY_OK))
C("_start_register_plugins_relay_hooks",
  io={**RELAY_OK, "plugins.discover_plugins": rs("RuntimeError", "disc"),
      "logger.log": None})
C("_start_register_plugins_relay_hooks",
  io={**RELAY_OK, "relay.register_relay_adapter": False})
C("_start_register_plugins_relay_hooks",
  io={**RELAY_OK, "relay.self_provision_relay": rs("OSError", "prov"),
      "logger.warning": None})
C("_start_register_plugins_relay_hooks",
  io={**RELAY_OK, "relay.relay_url": rs("RuntimeError", "url"),
      "logger.warning": None})
C("_start_register_plugins_relay_hooks",
  io={**RELAY_OK, "logger.info": rs("RuntimeError", "info"), "logger.warning": None})
C("_start_register_plugins_relay_hooks",
  io={**RELAY_OK, "secret_scope.is_multiplex_active": rs("RuntimeError", "mp")})
C("_start_register_plugins_relay_hooks",
  io={**RELAY_OK, "plugins.sweep_pending_plugin_deletes": rs("SystemExit", "bye")})
C("_start_register_plugins_relay_hooks", io={})

# ── 14. _recover_secondary_process_checkpoints ───────────────────────────────
C("_recover_secondary_process_checkpoints", state={"config": {}})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}},
  io={"hermes_constants.get_hermes_home": "/h",
      "Path.resolve": ["/h", "/h/p1", "/h/p2"],
      "run._multiplex_profile_homes": sc([["p1", "/h/p1"], ["p2", "/h/p2"]]),
      "run._profile_runtime_scope": [None, None],
      "process_registry.recover_from_checkpoint": [2, 3]})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}},
  io={"hermes_constants.get_hermes_home": "/h",
      "Path.resolve": ["/h", "/h"],
      "run._multiplex_profile_homes": sc([["p1", "/h/p1"]]),
      "process_registry.recover_from_checkpoint": [5]})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}},
  io={"hermes_constants.get_hermes_home": "/h",
      "Path.resolve": ["/h", "/h/p1"],
      "run._multiplex_profile_homes": sc([["p1", "/h/p1"]]),
      "run._profile_runtime_scope": None,
      "process_registry.recover_from_checkpoint": rs("OSError", "ckpt"),
      "logger.warning": None})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}},
  io={"hermes_constants.get_hermes_home": "/h",
      "Path.resolve": ["/h", "/h/p1"],
      "run._multiplex_profile_homes": sc([["p1", "/h/p1"]]),
      "run._profile_runtime_scope": None,
      "process_registry.recover_from_checkpoint": "x", "logger.warning": None})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}},
  io={"hermes_constants.get_hermes_home": "/h",
      "Path.resolve": ["/h", "/h/p1"],
      "run._multiplex_profile_homes": sc([["p1", "/h/p1"]]),
      "run._profile_runtime_scope": rs("RuntimeError", "scope"),
      "logger.warning": None})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}},
  io={"hermes_constants.get_hermes_home": "/h",
      "Path.resolve": ["/h", rs("OSError", "resolve")]})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}},
  io={"hermes_constants.get_hermes_home": "/h", "Path.resolve": ["/h", "/h/p1"],
      "run._multiplex_profile_homes": sc([["p1", "/h/p1"]]),
      "run._profile_runtime_scope": None,
      "process_registry.recover_from_checkpoint": rs("KeyboardInterrupt", "stop")})
C("_recover_secondary_process_checkpoints",
  state={"config": {"multiplex_profiles": True}}, io={})

# ── 15. _start_log_retired_session_reset ─────────────────────────────────────
C("_start_log_retired_session_reset", state={"config": {"multiplex_profiles": False}},
  io={"profiles.profiles_to_serve": sc([["default", "/h"]]),
      "config_loader.read_yaml_layers": [sc({})],
      "session_reset_retirement.retired_reset_policy": [None]})
C("_start_log_retired_session_reset", state={"config": {"multiplex_profiles": True}},
  io={"profiles.profiles_to_serve": sc([["default", "/h"]]),
      "config_loader.read_yaml_layers": [sc({"a": 1})],
      "session_reset_retirement.retired_reset_policy": [["/h/c.yaml", "idle"]],
      "session_reset_retirement.reset_plugin_enabled": False,
      "session_reset_retirement.format_notice": "notice text", "logger.warning": None})
C("_start_log_retired_session_reset", state={"config": {"multiplex_profiles": False}},
  io={"profiles.profiles_to_serve": sc([["default", "/h"]]),
      "config_loader.read_yaml_layers": [sc({})],
      "session_reset_retirement.retired_reset_policy": [["/h/c.yaml", "daily"]],
      "session_reset_retirement.reset_plugin_enabled": True})
C("_start_log_retired_session_reset", state={"config": {"multiplex_profiles": False}},
  io={"profiles.profiles_to_serve": sc([["default", "/h"]]),
      "config_loader.read_yaml_layers": [sc({})],
      "session_reset_retirement.retired_reset_policy": rs("ValueError", "pol"),
      "logger.log": None})
C("_start_log_retired_session_reset", state={"config": {}},
  io={"logger.log": None})
C("_start_log_retired_session_reset", state={"config": {"multiplex_profiles": False}},
  io={"profiles.profiles_to_serve": sc([["default", "/h"]]),
      "config_loader.read_yaml_layers": [sc({})],
      "session_reset_retirement.retired_reset_policy": [["/h/c.yaml", "idle"]],
      "session_reset_retirement.reset_plugin_enabled": False,
      "session_reset_retirement.format_notice": rs("RuntimeError", "fmt"),
      "logger.log": None})
C("_start_log_retired_session_reset", state={"config": {"multiplex_profiles": False}},
  io={"profiles.profiles_to_serve": 5, "logger.log": None})
C("_start_log_retired_session_reset", state={"config": {"multiplex_profiles": False}},
  io={})

# ── 16. _start_log_systemd_timing_alignment ──────────────────────────────────
ALIGN = {"mismatch": True, "unit": "hermes.service", "timeout_stop_sec": 30,
         "drain_timeout": 60, "cron_drain_timeout": 30, "expected_min": 90}
C("_start_log_systemd_timing_alignment", state={"_restart_drain_timeout": 60},
  io={"shutdown_forensics.check_systemd_timing_alignment": None})
C("_start_log_systemd_timing_alignment", state={"_restart_drain_timeout": 60},
  io={"shutdown_forensics.check_systemd_timing_alignment": sc(ALIGN),
      "logger.warning": None})
C("_start_log_systemd_timing_alignment", state={"_restart_drain_timeout": 60},
  io={"shutdown_forensics.check_systemd_timing_alignment":
      sc({"mismatch": True, "unit": "u", "drain_timeout": 60, "expected_min": 90}),
      "logger.log": None})
C("_start_log_systemd_timing_alignment", state={"_restart_drain_timeout": 60},
  io={"shutdown_forensics.check_systemd_timing_alignment": rs("OSError", "sys"),
      "logger.log": None})
C("_start_log_systemd_timing_alignment", state={}, io={"logger.log": None})
C("_start_log_systemd_timing_alignment", state={"_restart_drain_timeout": 60},
  io={"shutdown_forensics.check_systemd_timing_alignment":
      sc({"mismatch": False})})
C("_start_log_systemd_timing_alignment", state={"_restart_drain_timeout": 60},
  io={"shutdown_forensics.check_systemd_timing_alignment": 5, "logger.log": None})
C("_start_log_systemd_timing_alignment",
  state={"_restart_drain_timeout": 60, "_cron_drain_timeout": 45},
  io={"shutdown_forensics.check_systemd_timing_alignment": sc(ALIGN),
      "logger.warning": None})
C("_start_log_systemd_timing_alignment", state={"_restart_drain_timeout": 60}, io={})

# ── 17. _open_faulthandler_log ───────────────────────────────────────────────
C("_open_faulthandler_log", state={"config": {"log_dir": "/logs"}},
  io={"os.makedirs": None, "open": sc({"__file__": "fh"})})
C("_open_faulthandler_log", state={"config": {}},
  io={"run.get_hermes_home": "/h", "os.makedirs": None, "open": sc({"__file__": "fh"})})
C("_open_faulthandler_log", state={"config": {"log_dir": "/logs"}},
  io={"os.makedirs": rs("OSError", "mkdir")})
C("_open_faulthandler_log", state={"config": {"log_dir": "/logs"}},
  io={"os.makedirs": None, "open": rs("OSError", "open")})
C("_open_faulthandler_log", state={"config": {}},
  io={"run.get_hermes_home": rs("RuntimeError", "home")})
C("_open_faulthandler_log", state={"config": {"log_dir": "/logs"}}, io={})

# ── 18. _start_install_faulthandler ──────────────────────────────────────────
C("_start_install_faulthandler",
  io={"faulthandler.enable": None, "signal.SIGUSR2": None})
C("_start_install_faulthandler", state={"config": {"log_dir": "/logs"}},
  io={"faulthandler.enable": [rs("RuntimeError", "no stderr"), None],
      "log_suppressed": None, "os.makedirs": None, "open": sc({"__file__": "fh"}),
      "signal.SIGUSR2": None})
C("_start_install_faulthandler",
  io={"faulthandler.enable": rs("TypeError", "bad")})
C("_start_install_faulthandler", state={"config": {"log_dir": "/logs"}},
  io={"faulthandler.enable": None, "signal.SIGUSR2": 12, "log_suppressed": None,
      "os.makedirs": None, "open": sc({"__file__": "fh"}),
      "faulthandler.register": None})
C("_start_install_faulthandler", faulthandler_has_register=False,
  io={"faulthandler.enable": None, "signal.SIGUSR2": 12})
C("_start_install_faulthandler", state={"config": {"log_dir": "/logs"}},
  io={"faulthandler.enable": rs("ValueError", "v"), "log_suppressed": None,
      "os.makedirs": rs("OSError", "mkdir"), "logger.log": None,
      "signal.SIGUSR2": None})
C("_start_install_faulthandler", state={"config": {"log_dir": "/logs"}},
  io={"faulthandler.enable": None, "signal.SIGUSR2": 12, "log_suppressed": None,
      "os.makedirs": None, "open": sc({"__file__": "fh"}),
      "faulthandler.register": rs("OSError", "reg"), "logger.log": None})
C("_start_install_faulthandler",
  io={"faulthandler.enable": rs("SystemExit", "bye")})
C("_start_install_faulthandler", io={})

# ── 19. _start_flush_runtime_status ──────────────────────────────────────────
C("_start_flush_runtime_status", io={"status.flush_runtime_status_async": True})
C("_start_flush_runtime_status",
  io={"status.flush_runtime_status_async": False, "logger.warning": None})
C("_start_flush_runtime_status",
  io={"status.flush_runtime_status_async": rs("OSError", "flush"),
      "logger.debug": None})
C("_start_flush_runtime_status",
  io={"status.flush_runtime_status_async": rs("CancelledError", "c")})
C("_start_flush_runtime_status",
  io={"status.flush_runtime_status_async": False,
      "logger.warning": rs("RuntimeError", "log")})
C("_start_flush_runtime_status", io={})

# ── 20. _start_check_access_policy ───────────────────────────────────────────
C("_start_check_access_policy", state={"config": {"platforms": {}}},
  io={"platform_registry.loaded_entries": sc([]),
      "run._own_policy_open_startup_violation": None})
C("_start_check_access_policy",
  state={"config": {"platforms": {"telegram": {"enabled": True}}}},
  io={"platform_registry.loaded_entries": sc([]), "os.getenv": [None] * 21 + [""] * 18,
      "logger.warning": None, "run._own_policy_open_startup_violation": None})
C("_start_check_access_policy",
  state={"config": {"platforms": {"telegram": {"enabled": True}}}},
  io={"platform_registry.loaded_entries": sc([]), "os.getenv": ["1"],
      "run._own_policy_open_startup_violation": None})
C("_start_check_access_policy",
  state={"config": {"platforms": {"telegram": {"enabled": True}}}},
  io={"platform_registry.loaded_entries": sc([]),
      "os.getenv": [None] * 21 + ["true"],
      "run._own_policy_open_startup_violation": None})
C("_start_check_access_policy",
  state={"config": {"platforms": {"telegram": {"enabled": True}}}},
  io={"platform_registry.loaded_entries": sc([]),
      "run._own_policy_open_startup_violation": "telegram:open",
      "platform_registry.get": None, "platform_env.authz_field": None,
      "logger.error": None, "run._write_runtime_status_quiet": None,
      "self._request_clean_exit": None})
C("_start_check_access_policy",
  state={"config": {"platforms": {"telegram": {"enabled": True}}}},
  io={"platform_registry.loaded_entries": sc([]),
      "run._own_policy_open_startup_violation": "telegram:open",
      "platform_registry.get": sc({}),
      "platform_env.authz_field": [["TG_DM", "TG_GROUP"], "TG_ALL"],
      "logger.error": None, "run._write_runtime_status_quiet": None,
      "self._request_clean_exit": None})
C("_start_check_access_policy", state={"config": {"platforms": {}}},
  io={"platform_registry.loaded_entries": sc([]),
      "run._own_policy_open_startup_violation": rs("RuntimeError", "viol")})
C("_start_check_access_policy",
  state={"config": {"platforms": {"telegram": {"enabled": True}}}},
  io={"platform_registry.loaded_entries":
      sc([{"source": "plugin", "allowed_users_env": "X_ALLOWED_USERS",
           "allow_all_env": "X_ALLOW_ALL"}]),
      "os.getenv": [None] * 22 + ["1"],
      "run._own_policy_open_startup_violation": None})
C("_start_check_access_policy", state={"config": {"platforms": {}}},
  io={"platform_registry.loaded_entries": sc([]),
      "run._own_policy_open_startup_violation": "no_such_xyz:open"})
C("_start_check_access_policy", state={"config": {"platforms": {}}},
  io={"platform_registry.loaded_entries": sc([]),
      "run._own_policy_open_startup_violation": rs("SystemExit", "bye")})
C("_start_check_access_policy", state={"config": {"platforms": {}}}, io={})

# ── 21. _handoff_resolve_scope ───────────────────────────────────────────────
C("_handoff_resolve_scope", profile_name=None,
  state={"config": {"x": 1}, "adapters": {"telegram": {"id": "a"}}})
C("_handoff_resolve_scope", profile_name="default",
  state={"config": {"x": 1}, "adapters": {}})
C("_handoff_resolve_scope", profile_name="p1",
  state={"_profile_adapters": {"p1": {"telegram": {"id": "s"}}}},
  io={"run.load_gateway_config": sc({"loaded": True})})
C("_handoff_resolve_scope", profile_name="p1", state={"_profile_adapters": {}})
C("_handoff_resolve_scope", profile_name="p1",
  state={"_profile_adapters": {"p1": {"telegram": {}}}},
  io={"run.load_gateway_config": rs("OSError", "cfg"), "logger.error": None})
C("_handoff_resolve_scope", profile_name="p1",
  state={"_profile_adapters": {"p1": {"telegram": {}}}},
  io={"run.load_gateway_config": rs("SystemExit", "bye")})
C("_handoff_resolve_scope", profile_name="p1",
  state={"_profile_adapters": {"p1": {"telegram": {}}}},
  io={"run.load_gateway_config": rs("OSError", "cfg"),
      "logger.error": rs("RuntimeError", "log")})
C("_handoff_resolve_scope", profile_name="p1",
  state={"_profile_adapters": {"p1": {"telegram": {}}}}, io={})

# ── 22. _handoff_home_chat_type ──────────────────────────────────────────────
C("_handoff_home_chat_type", platform="telegram", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": sc({"type": "group"})})
C("_handoff_home_chat_type", platform="matrix", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": sc({"type": "dm"})})
C("_handoff_home_chat_type", platform="discord", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": sc({"type": "weird"})})
C("_handoff_home_chat_type", platform="telegram", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": sc({"type": "channel"})})
C("_handoff_home_chat_type", platform="telegram", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": None})
C("_handoff_home_chat_type", platform="slack", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": rs("OSError", "info"), "logger.debug": None})
C("_handoff_home_chat_type", platform="telegram", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": sc({"type": "  GROUP  "})})
C("_handoff_home_chat_type", platform="telegram", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": 5})
C("_handoff_home_chat_type", platform="matrix", home_chat_id="c1", adapter={},
  io={"adapter.get_chat_info": rs("KeyboardInterrupt", "stop")})
C("_handoff_home_chat_type", platform="telegram", home_chat_id="c1", adapter={}, io={})

# ── 23. _start_aggregate_connect_results ─────────────────────────────────────
OK_AD = {"send_path_degraded": False}
DEG_AD = {"send_path_degraded": True, "DEGRADED_STATUS_MESSAGE": "degraded msg"}
FATAL_AD = {"has_fatal_error": True, "fatal_error_code": "auth",
            "fatal_error_retryable": True, "fatal_error_message": "bad token"}
C("_start_aggregate_connect_results", raw=[])
C("_start_aggregate_connect_results",
  raw=[["telegram", OK_AD, {}, "ok", None]],
  io={"self._publish_primary_adapter": None,
      "self._update_platform_runtime_status": None, "logger.info": None})
C("_start_aggregate_connect_results",
  raw=[["telegram", DEG_AD, {}, "ok", None]],
  io={"self._publish_primary_adapter": None,
      "self._update_platform_runtime_status": None, "logger.info": None})
C("_start_aggregate_connect_results",
  raw=[["telegram", {}, {}, "exception", rs("OSError", "conn")]],
  io={"logger.error": None, "self._safe_adapter_disconnect": None,
      "self._update_platform_runtime_status": None,
      "self._reconnect_queue_entry": sc(ENTRY)})
C("_start_aggregate_connect_results",
  raw=[["telegram", {"has_fatal_error": False}, {}, "failed", None]],
  io={"logger.warning": None, "self._safe_adapter_disconnect": None,
      "self._update_platform_runtime_status": None,
      "self._reconnect_queue_entry": sc(ENTRY)})
C("_start_aggregate_connect_results",
  raw=[["telegram", FATAL_AD, {}, "failed", None]],
  io={"logger.warning": None, "self._safe_adapter_disconnect": None,
      "restart.is_global_startup_conflict": False,
      "self._update_platform_runtime_status": None,
      "self._reconnect_queue_entry": sc(ENTRY)})
C("_start_aggregate_connect_results",
  raw=[["telegram", {**FATAL_AD, "fatal_error_retryable": False}, {}, "failed", None]],
  io={"logger.warning": None, "self._safe_adapter_disconnect": None,
      "restart.is_global_startup_conflict": False,
      "self._update_platform_runtime_status": None})
C("_start_aggregate_connect_results",
  raw=[["telegram", FATAL_AD, {}, "failed", None]],
  io={"logger.warning": None, "self._safe_adapter_disconnect": None,
      "restart.is_global_startup_conflict": True,
      "self._update_platform_runtime_status": None})
C("_start_aggregate_connect_results", raw=[rs("RuntimeError", "escape")],
  io={"logger.error": None})
C("_start_aggregate_connect_results", raw=[["telegram", {}, {}, "aborted", None]])
C("_start_aggregate_connect_results", raw=[["telegram", {}, {}, "ok", None]],
  io={"self._publish_primary_adapter": None})
C("_start_aggregate_connect_results", raw=[["telegram", {}]], io={})
C("_start_aggregate_connect_results", raw=[["telegram", OK_AD, {}, "ok", None]],
  io={"self._publish_primary_adapter": rs("SystemExit", "bye")})
C("_start_aggregate_connect_results", raw=[["telegram", OK_AD, {}, "ok", None]], io={})

# ── 24. _start_secondary_profiles ────────────────────────────────────────────
C("_start_secondary_profiles", connected_count=1,
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": 2,
      "self._unserved_shared_ingress_warnings": sc([])})
C("_start_secondary_profiles", connected_count=1,
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": rs("MultiplexConfigError", "bad mux"),
      "logger.error": None, "self._startup_fail_fatal_config": None})
C("_start_secondary_profiles", connected_count=1,
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": rs("OSError", "sec"),
      "logger.error": None, "self._unserved_shared_ingress_warnings": sc([])})
C("_start_secondary_profiles", connected_count=1,
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": "x", "logger.error": None,
      "self._unserved_shared_ingress_warnings": sc([])})
C("_start_secondary_profiles", connected_count=0,
  io={"process_registry.restore_completions": rs("RuntimeError", "restore")})
C("_start_secondary_profiles", connected_count=0,
  state={"multiplex_skipped_platforms": ["whatsapp"]},
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": 0,
      "logger.warning": None, "self._unserved_shared_ingress_warnings": sc([])})
C("_start_secondary_profiles", connected_count=0,
  state={"multiplex_skipped_platforms": ["whatsapp"],
         "_profile_adapters": {"p1": {"whatsapp": {"id": "w"}}}},
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": 0,
      "self._unserved_shared_ingress_warnings": sc([])})
C("_start_secondary_profiles", connected_count=0,
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": 0,
      "self._unserved_shared_ingress_warnings": sc(["line one", "line two"]),
      "logger.warning": [None, None]})
C("_start_secondary_profiles", connected_count=0,
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": rs("SystemExit", "bye")})
C("_start_secondary_profiles", connected_count=0,
  io={"process_registry.restore_completions": None,
      "self._start_secondary_profile_adapters": rs("MultiplexConfigError", "m"),
      "logger.error": None,
      "self._startup_fail_fatal_config": rs("RuntimeError", "fatal")})
C("_start_secondary_profiles", connected_count=0, io={})

# ── exception-hierarchy completeness locks (py3.14 aliases / new classes) ────
C("_release_runtime_claim_quiet", obligation_id="ob1", log_fmt="f",
  io={"delivery_ledger.release_runtime_claim": rs("IOError", "alias of OSError"),
      "logger.debug": None})
C("_start_install_faulthandler", state={"config": {"log_dir": "/logs"}},
  io={"faulthandler.enable": [rs("PythonFinalizationError", "fin"), None],
      "log_suppressed": None, "os.makedirs": None, "open": sc({"__file__": "fh"}),
      "signal.SIGUSR2": None})
C("_start_flush_runtime_status",
  io={"status.flush_runtime_status_async": rs("WindowsError", "win alias"),
      "logger.debug": None})


def main():
    # _log_suppressed CM factory is a seam; script it (None) for the flows that
    # use it, unless the case deliberately leaves io empty (factory fail-closed).
    ls_funcs = {"_start_log_retired_session_reset", "_start_log_systemd_timing_alignment",
                "_start_register_plugins_relay_hooks"}
    for c in CASES:
        if c["func"] in ls_funcs:
            io = c["in"].get("io")
            if io and "log_suppressed" not in io:
                io["log_suppressed"] = None
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count check (>=5)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    bad = [k for k, v in counts.items() if v < 5]
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if bad:
        raise SystemExit("funcs with <5 cases: %s" % bad)


if __name__ == "__main__":
    main()