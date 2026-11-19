#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_run_flow2.jsonl — differential cases for
the gateway/run.py second-wave FLOW port (src/gateway/gateway_run_flow2.c).

Deterministic, >=5 cases per ported func: branch boundaries, scripted
sequences, fail-closed seams (missing/empty script -> RuntimeError), real
exception kinds (Exception swallowed where live swallows; BaseException
passes through), and the documented scalar/path/float domains.

Run: python tests/gen_gateway_run_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_run_flow2.jsonl")

CASES = []


def C(func, io=None, **inp):
    CASES.append({"func": func, "in": dict(inp, io=io if io is not None else {})})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


# ── context_overflow_reply ──
C("context_overflow_reply", {"t": "context overflow reply"})
C("context_overflow_reply", {"t": None})
C("context_overflow_reply", {})  # fail-closed t
C("context_overflow_reply", {"t": R("ValueError", "bad catalog")})
C("context_overflow_reply", {"t": R("KeyboardInterrupt")})
C("context_overflow_reply", {"t": ["first", "second"]})

# ── rate_limit_reply ──
C("rate_limit_reply", {"reset_delay_from_message": 30, "t": "rate limited"}, text="429 slow down")
C("rate_limit_reply", {"reset_delay_from_message": None, "t": "rate limited"}, text="")
C("rate_limit_reply", {"reset_delay_from_message": 0, "t": "rate limited"}, text="x")
C("rate_limit_reply", {"reset_delay_from_message": 119, "t": "rate limited"}, text="x")
C("rate_limit_reply", {"reset_delay_from_message": 120, "format_reset_window": "2m",
                       "t": "resets in {window}"}, text="quota")
C("rate_limit_reply", {"reset_delay_from_message": 14400, "format_reset_window": "4hr",
                       "t": "resets in {window}"}, text="weekly cap")
C("rate_limit_reply", {"t": "rate limited"}, text="x")  # fail-closed reset_delay
C("rate_limit_reply", {"reset_delay_from_message": 300,
                       "format_reset_window": R("ValueError", "bad seconds")}, text="x")
C("rate_limit_reply", {"reset_delay_from_message": 300, "format_reset_window": "5m",
                       "t": R("SystemExit", "bye")}, text="x")
C("rate_limit_reply", {"reset_delay_from_message": "5", "t": "rate limited"}, text="x")

# ── bridge_section_to_env ──
C("bridge_section_to_env", {}, section={"a": "x", "b": 5},
  mapping={"a": "ENV_A", "b": "ENV_B"}, env={"ENV_A": "old"})
C("bridge_section_to_env", {}, section={"flag": True, "nothing": None, "n": 7},
  mapping={"flag": "E_FLAG", "nothing": "E_NONE", "n": "E_N"}, env={})
C("bridge_section_to_env", {}, section="not a dict", mapping={"a": "E_A"}, env={"E_A": "keep"})
C("bridge_section_to_env", {}, section=None, mapping={"a": "E_A"}, env={})
C("bridge_section_to_env", {}, section={"a": 1}, mapping=None, env={})
C("bridge_section_to_env", {}, section={}, mapping={"a": "E_A"}, env={"E_A": "keep"})
C("bridge_section_to_env", {}, section={"a": "x", "z": "absent-mapped"},
  mapping={"a": "E_A", "missing": "E_M"}, env={})

# ── bridge_max_turns_to_env ──
C("bridge_max_turns_to_env", {}, agent_cfg={"max_turns": "none"}, env={})
C("bridge_max_turns_to_env", {}, agent_cfg={"max_turns": "unlimited"}, env={})
C("bridge_max_turns_to_env", {}, agent_cfg={"max_turns": 120}, env={"HERMES_MAX_ITERATIONS": "60"})
C("bridge_max_turns_to_env", {}, agent_cfg={"max_turns": None}, env={"HERMES_MAX_ITERATIONS": "60"})
C("bridge_max_turns_to_env", {}, agent_cfg={"max_turns": None}, env={})
C("bridge_max_turns_to_env", {}, agent_cfg={"other": 1}, env={"HERMES_MAX_ITERATIONS": "keep"})
C("bridge_max_turns_to_env", {}, agent_cfg="not a dict", env={})
C("bridge_max_turns_to_env", {}, agent_cfg=None, env={})

