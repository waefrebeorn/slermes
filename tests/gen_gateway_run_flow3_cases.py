#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_run_flow3.jsonl — differential cases for
the gateway/run.py third-wave FLOW port (src/gateway/gateway_run_flow3.c).

Deterministic, >=5 cases per ported func: branch boundaries, scripted
sequences, fail-closed seams (missing/empty script -> RuntimeError), real
exception kinds (Exception swallowed where live swallows; BaseException
passes through), and the documented marker/scalar/numeric domains.

Run: python tests/gen_gateway_run_flow3_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_run_flow3.jsonl")
CASES = []


def C(fn, io=None, **inp):
    CASES.append({"func": fn, "in": dict(inp, io=io if io is not None else {})})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


# ── telegramize_command_mentions ──
C("telegramize_command_mentions", {}, platform="discord", text="/start here")
C("telegramize_command_mentions", {}, platform={"value": "telegram"}, text="no commands")
C("telegramize_command_mentions", {"sanitize_telegram_name": "start"},
  platform="telegram", text="hello /start world")
C("telegramize_command_mentions", {"sanitize_telegram_name": ""},
  platform="telegram", text="/Start")
C("telegramize_command_mentions", {"sanitize_telegram_name": "mycmd_2"},
  platform="telegram", text="/MyCmd-2")
C("telegramize_command_mentions", {"sanitize_telegram_name": ["x", "y"]},
  platform="telegram", text="/a and /b_c")
C("telegramize_command_mentions", {"sanitize_telegram_name": "ok"},
  platform="telegram", text="a/b :c/d //e /ok")
C("telegramize_command_mentions", {"sanitize_telegram_name": ["p", "q"]},
  platform="telegram", text="/foo-/bar")
C("telegramize_command_mentions", {}, platform="telegram", text="plain text")
C("telegramize_command_mentions", {}, platform="telegram", text="/x")  # fail-closed
C("telegramize_command_mentions", {"sanitize_telegram_name": R("ValueError", "bad name")},
  platform="telegram", text="/x")
C("telegramize_command_mentions", {"sanitize_telegram_name": R("KeyboardInterrupt")},
  platform="telegram", text="/x")
C("telegramize_command_mentions", {}, platform="telegram", text="/x", import_fail=True)
C("telegramize_command_mentions", {"sanitize_telegram_name": None},
  platform="telegram", text="/Keep")
C("telegramize_command_mentions", {}, platform={"novalue": 1}, text="/start")
C("telegramize_command_mentions", {"sanitize_telegram_name": ["a1", "a2", "a3"]},
  platform="telegram", text="/one /two /three")

# ── watch_gateway_turn_inactivity ──
WBASE = dict(task_id="t1", process_baseline={"pid": 7}, timeout=10,
             worker_done="ev1", timeout_fired="tf1", cleanup_lock="lk1",
             poll_interval=5.0)
C("watch_gateway_turn_inactivity",
  {"time.monotonic": 100, "worker_done.wait": True}, agent_holder=[], **WBASE)
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 101], "worker_done.wait": [False, True]},
  agent_holder=[], **WBASE)
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 200], "worker_done.wait": False,
   "_abandon_timed_out_gateway_turn": None},
  agent_holder=[], **dict(WBASE, timeout=50))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": {"seconds_since_activity": 5}, "awake_idle.measure": 5},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": {"seconds_since_activity": None}, "awake_idle.measure": 5},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": {}},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": "not a dict"},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": R("RuntimeError", "provider hang")},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": R("KeyboardInterrupt")},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110, 120], "worker_done.wait": [False, False, True],
   "get_activity_summary": {"seconds_since_activity": 5},
   "awake_idle.measure": R("ValueError", "bad meter")},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": {"seconds_since_activity": -5}, "awake_idle.measure": 0},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 200], "worker_done.wait": False,
   "_abandon_timed_out_gateway_turn": R("SystemExit", "bye")},
  agent_holder=[], **dict(WBASE, timeout=50))
