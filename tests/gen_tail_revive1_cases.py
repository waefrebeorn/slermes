#!/usr/bin/env python3
"""Generate tests/fixtures/tail_revive1.jsonl for the tail_revive1 oracle.

Case shape: {"func": <c_name>, "in": {...}}. "in" carries args, state,
env and io (seam scripts). Script semantics: bare array = per-call
queue; other value = constant; {"__script__": true, "ret": X} = constant
X (needed for list returns); {"__raise__": cls, "msg": m} = exception.

Conventions (documented in the ports): integral JSON numbers model
Python ints; ASCII-only text; the callable marker "fn" models a callable
attribute; sets model as arrays.
"""
import json
from pathlib import Path

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_spec(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def script(ret):
    return {"__script__": True, "ret": ret}


# ---- compression_cancelled ----
add("compression_cancelled", state={})
add("compression_cancelled", state={"_compression_cancelled_check": None})
add("compression_cancelled", state={"_compression_cancelled_check": 42})
add("compression_cancelled", state={"_compression_cancelled_check": "fn"},
    io={"cancelled_check": True})
add("compression_cancelled", state={"_compression_cancelled_check": "fn"},
    io={"cancelled_check": 0})
add("compression_cancelled", state={"_compression_cancelled_check": "fn"},
    io={"cancelled_check": raise_spec("RuntimeError", "boom"), "log_debug": None})
add("compression_cancelled", state={"_compression_cancelled_check": "fn"},
    io={"cancelled_check": raise_spec("SystemExit", "bye")})
add("compression_cancelled", state={"_compression_cancelled_check": "fn"},
    io={"cancelled_check": [False, True]})

# ---- is_hygiene_preagent_only_cooldown ----
add("is_hygiene_preagent_only_cooldown", error=None)
add("is_hygiene_preagent_only_cooldown", error="")
add("is_hygiene_preagent_only_cooldown", error="Session Hygiene Compression Timed Out")
add("is_hygiene_preagent_only_cooldown",
    error="  hygiene compression deferred: turn-hold budget expired  ")
add("is_hygiene_preagent_only_cooldown", error="auxiliary model failed")
add("is_hygiene_preagent_only_cooldown", error=0)
add("is_hygiene_preagent_only_cooldown", error=["session hygiene compression timed out"])
add("is_hygiene_preagent_only_cooldown", error=False)
add("is_hygiene_preagent_only_cooldown",
    error="prefix session hygiene compression timed out suffix")

# ---- stale_thinking_on_wire ----
add("stale_thinking_on_wire", state={}, io={"stale_thinking_reaches_wire": True})
add("stale_thinking_on_wire",
    state={"api_mode": "anthropic_messages", "provider": "anthropic",
           "model": "claude", "base_url": "https://api.anthropic.com"},
    io={"stale_thinking_reaches_wire": False})
add("stale_thinking_on_wire",
    state={"api_mode": 0, "provider": "", "model": None, "base_url": False},
    io={"stale_thinking_reaches_wire": True})
add("stale_thinking_on_wire", state={"api_mode": "chat_completions"},
    io={"stale_thinking_reaches_wire": raise_spec("RuntimeError", "x")})
add("stale_thinking_on_wire", state={},
    io={"stale_thinking_reaches_wire": raise_spec("KeyboardInterrupt", "ki")})
add("stale_thinking_on_wire", state={}, io={"stale_thinking_reaches_wire": 1})
add("stale_thinking_on_wire", state={"api_mode": 42},
    io={"stale_thinking_reaches_wire": False})

# ---- billed_basis_over_threshold ----
add("billed_basis_over_threshold", state={"threshold_tokens": 100}, current_tokens=None)
add("billed_basis_over_threshold", state={"threshold_tokens": 100}, current_tokens=100)
add("billed_basis_over_threshold", state={"threshold_tokens": 100}, current_tokens=99)
add("billed_basis_over_threshold", state={"threshold_tokens": 0}, current_tokens=5)
add("billed_basis_over_threshold", state={"threshold_tokens": -5}, current_tokens=0)
add("billed_basis_over_threshold", state={"threshold_tokens": 1}, current_tokens=True)
add("billed_basis_over_threshold", state={}, current_tokens=5)
add("billed_basis_over_threshold", state={"threshold_tokens": "x"}, current_tokens=5)
add("billed_basis_over_threshold", state={"threshold_tokens": 100}, current_tokens="50")

# ---- agent_stale_thinking_on_wire ----
add("agent_stale_thinking_on_wire", state={}, io={"stale_thinking_reaches_wire": False})
add("agent_stale_thinking_on_wire",
    state={"api_mode": "anthropic_messages", "provider": "anthropic",
           "model": "claude", "base_url": ""},
    io={"stale_thinking_reaches_wire": True})
add("agent_stale_thinking_on_wire", state={"model": 0, "base_url": []},
    io={"stale_thinking_reaches_wire": False})
add("agent_stale_thinking_on_wire", state={},
    io={"stale_thinking_reaches_wire": raise_spec("ValueError", "v")})
add("agent_stale_thinking_on_wire", state={},
    io={"stale_thinking_reaches_wire": raise_spec("SystemExit", "s")})
add("agent_stale_thinking_on_wire", state={"provider": 7},
    io={"stale_thinking_reaches_wire": True})

# ---- review_fork_first_request_pending ----
add("review_fork_first_request_pending", state={})
add("review_fork_first_request_pending",
    state={"_review_defer_compaction_before_first_response": True,
           "_turn_received_provider_response": False})
add("review_fork_first_request_pending",
    state={"_review_defer_compaction_before_first_response": True,
           "_turn_received_provider_response": True})
add("review_fork_first_request_pending",
    state={"_review_defer_compaction_before_first_response": 1,
           "_turn_received_provider_response": 0})
add("review_fork_first_request_pending",
    state={"_review_defer_compaction_before_first_response": "",
           "_turn_received_provider_response": False})
add("review_fork_first_request_pending",
    state={"_review_defer_compaction_before_first_response": True,
           "_turn_received_provider_response": None})
add("review_fork_first_request_pending",
    state={"_review_defer_compaction_before_first_response": [],
           "_turn_received_provider_response": {}})

# ---- ctx_bytes ----
add("ctx_bytes", profile={"layers": [["full", 1024]]}, window=1000, flash_attention=True)
add("ctx_bytes", profile={"layers": [["swa", 2048]], "swa_window": 512},
    window=4096, flash_attention=True)
add("ctx_bytes", profile={"layers": [["recurrent", 999]]}, window=8192, flash_attention=True)
add("ctx_bytes", profile={"layers": [["full", 1024]]}, window=1000, flash_attention=False)
add("ctx_bytes", profile={"layers": [["full", 1024]], "kv_q8_ok": False},
    window=1000, flash_attention=True)
add("ctx_bytes", profile={"layers": [["full", 1]], "kv_scale": 1.2},
    window=3, flash_attention=False)
add("ctx_bytes", profile={"layers": [["full", 100]], "window_compute_per_token": 7},
    window=100, flash_attention=False)
add("ctx_bytes",
    profile={"layers": [["full", 512], ["swa", 256], ["recurrent", 0]],
             "swa_window": 1024, "kv_scale": 1.2, "window_compute_per_token": 3},
    window=2048, flash_attention=True)

# ---- per_token_kv_f16 ----
add("per_token_kv_f16", layers=[["full", 100], ["swa", 50], ["recurrent", 999]])
add("per_token_kv_f16", layers=[])
add("per_token_kv_f16", layers=[["recurrent", 1], ["recurrent", 2]])
add("per_token_kv_f16", layers=[["swa", 7]])
add("per_token_kv_f16", layers=[["full", 1], ["full", 2], ["swa", 3]])

# ---- recurrent_layer_count ----
add("recurrent_layer_count", layers=[["full", 1], ["recurrent", 0], ["swa", 2]])
add("recurrent_layer_count", layers=[])
add("recurrent_layer_count", layers=[["full", 1]])
add("recurrent_layer_count", layers=[["recurrent", 0]] * 4)
add("recurrent_layer_count", layers=[["recurrent", 5], ["swa", 1], ["recurrent", 6]])

# ---- kv_dtype_factor ----
add("kv_dtype_factor", flash_attention=True, kv_q8_ok=True)
add("kv_dtype_factor", flash_attention=True, kv_q8_ok=False)
add("kv_dtype_factor", flash_attention=False, kv_q8_ok=True)
add("kv_dtype_factor", flash_attention=False, kv_q8_ok=False)
add("kv_dtype_factor", flash_attention=1, kv_q8_ok=True)
add("kv_dtype_factor", flash_attention=True, kv_q8_ok=0)

# ---- unified_pool_bytes ----
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": None})
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": script([8000, True])})
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": script([8000, False])})
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": script([8000, None])})
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": script([16000, None])})
add("unified_pool_bytes", smi_total=0, ram_total=16000,
    io={"device_pool_view": script([16000, None])})
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": script([1])})
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": 5})
add("unified_pool_bytes", smi_total=4000, ram_total=16000,
    io={"device_pool_view": raise_spec("OSError", "smi gone")})

# ---- nvidia_vram ----
add("nvidia_vram", io={"cached_nvidia_gpu_query": None})
add("nvidia_vram", io={"cached_nvidia_gpu_query":
    {"total_bytes": 100, "free_bytes": 50, "gpu_name": "Test GPU"}})
add("nvidia_vram", io={"cached_nvidia_gpu_query":
    {"total_bytes": 100, "free_bytes": 50, "gpu_name": "X", "gpu_pci_id": 123}})
add("nvidia_vram", io={"cached_nvidia_gpu_query":
    {"total_bytes": 100, "gpu_name": "X"}})
add("nvidia_vram", io={"cached_nvidia_gpu_query": 5})
add("nvidia_vram", io={"cached_nvidia_gpu_query": raise_spec("OSError", "no smi")})

# ---- is_cache_exempt_host ----
add("is_cache_exempt_host", url="https://a.com/x", io={"web_config": {}})
add("is_cache_exempt_host", url="https://preview.mysite.dev/x",
    io={"web_config": {"cache_exempt_hosts": ["mysite.dev"]}})
add("is_cache_exempt_host", url="https://mysite.dev/x",
    io={"web_config": {"cache_exempt_hosts": ["mysite.dev"]}})
add("is_cache_exempt_host", url="https://mysite.dev/x",
    io={"web_config": {"cache_exempt_hosts": None}})
add("is_cache_exempt_host", url="https://mysite.dev/x",
    io={"web_config": {"cache_exempt_hosts": "mysite.dev"}})
add("is_cache_exempt_host", url="not a url",
    io={"web_config": {"cache_exempt_hosts": ["mysite.dev"]}})
add("is_cache_exempt_host", url="https://mysite.dev/x", io={"web_config": None})
add("is_cache_exempt_host", url="http://[::1",
    io={"web_config": {"cache_exempt_hosts": ["mysite.dev"]}})
add("is_cache_exempt_host", url="https://sub.mysite.dev/",
    io={"web_config": {"cache_exempt_hosts": [".mysite.dev.", "*.mysite.dev"]}})
add("is_cache_exempt_host", url="https://mysite.dev/x",
    io={"web_config": raise_spec("SystemExit", "stop")})
add("is_cache_exempt_host", url="https://42/",
    io={"web_config": {"cache_exempt_hosts": [42]}})
add("is_cache_exempt_host", url="http://[::1]/x",
    io={"web_config": {"cache_exempt_hosts": ["::1"]}})
add("is_cache_exempt_host", url="http://[1.2.3.4]/x",
    io={"web_config": {"cache_exempt_hosts": ["1.2.3.4"]}})
add("is_cache_exempt_host", url="https://[fe80::1%25eth0]/x",
    io={"web_config": {"cache_exempt_hosts": ["fe80::1%25eth0"]}})
add("is_cache_exempt_host", url="http://[v1.x]/x",
    io={"web_config": {"cache_exempt_hosts": ["v1.x"]}})
add("is_cache_exempt_host", url="http://[zz]/x",
    io={"web_config": {"cache_exempt_hosts": ["zz"]}})

# ---- host_matches_pattern ----
add("host_matches_pattern", host="mysite.dev", pattern="mysite.dev")
add("host_matches_pattern", host="preview.mysite.dev", pattern="mysite.dev")
add("host_matches_pattern", host="preview.mysite.dev", pattern="*.mysite.dev")
add("host_matches_pattern", host="MYSITE.DEV", pattern="MySite.Dev")
add("host_matches_pattern", host=".mysite.dev.", pattern=".mysite.dev.")
add("host_matches_pattern", host="mysite.dev", pattern="")
add("host_matches_pattern", host="mysite.dev", pattern=None)
add("host_matches_pattern", host="xmysite.dev", pattern="mysite.dev")
add("host_matches_pattern", host=".x", pattern="x")
add("host_matches_pattern", host="mysite.dev", pattern="*.")

# ---- resolve_cron_drain_budget ----
add("resolve_cron_drain_budget", drain_timeout=5, cron_drain_timeout=0, watchdog_delay=100)
add("resolve_cron_drain_budget", drain_timeout=5, cron_drain_timeout=30, watchdog_delay=100)
add("resolve_cron_drain_budget", drain_timeout=50, cron_drain_timeout=30, watchdog_delay=100)
add("resolve_cron_drain_budget", drain_timeout=5, cron_drain_timeout=30,
    watchdog_delay=20, elapsed=5)
add("resolve_cron_drain_budget", drain_timeout=5, cron_drain_timeout=30,
    watchdog_delay=10, elapsed=5)
add("resolve_cron_drain_budget", drain_timeout="2.5", cron_drain_timeout="30",
    watchdog_delay="100")
add("resolve_cron_drain_budget", drain_timeout=None, cron_drain_timeout=30,
    watchdog_delay=100)
add("resolve_cron_drain_budget", drain_timeout=5, cron_drain_timeout=30,
    watchdog_delay=100, cleanup_reserve_s="abc")
add("resolve_cron_drain_budget", drain_timeout=5, cron_drain_timeout="nan",
    watchdog_delay=100)
add("resolve_cron_drain_budget", drain_timeout=5, cron_drain_timeout=30,
    watchdog_delay=100, elapsed=None, cleanup_reserve_s=None)
add("resolve_cron_drain_budget", drain_timeout=-2, cron_drain_timeout="1_0",
    watchdog_delay=100, elapsed="1.5", cleanup_reserve_s=2.5)

# ---- is_global_startup_conflict ----
add("is_global_startup_conflict", error_code="telegram_lock")
add("is_global_startup_conflict", error_code="lock_conflict")
add("is_global_startup_conflict", error_code=" LOCK_CONFLICT ")
add("is_global_startup_conflict", error_code="")
add("is_global_startup_conflict", error_code=None)
add("is_global_startup_conflict", error_code="lock")
add("is_global_startup_conflict", error_code="xlock_conflict")
add("is_global_startup_conflict", error_code="discord_lock ")

# ---- parse_signal_interrupt_grace_timeout ----
add("parse_signal_interrupt_grace_timeout", raw=None)
add("parse_signal_interrupt_grace_timeout", raw="")
add("parse_signal_interrupt_grace_timeout", raw="   ")
add("parse_signal_interrupt_grace_timeout", raw=0)
add("parse_signal_interrupt_grace_timeout", raw="0")
add("parse_signal_interrupt_grace_timeout", raw=2.5)
add("parse_signal_interrupt_grace_timeout", raw="abc")
add("parse_signal_interrupt_grace_timeout", raw="inf")
add("parse_signal_interrupt_grace_timeout", raw="-3")
add("parse_signal_interrupt_grace_timeout", raw=True)
add("parse_signal_interrupt_grace_timeout", raw="1_0")

# ---- buzz_terminal_context_active ----
add("buzz_terminal_context_active", env={"BUZZ_MANAGED_AGENT": "1"})
add("buzz_terminal_context_active", env={}, io={"get_session_env": "buzz"})
add("buzz_terminal_context_active", env={}, io={"get_session_env": " Buzz "})
add("buzz_terminal_context_active", env={}, io={"get_session_env": "telegram"})
add("buzz_terminal_context_active", env={"BUZZ_MANAGED_AGENT": ""},
    io={"get_session_env": "buzz"})
add("buzz_terminal_context_active", env={},
    io={"get_session_env": raise_spec("RuntimeError", "no ctx")})
add("buzz_terminal_context_active", env={},
    io={"get_session_env": raise_spec("SystemExit", "exit")})
add("buzz_terminal_context_active", env={}, io={"get_session_env": 42})
add("buzz_terminal_context_active", env={"BUZZ_MANAGED_AGENT": 0},
    io={"get_session_env": "buzz"})

# ---- is_terminal_first_party_env ----
add("is_terminal_first_party_env", name="BUZZ_PRIVATE_KEY",
    env={"BUZZ_MANAGED_AGENT": "1"})
add("is_terminal_first_party_env", name="OPENAI_API_KEY",
    env={"BUZZ_MANAGED_AGENT": "1"})
add("is_terminal_first_party_env", name="buzz_x", env={},
    io={"get_session_env": "buzz"})
add("is_terminal_first_party_env", name="BUZZ_", env={},
    io={"get_session_env": "telegram"})
add("is_terminal_first_party_env", name="BUZZX", env={"BUZZ_MANAGED_AGENT": "1"})
add("is_terminal_first_party_env", name="BUZZ_A", env={},
    io={"get_session_env": raise_spec("SystemExit", "x")})

# ---- matches_terminal_first_party_prefix ----
add("matches_terminal_first_party_prefix", name="BUZZ_X")
add("matches_terminal_first_party_prefix", name="buzz_x")
add("matches_terminal_first_party_prefix", name="BUZZ_")
add("matches_terminal_first_party_prefix", name="BUZZ")
add("matches_terminal_first_party_prefix", name="")
add("matches_terminal_first_party_prefix", name="XBUZZ_")

# ---- skill_should_show ----
add("skill_should_show", conditions={}, available_tools=None, available_toolsets=None)
add("skill_should_show", conditions={"session_platforms": ["telegram"]},
    available_tools=None, available_toolsets=None, session_platform="discord")
add("skill_should_show", conditions={"session_platforms": ["telegram"]},
    available_tools=None, available_toolsets=None, session_platform=" Telegram ")
add("skill_should_show", conditions={"session_platforms": ["telegram"]},
    available_tools=None, available_toolsets=None, session_platform=None)
add("skill_should_show", conditions={"requires_tools": ["terminal"]},
    available_tools=["terminal"], available_toolsets=None)
add("skill_should_show", conditions={"requires_tools": ["terminal"]},
    available_tools=[], available_toolsets=None)
add("skill_should_show", conditions={"fallback_for_tools": ["web"]},
    available_tools=["web"], available_toolsets=None)
add("skill_should_show", conditions={"fallback_for_tools": ["web"]},
    available_tools=[], available_toolsets=None)
add("skill_should_show", conditions={"requires_toolsets": ["core"]},
    available_tools=None, available_toolsets=["core"])
add("skill_should_show", conditions={"requires_toolsets": ["core"]},
    available_tools=None, available_toolsets=[])
add("skill_should_show", conditions={"session_platforms": [None, "  ", "Telegram"]},
    available_tools=None, available_toolsets=None, session_platform="telegram")
add("skill_should_show", conditions={"requires_tools": None},
    available_tools=["terminal"], available_toolsets=None)
add("skill_should_show", conditions={"session_platforms": 5},
    available_tools=None, available_toolsets=None)
add("skill_should_show", conditions={}, available_tools=None, available_toolsets=["x"])
add("skill_should_show", conditions={"fallback_for_toolsets": ["a"]},
    available_tools=None, available_toolsets=[])
add("skill_should_show",
    conditions={"fallback_for_toolsets": ["a"], "requires_tools": ["t"]},
    available_tools=["t"], available_toolsets=["a"])

# ---- clear_backend_probe_cache ----
add("clear_backend_probe_cache", state={"cache": {}})
add("clear_backend_probe_cache", state={"cache": {"k1": "v1", "k2": "v2"}})
add("clear_backend_probe_cache", state={"cache": {"k": {"nested": [1, 2]}}})
add("clear_backend_probe_cache", state={"cache": {"a": ""}})
add("clear_backend_probe_cache", state={"cache": {"x": None, "y": False}})


def main():
    out = Path(__file__).resolve().parent / "fixtures" / "tail_revive1.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        raise SystemExit(f"fewer than 5 cases for: {thin}")
    with out.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {out}")


if __name__ == "__main__":
    main()