# ── bridge_terminal_config_to_env ──
C("bridge_terminal_config_to_env",
  {"_is_ssh_remote_tilde_cwd": True},
  terminal_cfg={"backend": "SSH", "cwd": "~/work"}, env={})
C("bridge_terminal_config_to_env",
  {"_is_ssh_remote_tilde_cwd": False, "os.path.expanduser": "/home/u/work"},
  terminal_cfg={"backend": "local", "cwd": "~/work"}, env={})
C("bridge_terminal_config_to_env", {},
  terminal_cfg={"cwd": "."}, env={})
C("bridge_terminal_config_to_env", {},
  terminal_cfg={"cwd": "auto"}, env={})
C("bridge_terminal_config_to_env", {},
  terminal_cfg={"cwd": "cwd"}, env={})
C("bridge_terminal_config_to_env",
  {"_is_ssh_remote_tilde_cwd": False, "os.path.expanduser": "/home/u/x"},
  terminal_cfg={"cwd": "  ~/x  "}, env={"TERMINAL_ENV": "Docker"})
C("bridge_terminal_config_to_env", {},
  terminal_cfg={"docker_volumes": ["/a:/b", "/c:/d"],
                "docker_env": {"K": "V", "N": 1}, "timeout": 300}, env={})
C("bridge_terminal_config_to_env", {},
  terminal_cfg={"cwd": 5, "backend": None}, env={"TERMINAL_ENV": "local"})
C("bridge_terminal_config_to_env", {},
  terminal_cfg={"backend": "ssh", "cwd": "~/x"}, env={})  # fail-closed ssh seam
C("bridge_terminal_config_to_env",
  {"_is_ssh_remote_tilde_cwd": False, "os.path.expanduser": R("OSError", "no home")},
  terminal_cfg={"cwd": "~/x"}, env={})
C("bridge_terminal_config_to_env", {}, terminal_cfg=None, env={})
C("bridge_terminal_config_to_env",
  {"_is_ssh_remote_tilde_cwd": False, "os.path.expanduser": None},
  terminal_cfg={"cwd": "~/x"}, env={})
C("bridge_terminal_config_to_env",
  {"_is_ssh_remote_tilde_cwd": False, "os.path.expanduser": "/expanded"},
  terminal_cfg={"backend": "  LOCAL  ", "cwd": "~", "ssh_port": 22,
                "docker_forward_env": ["A", "B"]}, env={})

# ── bridge_auxiliary_config_to_env ──
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": [[]]},
  auxiliary_cfg={"vision": {"provider": "openai", "model": "gpt-4o", "base_url": "https://x",
                            "api_key": "sk-1"},
                 "approval": {"provider": "auto", "model": "  ", "api_key": "sk-2"}}, env={})
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": [[{"key": "foo"}]]},
  auxiliary_cfg={"foo": {"provider": "p", "model": "m"}, "vision": {"model": "v"}}, env={})
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": [[{"nokey": 1}]]},
  auxiliary_cfg={"vision": {"model": "v"}}, env={})
C("bridge_auxiliary_config_to_env", {},
  auxiliary_cfg={"vision": {"model": "v"}}, env={})  # fail-closed plugin discovery
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": R("KeyboardInterrupt")},
  auxiliary_cfg={}, env={})
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": None},
  auxiliary_cfg={"approval": {"provider": "p"}}, env={})
C("bridge_auxiliary_config_to_env",
  {"get_plugin_auxiliary_tasks": [[{"key": "foo"}, {"bad": 1}]]},
  auxiliary_cfg={"foo": {"model": "m"}, "vision": {"provider": "v"}}, env={})
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": [[]]},
  auxiliary_cfg={"vision": "not a dict", "approval": {"provider": None}}, env={})
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": [[]]},
  auxiliary_cfg=None, env={})