C("watch_gateway_turn_inactivity", {"time.monotonic": 100},
  agent_holder=[], **WBASE)  # fail-closed worker_done.wait
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 101], "worker_done.wait": [False, True]},
  agent_holder=[], **dict(WBASE, poll_interval=0.001))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 200], "worker_done.wait": False,
   "_abandon_timed_out_gateway_turn": None},
  agent_holder=[], is_still_current="cb1", **dict(WBASE, timeout=50))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 101], "worker_done.wait": [False, True]},
  agent_holder=None, **WBASE)
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 101], "worker_done.wait": [False, True]},
  agent_holder=[{"other": 1}], **WBASE)
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110, 120], "worker_done.wait": [False, False],
   "get_activity_summary": [{"seconds_since_activity": 5}, {"seconds_since_activity": 50}],
   "awake_idle.measure": [5, 50], "_abandon_timed_out_gateway_turn": None},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": {"seconds_since_activity": [1]}},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 110], "worker_done.wait": [False, True],
   "get_activity_summary": {"seconds_since_activity": 5},
   "awake_idle.measure": R("CancelledError")},
  agent_holder=[{"get_activity_summary": True}], **dict(WBASE, timeout=20))
C("watch_gateway_turn_inactivity",
  {"time.monotonic": [100, 200], "worker_done.wait": False,
   "_abandon_timed_out_gateway_turn": {"discarded": 1}},
  agent_holder=[], **dict(WBASE, timeout=50))  # non-null abandon ret -> None

# ── builtin_adapter_import ──
MOD_OK = {"APIServerAdapter": "cls:APIServerAdapter",
          "check_api_server_requirements": True}
C("builtin_adapter_import", {"importlib.import_module": MOD_OK},
  module="api_server", adapter_name="APIServerAdapter",
  requirement="check_api_server_requirements")
C("builtin_adapter_import",
  {"importlib.import_module": {"check_api_server_requirements": True}},
  module="api_server", adapter_name="APIServerAdapter",
  requirement="check_api_server_requirements")
C("builtin_adapter_import", {"importlib.import_module": {"APIServerAdapter": "cls:X"}},
  module="api_server", adapter_name="APIServerAdapter",
  requirement="check_api_server_requirements")
C("builtin_adapter_import", {"importlib.import_module": R("ImportError", "no aiohttp")},
  module="webhook", adapter_name="WebhookAdapter", requirement="check_webhook_requirements")
C("builtin_adapter_import", {"importlib.import_module": None},
  module="api_server", adapter_name="APIServerAdapter",
  requirement="check_api_server_requirements")
C("builtin_adapter_import", {},
  module="api_server", adapter_name="APIServerAdapter",
  requirement="check_api_server_requirements")  # fail-closed
C("builtin_adapter_import",
  {"importlib.import_module": {"A": "fn:a", "B": False}},
  module="webhook", adapter_name="A", requirement="B")
C("builtin_adapter_import", {"importlib.import_module": R("SystemExit")},
  module="api_server", adapter_name="APIServerAdapter",
  requirement="check_api_server_requirements")

# ── instantiate_builtin_adapter ──
C("instantiate_builtin_adapter", {}, platform="telegram", config={})
C("instantiate_builtin_adapter",
  {"importlib.import_module": MOD_OK, "APIServerAdapter": {"ok": 1}},
  platform="api_server", config={"port": 8080})
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"APIServerAdapter": "cls:APIServerAdapter",
                               "check_api_server_requirements": False},
   "logger.warning": None},
  platform="api_server", config={})
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"APIServerAdapter": "cls:APIServerAdapter",
                               "check_api_server_requirements": "fn:check_api_server_requirements"},
   "check_api_server_requirements": False, "logger.warning": None},
  platform="api_server", config={})
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"APIServerAdapter": "cls:APIServerAdapter",
                               "check_api_server_requirements": "fn:check_api_server_requirements"},
   "check_api_server_requirements": True, "APIServerAdapter": "made"},
  platform="api_server", config={"x": 1})
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"APIServerAdapter": "cls:APIServerAdapter",
                               "check_api_server_requirements": "fn:check_api_server_requirements"},
   "check_api_server_requirements": R("OSError", "no loop")},
  platform="api_server", config={})
C("instantiate_builtin_adapter",
  {"importlib.import_module": MOD_OK, "APIServerAdapter": R("ValueError", "bad config")},
  platform="api_server", config={})
