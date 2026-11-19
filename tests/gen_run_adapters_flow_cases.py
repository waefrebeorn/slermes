#!/usr/bin/env python3
"""Generate tests/fixtures/run_adapters_flow.jsonl for the run_adapters_flow
oracle. Each case is {"func": <name>, "in": {...}} with scripted seams in
in.io (plain value reused; {"__seq__": [...]} per-call; {"__raise__": kind,
"msg": text} raises; absent -> None). >=5 cases per ported function, exercising
the decision branches, short-circuits, error paths and state mutations.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "run_adapters_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


A1 = {"id": "a1", "platform": "telegram"}
PRED = {"id": "pred", "platform": "telegram"}
CFG = {"token": "x"}

# ── env_timeout_override ──
add("env_timeout_override", name="HERMES_GATEWAY_ADAPTER_DISCONNECT_TIMEOUT", io={})
add("env_timeout_override", name="N", io={"os.getenv": "   "})
add("env_timeout_override", name="N", io={"os.getenv": "5.5"})
add("env_timeout_override", name="N", io={"os.getenv": "-3"})
add("env_timeout_override", name="N", io={"os.getenv": "abc"})
add("env_timeout_override", name="N", io={"os.getenv": "1e2"})
add("env_timeout_override", name="N", io={"os.getenv": "0"})
add("env_timeout_override", name="N", io={"os.getenv": "  2.5  "})
add("env_timeout_override", name="N", io={"os.getenv": R("OSError", "env")})
add("env_timeout_override", name="N", io={"os.getenv": "1_0.5"})

# ── wait_or_detach ──
add("wait_or_detach", task="t1", timeout=5.0, io={"asyncio.wait": {"done": ["t1"], "pending": []}})
add("wait_or_detach", task="t1", timeout=5.0, io={"asyncio.wait": {"done": [], "pending": ["t1"]}})
add("wait_or_detach", task="t1", timeout=5.0, io={"asyncio.wait": R("RuntimeError", "wait")})
add("wait_or_detach", task="t1", timeout=5.0, io={})
add("wait_or_detach", task="t1", timeout=5.0,
    io={"asyncio.wait": {"done": [], "pending": ["t1"]}, "task.cancel": R("RuntimeError", "cancel")})
add("wait_or_detach", task="t1", timeout=5.0,
    io={"asyncio.wait": {"done": ["t1"], "pending": []}, "task.cancel": R("RuntimeError", "x")})
add("wait_or_detach", task="t1", timeout=5.0,
    io={"asyncio.wait": {"done": [], "pending": ["t1"]}, "task.add_done_callback": R("ValueError", "cb")})

# ── connect_adapter_bounded ──
add("connect_adapter_bounded", platform="telegram", adapter=A1, is_reconnect=False, initial=False,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": True})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": False})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": R("RuntimeError", "connect")})
add("connect_adapter_bounded", platform="telegram", adapter=A1, is_reconnect=True, initial=True,
    io={"platform_connect_timeout_secs": 5, "adapter.connect": True,
        "asyncio.ensure_future": "t1", "asyncio.wait": {"done": ["t1"], "pending": []}})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 5, "adapter.connect": True,
        "asyncio.ensure_future": "t1", "asyncio.wait": {"done": [], "pending": ["t1"]}})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 5, "adapter.connect": R("RuntimeError", "c")})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": R("ValueError", "t")})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 5, "adapter.connect": None,
        "asyncio.ensure_future": "t1", "asyncio.wait": {"done": ["t1"], "pending": []}})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": None})
add("connect_adapter_bounded", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 180, "adapter.connect": True,
        "asyncio.ensure_future": "t1", "asyncio.wait": {"done": [], "pending": ["t1"]}})

# ── connect_adapter_with_timeout ──
add("connect_adapter_with_timeout", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": True})
add("connect_adapter_with_timeout", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": False})
add("connect_adapter_with_timeout", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 5, "adapter.connect": True,
        "asyncio.ensure_future": "t1", "asyncio.wait": {"done": [], "pending": ["t1"]}})
add("connect_adapter_with_timeout", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": R("RuntimeError", "boom")})
add("connect_adapter_with_timeout", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": R("CancelledError", "cancel")})
add("connect_adapter_with_timeout", platform="telegram", adapter=A1, is_reconnect=True,
    io={"platform_connect_timeout_secs": 5, "adapter.connect": True,
        "asyncio.ensure_future": "t1", "asyncio.wait": {"done": ["t1"], "pending": []}})
add("connect_adapter_with_timeout", platform="telegram", adapter=A1,
    io={"platform_connect_timeout_secs": 0, "adapter.connect": True,
        "record_platform_connect": R("OSError", "metric")})

# ── extend_systemd_boot_for ──
add("extend_systemd_boot_for", platform="telegram", self={"_running": True},
    io={"platform_connect_timeout_secs": 5})
add("extend_systemd_boot_for", platform="telegram",
    self={"_running": False, "config": {"systemd_watchdog_seconds": 0}},
    io={"platform_connect_timeout_secs": 5})
add("extend_systemd_boot_for", platform="telegram",
    self={"config": {"systemd_watchdog_seconds": 90}}, io={"platform_connect_timeout_secs": 0})
add("extend_systemd_boot_for", platform="telegram",
    self={"config": {"systemd_watchdog_seconds": 90}}, io={"platform_connect_timeout_secs": 5})
add("extend_systemd_boot_for", platform="telegram", self={"_running": False},
    io={"platform_connect_timeout_secs": 5})
add("extend_systemd_boot_for", platform="telegram", self={"config": {}},
    io={"platform_connect_timeout_secs": 5})
add("extend_systemd_boot_for", platform="telegram", self={"config": {"systemd_watchdog_seconds": 90}},
    io={"platform_connect_timeout_secs": R("ValueError", "t")})
add("extend_systemd_boot_for", platform="telegram",
    self={"config": {"systemd_watchdog_seconds": -1}}, io={"platform_connect_timeout_secs": 5})
add("extend_systemd_boot_for", platform="telegram",
    self={"config": {"systemd_watchdog_seconds": 90}},
    io={"platform_connect_timeout_secs": 5, "extend_start_timeout": R("OSError", "sd")})

# ── handle_reaction_event ──
add("handle_reaction_event", self={"hooks": {}}, ctx={"event_name": "reaction:added"}, io={})
add("handle_reaction_event", self={"hooks": {}}, ctx={}, io={})
add("handle_reaction_event", self={"hooks": {}}, ctx={"event_name": ""}, io={})
add("handle_reaction_event", self={"hooks": {}}, ctx={"event_name": "custom"}, io={})
add("handle_reaction_event", self={"hooks": {}}, ctx={"event_name": "x"},
    io={"hooks.emit": R("RuntimeError", "emit")})
add("handle_reaction_event", self={}, ctx={"event_name": "x"}, io={})
add("handle_reaction_event", self={"hooks": {}}, ctx={"event_name": "x"},
    io={"hooks.emit": R("CancelledError", "c")})
add("handle_reaction_event", self={"hooks": {}}, ctx={"event_name": 5}, io={})
add("handle_reaction_event", self={"hooks": {}}, ctx={"event_name": "x"},
    io={"hooks.emit": R("RuntimeError", "e"), "logger.log": R("OSError", "log")})

# ── on_all_adapters_down ──
add("on_all_adapters_down", self={"config": {"on_all_adapters_down": "exit"}})
add("on_all_adapters_down", self={"config": {"on_all_adapters_down": "stay_alive"}})
add("on_all_adapters_down", self={"config": {"on_all_adapters_down": "bogus"}})
add("on_all_adapters_down", self={})
add("on_all_adapters_down", self={"config": {}})
add("on_all_adapters_down", self={"config": {"on_all_adapters_down": None}})

# ── adapter_listener_claim ──
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "0.0.0.0", "_sidecar_port": 8080})
add("adapter_listener_claim", platform="telegram",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "h", "_sidecar_port": 8080})
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "   ", "_sidecar_port": 8080})
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "HOST", "_sidecar_port": "9090"})
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "h", "_sidecar_port": "abc"})
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "h"})
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "h", "_sidecar_port": 80.9})
add("adapter_listener_claim", platform="photon", adapter={"id": "p", "platform": "photon"})
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "h", "_sidecar_port": True})
add("adapter_listener_claim", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "h", "_sidecar_port": " 7070 "})

# ── adapter_credential_claim ──
add("adapter_credential_claim", platform="telegram", adapter=A1, io={"adapter_credential_fingerprint": "abc123"})
add("adapter_credential_claim", platform="telegram", adapter=A1, io={"adapter_credential_fingerprint": None})
add("adapter_credential_claim", platform="telegram", adapter=A1, io={})
add("adapter_credential_claim", platform="telegram", adapter=A1,
    io={"adapter_credential_fingerprint": R("RuntimeError", "fp")})
add("adapter_credential_claim", platform="telegram", adapter=A1, io={"adapter_credential_fingerprint": ""})

# ── reconnect_queue_entry ──
add("reconnect_queue_entry", platform="telegram", adapter=A1, platform_config=CFG,
    attempts=0, delay=30.0, queued=True,
    io={"time.monotonic": 100, "adapter_credential_fingerprint": "fp"})
add("reconnect_queue_entry", platform="telegram", adapter=A1, platform_config=CFG,
    attempts=2, delay=0.0, queued=False,
    io={"time.monotonic": 100, "adapter_credential_fingerprint": "fp"})
add("reconnect_queue_entry", platform="telegram", adapter=A1, platform_config=CFG,
    attempts=0, delay=30.0, io={"time.monotonic": 100, "adapter_credential_fingerprint": None})
add("reconnect_queue_entry", platform="photon",
    adapter={"id": "p", "platform": "photon", "_sidecar_bind": "H", "_sidecar_port": 8080},
    platform_config=CFG, attempts=1, delay=5.0,
    io={"time.monotonic": 50, "adapter_credential_fingerprint": "fp"})
add("reconnect_queue_entry", platform="telegram", adapter=A1, platform_config=CFG,
    attempts=0, delay=30.0, io={"time.monotonic": R("OSError", "clock")})
add("reconnect_queue_entry", platform="telegram", adapter=A1, platform_config=CFG,
    attempts=0, delay=30.0, io={"time.monotonic": 100, "adapter_credential_fingerprint": R("ValueError", "fp")})

# ── queue_retryable_best_effort ──
add("queue_retryable_best_effort", adapter=A1, why="exception", io={"queue_retryable_fatal_platform": True})
add("queue_retryable_best_effort", adapter=A1, why="exception", io={"queue_retryable_fatal_platform": False})
add("queue_retryable_best_effort", adapter=A1, why="cancellation",
    io={"queue_retryable_fatal_platform": R("RuntimeError", "q")})
add("queue_retryable_best_effort", adapter=A1, why="exception",
    io={"queue_retryable_fatal_platform": R("CancelledError", "c")})
add("queue_retryable_best_effort", adapter={"id": "a1"}, why="exception", io={})
add("queue_retryable_best_effort", adapter=A1, why="exception",
    io={"queue_retryable_fatal_platform": R("RuntimeError", "q"), "logger.log": R("OSError", "log")})

# ── retain_background_task ──
add("retain_background_task", task="t1", self={}, io={})
add("retain_background_task", task="t1", self={"_background_tasks": {"set": ["t1"]}}, io={})
add("retain_background_task", task="t1", self={"_background_tasks": {"set": ["t0"]}}, io={})
add("retain_background_task", task="t1", self={"_background_tasks": {"other": [1, 2]}}, io={})
add("retain_background_task", task="t1", self={}, io={"task.add_done_callback": R("RuntimeError", "cb")})
add("retain_background_task", task="t1", self={"_background_tasks": {"set": ["t2", "t0"]}}, io={})

# ── track_task_in ──
add("track_task_in", tasks=[], task="t1", io={})
add("track_task_in", tasks=["t1"], task="t1", io={})
add("track_task_in", tasks=["t0"], task="t1", io={})
add("track_task_in", tasks=["t0"], task="t1", io={"task.add_done_callback": R("RuntimeError", "cb")})
add("track_task_in", tasks=["t2", "t0"], task="t1", io={})

# ── request_clean_exit ──
add("request_clean_exit", reason="shutdown", self={"_shutdown_event": {}}, io={})
add("request_clean_exit", reason="", self={"_shutdown_event": {}}, io={})
add("request_clean_exit", reason="x", self={}, io={})
add("request_clean_exit", reason="x", self={"_shutdown_event": {}},
    io={"shutdown_event.set": R("RuntimeError", "set")})
add("request_clean_exit", reason="bye", self={"_shutdown_event": {}, "extra": 7}, io={})

# ── flag_reconnect_needs_attention ──
DT = "2026-11-19T12:00:00+00:00"
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0,
    info={"attention_flagged": True, "attempts": 3}, io={})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0, info={"attempts": 0}, io={})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0, info={"attempts": 2},
    io={"_reconnect_needs_attention": False})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0,
    info={"attempts": 2, "queued_at": 100.0},
    io={"_reconnect_needs_attention": True, "datetime.now": DT})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0, info={"attempts": 3},
    io={"_reconnect_needs_attention": True, "datetime.now": DT})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0, status_key="prof:telegram",
    info={"attempts": 2, "queued_at": 100.0},
    io={"_reconnect_needs_attention": True, "datetime.now": DT})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0, info={"attempts": 2},
    io={"_reconnect_needs_attention": R("RuntimeError", "na")})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0,
    info={"attempts": 2, "queued_at": 100.0},
    io={"_reconnect_needs_attention": True, "datetime.now": DT,
        "update_platform_runtime_status": R("OSError", "status")})
add("flag_reconnect_needs_attention", platform="telegram", now=1000.0,
    info={"attempts": 1, "queued_at": 1000.0},
    io={"_reconnect_needs_attention": True, "datetime.now": "2026-11-19T12:00:00.500000+00:00"})

# ── bump_reconnect_backoff ──
add("bump_reconnect_backoff", platform="telegram", attempt=2, error_code="x", error_message="m",
    info={"retry_now": True}, io={"time.monotonic": 100})
add("bump_reconnect_backoff", platform="telegram", attempt=2, error_code="x", error_message="m",
    info={}, io={"_reconnect_backoff": 60, "time.monotonic": 100})
add("bump_reconnect_backoff", platform="telegram", attempt=1, error_code="x", error_message="m",
    info={"retry_now": False}, io={"_reconnect_backoff": 30, "time.monotonic": 100})
add("bump_reconnect_backoff", platform="telegram", attempt=3, error_code="x", error_message="m",
    info={"retry_now": 0}, io={"_reconnect_backoff": 120, "time.monotonic": 100})
add("bump_reconnect_backoff", platform="telegram", attempt=2, error_code="x", error_message="m",
    info={"retry_now": True}, io={"update_platform_runtime_status": R("OSError", "s")})
add("bump_reconnect_backoff", platform="telegram", attempt=2, error_code=None, error_message="m",
    info={}, io={"_reconnect_backoff": 60, "time.monotonic": 100})
add("bump_reconnect_backoff", platform="telegram", attempt=2, error_code="x", error_message="m",
    info={}, io={"_reconnect_backoff": R("ValueError", "b"), "time.monotonic": 100})

# ── adapter_may_heal ──
add("adapter_may_heal", platform="myplugin", platform_config=CFG, _BUILTIN_ADAPTERS=["api_server"],
    io={"platform_registry.is_registered": False, "_platform_has_bot_credential": True})
add("adapter_may_heal", platform="api_server", platform_config=CFG, _BUILTIN_ADAPTERS=["api_server"], io={})
add("adapter_may_heal", platform="myplugin", platform_config=CFG, _BUILTIN_ADAPTERS=["api_server"],
    io={"platform_registry.is_registered": True})
add("adapter_may_heal", platform="myplugin", platform_config=CFG, _BUILTIN_ADAPTERS=["api_server"],
    io={"platform_registry.is_registered": False, "_platform_has_bot_credential": False})
add("adapter_may_heal", platform="myplugin", platform_config=CFG, _BUILTIN_ADAPTERS=["api_server"],
    io={"platform_registry.is_registered": R("RuntimeError", "reg")})
add("adapter_may_heal", platform="myplugin", platform_config=CFG, _BUILTIN_ADAPTERS=["api_server"],
    io={"platform_registry.is_registered": False, "_platform_has_bot_credential": R("OSError", "cred")})

# ── reconnect_failed_platform ──
def rinfo(**over):
    base = {"config": CFG, "attempts": 1, "next_retry": 500.0, "queued_at": 100.0, "predecessor": PRED}
    base.update(over)
    return base


def rself(info=None, **extra):
    fp = {} if info is None else {"telegram": info}
    s = {"_failed_platforms": fp, "adapters": {}}
    s.update(extra)
    return s


BASE_IO = {"_reconnect_needs_attention": False, "_platform_has_bot_credential": True}


def merge(*ds):
    out = {}
    for d in ds:
        out.update(d)
    return out


# 1: no info
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(None), io={})
# 2: paused
add("reconnect_failed_platform", platform="telegram", now=1000.0,
    self=rself(rinfo(paused=True)), io={})
# 3: not time yet
add("reconnect_failed_platform", platform="telegram", now=100.0, self=rself(rinfo()), io=BASE_IO)
# 4: no credential -> drop
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io={"_reconnect_needs_attention": False, "_platform_has_bot_credential": False})
# 5: success install
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": {"id": "a2", "platform": "telegram", "has_fatal_error": False},
                       "connect_adapter_with_timeout": True}))
# 6: create None, cannot heal -> mark unavailable + drop
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": None, "platform_registry.is_registered": True}))
# 7: create None, may heal -> rearm + bump
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": None, "platform_registry.is_registered": False,
                       "_adapter_unavailable_message": "No adapter available", "_reconnect_backoff": 60,
                       "time.monotonic": 1000}))
# 8: connect false, non-retryable fatal -> mark fatal + dispose + del
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": {"id": "a2", "platform": "telegram", "has_fatal_error": True,
                                                        "fatal_error_retryable": False, "fatal_error_message": "revoked"},
                       "connect_adapter_with_timeout": False}))
# 9: connect false, retryable -> bump + dispose + hand_over
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": {"id": "a2", "platform": "telegram", "has_fatal_error": False,
                                                        "fatal_error_code": "e", "fatal_error_message": "m"},
                       "connect_adapter_with_timeout": False, "_reconnect_backoff": 60, "time.monotonic": 1000}))
# 10: create raises -> except (adapter None)
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": R("RuntimeError", "create failed"),
                       "_reconnect_backoff": 60, "time.monotonic": 1000}))
# 11: connect raises -> except (adapter present)
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": {"id": "a2", "platform": "telegram", "has_fatal_error": False},
                       "connect_adapter_with_timeout": R("RuntimeError", "connect failed"),
                       "_reconnect_backoff": 60, "time.monotonic": 1000}))
# 12: connect CancelledError -> BaseException propagates
add("reconnect_failed_platform", platform="telegram", now=1000.0, self=rself(rinfo()),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": {"id": "a2", "platform": "telegram", "has_fatal_error": False},
                       "connect_adapter_with_timeout": R("CancelledError", "cancel")}))
# 13: next_retry missing -> KeyError
add("reconnect_failed_platform", platform="telegram", now=1000.0,
    self=rself({"config": CFG, "attempts": 1}), io=BASE_IO)
# 14: attention flag fires integrated
add("reconnect_failed_platform", platform="telegram", now=1000.0,
    self=rself(rinfo(attempts=2)),
    io=merge(BASE_IO, {"_reconnect_needs_attention": True,
                       "create_adapter_off_loop_deps": {"id": "a2", "platform": "telegram", "has_fatal_error": False},
                       "connect_adapter_with_timeout": True, "datetime.now": DT}))
# 15: retry_now present -> popped, bump immediate
add("reconnect_failed_platform", platform="telegram", now=1000.0,
    self=rself(rinfo(retry_now=True)),
    io=merge(BASE_IO, {"create_adapter_off_loop_deps": {"id": "a2", "platform": "telegram", "has_fatal_error": False,
                                                        "fatal_error_code": "e", "fatal_error_message": "m"},
                       "connect_adapter_with_timeout": False, "time.monotonic": 1000}))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

# per-func count check
from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print("total cases:", len(CASES))
for fn, n in sorted(cnt.items()):
    flag = "  <-- TOO FEW" if n < 5 else ""
    print("  %-34s %d%s" % (fn, n, flag))
print("wrote", OUT)