C("bridge_auxiliary_config_to_env", {"get_plugin_auxiliary_tasks": [[{"key": "vision"}]]},
  auxiliary_cfg={"vision": {"provider": "  trim  ", "base_url": " u "}}, env={})

# ── bridge_config_to_env ──
C("bridge_config_to_env",
  {"bridge_config_scalars": None, "get_plugin_auxiliary_tasks": [],
   "apply_media_policy_env": None},
  cfg={"terminal": {"timeout": 60}, "auxiliary": {"vision": {"model": "v"}},
       "agent": {"max_turns": 500, "gateway_timeout": 1800},
       "sessions": {"search_slow_ms": 250},
       "display": {"busy_input_mode": "queue", "busy_steer_ack_enabled": True},
       "timezone": "  UTC  ", "security": {"redact_secrets": True},
       "gateway": {"trust_recent_files_seconds": 30, "platform_connect_timeout": 15}},
  env={"HERMES_MAX_ITERATIONS": "60"})
C("bridge_config_to_env",
  {"bridge_config_scalars": None, "get_plugin_auxiliary_tasks": [],
   "apply_media_policy_env": None},
  cfg={"display": {"busy_steer_ack_enabled": False},
       "gateway": {"platform_connect_timeout": 15}},
  env={"HERMES_GATEWAY_BUSY_STEER_ACK_ENABLED": "true",
       "HERMES_GATEWAY_PLATFORM_CONNECT_TIMEOUT": "99"})
C("bridge_config_to_env",
  {"bridge_config_scalars": None, "get_plugin_auxiliary_tasks": [],
   "apply_media_policy_env": None},
  cfg={"gateway": {"platform_connect_timeout": 15, "trust_recent_files_seconds": None}},
  env={"HERMES_GATEWAY_PLATFORM_CONNECT_TIMEOUT": "   "})
C("bridge_config_to_env",
  {"bridge_config_scalars": None, "get_plugin_auxiliary_tasks": [],
   "apply_media_policy_env": None},
  cfg={"security": {"redact_secrets": None}, "agent": {"max_turns": None},
       "timezone": "", "terminal": "not a dict", "auxiliary": {}},
  env={"HERMES_MAX_ITERATIONS": "stale", "HERMES_REDACT_SECRETS": "keep"})
C("bridge_config_to_env", {}, cfg={}, env={})  # fail-closed bridge_config_scalars
C("bridge_config_to_env",
  {"bridge_config_scalars": None, "get_plugin_auxiliary_tasks": [],
   "apply_media_policy_env": R("ValueError", "bad policy")},
  cfg={"gateway": {}}, env={})
C("bridge_config_to_env", {}, cfg=None, env={})
C("bridge_config_to_env",
  {"bridge_config_scalars": R("SystemExit", "halt")}, cfg={}, env={})
C("bridge_config_to_env",
  {"bridge_config_scalars": None, "get_plugin_auxiliary_tasks": [],
   "apply_media_policy_env": None,
   "_is_ssh_remote_tilde_cwd": False, "os.path.expanduser": "/home/u/w"},
  cfg={"terminal": {"backend": "local", "cwd": "~/w"},
       "display": {"busy_ack_enabled": False}}, env={})

# ── load_bridge_config ──
C("load_bridge_config", {"worker_config_snapshot": {"frozen": True}}, config_path="/h/config.yaml")
C("load_bridge_config", {"worker_config_snapshot": {}}, config_path="/h/config.yaml")
C("load_bridge_config", {"worker_config_snapshot": None,
                         "load_user_config_effective": {"agent": {"max_turns": 5}}},
  config_path="/h/config.yaml")
C("load_bridge_config", {}, config_path="/h/config.yaml")  # fail-closed snapshot
C("load_bridge_config", {"worker_config_snapshot": None,
                         "load_user_config_effective": R("ImportError", "no module")},
  config_path="/h/config.yaml")
C("load_bridge_config", {"worker_config_snapshot": None,
                         "load_user_config_effective": {"ok": 1}},
  config_path="/h//sub/./config.yaml")