C("instantiate_builtin_adapter",
  {"importlib.import_module": MOD_OK, "APIServerAdapter": R("SystemExit")},
  platform="api_server", config={})
C("instantiate_builtin_adapter", {}, platform="nope", config={})  # invalid enum
C("instantiate_builtin_adapter", {"APIServerAdapter": "made"},
  platform="api_server", config={})  # fail-closed import
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"check_api_server_requirements": True}},
  platform="api_server", config={})  # missing adapter attr
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"APIServerAdapter": True,
                               "check_api_server_requirements": True}},
  platform="api_server", config={})  # non-callable adapter_cls
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"APIServerAdapter": "cls:APIServerAdapter",
                               "check_api_server_requirements": False},
   "logger.warning": R("RuntimeError", "log down")},
  platform="api_server", config={})
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"MSGraphWebhookAdapter": "cls:MSGraphWebhookAdapter",
                               "check_msgraph_webhook_requirements": False},
   "logger.warning": None},
  platform="msgraph_webhook", config={})
C("instantiate_builtin_adapter",
  {"importlib.import_module": {"WebhookAdapter": "cls:WebhookAdapter",
                               "check_webhook_requirements": True},
   "WebhookAdapter": None},
  platform="webhook", config=None)

# ── housekeeping_channel_directory ──
C("housekeeping_channel_directory", {}, adapters={"slack": "x"}, loop=None)
C("housekeeping_channel_directory",
  {"build_channel_directory": "coro1", "safe_schedule_threadsafe": "fut1",
   "fut.result": None},
  adapters={"slack": "x"}, loop="loop1")
C("housekeeping_channel_directory",
  {"build_channel_directory": "coro1", "safe_schedule_threadsafe": None},
  adapters={}, loop="loop1")
C("housekeeping_channel_directory",
  {"build_channel_directory": R("RuntimeError", "no adapters")},
  adapters={}, loop="loop1")
C("housekeeping_channel_directory",
  {"build_channel_directory": "coro1", "safe_schedule_threadsafe": R("TimeoutError", "slow")},
  adapters={}, loop="loop1")
C("housekeeping_channel_directory",
  {"build_channel_directory": "coro1", "safe_schedule_threadsafe": "fut1",
   "fut.result": R("TimeoutError", "30s")},
  adapters={}, loop="loop1")
C("housekeeping_channel_directory",
  {"build_channel_directory": "coro1", "safe_schedule_threadsafe": "fut1",
   "fut.result": R("KeyboardInterrupt")},
  adapters={}, loop="loop1")
C("housekeeping_channel_directory", {"safe_schedule_threadsafe": "fut1", "fut.result": None},
  adapters={}, loop="loop1")  # fail-closed build
C("housekeeping_channel_directory",
  {"build_channel_directory": "coro1", "safe_schedule_threadsafe": "fut1",
   "fut.result": {"rows": 3}},
  adapters=[{"platform": "slack"}], loop="loop2")

# ── housekeeping_curator ──
C("housekeeping_curator", {"run_idle_jobs": None}, idle_fence=None)
C("housekeeping_curator", {"run_idle_jobs": None}, idle_fence={"k": 1})
C("housekeeping_curator", {"run_idle_jobs": None, "logger.info": [None, None]},
  idle_fence=None, summary_msgs=["a", "b"])
C("housekeeping_curator", {"run_idle_jobs": R("RuntimeError", "idle down")},
  idle_fence=None, summary_msgs=["a"])
C("housekeeping_curator", {"run_idle_jobs": None, "logger.info": [None, R("ValueError", "log")]},
  idle_fence=None, summary_msgs=["a", "b"])
C("housekeeping_curator", {"run_idle_jobs": None, "logger.info": R("SystemExit")},
  idle_fence=None, summary_msgs=["a"])
C("housekeeping_curator", {}, idle_fence=None)  # fail-closed
C("housekeeping_curator", {"run_idle_jobs": None}, idle_fence=0)
C("housekeeping_curator", {"run_idle_jobs": R("GeneratorExit")}, idle_fence=None)

