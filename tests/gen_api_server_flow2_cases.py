#!/usr/bin/env python3
"""Generate tests/fixtures/api_server_flow2.jsonl — >=5 cases per ported func.

Cases nest io INSIDE in ({"func","in":{...,"io":{...},"state":{...}}}). Scripts:
a bare array is a per-call queue (last value does NOT repeat — exhaustion is
fail-closed); a scalar/object is a constant; {"__script__":true,"ret":v} forces
a constant list/object. A missing script is fail-closed (RuntimeError).
Domain: ASCII strings; integral-int numbers in str() positions; finite floats.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "api_server_flow2.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def sc(ret):
    return {"__script__": True, "ret": ret}


def raise_spec(kind, msg):
    return {"__raise__": kind, "message": msg}


# ── mark_shutdown_requested ──────────────────────────────────────────────────
case("mark_shutdown_requested", io={"runs._mark_shutdown_requested": 0})
case("mark_shutdown_requested", io={"runs._mark_shutdown_requested": 3})
case("mark_shutdown_requested", io={"runs._mark_shutdown_requested": 7})
case("mark_shutdown_requested", io={})  # fail-closed
case("mark_shutdown_requested", io={"runs._mark_shutdown_requested": raise_spec("RuntimeError", "drain fail")})

# ── _active_structured_run_count ─────────────────────────────────────────────
case("_active_structured_run_count", state={"run_statuses": {}})
case("_active_structured_run_count", state={"run_statuses": {
    "r1": {"status": "running"}, "r2": {"status": "completed"}}})
case("_active_structured_run_count", state={"run_statuses": {
    "r1": {"status": "queued"}, "r2": {"status": "waiting_for_approval"},
    "r3": {"status": "stopping"}, "r4": {"status": "running"}}})
case("_active_structured_run_count", state={"run_statuses": {
    "r1": {"status": "completed"}, "r2": {"status": "failed"}, "r3": {"status": "cancelled"}}})
case("_active_structured_run_count", state={"run_statuses": {"r1": {"status": "interrupted"}, "r2": {}}})

# ── _resolve_api_server_int ──────────────────────────────────────────────────
case("_resolve_api_server_int", key="max_concurrent_runs", default=10,
     io={"config.load_config": {"cfg": 1}, "config.cfg_get": "5"})
case("_resolve_api_server_int", key="max_concurrent_runs", default=10,
     io={"config.load_config": {"cfg": 1}, "config.cfg_get": -3})
case("_resolve_api_server_int", key="max_concurrent_runs", default=10,
     io={"config.load_config": {"cfg": 1}, "config.cfg_get": 0})
case("_resolve_api_server_int", key="max_concurrent_runs", default=10,
     io={"config.load_config": {"cfg": 1}, "config.cfg_get": raise_spec("RuntimeError", "no config")})
case("_resolve_api_server_int", key="max_concurrent_runs", default=10,
     io={"config.load_config": {"cfg": 1}, "config.cfg_get": "abc"})
case("_resolve_api_server_int", key="max_concurrent_runs", default=10, io={})  # fail-closed load_config

# ── _metrics_day_key ─────────────────────────────────────────────────────────
case("_metrics_day_key", timestamp=86400)
case("_metrics_day_key", timestamp=1700000000)
case("_metrics_day_key", timestamp=0, io={"time.time": 1700000000})  # 0 falsy -> clock
case("_metrics_day_key", timestamp=None, io={"time.time": 86400})
case("_metrics_day_key", timestamp=None, io={"time.time": raise_spec("RuntimeError", "clock fail")})

# ── _iso_timestamp ───────────────────────────────────────────────────────────
case("_iso_timestamp", timestamp=0)
case("_iso_timestamp", timestamp=1700000000)
case("_iso_timestamp", timestamp=None)
case("_iso_timestamp", timestamp=1.5)
case("_iso_timestamp", timestamp=-1)

# ── _reset_metrics_if_needed ─────────────────────────────────────────────────
case("_reset_metrics_if_needed", state={"metrics_day": "2023-11-14"}, io={"time.time": 1700000000})
case("_reset_metrics_if_needed", state={"metrics_day": "2020-01-01", "metrics_requests_today": 5,
     "metrics_latency_ms": [1.0, 2.0]}, io={"time.time": 1700000000})
case("_reset_metrics_if_needed", state={"metrics_day": ""}, io={"time.time": 0})
case("_reset_metrics_if_needed", io={"time.time": raise_spec("RuntimeError", "clock fail")})
case("_reset_metrics_if_needed", state={"metrics_day": "2023-11-14", "metrics_tokens_today": 9},
     io={"time.time": 1700086400})

# ── _total_tokens_from_usage ─────────────────────────────────────────────────
case("_total_tokens_from_usage", usage={"total_tokens": 5})
case("_total_tokens_from_usage", usage=None)
case("_total_tokens_from_usage", usage={"total_tokens": None})
case("_total_tokens_from_usage", usage={"total_tokens": "abc"})
case("_total_tokens_from_usage", usage={"total_tokens": -3})
case("_total_tokens_from_usage", usage={"total_tokens": 3})
case("_total_tokens_from_usage", usage="not a dict")

# ── _latency_p95_ms ──────────────────────────────────────────────────────────
case("_latency_p95_ms", samples=[])
case("_latency_p95_ms", samples=[1.0])
case("_latency_p95_ms", samples=[1.0, 2.0, 3.0, 4.0])
case("_latency_p95_ms", samples=[10.0, 20.0, 30.0, 40.0, 50.0])
case("_latency_p95_ms", samples=[5.0, 1.0, 4.0, 2.0, 3.0])
case("_latency_p95_ms", samples=[0.125, 0.375, 1.005])

# ── _api_server_status_payload ───────────────────────────────────────────────
case("_api_server_status_payload", io={"time.time": 1700000000})
case("_api_server_status_payload", heartbeat_at=1700000000, io={"time.time": 1700000000})
case("_api_server_status_payload", state={"metrics_day": "2023-11-14", "metrics_requests_today": 4,
     "metrics_messages_today": 4, "metrics_tokens_today": 100, "metrics_latency_ms": [10.0, 20.0],
     "metrics_last_request_at": 1700000000, "inflight_agent_runs": 2,
     "run_statuses": {"r1": {"status": "running"}}}, io={"time.time": 1700000000})
case("_api_server_status_payload", io={"time.time": raise_spec("RuntimeError", "clock fail")})
case("_api_server_status_payload", heartbeat_at=None, state={"metrics_last_heartbeat_at": 1700000000},
     io={"time.time": 1700000000})

# ── _publish_runtime_status ──────────────────────────────────────────────────
case("_publish_runtime_status", io={"time.time": 1700000000, "is_connected": True,
     "self._write_runtime_status_safe": None})
case("_publish_runtime_status", io={"time.time": 1700000000, "is_connected": False,
     "self._write_runtime_status_safe": None})
case("_publish_runtime_status", io={"time.time": raise_spec("RuntimeError", "clock fail")})
case("_publish_runtime_status", io={"time.time": 1700000000, "is_connected": True,
     "self._write_runtime_status_safe": raise_spec("OSError", "disk full")})
case("_publish_runtime_status", io={"time.time": 1700000000})  # fail-closed is_connected

# ── _record_api_metrics ──────────────────────────────────────────────────────
case("_record_api_metrics", usage={"total_tokens": 7}, latency_seconds=0.5,
     io={"time.time": 1700000000, "is_connected": True, "self._write_runtime_status_safe": None})
case("_record_api_metrics", usage=None, latency_seconds=-1.0,
     io={"time.time": 1700000000, "is_connected": True, "self._write_runtime_status_safe": None})
case("_record_api_metrics", usage={"total_tokens": 3}, latency_seconds=2.0,
     state={"metrics_latency_ms": [float(i) for i in range(512)]},
     io={"time.time": 1700000000, "is_connected": True, "self._write_runtime_status_safe": None})
case("_record_api_metrics", usage={"total_tokens": 1}, latency_seconds=0.1,
     io={"time.time": raise_spec("RuntimeError", "clock fail")})
case("_record_api_metrics", usage={"total_tokens": 1}, latency_seconds=0.1,
     io={"time.time": 1700000000, "is_connected": True,
         "self._write_runtime_status_safe": raise_spec("OSError", "write fail")})

# ── _heartbeat_loop ──────────────────────────────────────────────────────────
case("_heartbeat_loop", io={"is_connected": [False]})
case("_heartbeat_loop", io={"is_connected": [True, True, False], "time.time": 1700000000,
     "self._write_runtime_status_safe": None})
case("_heartbeat_loop", io={"is_connected": [True, True, True, True, False], "time.time": 1700000000,
     "asyncio.sleep": None, "self._write_runtime_status_safe": None})
case("_heartbeat_loop", io={"is_connected": [True, True], "time.time": 1700000000,
     "self._write_runtime_status_safe": raise_spec("OSError", "write fail")})
case("_heartbeat_loop", io={"is_connected": [True, True, True], "time.time": 1700000000,
     "asyncio.sleep": raise_spec("RuntimeError", "sleep fail"), "self._write_runtime_status_safe": None})
# ── _resolve_provider_runtime ────────────────────────────────────────────────
case("_resolve_provider_runtime", provider="  ", required=True)  # empty -> None
case("_resolve_provider_runtime", provider="openai", target_model="gpt", required=True,
     io={"resolve_request_runtime_agent_kwargs": {"provider": "openai", "api_key": "k"}})
case("_resolve_provider_runtime", provider="openai", required=True,
     io={"resolve_request_runtime_agent_kwargs": raise_spec("RuntimeError", "boom"),
         "run._resolve_runtime_agent_kwargs_for_provider": raise_spec("RuntimeError", "fallback fail")})
case("_resolve_provider_runtime", provider="openai", required=True,
     io={"resolve_request_runtime_agent_kwargs": raise_spec("RuntimeError", "boom"),
         "run._resolve_runtime_agent_kwargs_for_provider": {"provider": "openai", "api_key": "fb"}})
case("_resolve_provider_runtime", provider="openai", required=False,
     io={"resolve_request_runtime_agent_kwargs": raise_spec("RuntimeError", "boom"),
         "run._resolve_runtime_agent_kwargs_for_provider": raise_spec("RuntimeError", "fallback fail")})
case("_resolve_provider_runtime", provider="openai", required=True,
     io={"resolve_request_runtime_agent_kwargs": raise_spec("KeyboardInterrupt", "stop")})

# ── _apply_provider_runtime ──────────────────────────────────────────────────
case("_apply_provider_runtime", provider="openai", state={"runtime_kwargs": {"provider": "old"}},
     io={"resolve_request_runtime_agent_kwargs": {"provider": "openai", "api_key": "k", "base_url": "u"}})
case("_apply_provider_runtime", provider="openai", io={"resolve_request_runtime_agent_kwargs": None})
case("_apply_provider_runtime", provider="openai", required=True,
     io={"resolve_request_runtime_agent_kwargs": raise_spec("RuntimeError", "boom"),
         "run._resolve_runtime_agent_kwargs_for_provider": raise_spec("RuntimeError", "fb")})
case("_apply_provider_runtime", provider="", io={})
case("_apply_provider_runtime", provider="openai", state={"runtime_kwargs": {}},
     io={"resolve_request_runtime_agent_kwargs": {"args": ["a", "b"], "provider": "openai"}})

# ── _recover_or_record_model ─────────────────────────────────────────────────
case("_recover_or_record_model", model="gpt", gateway_session_key="k1",
     state={"model_name": "hermes-agent", "runtime_kwargs": {}})
case("_recover_or_record_model", model="", gateway_session_key="k1",
     state={"runtime_kwargs": {"provider": "openai"}, "model_name": "hermes-agent"},
     io={"models.get_default_model_for_provider": "gpt-4"})
case("_recover_or_record_model", model="", gateway_session_key="k1",
     state={"runtime_kwargs": {}, "model_name": "hermes-agent",
            "last_resolved_model": {"k1": "known-good"}})
case("_recover_or_record_model", model="", gateway_session_key="k2",
     state={"runtime_kwargs": {}, "model_name": "hermes-agent",
            "last_resolved_model": {"*": "global-good"}})
case("_recover_or_record_model", model="hermes-agent", gateway_session_key="k1",
     state={"model_name": "hermes-agent", "runtime_kwargs": {}})
case("_recover_or_record_model", model="", gateway_session_key="",
     state={"runtime_kwargs": {"provider": "openai"}, "model_name": "hermes-agent"},
     io={"models.get_default_model_for_provider": raise_spec("RuntimeError", "no catalog")})

# ── _select_agent_runtime ────────────────────────────────────────────────────
case("_select_agent_runtime", model="global", gateway_session_key="k1",
     state={"runtime_kwargs": {"provider": "cur"}},
     io={"session_model_override_for": {"provider": "openai", "model": "ov"},
         "model_switch.resolve_effective_model": "ov-model",
         "resolve_request_runtime_agent_kwargs": {"provider": "openai", "api_key": "k"}})
case("_select_agent_runtime", model="global", session_model="sess-model", gateway_session_key="k1",
     state={"runtime_kwargs": {"provider": "cur"}},
     io={"session_model_override_for": None,
         "resolve_request_runtime_agent_kwargs": {"provider": "cur"},
         "model_switch.resolve_effective_model": "sess-model"})
case("_select_agent_runtime", model="global", requested_model="req", requested_provider="openai",
     state={"runtime_kwargs": {"provider": "cur"}},
     io={"session_model_override_for": None,
         "resolve_request_runtime_agent_kwargs": {"provider": "openai", "api_key": "k"}})
case("_select_agent_runtime", model="global", route={"model": "route-model", "provider": "rprov", "api_key": "rk"},
     state={"runtime_kwargs": {"provider": "cur"}},
     io={"session_model_override_for": None,
         "resolve_request_runtime_agent_kwargs": {"provider": "rprov"}})
case("_select_agent_runtime", model="global", requested_provider="openai", confirmed_runtime_lock=True,
     state={"runtime_kwargs": {"provider": "cur"}},
     io={"resolve_request_runtime_agent_kwargs": raise_spec("RuntimeError", "boom"),
         "run._resolve_runtime_agent_kwargs_for_provider": raise_spec("RuntimeError", "fb")})
case("_select_agent_runtime", model="global", requested_provider="openai",
     state={"runtime_kwargs": {"provider": "cur"}},
     io={"session_model_override_for": None, "resolve_request_runtime_agent_kwargs": None})

# ── is_rate_limited ──────────────────────────────────────────────────────────
case("is_rate_limited", error={"message": "auth failed", "cause": None},
     io={"auth.is_rate_limited_auth_error": False})
case("is_rate_limited", error={"message": "auth failed",
     "cause": {"kind": "RuntimeError", "message": "wrap", "cause": {"kind": "AuthError", "message": "429"}}},
     io={"auth.is_rate_limited_auth_error": True})
case("is_rate_limited", error={"message": "auth failed", "cause": {"kind": "ValueError", "message": "v"}},
     io={"auth.is_rate_limited_auth_error": False})
case("is_rate_limited", error={"message": "auth failed", "cause": None},
     io={"auth.is_rate_limited_auth_error": raise_spec("RuntimeError", "probe fail")})
case("is_rate_limited", error={"message": "auth failed", "cause": None}, io={})  # fail-closed

# ── user_text ────────────────────────────────────────────────────────────────
case("user_text", error={"message": "auth failed", "cause": None},
     io={"auth.is_rate_limited_auth_error": False, "t": ["Auth failed", "Error: {label}: {error}"]})
case("user_text", error={"message": "quota",
     "cause": {"kind": "RuntimeError", "message": "w", "cause": {"kind": "AuthError", "message": "429"}}},
     io={"auth.is_rate_limited_auth_error": True, "t": ["Rate limited", "Line: {label} ({error})"]})
case("user_text", error={"message": "auth failed", "cause": None},
     io={"auth.is_rate_limited_auth_error": False, "t": ["Auth failed", raise_spec("RuntimeError", "i18n fail")]})
case("user_text", error={"message": "auth failed", "cause": None},
     io={"auth.is_rate_limited_auth_error": raise_spec("RuntimeError", "probe fail"), "t": ["x", "y"]})
case("user_text", error={"message": "auth failed", "cause": None},
     io={"auth.is_rate_limited_auth_error": False})  # fail-closed t

# ── _effective_turn_runtime ──────────────────────────────────────────────────
case("_effective_turn_runtime", runtime_request={"requested": {"model": "m"}, "route_source": "raw_request"},
     result={"runtime": {"provider": "p", "model": "m"}}, usage={})
case("_effective_turn_runtime", runtime_request={"route_source": "global"},
     result={}, usage={"runtime": {"provider": "u", "model": "um"}})
case("_effective_turn_runtime", runtime_request={"require_model_lock": True, "requested": {"provider": "p"}},
     result={"runtime": {"provider": "p", "model": "m"}}, usage={})
case("_effective_turn_runtime", runtime_request={"require_model_lock": True}, result={}, usage={})
case("_effective_turn_runtime", runtime_request={}, result=None, usage=None)

# ── _turn_runtime_metadata ───────────────────────────────────────────────────
case("_turn_runtime_metadata", agent={"provider": "openai", "model": "gpt", "_hermes_api_runtime": {"provider": "openai"}},
     route=None, requested_runtime=None, route_source="global", confirmed_runtime_lock=False)
case("_turn_runtime_metadata", agent={"provider": "openai", "model": "gpt"},
     route={"provider": "anthropic", "model": "claude"}, requested_runtime={},
     route_source="raw_request", confirmed_runtime_lock=True)  # mismatch raise
case("_turn_runtime_metadata", agent={"provider": "openai", "model": "gpt", "_hermes_api_runtime": {"provider": "openai"}},
     route={"provider": "openai", "model": "gpt"}, requested_runtime={"provider": "openai", "model": "gpt"},
     route_source="raw_request", confirmed_runtime_lock=True)
case("_turn_runtime_metadata", agent={"provider": "openai", "model": "gpt"},
     route=None, requested_runtime={"provider": "openai", "model": "gpt"},
     route_source="", confirmed_runtime_lock=False)
case("_turn_runtime_metadata", agent={"_hermes_api_runtime": {"provider": "p", "route_source": "session_model_lock"}},
     route=None, requested_runtime=None, route_source="", confirmed_runtime_lock=False)

# ── _finish_turn_result ──────────────────────────────────────────────────────
case("_finish_turn_result", agent={"session_id": "s2", "session_prompt_tokens": 10,
     "session_completion_tokens": 20, "session_total_tokens": 30}, session_id="s1",
     route=None, requested_runtime=None, route_source="global", confirmed_runtime_lock=False,
     state={"result": {"content": "hi"}})
case("_finish_turn_result", agent={"session_id": "s1", "_last_compaction_in_place": True}, session_id="s1",
     route=None, requested_runtime=None, route_source="global", confirmed_runtime_lock=False,
     state={"result": {}})
case("_finish_turn_result", agent={"session_id": "s1", "provider": "openai", "model": "gpt"}, session_id="s1",
     route=None, requested_runtime={"provider": "openai", "model": "gpt"}, route_source="raw_request",
     confirmed_runtime_lock=False, state={"result": {"content": "x"}})
case("_finish_turn_result", agent={"session_id": "s1"}, session_id="s1",
     route=None, requested_runtime=None, route_source="global", confirmed_runtime_lock=False,
     state={"result": {"content": "y"}})
case("_finish_turn_result", agent={"session_id": "s2", "provider": "openai", "model": "gpt"}, session_id="s1",
     route={"provider": "anthropic", "model": "claude"}, requested_runtime={}, route_source="raw_request",
     confirmed_runtime_lock=True, state={"result": {}})  # lock mismatch raise

# ── _handle_health ───────────────────────────────────────────────────────────
case("_handle_health", io={"hermes_version": "1.2.3", "web.json_response": {"__response__": True}})
case("_handle_health", io={"hermes_version": raise_spec("RuntimeError", "version fail"),
     "web.json_response": {"__response__": True}})
case("_handle_health", io={"hermes_version": "1.0"})  # fail-closed web
case("_handle_health", io={"hermes_version": None, "web.json_response": {"__response__": True}})
case("_handle_health", io={"hermes_version": "1.0", "web.json_response": raise_spec("OSError", "write fail")})

# ── _handle_models ───────────────────────────────────────────────────────────
case("_handle_models", state={"model_name": "hermes-agent"},
     io={"time.time": 1700000000, "request_profile": None, "web.json_response": {"__response__": True}})
case("_handle_models", state={"model_name": "hermes-agent"},
     io={"time.time": 1700000000, "request_profile": "prof", "resolve_model_name": "prof-model",
         "web.json_response": {"__response__": True}})
case("_handle_models", state={"model_name": "hermes-agent", "model_routes": {"alias1": {"model": "gpt"}, "alias2": {}}},
     io={"time.time": 1700000000, "request_profile": None, "web.json_response": {"__response__": True}})
case("_handle_models", state={"model_name": "hermes-agent", "model_routes": {"hermes-agent": {"model": "x"}}},
     io={"time.time": 1700000000, "request_profile": None, "web.json_response": {"__response__": True}})
case("_handle_models", io={"time.time": raise_spec("RuntimeError", "clock fail")})

# ── _handle_capabilities ─────────────────────────────────────────────────────
case("_handle_capabilities", state={"gateway_runner": {"session_authority": {"x": 1}}, "cors_origins": ["*"], "api_key": "k"},
     io={"runs._idempotency_capabilities": {"enabled": True}, "web.json_response": {"__response__": True}})
case("_handle_capabilities", state={"gateway_runner": None, "cors_origins": [], "api_key": ""},
     io={"runs._idempotency_capabilities": {"enabled": False}, "web.json_response": {"__response__": True}})
case("_handle_capabilities", state={"gateway_runner": {"session_authority": None}, "cors_origins": ["a"], "api_key": "k"},
     io={"runs._idempotency_capabilities": {}, "web.json_response": {"__response__": True}})
case("_handle_capabilities", io={"runs._idempotency_capabilities": raise_spec("RuntimeError", "idem fail"),
     "web.json_response": {"__response__": True}})
case("_handle_capabilities", state={"gateway_runner": {"session_authority": {"x": 1}}},
     io={"runs._idempotency_capabilities": {"enabled": True}})  # fail-closed web


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func count check (>=5)
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if short:
        print("FUNCS WITH <5 CASES:", short)
        raise SystemExit(1)


if __name__ == "__main__":
    main()