# ── best_effort ──
C("best_effort", {"fn": 42}, debug_msg="probe failed: %s")
C("best_effort", {"fn": None}, debug_msg=None)
C("best_effort", {"fn": R("ValueError", "boom"), "logger.debug": None},
  debug_msg="probe failed: %s")
C("best_effort", {"fn": R("ValueError", "boom")}, debug_msg=None)
C("best_effort", {"fn": R("SystemExit", "halt")}, debug_msg="probe failed: %s")
C("best_effort", {"fn": R("ValueError", "boom")}, debug_msg="probe failed: %s")  # log fail-closed
C("best_effort", {"logger.debug": None}, debug_msg="probe failed: %s")  # fn fail-closed
C("best_effort", {"fn": R("CancelledError")}, debug_msg=None)

# ── runtime_agent_kwargs ──
C("runtime_agent_kwargs", {}, runtime={"api_key": "k", "base_url": "u", "provider": "p",
                                       "requested_provider": "rp", "api_mode": "chat",
                                       "command": "cmd", "args": ["--flag", "x"],
                                       "credential_pool": {"pool": 1},
                                       "request_overrides": {"temperature": 1}})
C("runtime_agent_kwargs", {}, runtime={})
C("runtime_agent_kwargs", {}, runtime={"args": None, "provider": "nous"})
C("runtime_agent_kwargs", {}, runtime={"args": []})
C("runtime_agent_kwargs", {}, runtime=None)
C("runtime_agent_kwargs", {}, runtime={"args": 5})
C("runtime_agent_kwargs", {}, runtime={"args": "", "api_key": None})

# ── event_media_kind_is ──
C("event_media_kind_is", {"_event_media_type_at": "image/png"},
  event={"media_types": ["image/png"], "message_type": "document"},
  index=0, mime_prefix="image/", fallback_types=["photo"])
C("event_media_kind_is", {"_event_media_type_at": "application/pdf"},
  event={"message_type": "photo"}, index=0, mime_prefix="image/", fallback_types=["photo"])
C("event_media_kind_is", {"_event_media_type_at": ""},
  event={"message_type": "photo"}, index=1, mime_prefix="image/", fallback_types=["photo"])
C("event_media_kind_is", {"_event_media_type_at": None},
  event={"message_type": "document"}, index=0, mime_prefix="image/", fallback_types=["photo"])
C("event_media_kind_is", {"_event_media_type_at": ""},
  event={}, index=0, mime_prefix="audio/", fallback_types=["voice", "audio"])
C("event_media_kind_is", {}, event={"message_type": "photo"},
  index=0, mime_prefix="image/", fallback_types=["photo"])  # fail-closed sibling
C("event_media_kind_is", {"_event_media_type_at": ""},
  event={"message_type": "bogus"}, index=0, mime_prefix="image/", fallback_types=["photo"])
C("event_media_kind_is", {"_event_media_type_at": ""},
  event={"message_type": "photo"}, index=0, mime_prefix="image/", fallback_types=["bogus"])
C("event_media_kind_is", {"_event_media_type_at": 5},
  event={}, index=0, mime_prefix="image/", fallback_types=["photo"])
C("event_media_kind_is", {"_event_media_type_at": R("KeyboardInterrupt")},
  event={}, index=0, mime_prefix="image/", fallback_types=[])
C("event_media_kind_is", {"_event_media_type_at": "audio/ogg"},
  event={"message_type": None}, index=2, mime_prefix="audio/", fallback_types=["voice"])

# ── format_gateway_process_notification ──
C("format_gateway_process_notification", {},
  evt={"type": "watch_disabled", "message": "watch stopped"})
C("format_gateway_process_notification", {},
  evt={"type": "watch_overflow_tripped", "message": None})
C("format_gateway_process_notification", {},
  evt={"type": "watch_overflow_released"})
C("format_gateway_process_notification", {},
  evt={"type": "watch_match", "session_id": "s1", "command": "make", "pattern": "ERROR",
       "output": "line1\nline2", "suppressed": 3})
C("format_gateway_process_notification", {},
  evt={"type": "watch_match", "suppressed": 0})
C("format_gateway_process_notification", {}, evt={"type": "watch_match"})
C("format_gateway_process_notification", {"format_process_notification": "[delegated]"},
  evt={"type": "async_delegation", "task": "t1"})