# ── housekeeping_state_db_maintenance ──
SCOPE = {"profile_dir": "/h/p"}
C("housekeeping_state_db_maintenance", {"load_config": {"sessions": {}}}, launch=None)
C("housekeeping_state_db_maintenance", {"load_config": {}}, launch=None)
C("housekeeping_state_db_maintenance", {"load_config": {"sessions": None}}, launch=None)
C("housekeeping_state_db_maintenance", {"load_config": None}, launch=None)
C("housekeeping_state_db_maintenance", {"load_config": "x"}, launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "run_auto_archive": None, "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True, "auto_archive_days": 7.5,
                                "min_interval_hours": 12}},
   "profile_scope": SCOPE, "open_persistence": "store1",
   "run_auto_archive": None, "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True, "retention_days": 30,
                                "min_interval_hours": 12, "min_vacuum_interval_days": 7,
                                "vacuum_after_prune": False}},
   "profile_scope": SCOPE, "open_persistence": "store1", "open_store_admin": "admin1",
   "native_inputs_root": "/media", "_profile_sessions_dir": "/h/p/sessions",
   "run_auto_prune_and_vacuum": None, "store.close": None},
  launch=["/h/p", "/h/p/transcripts"])
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True, "auto_prune": True}},
   "profile_scope": SCOPE, "open_persistence": "store1", "run_auto_archive": None,
   "open_store_admin": "admin1", "native_inputs_root": "/media",
   "_profile_sessions_dir": "/h/p/sessions", "run_auto_prune_and_vacuum": None,
   "store.close": None},
  launch=["/h/p", "/h/p/transcripts"])
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "run_auto_archive": R("RuntimeError", "archive down"),
   "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "open_store_admin": "admin1",
   "native_inputs_root": "/media", "_profile_sessions_dir": "/h/p/sessions",
   "run_auto_prune_and_vacuum": R("KeyboardInterrupt"), "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "run_auto_archive": None,
   "store.close": R("OSError", "close down")},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True}}, "profile_scope": SCOPE,
   "open_persistence": R("OSError", "open down")},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True}},
   "profile_scope": R("RuntimeError", "no scope")},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True}}, "profile_scope": {},
   "open_persistence": "store1", "open_store_admin": "admin1", "store.close": None},
  launch=None)  # missing profile_dir
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True, "auto_archive_days": None}},
   "profile_scope": SCOPE, "open_persistence": "store1", "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True, "retention_days": None}},
   "profile_scope": SCOPE, "open_persistence": "store1", "open_store_admin": "admin1",
   "native_inputs_root": "/media", "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True, "min_interval_hours": 24.9}},
   "profile_scope": SCOPE, "open_persistence": "store1", "run_auto_archive": None,
   "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True, "vacuum_after_prune": "x"}},
   "profile_scope": SCOPE, "open_persistence": "store1", "open_store_admin": "admin1",
   "native_inputs_root": "/media", "_profile_sessions_dir": "/h/p/sessions",
   "run_auto_prune_and_vacuum": None, "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "open_store_admin": "admin1",
   "native_inputs_root": None, "_profile_sessions_dir": "/h/p/sessions",
   "run_auto_prune_and_vacuum": None, "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance", {}, launch=None)  # fail-closed load_config
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": False, "auto_prune": False}}},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "open_store_admin": "admin1",
   "native_inputs_root": R("OSError", "no media"), "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "open_store_admin": "admin1",
   "native_inputs_root": "/media", "_profile_sessions_dir": R("ValueError", "bad launch"),
   "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_prune": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "open_store_admin": R("PermissionError", "admin"),
   "store.close": None},
  launch=None)
C("housekeeping_state_db_maintenance",
  {"load_config": {"sessions": {"auto_archive": True}}, "profile_scope": SCOPE,
   "open_persistence": "store1", "run_auto_archive": R("RuntimeError", "x"),
   "store.close": R("OSError", "close wins")},
  launch=None)

