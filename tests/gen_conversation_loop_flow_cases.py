#!/usr/bin/env python3
"""Generate tests/fixtures/conversation_loop_flow.jsonl — scripted universal-seam
cases for the 24 UNCLAIMED agent/conversation_loop.py FLOW defs ported in
src/agent/conversation_loop_flow.c.

Each case is {"func": <live_name>, "in": {...}} where in.io carries the seam
scripts (seam-name -> constant / per-call queue / {"__raise__":...} /
{"__script__":true,"ret":...}), in.state carries the mutable carriers, and the
remaining in.* keys are scalar / double inputs. >=5 cases per function cover the
decision branches, ordered-trace short-circuits, scripted raises, BaseException
passthrough, and fail-closed (missing-script) propagation.

Value model: attr doubles {"__attr__":{...}}; class-tagged _LoopState; callable
doubles {"__callable__":"<seam>"}; identity refs {"__ref__":"<id>"}. ASCII text;
integral floats == ints. Loop fixtures stay in the terminating domain.

Run: python tests/gen_conversation_loop_flow_cases.py
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "conversation_loop_flow.jsonl"


def attr(d, cls=None):
    o = {"__attr__": d}
    if cls:
        o["__class__"] = cls
    return o


def ls(d):
    return attr(d, "_LoopState")


def call(seam):
    return {"__callable__": seam}


def rs(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def sc(ret):
    return {"__script__": True, "ret": ret}


CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── 1. _continuation_overlap_length (pure KMP) ────────────────────────────────
case("_continuation_overlap_length", previous="a" * 40, continuation="a" * 40)
case("_continuation_overlap_length", previous="short", continuation="a" * 40)
case("_continuation_overlap_length", previous="a" * 40, continuation="short")
case("_continuation_overlap_length", previous="x" * 35 + "abcde", continuation="abcde" + "y" * 35)
case("_continuation_overlap_length", previous="abc" * 20, continuation="abc" * 20)
case("_continuation_overlap_length", previous="z" * 60, continuation="q" * 60)
case("_continuation_overlap_length", previous="ab" * 30, continuation="b" * 60)

# ── 2. _partial_turn_result (pure dict + **flags) ─────────────────────────────
case("_partial_turn_result", final_response="hi", messages=[{"role": "user"}], api_call_count=2, flags={})
case("_partial_turn_result", final_response="x", messages=[], api_call_count=0,
     flags={"failed": True, "compression_deferred": True})
case("_partial_turn_result", final_response="y", messages=[{"role": "tool"}], api_call_count=5,
     flags={"failed": False, "session_id": "s1"})
case("_partial_turn_result", final_response="z", messages=[], api_call_count=1,
     flags={"partial": False, "error": "override"})
case("_partial_turn_result", final_response="", messages=[], api_call_count=0, flags={"turn_exit_reason": "r"})
case("_partial_turn_result", final_response="a", messages=[], api_call_count=3,
     flags={"failed": True, "failure_reason": "context_overflow", "failure_retryable": False})

# ── 3. _redact_display_metadata ───────────────────────────────────────────────
case("_redact_display_metadata", metadata={"error": "secret"}, io={"redact_sensitive_text": "R"})
case("_redact_display_metadata", metadata={"error": "e", "error_surface": {"kind": "billing", "n": 5}},
     io={"redact_sensitive_text": "R"})
case("_redact_display_metadata", metadata={"list": ["a", "b"], "nested": {"x": "y"}},
     io={"redact_sensitive_text": "R"})
case("_redact_display_metadata", metadata={}, io={})
case("_redact_display_metadata", metadata={"error": "e"}, io={})  # fail-closed
case("_redact_display_metadata", metadata={"error": "e"}, io={"redact_sensitive_text": rs("SystemExit", "bye")})
case("_redact_display_metadata", metadata={"a": "1", "b": "2"},
     io={"redact_sensitive_text": ["R1", "R2"]})

# ── 4. _peel_moa_guidance ─────────────────────────────────────────────────────
case("_peel_moa_guidance", messages=[{"role": "user"}], guidance="g",
     io={"peel_reference_guidance": sc([{"role": "user"}])})
case("_peel_moa_guidance", messages=[], guidance=None, io={"peel_reference_guidance": sc([])})
case("_peel_moa_guidance", messages=[{"role": "user"}], guidance="g", io={})  # fail-closed
case("_peel_moa_guidance", messages=[], guidance="g",
     io={"peel_reference_guidance": rs("ValueError", "bad")})
case("_peel_moa_guidance", messages=[], guidance="g",
     io={"peel_reference_guidance": rs("KeyboardInterrupt", "stop")})
case("_peel_moa_guidance", messages=[{"role": "assistant"}], guidance={"x": 1},
     io={"peel_reference_guidance": sc([{"role": "assistant"}])})

# ── 5. _credential_refresh_profile ────────────────────────────────────────────
case("_credential_refresh_profile", state={"agent": attr({"provider": " Nous ", "base_url": "http://x"})},
     io={"route_profiles": sc(["profA", "profB"])})
case("_credential_refresh_profile", state={"agent": attr({"provider": "", "base_url": ""})},
     io={"route_profiles": sc([])})
case("_credential_refresh_profile", state={"agent": attr({})}, io={"route_profiles": sc([])})
case("_credential_refresh_profile", state={"agent": attr({"provider": "p", "base_url": None})},
     io={"route_profiles": sc(["only"])})
case("_credential_refresh_profile", state={"agent": attr({"provider": "p"})}, io={})  # fail-closed
case("_credential_refresh_profile", state={"agent": attr({"provider": "p"})},
     io={"route_profiles": rs("SystemExit", "bye")})

# ── 6. _decode_inline_moa_turn ────────────────────────────────────────────────
case("_decode_inline_moa_turn", user_message="u", persist_user_message=None,
     io={"decode_moa_turn": sc(["decoded", {"cfg": 1}])})
case("_decode_inline_moa_turn", user_message="u", persist_user_message="keep",
     io={"decode_moa_turn": sc(["decoded", {"cfg": 1}])})
case("_decode_inline_moa_turn", user_message="u", persist_user_message=None,
     io={"decode_moa_turn": sc(["decoded", None])})
case("_decode_inline_moa_turn", user_message="u", persist_user_message=None,
     io={"decode_moa_turn": rs("ValueError", "bad")})
case("_decode_inline_moa_turn", user_message="u", persist_user_message=None, io={})  # fail-closed
case("_decode_inline_moa_turn", user_message="u", persist_user_message=None,
     io={"decode_moa_turn": rs("GeneratorExit", "bye")})
case("_decode_inline_moa_turn", user_message="u", persist_user_message=None,
     io={"decode_moa_turn": sc(["only_one"])})  # wrong unpack shape -> unchanged

# ── 7. _print_guidance ────────────────────────────────────────────────────────
case("_print_guidance", message="line1\nline2", state={"agent": attr({"log_prefix": "[P]"})},
     io={"vprint": None})
case("_print_guidance", message="", state={"agent": attr({"log_prefix": "[P]"})}, io={})
case("_print_guidance", message="single", state={"agent": attr({"log_prefix": ""})}, io={"vprint": None})
case("_print_guidance", message="a\r\nb\rc", state={"agent": attr({"log_prefix": "X"})}, io={"vprint": None})
case("_print_guidance", message="a\nb", state={"agent": attr({})}, io={"vprint": None})  # absent log_prefix
case("_print_guidance", message="a", state={"agent": attr({"log_prefix": "P"})},
     io={"vprint": rs("SystemExit", "bye")})
case("_print_guidance", message="a\nb", state={"agent": attr({"log_prefix": "P"})}, io={})  # fail-closed

# ── 8. _engine_overrides_hook ─────────────────────────────────────────────────
case("_engine_overrides_hook", engine=attr({"select_context": {"__callable__": "hook", "__func__": {"__ref__": "ce_func"}}}),
     name="select_context", ce_has_hook=True)
case("_engine_overrides_hook", engine=attr({"select_context": {"__callable__": "hook", "__func__": {"__ref__": "own_func"}}}),
     name="select_context", ce_has_hook=True)
case("_engine_overrides_hook", engine=attr({"select_context": {"__callable__": "hook"}}),
     name="select_context", ce_has_hook=True)
case("_engine_overrides_hook", engine=None, name="select_context", ce_has_hook=True)
case("_engine_overrides_hook", engine=attr({"select_context": "notcallable"}), name="select_context", ce_has_hook=True)
case("_engine_overrides_hook", engine=attr({"select_context": {"__callable__": "hook"}}),
     name="select_context", ce_has_hook=False)
case("_engine_overrides_hook", engine=attr({"select_context": {"__callable__": "hook"}}),
     name="select_context", import_fails=True)

# ── 9. _maybe_inject_run_budget_wrapup ────────────────────────────────────────
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 100, "_run_budget_started_at": 0}),
            "messages": [{"role": "tool", "content": "x"}]},
     io={"time": [100, 100], "logger.info": None})
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 100, "_run_budget_started_at": 0}),
            "messages": [{"role": "tool", "content": "x"}]},
     io={"time": [10]})  # below 80% -> False, one time call
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 0, "_run_budget_started_at": 0}), "messages": []}, io={})
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 100, "_run_budget_started_at": 0,
                           "_run_budget_wrapup_injected": True}), "messages": []}, io={})
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 100, "_run_budget_started_at": 0}),
            "messages": [{"role": "tool", "content": "x", "_db_persisted": True}]},
     io={"time": [100]})  # persisted marker -> False
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 100, "_run_budget_started_at": 0}),
            "messages": [{"role": "user", "content": "u"}]}, io={"time": [100]})  # no tool row
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 100, "_run_budget_started_at": 0}),
            "messages": [{"role": "tool", "content": [{"type": "text", "text": "a"}]}]},
     io={"time": [100, 100], "logger.info": None})  # multimodal list
case("_maybe_inject_run_budget_wrapup",
     state={"agent": attr({"run_budget_seconds": 100, "_run_budget_started_at": 0}),
            "messages": [{"role": "tool", "content": "x"}]},
     io={"time": [100, 100]})  # fail-closed logger.info

# ── 10. _ollama_context_limit_error ───────────────────────────────────────────
_OL_AGENT = attr({"_ollama_num_ctx": 4000, "tools": [{"name": "t"}], "model": "m",
                  "provider": "ollama", "base_url": "http://l", "session_id": "s1"})
case("_ollama_context_limit_error", request_tokens=500, state={"agent": _OL_AGENT}, io={"logger.warning": None})
case("_ollama_context_limit_error", request_tokens=500,
     state={"agent": attr({"_ollama_num_ctx": 4000, "tools": []})}, io={})  # no tools
case("_ollama_context_limit_error", request_tokens=500,
     state={"agent": attr({"_ollama_num_ctx": 70000, "tools": [{"name": "t"}]})}, io={})  # ctx too big
case("_ollama_context_limit_error", request_tokens=500,
     state={"agent": attr({"_ollama_num_ctx": "4000", "tools": [{"name": "t"}]})}, io={})  # non-int
case("_ollama_context_limit_error", request_tokens=500,
     state={"agent": attr({"_ollama_num_ctx": 4000, "tools": [{"name": "t"}], "model": "",
                           "provider": "", "base_url": "", "session_id": None})},
     io={"logger.warning": None})  # defaults
case("_ollama_context_limit_error", request_tokens=500, state={"agent": _OL_AGENT}, io={})  # fail-closed
case("_ollama_context_limit_error", request_tokens=500, state={"agent": _OL_AGENT},
     io={"logger.warning": rs("SystemExit", "bye")})

# ── 11. _persist_system_prompt ────────────────────────────────────────────────
def persist_agent(**extra):
    d = {"_session_db": attr({}), "session_id": "s1", "_cached_system_prompt": "SP"}
    d.update(extra)
    return attr(d)

case("_persist_system_prompt", failure_message="fail %s %s", state={"agent": persist_agent()},
     io={"agent_interface": None, "update_system_prompt": None})
case("_persist_system_prompt", failure_message="fail %s %s",
     state={"agent": persist_agent(_session_db=None)}, io={})  # no db -> return
case("_persist_system_prompt", failure_message="fail %s %s", state={"agent": persist_agent()},
     io={"agent_interface": attr({"sessions": attr({})}), "update_system_prompt": None})  # store path
case("_persist_system_prompt", failure_message="fail %s %s", state={"agent": persist_agent()},
     io={"agent_interface": None, "update_system_prompt": None, "persist_agent_tool_names": None},
     persist_tools=True)
case("_persist_system_prompt", failure_message="fail %s %s", state={"agent": persist_agent()},
     io={"agent_interface": None, "update_system_prompt": rs("ValueError", "db down"), "logger.warning": None})
case("_persist_system_prompt", failure_message="fail %s %s", state={"agent": persist_agent()},
     io={"agent_interface": None, "update_system_prompt": rs("SystemExit", "bye")})
case("_persist_system_prompt", failure_message="fail %s %s", state={"agent": persist_agent()},
     io={"agent_interface": None})  # fail-closed update_system_prompt -> warning needs script
case("_persist_system_prompt", failure_message="fail %s %s",
     state={"agent": attr({"session_id": "s1", "_cached_system_prompt": "SP"})}, io={})  # absent _session_db

# ── 12. _restore_pinned_tools ─────────────────────────────────────────────────
case("_restore_pinned_tools", session_row={"tool_names": '["a","b"]'}, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a", "b"]), "restore_agent_tool_prefix": None})
case("_restore_pinned_tools", session_row={"tool_names": "not json"}, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a"]), "persist_agent_tool_names": None})
case("_restore_pinned_tools", session_row=None, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a"])})
case("_restore_pinned_tools", session_row={}, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a"])})
case("_restore_pinned_tools", session_row={"tool_names": '["a"]'},
     state={"agent": attr({"_persist_disabled": True})},
     io={"agent_tool_names": sc(["a"]), "restore_agent_tool_prefix": None})
case("_restore_pinned_tools", session_row={"tool_names": "[]"}, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a"]), "persist_agent_tool_names": None})  # empty pin -> persist
case("_restore_pinned_tools", session_row={"tool_names": '["a"]'}, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a"]), "restore_agent_tool_prefix": rs("ValueError", "x"), "logger.debug": None})
case("_restore_pinned_tools", session_row={"tool_names": '["a"]'}, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a"]), "restore_agent_tool_prefix": rs("SystemExit", "bye")})
case("_restore_pinned_tools", session_row={"tool_names": '["a"]'}, state={"agent": attr({})},
     io={"agent_tool_names": sc(["a"])})  # fail-closed restore -> logger.debug fail-closed

# ── 13. _refresh_bot_chat_tools ───────────────────────────────────────────────
case("_refresh_bot_chat_tools", state={"agent": attr({"platform": "desktop", "session_id": "s"})},
     io={"active_policy": None, "load_enabled_toolsets": sc(["e"]), "load_disabled_toolsets": sc(["d"]),
         "refresh_agent_mcp_tools": None})
case("_refresh_bot_chat_tools", state={"agent": attr({"platform": "cli", "session_id": "s"})},
     io={"active_policy": None})  # policy None + non-desktop/tui -> return
case("_refresh_bot_chat_tools", state={"agent": attr({"platform": "cli", "session_id": "s"})},
     io={"active_policy": attr({"toolsets": ["t1", "t2"]}), "refresh_agent_mcp_tools": None})
case("_refresh_bot_chat_tools", state={"agent": attr({"platform": "tui", "session_id": "s"})},
     io={"active_policy": None, "load_enabled_toolsets": sc([]), "load_disabled_toolsets": sc([]),
         "refresh_agent_mcp_tools": None})
case("_refresh_bot_chat_tools", state={"agent": attr({"platform": "desktop", "session_id": "s"})},
     io={"active_policy": None, "load_enabled_toolsets": sc(["e"]), "load_disabled_toolsets": sc([]),
         "refresh_agent_mcp_tools": rs("ValueError", "x"), "logger.warning": None})
case("_refresh_bot_chat_tools", state={"agent": attr({"platform": "desktop", "session_id": "s"})},
     io={"active_policy": None, "load_enabled_toolsets": rs("SystemExit", "bye")})
case("_refresh_bot_chat_tools", state={"agent": attr({"platform": "desktop", "session_id": "s"})},
     io={"active_policy": None})  # fail-closed load_enabled

# ── 14. _bot_chat_prompt_stale ────────────────────────────────────────────────
def stale_agent(**extra):
    d = {"_session_db": attr({}), "session_id": "s1", "_session_title_hint": ""}
    d.update(extra)
    return attr(d)

case("_bot_chat_prompt_stale", stored_prompt="P", state={"agent": stale_agent()},
     io={"agent_home": "/h", "stored_prompt_capability_stale": True})
case("_bot_chat_prompt_stale", stored_prompt="", state={"agent": stale_agent()}, io={})
case("_bot_chat_prompt_stale", stored_prompt="P", state={"agent": stale_agent(_bot_mode_protocol=False)},
     io={"agent_home": "/h", "stored_prompt_capability_stale": False})
case("_bot_chat_prompt_stale", stored_prompt="P",
     state={"agent": stale_agent(_session_title_hint="Bot Chat")},
     io={"agent_home": "/h", "stored_prompt_capability_stale": False,
         "stored_bot_chat_prompt_needs_upgrade": True})
case("_bot_chat_prompt_stale", stored_prompt="P",
     state={"agent": stale_agent(_session_title_hint="Other")},
     io={"agent_home": "/h", "stored_prompt_capability_stale": False})
case("_bot_chat_prompt_stale", stored_prompt="P", state={"agent": stale_agent()},
     io={"agent_home": "/h", "stored_prompt_capability_stale": False, "agent_interface": None,
         "db_get_session_title": "Bot Chat", "stored_bot_chat_prompt_needs_upgrade": True})
case("_bot_chat_prompt_stale", stored_prompt="P", state={"agent": stale_agent()},
     io={"agent_home": rs("ValueError", "x"), "stored_prompt_capability_stale": False,
         "stored_bot_chat_prompt_needs_upgrade": False, "agent_interface": None,
         "db_get_session_title": "Bot Chat"})
case("_bot_chat_prompt_stale", stored_prompt="P", state={"agent": stale_agent()},
     io={"agent_home": "/h", "stored_prompt_capability_stale": rs("SystemExit", "bye")})
case("_bot_chat_prompt_stale", stored_prompt="P", state={"agent": stale_agent()},
     io={"agent_home": "/h"})  # fail-closed capability_stale -> outer catch -> False
case("_bot_chat_prompt_stale", stored_prompt="P", state={"agent": stale_agent()},
     io={"agent_home": "/h", "stored_prompt_capability_stale": False,
         "agent_interface": attr({"sessions": attr({})}),
         "sessions_get_title": "Bot Chat", "stored_bot_chat_prompt_needs_upgrade": True})  # store.sessions path

# ── 15. _usage_window_refusal ─────────────────────────────────────────────────
def uw_agent(state_present=True):
    return attr({"_credits_state": attr({"x": 1}) if state_present else None})

case("_usage_window_refusal", state={"agent": uw_agent()},
     classified=attr({"error_context": {"usage_window": True}, "message": "srv"}),
     io={"current_model_is_top_tier": False, "window_hard_stop": "✕ Daily limit hit",
         "window_stop_resets_at_ms": 60000, "limit_hit": None})
case("_usage_window_refusal", state={"agent": uw_agent()},
     classified=attr({"error_context": {"usage_window": False}}), io={})
case("_usage_window_refusal", state={"agent": uw_agent()},
     classified=attr({"error_context": {"usage_window": True, "display_message": "  Server says  "},
                      "message": "srv"}),
     io={"current_model_is_top_tier": True, "window_hard_stop": "✕ stop",
         "window_stop_resets_at_ms": 0, "limit_hit": None})
case("_usage_window_refusal", state={"agent": uw_agent(False)},
     classified=attr({"error_context": {"usage_window": True}, "message": "server msg"}),
     io={"current_model_is_top_tier": False, "limit_hit": None})
case("_usage_window_refusal", state={"agent": uw_agent()},
     classified=attr({"error_context": {"usage_window": True}, "message": ""}),
     io={"current_model_is_top_tier": False, "window_hard_stop": None, "window_stop_resets_at_ms": None,
         "limit_hit": None})  # no message -> empty triple
case("_usage_window_refusal", state={"agent": uw_agent()},
     classified=attr({"error_context": {"usage_window": True}, "message": "srv"}),
     api_messages=[{"role": "user"}],
     io={"current_model_is_top_tier": False, "window_hard_stop": "✕ stop",
         "window_stop_resets_at_ms": 1500, "estimate_messages_tokens_rough": 100,
         "limit_hit": attr({"kind": "daily"}), "limit_hit_payload": {"kind": "daily", "n": 1}})
case("_usage_window_refusal", state={"agent": uw_agent()},
     classified=attr({"error_context": {"usage_window": True}, "message": "srv"}),
     io={"current_model_is_top_tier": rs("SystemExit", "bye")})
case("_usage_window_refusal", state={"agent": uw_agent()},
     classified=attr({"error_context": {"usage_window": True}, "message": "srv"}),
     io={"current_model_is_top_tier": False, "window_hard_stop": "✕ stop",
         "window_stop_resets_at_ms": 1000, "limit_hit": rs("ValueError", "x"), "logger.debug": None})

# ── 16. _arm_fallback_restart ─────────────────────────────────────────────────
case("_arm_fallback_restart", api_messages=[], active_system_prompt="old",
     state={"agent": attr({}), "retry": attr({"primary_recovery_attempted": True})},
     io={"sync_failover_system_message": "new"})
case("_arm_fallback_restart", api_messages=[{"role": "system"}], active_system_prompt="old",
     state={"agent": attr({}), "retry": attr({})}, io={"sync_failover_system_message": "new2"})
case("_arm_fallback_restart", api_messages=[], active_system_prompt="old",
     state={"agent": attr({}), "retry": attr({})}, io={})  # fail-closed
case("_arm_fallback_restart", api_messages=[], active_system_prompt="old",
     state={"agent": attr({}), "retry": attr({})},
     io={"sync_failover_system_message": rs("SystemExit", "bye")})
case("_arm_fallback_restart", api_messages=[], active_system_prompt=None,
     state={"agent": attr({}), "retry": attr({"restart_with_rebuilt_messages": False})},
     io={"sync_failover_system_message": None})

# ── 17. _preflight_timeout_result ─────────────────────────────────────────────
case("_preflight_timeout_result", exc=rs("TimeoutError", "timed out"),
     conversation_history=[{"role": "user"}], state={"agent": attr({})},
     io={"logger.warning": None, "note_turn_persisted": None})
case("_preflight_timeout_result", exc=rs("RuntimeError", "boom"), conversation_history=None,
     state={"agent": attr({})}, io={"logger.warning": None, "note_turn_persisted": None})
case("_preflight_timeout_result", exc=rs("ValueError", "v"), conversation_history=[],
     state={"agent": attr({})}, io={"logger.warning": None, "note_turn_persisted": None})
case("_preflight_timeout_result", exc=rs("TimeoutError", "t"), conversation_history=None,
     state={"agent": attr({})}, io={"logger.warning": None})  # fail-closed note_turn_persisted
case("_preflight_timeout_result", exc=rs("TimeoutError", "t"), conversation_history=None,
     state={"agent": attr({})}, io={"logger.warning": rs("SystemExit", "bye")})
case("_preflight_timeout_result", exc=rs("KeyboardInterrupt", "stop"), conversation_history=None,
     state={"agent": attr({})}, io={"logger.warning": None, "note_turn_persisted": None})

# ── 18. _run_phase ────────────────────────────────────────────────────────────
def phase(params, name="phase_fn", seam="phase_fn"):
    return {"__callable__": seam, "__params__": params, "__name__": name}

case("_run_phase", fn=phase(["messages", "retry_count"]), agent=attr({}),
     state={"state": ls({"messages": [], "retry_count": 0})},
     io={"phase_fn": {"action": "continue", "retry_count": 1, "messages": [{"role": "user"}]}})
case("_run_phase", fn=phase(["retry_count"], name="handle_api_error"), agent=attr({}),
     state={"state": ls({"retry_count": 0, "_provider_overflow_recovery_pending": True})},
     io={"phase_fn": {"action": "continue", "_provider_overflow_recovery_pending": False}})  # latched stays True
case("_run_phase", fn=phase(["retry_count"], name="handle_api_error"), agent=attr({}),
     state={"state": ls({"retry_count": 0, "_provider_overflow_recovery_pending": False})},
     io={"phase_fn": {"action": "continue", "_provider_overflow_recovery_pending": True}})  # latched arms
case("_run_phase", fn=phase(["api_error"], name="handle_api_error"), agent=attr({}),
     state={"state": ls({"retry_count": 0})}, extra={"api_error": rs("ValueError", "boom")},
     io={"phase_fn": {"action": "break"}})
case("_run_phase", fn=phase(["missing_field"]), agent=attr({}),
     state={"state": ls({"retry_count": 0})}, io={"phase_fn": {"action": "continue"}})  # getattr absent
case("_run_phase", fn=phase(["retry_count"]), agent=attr({}),
     state={"state": ls({"retry_count": 0})}, io={})  # fail-closed phase
case("_run_phase", fn=phase(["retry_count"]), agent=attr({}),
     state={"state": ls({"retry_count": 0})}, io={"phase_fn": rs("SystemExit", "bye")})
case("_run_phase", fn=phase(["retry_count"]), agent=attr({}),
     state={"state": ls({"retry_count": 5})}, extra={"retry_count": 9},
     io={"phase_fn": {"action": "continue", "result": {"x": 1}}})  # extra overrides state; result skipped

# ── 19. _run_api_retry_loop ───────────────────────────────────────────────────
def phases(**over):
    base = {nm: {"__callable__": nm, "__params__": [], "__name__": nm}
            for nm in ["nous_rate_limit_guard", "build_api_request", "perform_api_call",
                       "check_api_response", "handle_api_interrupt", "handle_api_error"]}
    base.update(over)
    return base

def loop_state(rc=0, mr=3):
    return {"agent": attr({}), "s": ls({"retry_count": rc, "max_retries": mr})}

case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "break"}})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "return", "result": {"final_response": "guard"}}})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": {"action": "continue"},
         "perform_api_call": {"action": "break"}})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": {"action": "continue"},
         "perform_api_call": {"action": "continue"},
         "check_api_response": {"action": "return", "result": {"final_response": "checked"}}})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": {"action": "continue"},
         "perform_api_call": {"action": "continue"}, "check_api_response": {"action": "break"}})
case("_run_api_retry_loop", state=loop_state(rc=3, mr=3), phases=phases(), io={})  # exhausted, empty trace
case("_run_api_retry_loop", state=loop_state(mr=1), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": {"action": "continue"},
         "perform_api_call": {"action": "continue"}, "check_api_response": {"action": "continue", "retry_count": 1}})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": rs("InterruptedError", "int"),
         "handle_api_interrupt": {"action": "break"}})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": rs("ValueError", "bad"),
         "handle_api_error": {"action": "return", "result": {"final_response": "err"}}})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": rs("SystemExit", "bye")})
case("_run_api_retry_loop", state=loop_state(), phases=phases(),
     io={"nous_rate_limit_guard": {"action": "continue"}, "build_api_request": rs("ValueError", "bad"),
         "handle_api_error": {"action": "continue", "retry_count": 3}})  # error then exhaust

# ── 20. _finalize_from_state ──────────────────────────────────────────────────
case("_finalize_from_state", finalize_params=["final_response", "api_call_count"],
     state={"agent": attr({}), "s": ls({"final_response": "done", "api_call_count": 1})},
     io={"finalize_turn": {"final_response": "done", "completed": True}})
case("_finalize_from_state", finalize_params=["final_response", "missing"],
     state={"agent": attr({}), "s": ls({"final_response": "done"})}, io={"finalize_turn": {}})  # absent field
case("_finalize_from_state", finalize_params=["final_response"],
     state={"agent": attr({}), "s": ls({"final_response": "d"})}, io={})  # fail-closed
case("_finalize_from_state", finalize_params=["final_response"],
     state={"agent": attr({}), "s": ls({"final_response": "d"})},
     io={"finalize_turn": rs("SystemExit", "bye")})
case("_finalize_from_state", finalize_params=[],
     state={"agent": attr({}), "s": ls({})}, io={"finalize_turn": {"completed": True}})

# ── 21. _turn_without_loop ────────────────────────────────────────────────────
case("_turn_without_loop", block_message="blocked", finalize_params=["final_response", "_turn_exit_reason"],
     state={"agent": attr({"api_mode": "chat"}), "s": ls({"final_response": None, "_turn_exit_reason": "unknown"})},
     io={"finalize_turn": {"completed": False}})
case("_turn_without_loop", block_message="", finalize_params=["final_response"],
     state={"agent": attr({"api_mode": "chat"}), "s": ls({"final_response": "d"})}, io={})
case("_turn_without_loop", block_message="",
     state={"agent": attr({"api_mode": "codex_app_server"}),
            "s": ls({"user_message": "u", "original_user_message": "u", "messages": [],
                     "effective_task_id": "t", "_should_review_memory": False, "turn_id": "tid",
                     "active_system_prompt": "sp"})},
     io={"run_codex_app_server_turn": {"api_calls": 1, "final_response": "r"},
         "activate_codex_app_server_fallback": False})
case("_turn_without_loop", block_message="", state={"agent": attr({}), "s": ls({})}, io={})  # absent api_mode
case("_turn_without_loop", block_message="b", finalize_params=["final_response"],
     state={"agent": attr({"api_mode": "chat"}), "s": ls({"final_response": None})}, io={})  # fail-closed finalize

# ── 22. _codex_app_server_turn ────────────────────────────────────────────────
def codex_state(**extra):
    d = {"user_message": "u", "original_user_message": "u", "messages": [],
         "effective_task_id": "t", "_should_review_memory": False, "turn_id": "tid",
         "active_system_prompt": "sp"}
    d.update(extra)
    return {"agent": attr({}), "s": ls(d)}

case("_codex_app_server_turn", state=codex_state(),
     io={"run_codex_app_server_turn": {"api_calls": 2, "final_response": "r"},
         "activate_codex_app_server_fallback": False})
case("_codex_app_server_turn", state=codex_state(),
     io={"run_codex_app_server_turn": {"api_calls": 3, "final_response": "r"},
         "activate_codex_app_server_fallback": True, "sync_failover_system_message": "newsp"})
case("_codex_app_server_turn", state=codex_state(),
     io={"run_codex_app_server_turn": {"api_calls": None}, "activate_codex_app_server_fallback": True,
         "sync_failover_system_message": "sp2"})  # api_calls None -> 0
case("_codex_app_server_turn", state=codex_state(),
     io={"run_codex_app_server_turn": {"api_calls": "4"}, "activate_codex_app_server_fallback": True,
         "sync_failover_system_message": "sp3"})  # str int
case("_codex_app_server_turn", state=codex_state(),
     io={"run_codex_app_server_turn": rs("SystemExit", "bye")})
case("_codex_app_server_turn", state=codex_state(),
     io={"run_codex_app_server_turn": {"api_calls": 1}, "activate_codex_app_server_fallback": True,
         "sync_failover_system_message": rs("ValueError", "x")})
case("_codex_app_server_turn", state={"agent": attr({}), "s": ls({"user_message": "u"})},
     io={"run_codex_app_server_turn": {}})  # missing s fields

# ── 23. _failed_turn_display_metadata ─────────────────────────────────────────
case("_failed_turn_display_metadata", result={"error": "boom"},
     state={"agent": attr({"provider": "p", "model": "m"})},
     io={"build_error_surface_from_result": {"kind": "billing"}, "redact_sensitive_text": "R"})
case("_failed_turn_display_metadata", result={"error": "boom"},
     state={"agent": attr({"provider": "p", "model": "m"})},
     io={"build_error_surface_from_result": rs("ValueError", "x"), "logger.debug": None,
         "redact_sensitive_text": "R"})
case("_failed_turn_display_metadata", result={"error": "boom"},
     state={"agent": attr({})}, io={"logger.debug": None, "redact_sensitive_text": "R"})  # absent provider
case("_failed_turn_display_metadata", result={"error": ""},
     state={"agent": attr({"provider": "p", "model": "m"})},
     io={"build_error_surface_from_result": None, "redact_sensitive_text": "R"})  # empty error dropped
case("_failed_turn_display_metadata", result={"error": "x" * 2500},
     state={"agent": attr({"provider": "p", "model": "m"})},
     io={"build_error_surface_from_result": None, "redact_sensitive_text": "R"})  # truncation
case("_failed_turn_display_metadata", result={"error": "boom"},
     state={"agent": attr({"provider": "p", "model": "m"})},
     io={"build_error_surface_from_result": rs("SystemExit", "bye")})
case("_failed_turn_display_metadata", result={},
     state={"agent": attr({"provider": "p", "model": "m"})},
     io={"build_error_surface_from_result": None})  # empty metadata

# ── 24. _close_durable_failed_turn ────────────────────────────────────────────
def close_state(result, **agent_extra):
    d = {"_session_db": attr({}), "session_id": "s1"}
    d.update(agent_extra)
    return {"agent": attr(d), "result": result}

def close_io(**over):
    base = {"agent_interface": None, "latest_conversation_role": "user",
            "failed_turn_notice": "notice", "build_error_surface_from_result": None,
            "append_message": None, "wall_time": 123, "flush_messages_to_session_db": None}
    base.update(over)
    return base

case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user", "content": "hi"}],
                        "current_turn_user_idx": 0}), io=close_io())
case("_close_durable_failed_turn",
     state=close_state({"completed": True, "messages": [{"role": "user"}]}), io={})
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "compression_exhausted": True, "messages": [{"role": "user"}]}), io={})
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "failure_reason": "context_overflow", "messages": [{"role": "user"}]}), io={})
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": []}), io={})
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user"}]}, _session_db=None), io={})
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user"}]}),
     io=close_io(latest_conversation_role="assistant"))  # tail closed
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user"}]}, _persist_disabled=True),
     io=close_io())
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user", "content": "u"},
                                                         {"role": "tool", "content": "t"}],
                        "current_turn_user_idx": 0}),
     io=close_io(failed_turn_notice="partial notice"))  # tool tail
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user"}], "error": "boom",
                        "current_turn_user_idx": 0}),
     io=close_io(build_error_surface_from_result={"kind": "billing"}, redact_sensitive_text="R"))
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user"}]}),
     io=close_io(latest_conversation_role=rs("SystemExit", "bye")))
case("_close_durable_failed_turn",
     state=close_state({"completed": False, "messages": [{"role": "user"}]}),
     io=close_io(**{"append_message": rs("ValueError", "x"), "logger.debug": None}))


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(cnt):
        print("  %-38s %d" % (fn, cnt[fn]))
    missing = [fn for fn in cnt if cnt[fn] < 5]
    if missing:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % missing)


if __name__ == "__main__":
    main()