C("format_gateway_process_notification", {"format_process_notification": "[hb]"},
  evt={"type": "heartbeat"})
C("format_gateway_process_notification", {}, evt={"type": "completion"})
C("format_gateway_process_notification", {}, evt=None)
C("format_gateway_process_notification", {}, evt={"type": None})
C("format_gateway_process_notification", {}, evt={"type": "heartbeat"})  # fail-closed
C("format_gateway_process_notification", {"format_process_notification": R("ValueError", "x")},
  evt={"type": "async_delegation"})

# ── reconnect_attention_after_secs ──
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": 300}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": "  45.5  "}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": None}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": "abc"}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": "1_000"}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": "not a dict"}})
C("reconnect_attention_after_secs", {"load_config_readonly": None})
C("reconnect_attention_after_secs", {})  # fail-closed
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": []}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": "1e2"}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": True}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {}}})
C("reconnect_attention_after_secs", {"load_config_readonly": {"agent": {"reconnect_attention_after": "1__0"}}})
# ── write_runtime_status_quiet ──
C("write_runtime_status_quiet", {"publish_runtime_status": None}, fields={"status": "ok", "pid": 123})
C("write_runtime_status_quiet", {"publish_runtime_status": R("ValueError", "disk full")},
  fields={"status": "ok"})
C("write_runtime_status_quiet", {"publish_runtime_status": R("SystemExit", "halt")},
  fields={"status": "ok"})
C("write_runtime_status_quiet", {}, fields={"status": "ok"})  # fail-closed swallowed
C("write_runtime_status_quiet", {"publish_runtime_status": None}, fields={})
C("write_runtime_status_quiet", {"publish_runtime_status": None},
  fields={"status": "ok", "note": None})
C("write_runtime_status_quiet", {"publish_runtime_status": R("KeyboardInterrupt")}, fields={})

# ── clear_takeover_marker_quiet ──
C("clear_takeover_marker_quiet", {"clear_takeover_marker": None})
C("clear_takeover_marker_quiet", {"clear_takeover_marker": R("OSError", "readonly")})
C("clear_takeover_marker_quiet", {"clear_takeover_marker": R("KeyboardInterrupt")})
C("clear_takeover_marker_quiet", {})  # fail-closed swallowed
C("clear_takeover_marker_quiet", {"clear_takeover_marker": "ignored result"})
C("clear_takeover_marker_quiet", {"clear_takeover_marker": R("CancelledError")})

# ── housekeeping_chore ──
C("housekeeping_chore", {"fn": None}, label="sweep", args=[1, "x"], kwargs={"k": True})
C("housekeeping_chore", {"fn": R("ValueError", "chore broke"), "logger.debug": None},
  label="sweep", args=[], kwargs={})
C("housekeeping_chore", {"fn": R("SystemExit", "halt")}, label="sweep")
C("housekeeping_chore", {"logger.debug": None}, label="sweep", args=[2], kwargs={})
C("housekeeping_chore", {"fn": R("ValueError", "x")}, label="sweep")  # log fail-closed
C("housekeeping_chore", {"fn": "discarded"}, label="sweep", args=[], kwargs={"a": 1})
C("housekeeping_chore", {"fn": R("CancelledError"), "logger.debug": None}, label="sweep")

# ── housekeeping_paste_sweep ──
C("housekeeping_paste_sweep", {"_sweep_expired_pastes": [[3, 5]], "logger.info": None})
C("housekeeping_paste_sweep", {"_sweep_expired_pastes": [[0, 5]]})
C("housekeeping_paste_sweep", {"_sweep_expired_pastes": None})
C("housekeeping_paste_sweep", {"_sweep_expired_pastes": [[1]]})
C("housekeeping_paste_sweep", {"_sweep_expired_pastes": [[1, 2, 3]]})
C("housekeeping_paste_sweep", {})  # fail-closed propagates (no try)
C("housekeeping_paste_sweep", {"_sweep_expired_pastes": [[2, 0]]})  # logger.info fail-closed
C("housekeeping_paste_sweep", {"_sweep_expired_pastes": R("KeyboardInterrupt")})