# ── housekeeping_deferred_fts_retry ──
ST1 = {"marker": "s1", "scope": {"home": "/h"}}
ST2 = {"marker": "s2", "scope": {"home": "/h2"}}
C("housekeeping_deferred_fts_retry", {"live_stores": [[]]})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1]], "retry_deferred_search_recovery": False})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1]], "retry_deferred_search_recovery": True,
   "store_location": "/h/state.db", "logger.info": None})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1, ST2]], "retry_deferred_search_recovery": [False, True],
   "store_location": "/h2/state.db", "logger.info": None})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1]], "retry_deferred_search_recovery": R("RuntimeError", "locked")})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1]], "retry_deferred_search_recovery": R("SystemExit")})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1]], "retry_deferred_search_recovery": True,
   "store_location": R("ValueError", "no scope")})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1]], "retry_deferred_search_recovery": True,
   "store_location": "/h/state.db", "logger.info": R("OSError", "log down")})
C("housekeeping_deferred_fts_retry", {})  # fail-closed live_stores
C("housekeeping_deferred_fts_retry", {"live_stores": R("ImportError", "no assembly")})
C("housekeeping_deferred_fts_retry",
  {"live_stores": [[ST1, ST2]], "retry_deferred_search_recovery": [True, True],
   "store_location": ["/h/state.db", "/h2/state.db"], "logger.info": [None, None]})

# ── start_gateway_make_restart_signal_handler ──
C("start_gateway_make_restart_signal_handler",
  {"logger.info": None, "request_restart": None}, runner={"request_restart": True})
C("start_gateway_make_restart_signal_handler", {"logger.info": None}, runner={})
C("start_gateway_make_restart_signal_handler",
  {"logger.info": R("RuntimeError", "log down")}, runner={"request_restart": True})
C("start_gateway_make_restart_signal_handler",
  {"logger.info": None, "request_restart": R("SystemExit", "restart")},
  runner={"request_restart": True})
C("start_gateway_make_restart_signal_handler",
  {"logger.info": None, "request_restart": R("ValueError", "bad")},
  runner={"request_restart": True})
C("start_gateway_make_restart_signal_handler", {"request_restart": None},
  runner={"request_restart": True})  # fail-closed logger.info
C("start_gateway_make_restart_signal_handler",
  {"logger.info": None, "request_restart": None}, runner=None)
C("start_gateway_make_restart_signal_handler",
  {"logger.info": R("KeyboardInterrupt")}, runner={"request_restart": True})
C("start_gateway_make_restart_signal_handler",
  {"logger.info": None, "request_restart": {"discarded": 1}},
  runner={"request_restart": True})  # non-null request_restart ret -> None

# ── running_agent_ids ──
C("running_agent_ids", {"_running_agent_items": [[["k1", "a1"], ["k2", "a2"]]]},
  runner={"_running_agent_items": True})
C("running_agent_ids", {"_running_agent_items": [[["k1", None], ["k2", "a1"]]]},
  runner={"_running_agent_items": True})
C("running_agent_ids",
  {"_running_agent_items": [[["k1", {"__sentinel__": True}], ["k2", "a1"]]]},
  runner={"_running_agent_items": True})
C("running_agent_ids", {"_running_agent_items": [[]]},
  runner={"_running_agent_items": True})
C("running_agent_ids", {"_running_agent_items": [[["k1", "a1"], ["k2", "a1"]]]},
  runner={"_running_agent_items": True})
C("running_agent_ids", {"_running_agent_items": [[["k1", "b"], ["k2", "a"]]]},
  runner={"_running_agent_items": True})
C("running_agent_ids", {}, runner={})
C("running_agent_ids", {}, runner={"_running_agent_items": True})  # fail-closed
C("running_agent_ids", {"_running_agent_items": R("RuntimeError", "map down")},
  runner={"_running_agent_items": True})
C("running_agent_ids", {"_running_agent_items": R("GeneratorExit")},
  runner={"_running_agent_items": True})
C("running_agent_ids", {"_running_agent_items": [[["k1", "a1"]]]}, runner=None)