# ── housekeeping_misfire_catch_up ──
C("housekeeping_misfire_catch_up", {"fire_overdue_jobs": 2, "logger.info": None},
  cron_provider="ext", adapters={"telegram": {"id": "a1"}}, loop={"id": "loop1"})
C("housekeeping_misfire_catch_up", {"fire_overdue_jobs": 0},
  cron_provider=None, adapters=None, loop=None)
C("housekeeping_misfire_catch_up", {"fire_overdue_jobs": None}, cron_provider="ext")
C("housekeeping_misfire_catch_up", {"fire_overdue_jobs": R("ValueError", "bad cron")},
  cron_provider="ext")
C("housekeeping_misfire_catch_up", {}, cron_provider="ext")  # fail-closed
C("housekeeping_misfire_catch_up", {"fire_overdue_jobs": 1}, cron_provider="ext")  # log fail-closed
C("housekeeping_misfire_catch_up", {"fire_overdue_jobs": R("SystemExit", "halt")},
  cron_provider="ext")

# ── housekeeping_memory_trim ──
C("housekeeping_memory_trim", {"trim_memory_when_idle": None})
C("housekeeping_memory_trim", {"trim_memory_when_idle": R("ValueError", "trim failed")})
C("housekeeping_memory_trim", {"trim_memory_when_idle": R("SystemExit", "halt")})
C("housekeeping_memory_trim", {})  # fail-closed
C("housekeeping_memory_trim", {"trim_memory_when_idle": "discarded"})

# ── housekeeping_checkpoint_prune ──
C("housekeeping_checkpoint_prune", {"auto_prune_from_config": None})
C("housekeeping_checkpoint_prune", {"auto_prune_from_config": R("OSError", "gc failed")})
C("housekeeping_checkpoint_prune", {"auto_prune_from_config": R("KeyboardInterrupt")})
C("housekeeping_checkpoint_prune", {})  # fail-closed
C("housekeeping_checkpoint_prune", {"auto_prune_from_config": 7})

# ── housekeeping_plugin_update_check ──
C("housekeeping_plugin_update_check", {"maybe_run_gateway_check": None})
C("housekeeping_plugin_update_check", {"maybe_run_gateway_check": R("OSError", "network")})
C("housekeeping_plugin_update_check", {"maybe_run_gateway_check": R("SystemExit", "halt")})
C("housekeeping_plugin_update_check", {})  # fail-closed
C("housekeeping_plugin_update_check", {"maybe_run_gateway_check": "discarded"})

# ── launch_sessions_dir ──
C("launch_sessions_dir", {"get_hermes_home": "/home/u"}, config={"sessions_dir": "/data/sessions"})
C("launch_sessions_dir", {"get_hermes_home": "/home/u"}, config={"sessions_dir": None})
C("launch_sessions_dir", {"get_hermes_home": "/home/u"}, config={})
C("launch_sessions_dir", {"get_hermes_home": "/home/u"}, config=None)
C("launch_sessions_dir", {}, config={"sessions_dir": "/data"})  # fail-closed home
C("launch_sessions_dir", {"get_hermes_home": "/home/u/"},
  config={"sessions_dir": "sessions//sub/."})
C("launch_sessions_dir", {"get_hermes_home": "//srv/home"},
  config={"sessions_dir": "/data"})
C("launch_sessions_dir", {"get_hermes_home": R("OSError", "no home")},
  config={"sessions_dir": "/data"})

# ── profile_sessions_dir ──
C("profile_sessions_dir", {"get_hermes_home": "/home/u"}, launch=None)
C("profile_sessions_dir", {"get_hermes_home": "/home/u"}, launch=["/home/u", "/custom/transcripts"])
C("profile_sessions_dir", {"get_hermes_home": "/home/u"}, launch=["/other/home", "/custom"])
C("profile_sessions_dir", {"get_hermes_home": "/home/u"}, launch=[])
C("profile_sessions_dir", {"get_hermes_home": "/home/u"}, launch=["/home/u"])
C("profile_sessions_dir", {"get_hermes_home": "/"}, launch=None)
C("profile_sessions_dir", {"get_hermes_home": "."}, launch=None)
C("profile_sessions_dir", {"get_hermes_home": "/a"}, launch=["/a//.", "/c/"])
C("profile_sessions_dir", {}, launch=None)  # fail-closed home
C("profile_sessions_dir", {"get_hermes_home": "/home/u"}, launch=["/home/u", "/custom/../keep"])

# ── guard_corrupt_user_config ──
C("guard_corrupt_user_config", {"require_parseable_user_config": None})
C("guard_corrupt_user_config",
  {"require_parseable_user_config": R("InvalidUserConfigError", "yaml: line 3"),
   "print": None})
C("guard_corrupt_user_config", {"require_parseable_user_config": R("ValueError", "other")})
C("guard_corrupt_user_config", {})  # fail-closed RuntimeError propagates
C("guard_corrupt_user_config",
  {"require_parseable_user_config": R("InvalidUserConfigError", "bad yaml")})  # print fail-closed
C("guard_corrupt_user_config", {"require_parseable_user_config": R("KeyboardInterrupt")})
C("guard_corrupt_user_config",
  {"require_parseable_user_config": R("InvalidUserConfigError", "e"),
   "print": R("OSError", "stderr closed")})

# ── wait_for_pid_exit ──
C("wait_for_pid_exit", {"_pid_exists": [False]}, pid=100, attempts=3, delay=0.5)
C("wait_for_pid_exit", {"_pid_exists": [True, True, False], "asyncio.sleep": None},
  pid=100, attempts=5, delay=0.1)
C("wait_for_pid_exit", {"_pid_exists": [True], "asyncio.sleep": None},
  pid=100, attempts=3, delay=0.2)
C("wait_for_pid_exit", {}, pid=100, attempts=0, delay=0.5)
C("wait_for_pid_exit", {}, pid=100, attempts=2, delay=0.5)  # fail-closed _pid_exists
C("wait_for_pid_exit", {"_pid_exists": [True], "asyncio.sleep": R("CancelledError")},
  pid=100, attempts=2, delay=0.5)
C("wait_for_pid_exit", {"_pid_exists": [True]}, pid=100, attempts=-1, delay=0.5)
C("wait_for_pid_exit", {"_pid_exists": [True, False], "asyncio.sleep": R("SystemExit", "h")},
  pid=100, attempts=4, delay=1)

# ── resolve_handoff_watch_scopes ──
C("resolve_handoff_watch_scopes", {}, runner={"config": {"multiplex_profiles": False}})
C("resolve_handoff_watch_scopes",
  {"run_in_executor_with_context": [[["/h1", "p1"], [None, None]]]},
  runner={"config": None, "_run_in_executor_with_context": {"fn": 1}})
C("resolve_handoff_watch_scopes", {"_handoff_watch_scopes": [[[None, None]]]},
  runner={"config": {"multiplex_profiles": True}})
C("resolve_handoff_watch_scopes", {"_handoff_watch_scopes": [[["/h", "default"]]]},
  runner={"_run_in_executor_with_context": {}})
C("resolve_handoff_watch_scopes", {},
  runner={"config": {"multiplex_profiles": True}, "_run_in_executor_with_context": {"fn": 1}})
C("resolve_handoff_watch_scopes", {"_handoff_watch_scopes": R("ValueError", "fs walk failed")},
  runner={"config": {"multiplex_profiles": True}})
C("resolve_handoff_watch_scopes",
  {"run_in_executor_with_context": R("KeyboardInterrupt")},
  runner={"config": {"multiplex_profiles": True}, "_run_in_executor_with_context": {"fn": 1}})
C("resolve_handoff_watch_scopes", {"_handoff_watch_scopes": [[]]}, runner={})