# ── admit_bot_message_for_source ──
RUNNER_AUTH = {"_authorization_home_for_source": True, "_admit_bot_message": True}
SRC = {"platform": "telegram", "user_id": "u1"}
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": None, "_admit_bot_message": True},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": "/h/p", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": None, "_admit_bot_message": False},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": "/h/p", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": None, "_admit_bot_message": R("ValueError", "deny")},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": "/h/p", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": None, "_admit_bot_message": R("SystemExit")},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": "/h/p",
   "_profile_runtime_scope.enter": R("RuntimeError", "bind down")},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": "/h/p", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": R("OSError", "reset down"), "_admit_bot_message": True},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": "/h/p", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": R("OSError", "exit wins"),
   "_admit_bot_message": R("ValueError", "body")},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": R("PermissionError", "no home")},
  runner=RUNNER_AUTH, source=SRC)
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": None},
  runner={"_authorization_home_for_source": True}, source=SRC)  # missing admit
C("admit_bot_message_for_source", {"_admit_bot_message": True},
  runner=RUNNER_AUTH, source=SRC)  # fail-closed auth home
C("admit_bot_message_for_source",
  {"_authorization_home_for_source": None, "_admit_bot_message": None},
  runner=RUNNER_AUTH, source=None)

# ── under_authorization_profile ──
C("under_authorization_profile",
  {"_authorization_home_for_source": None, "check": 42},
  runner={"_authorization_home_for_source": True}, source=SRC)
C("under_authorization_profile",
  {"_authorization_home_for_source": "/h", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": None, "check": None},
  runner={"_authorization_home_for_source": True}, source=SRC)
C("under_authorization_profile",
  {"_authorization_home_for_source": "/h", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": None, "check": R("RuntimeError", "k")},
  runner={"_authorization_home_for_source": True}, source=SRC)
C("under_authorization_profile",
  {"_authorization_home_for_source": "/h", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": None, "check": R("KeyboardInterrupt")},
  runner={"_authorization_home_for_source": True}, source=SRC)
C("under_authorization_profile",
  {"_authorization_home_for_source": "/h",
   "_profile_runtime_scope.enter": R("RuntimeError", "bind")},
  runner={"_authorization_home_for_source": True}, source=SRC)
C("under_authorization_profile",
  {"_authorization_home_for_source": "/h", "_profile_runtime_scope.enter": None,
   "_profile_runtime_scope.exit": R("OSError", "reset"), "check": 1},
  runner={"_authorization_home_for_source": True}, source=SRC)
C("under_authorization_profile", {"check": 1},
  runner={"_authorization_home_for_source": True}, source=SRC)  # fail-closed home
C("under_authorization_profile",
  {"_authorization_home_for_source": R("PermissionError", "no")},
  runner={"_authorization_home_for_source": True}, source=SRC)
C("under_authorization_profile",
  {"_authorization_home_for_source": None},
  runner={"_authorization_home_for_source": True}, source=SRC)  # fail-closed check
C("under_authorization_profile",
  {"_authorization_home_for_source": None, "check": 7}, runner=None, source=SRC)

# ── session_env_scope ──
RUNNER_ENV = {"_set_session_env": True, "_clear_session_env": True}
CTX = {"session_key": "s1", "platform": "telegram"}
C("session_env_scope",
  {"_set_session_env": "tok1", "body": None, "_clear_session_env": None},
  runner=RUNNER_ENV, context=CTX)
C("session_env_scope", {"_set_session_env": R("RuntimeError", "bind down")},
  runner=RUNNER_ENV, context=CTX)
C("session_env_scope",
  {"_set_session_env": "tok1", "body": R("ValueError", "handler"),
   "_clear_session_env": None},
  runner=RUNNER_ENV, context=CTX)
C("session_env_scope",
  {"_set_session_env": "tok1", "body": R("SystemExit"), "_clear_session_env": None},
  runner=RUNNER_ENV, context=CTX)
C("session_env_scope",
  {"_set_session_env": "tok1", "body": None, "_clear_session_env": R("OSError", "clear")},
  runner=RUNNER_ENV, context=CTX)
C("session_env_scope",
  {"_set_session_env": "tok1", "body": R("ValueError", "body"),
   "_clear_session_env": R("OSError", "clear wins")},
  runner=RUNNER_ENV, context=CTX)
C("session_env_scope",
  {"_set_session_env": None, "body": None, "_clear_session_env": None},
  runner=RUNNER_ENV, context=CTX)
C("session_env_scope", {"body": None, "_clear_session_env": None},
  runner={"_clear_session_env": True}, context=CTX)  # missing set
C("session_env_scope", {"_set_session_env": "tok1", "body": None},
  runner={"_set_session_env": True}, context=CTX)  # missing clear
C("session_env_scope", {"body": None, "_clear_session_env": None},
  runner=RUNNER_ENV, context=CTX)  # fail-closed set
C("session_env_scope",
  {"_set_session_env": "tok1", "body": None, "_clear_session_env": None},
  runner=RUNNER_ENV, context=None)
C("session_env_scope",
  {"_set_session_env": "tok1", "body": None, "_clear_session_env": None},
  runner=None, context=CTX)
C("session_env_scope",
  {"_set_session_env": "tok1", "body": {"discarded": 1}, "_clear_session_env": None},
  runner=RUNNER_ENV, context=CTX)  # non-null body ret -> None

# ── run_housekeeping_in_executor ──
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": "exec1",
   "copy_context": "ctx1", "loop.run_in_executor": {"result": 1}},
  runner={}, func="fn:chore", args=[1, "x"])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": None,
   "ThreadPoolExecutor": "exec2", "copy_context": "ctx1",
   "loop.run_in_executor": None},
  runner={}, func="fn:chore", args=[])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": None,
   "ThreadPoolExecutor": R("RuntimeError", "no threads"), "copy_context": "ctx1"},
  runner={}, func="fn:chore", args=[])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": R("RuntimeError", "no loop")},
  runner={}, func="fn:chore", args=[])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1",
   "_get_or_create_pool": R("RuntimeError", "Gateway is shutting down; executor unavailable")},
  runner={}, func="fn:chore", args=[])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": "exec1",
   "copy_context": R("ValueError", "ctx")},
  runner={}, func="fn:chore", args=[])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": "exec1",
   "copy_context": "ctx1", "loop.run_in_executor": R("TimeoutError", "slow")},
  runner={}, func="fn:chore", args=[])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": "exec1",
   "copy_context": "ctx1", "loop.run_in_executor": R("CancelledError")},
  runner={}, func="fn:chore", args=[])