# ── log_standalone_profiles_at_boot ──
C("log_standalone_profiles_at_boot", {"logger.warning": None}, runner={})
C("log_standalone_profiles_at_boot", {}, runner={"config": None})
C("log_standalone_profiles_at_boot", {}, runner={"config": {"multiplex_profiles": False}})
C("log_standalone_profiles_at_boot",
  {"served_profile_names": [[]], "profiles_to_serve": [[["default", "/h"], ["p1", "/h1"]]],
   "profile_is_standalone": True, "logger.warning": None},
  runner={"config": {"multiplex_profiles": True}})
C("log_standalone_profiles_at_boot",
  {"served_profile_names": [["p1"]], "profiles_to_serve": [[["p1", "/h1"], ["p2", "/h2"]]],
   "profile_is_standalone": False},
  runner={"config": {"multiplex_profiles": True}})
C("log_standalone_profiles_at_boot", {"logger.warning": None},
  runner={"config": {"multiplex_profiles": True}})  # served fail-closed
C("log_standalone_profiles_at_boot",
  {"served_profile_names": [[]], "profiles_to_serve": R("ValueError", "fs"),
   "logger.warning": None},
  runner={"config": {"multiplex_profiles": True}})
C("log_standalone_profiles_at_boot",
  {"served_profile_names": [[]], "profiles_to_serve": R("SystemExit", "halt")},
  runner={"config": {"multiplex_profiles": True}})
C("log_standalone_profiles_at_boot", {},
  runner={"config": {"multiplex_profiles": True}})  # except-path warning fail-closed
C("log_standalone_profiles_at_boot",
  {"served_profile_names": [[]], "profiles_to_serve": [[["p1"]]], "logger.warning": None},
  runner={"config": {"multiplex_profiles": True}})
C("log_standalone_profiles_at_boot",
  {"served_profile_names": [[]], "profiles_to_serve": [[["p1", "/h1"], ["p2", "/h2"]]],
   "profile_is_standalone": [True, False], "logger.warning": None},
  runner={"config": {"multiplex_profiles": True}})
C("log_standalone_profiles_at_boot",
  {"served_profile_names": None, "logger.warning": None},
  runner={"config": {"multiplex_profiles": True}})

# ── host_attach_or_none ──
C("host_attach_or_none", {"logger.warning": None}, replace=False, force=True)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "attach", "owner": {"pid": 42, "describe": "gw pid 42"},
              "message": "attaching"},
   "owner.describe": "gateway pid 42 (home /home/u)", "logger.info": None, "print": None},
  replace=False, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "attach", "owner": None, "message": "attaching"},
   "logger.info": None, "print": None},
  replace=True, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "refuse", "owner": {"pid": 42}, "message": "refusing"},
   "owner.describe": "gw 42", "logger.error": None, "print": None},
  replace=False, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "replace-host", "owner": {"pid": 42}, "message": "replacing"},
   "_start_gateway_replace_existing_instance": True},
  replace=True, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "replace-host", "owner": {"pid": 42}, "message": "replacing"},
   "_start_gateway_replace_existing_instance": False},
  replace=True, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "replace-host", "owner": None, "message": "x"}},
  replace=False, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u", "decide": {"outcome": "start", "owner": None, "message": "x"}},
  replace=False, force=False)
C("host_attach_or_none", {"get_hermes_home": "/home/u"}, replace=False, force=False)
C("host_attach_or_none", {}, replace=False, force=True)  # force warning fail-closed
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "attach", "owner": {"pid": 42}, "message": "m"},
   "owner.describe": R("OSError", "owner gone")},
  replace=False, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home/u",
   "decide": {"outcome": "replace-host", "owner": {"pid": 42}, "message": "m"},
   "_start_gateway_replace_existing_instance": R("CancelledError")},
  replace=True, force=False)
C("host_attach_or_none",
  {"get_hermes_home": "/home//u/", "decide": {"outcome": "start", "owner": None, "message": "x"}},
  replace=False, force=False)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    seen = {}
    for c in CASES:
        seen[c["func"]] = seen.get(c["func"], 0) + 1
    thin = [f for f, n in seen.items() if n < 5]
    if thin:
        raise SystemExit("funcs with <5 cases: %s" % thin)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print("wrote %d cases (%d funcs) -> %s" % (len(CASES), len(seen), OUT))


if __name__ == "__main__":
    main()