C("run_housekeeping_in_executor",
  {"_get_or_create_pool": "exec1", "copy_context": "ctx1",
   "loop.run_in_executor": None},
  runner={}, func="fn:chore", args=[])  # fail-closed get_running_loop
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": "exec1",
   "copy_context": "ctx1", "loop.run_in_executor": "done"},
  runner={"x": 1}, func="fn:other", args=[{"k": 2}])
C("run_housekeeping_in_executor",
  {"asyncio.get_running_loop": "loop1", "_get_or_create_pool": "exec1",
   "copy_context": "ctx1", "loop.run_in_executor": R("SystemExit")},
  runner={}, func="fn:chore", args=[])

# ── get_housekeeping_executor ──
C("get_housekeeping_executor", {"_get_or_create_pool": "exec1"}, runner={})
C("get_housekeeping_executor",
  {"_get_or_create_pool": None, "ThreadPoolExecutor": "exec2"}, runner={})
C("get_housekeeping_executor",
  {"_get_or_create_pool": R("RuntimeError", "Gateway is shutting down; executor unavailable")},
  runner={})
C("get_housekeeping_executor",
  {"_get_or_create_pool": None, "ThreadPoolExecutor": R("MemoryError", "oom")},
  runner={})
C("get_housekeeping_executor",
  {"_get_or_create_pool": None, "ThreadPoolExecutor": R("SystemExit")}, runner={})
C("get_housekeeping_executor", {}, runner={})  # fail-closed pool
C("get_housekeeping_executor", {"_get_or_create_pool": "exec9"}, runner={"x": 1})
C("get_housekeeping_executor", {"_get_or_create_pool": "exec1"}, runner=None)


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert len(counts) == 15, counts
    for f, n in sorted(counts.items()):
        assert n >= 5, (f, n)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True, separators=(",", ":")) + "\n")
    print("wrote %s: %d cases over %d funcs" % (OUT, len(CASES), len(counts)))
    for f, n in sorted(counts.items()):
        print("  %-45s %d" % (f, n))


if __name__ == "__main__":
    main()
