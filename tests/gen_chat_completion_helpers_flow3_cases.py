#!/usr/bin/env python3
"""Generate tests/fixtures/chat_completion_helpers_flow3.jsonl (>=5 cases/func); scoped to 15 verified-clean defs.
Arity and object/scalar parameters are derived from the AST of the live module;
collaborator methods are seeded as seams and every seam is scripted in io.
"""
import json
from pathlib import Path
from collections import Counter

OUT = Path(__file__).resolve().parent / "fixtures" / "chat_completion_helpers_flow3.jsonl"
PARAMS = {'_accept_chat_chunk': {'pos': ['self', 'stream_attempt_id', 'chunk'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_add': {'pos': ['obj'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_after_first': {'pos': ['self', 'fire'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_anthropic_summary_attempt': {'pos': ['agent', 'api_messages', 'api_request_id'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_chat_summary_attempt': {'pos': ['agent', 'api_messages', 'api_request_id'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_chunk_identity': {'pos': ['diag', 'chunk', 'model_name', 'response_id', 'upstream_provider'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_close_stream_handle': {'pos': ['self', 'stream', 'reason'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_codex_summary_attempt': {'pos': ['agent', 'api_messages', 'api_request_id'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_fire_first': {'pos': ['self'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_headed': {'pos': [], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_log_runaway_cut': {'pos': ['self', 'channel'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_model': {'pos': ['self'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_stamp_event': {'pos': [], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, '_writer_still_current': {'pos': ['self', 'label'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}, 'set_stream_handle': {'pos': ['self', 'stream'], 'n_defaults': 0, 'kwonly': [], 'kw_defaults': []}}
TRACE_MASK = ['ProviderStreamError', '_NonStreamWatchdogs', '_abort_client', '_abort_request_openai_client', '_alias_tool_search_bridge_for_xai', '_anthropic_result', '_assemble_tool_call', '_attempt_result', '_bedrock_converse_call', '_build_summary_messages', '_bump_stale_streak', '_call', '_call_anthropic', '_call_chat_completions', '_call_done.set', '_cancel_current_stream_attempt', '_candidate_pool_exhausted', '_check_stale_giveup', '_cloud_stale_timeout_for', '_consume_ephemeral_max_output', '_count_stale_attempt', '_create_request_anthropic_client', '_create_request_openai_client', '_estimate_chunk_bytes', '_extract_reasoning', '_fire_first', '_get_transport', '_hb.join', '_hb.start', '_hb_stop.set', '_hb_stop.wait', '_is_entitlement_rejected', '_last_wire_aliases_reset', '_local_stream_stale_timeout_default', '_merge_nous_portal_messages_extra_body', '_new_diag', '_open_chat_stream', '_payload_chars', '_poll_result', '_provider_error_body', '_reabort_if_cancelled', '_rearm_or_abandon_after_kill', '_record_interrupted_provider_wait', '_rename_tool_search_bridge_for_xai', '_replay_deltas', '_report_stale_nonstream_kill', '_resolve_api_mode', '_safe_print', '_sanitize_surrogates_flat', '_shutdown_socket', '_shutdown_stale_attempt_socket', '_socket_from_response', '_stream_attempt_is_active', '_stream_close_callable', '_stream_diag_init', '_stream_stale_timeout_is_explicit', '_strip_think_blocks', '_touch_activity', '_touch_stale_kill_activity', '_uncounted_stale_attempt', '_with_stream_emitters', '_worker_result', '_writer_still_current', 'abort', 'build_kwargs', 'claim_stream_writer', 'classify_responses_route', 'close', 'close_once', 'converse_dispatch', 'estimate_request_context_tokens', 'fire', 'flatten_message_text', 'get_provider_request_timeout', 'invalidate_runtime_client', 'is_headed_mode', 'is_native_gemini_base_url', 'logger.debug', 'logger.info', 'logger.warning', 'mark_done', 'messages.stream', 'normalize_converse_response', 'on_first_delta', 'prompt_progress', 'repr', 'set_client', 'stream.close', 'stream_writer_is_current', 'sys.modules.get', 'threading.Timer', 'threading.get_ident', 'time.monotonic', 'time.time', 'waits_like_local', 'watchdog.cancel', 'watchdog.start']
SEAMS = ['APIConnectionError', 'AnthropicStreamAccumulator', 'BackendIdentity', 'DOTALL', 'EmptyStreamError', 'InterruptedError', 'ProviderStreamError', 'RunawayStreamWatch', 'Thread', 'Timeout', 'TimeoutError', 'Timer', '_NonStreamWatchdogs', '__enter__', '__exit__', '__name__', '_abort_client', '_abort_request_anthropic_client', '_abort_request_openai_client', '_active_request_abort', '_after_first', '_alias_tool_search_bridge_for_xai', '_anthropic_preserve_dots', '_anthropic_result', '_assemble_tool_call', '_attempt_request_client', '_attempt_result', '_attempt_stream_response', '_base_url_hostname', '_base_url_lower', '_bedrock_converse_call', '_bound_openai_codex_stale_timeout', '_buffer_diagnostic_status', '_build_api_kwargs', '_build_summary_messages', '_bump_stale_streak', '_cached_system_prompt', '_call', '_call_anthropic', '_call_chat_completions', '_call_done', '_call_done.set', '_cancel_current_stream_attempt', '_candidate_pool_exhausted', '_canonicalize_api_tool_calls', '_capture_credits', '_capture_nous_model_switch', '_capture_rate_limits', '_check_anthropic_message', '_check_openrouter_cache_status', '_check_stale_giveup', '_clone_message_for_send', '_close_managed_stream', '_close_request_anthropic_client', '_close_request_openai_client', '_close_stream_handle', '_cloud_stale_timeout_for', '_codex_on_first_delta', '_compute_non_stream_stale_timeout', '_consume_ephemeral_max_output', '_context_thread_target', '_copy_reasoning_content_for_api', '_count_chunk', '_count_stale_attempt', '_create_request_anthropic_client', '_create_request_openai_client', '_derive_responses_function_call_id', '_deterministic_call_id', '_disable_streaming', '_drop_thinking_only_and_merge_users', '_dump_if_model', '_emit_reasoning', '_emit_text', '_emit_tool_started', '_endpoint_reasoning_payload', '_estimate_chunk_bytes', '_extract_reasoning', '_fall_back_to_converse', '_fallback_entry_unavailable_without_network', '_finalize_bedrock_relay_events', '_fire_first', '_fire_first_delta', '_fire_reasoning_delta', '_fire_stream_delta', '_fire_tool_gen_started', '_get_bedrock_runtime_client', '_get_transport', '_has_stream_consumers', '_hb', '_hb.join', '_hb.start', '_hb_stop', '_hb_stop.set', '_hb_stop.wait', '_high_effort_silence_floor', '_interrupt_requested', '_interruptible_api_call', '_is_anthropic_oauth', '_is_anthropic_wire_url', '_is_azure_openai_url', '_is_direct_openai_url', '_is_entitlement_rejected', '_is_openai_codex_backend', '_is_openrouter_url', '_is_qwen_portal', '_join_worker_for_relay_teardown', '_last_wire_aliases', '_last_wire_aliases_reset', '_lmstudio_reasoning_options_cached', '_local_stream_stale_timeout_default', '_log_runaway_cut', '_managed_summary_call', '_mark_finish_seen', '_max_tokens_param', '_merge_nous_portal_messages_extra_body', '_moa_client_consumes_prepared_request', '_model', '_monitor_interrupted', '_new_diag', '_ollama_num_ctx', '_on_stale', '_open_chat_stream', '_open_stream', '_opencode_family_for_custom', '_payload_chars', '_poll_result', '_prepare_anthropic_messages_for_api', '_prepare_messages_for_non_vision_model', '_provider_error_body', '_provider_model_requires_responses_api', '_provider_preferences_for_agent', '_quiet', '_raise_if_interrupted', '_reabort_if_cancelled', '_rearm_or_abandon_after_kill', '_reasoning_endpoint_profile', '_record_interrupted_provider_wait', '_relay_stream_identity', '_relay_stream_metadata', '_rename_tool_search_bridge_for_xai', '_replay_deltas', '_replay_final_response', '_report_stale_nonstream_kill', '_request_cancelled', '_reset_stale_streak', '_resolve_api_mode', '_resolved_api_call_timeout', '_runaway_repetition', '_safe_print', '_sanitize_api_messages', '_sanitize_messages_surrogates', '_sanitize_surrogates', '_sanitize_surrogates_flat', '_sanitize_tool_calls_for_strict_api', '_set_managed_stream', '_should_sanitize_tool_calls', '_shutdown_socket', '_shutdown_stale_attempt_socket', '_socket_from_response', '_split_responses_tool_id', '_stale_counted_attempts', '_stream_attempt_is_active', '_stream_callback', '_stream_close_callable', '_stream_diag_capture_response', '_stream_diag_init', '_stream_stale_timeout', '_stream_stale_timeout_is_explicit', '_strip_think_blocks', '_summary_text', '_supports_reasoning_extra_body', '_touch_activity', '_touch_stale_kill_activity', '_uncounted_stale_attempt', '_watchdog', '_with_stream_emitters', '_worker', '_worker_result', '_writer_still_current', '_writer_token', 'abort', 'abort_hook', 'add', 'agent', 'any', 'api_kwargs', 'api_mode', 'append', 'arguments', 'base_url', 'base_url_host_matches', 'base_url_hostname', 'bool', 'build', 'build_kwargs', 'bypass_chat_sdk_request_transform', 'call_start', 'callable', 'cancel', 'cancelled', 'chat', 'choices', 'claim_stream_writer', 'classify_responses_route', 'client', 'clients', 'close', 'close_once', 'completions', 'context_length', 'converse', 'converse_dispatch', 'converse_stream', 'copy', 'create', 'current_message_snapshot', 'daemon', 'debug', 'deepcopy', 'deltas_were_sent', 'diag', 'dict', 'done', 'enumerate', 'env_float', 'environ', 'ephemeral_system_prompt', 'estimate_request_context_tokens', 'evict_stale_outbound_tool_images', 'feed', 'field', 'final_response', 'finalize', 'findall', 'finish', 'fire', 'first_delta_fired', 'flatten_message_text', 'float', 'function', 'get', 'get_final_message', 'get_ident', 'get_provider_profile', 'get_provider_request_timeout', 'getattr', 'has_reasoning_stream_observer_hooks', 'hasattr', 'id', 'info', 'insert', 'int', 'invalidate_runtime_client', 'is_alive', 'is_headed_mode', 'is_local_endpoint', 'is_native_gemini_base_url', 'is_stale_connection_error', 'is_streaming_access_denied_error', 'isfinite', 'isinstance', 'join', 'kind', 'last_chunk_time', 'last_event', 'len', 'list', 'lock', 'logger.debug', 'logger.info', 'logger.warning', 'lower', 'managed_stream_holder', 'map', 'mark_done', 'max', 'max_tokens', 'messages', 'messages.stream', 'method', 'min', 'model', 'model_extra', 'modules', 'monotonic', 'name', 'native_compaction_context_management', 'next', 'normalize_converse_response', 'normalize_stream_usage', 'nous_api_mode', 'observe', 'on_first_delta', 'on_stream_denied', 'openai_codex_stale_timeout_floor', 'opencode_model_api_mode', 'openrouter_min_coding_score', 'outbound_retired_image_ids', 'output_modified', 'owner_tid', 'pop', 'prefill_messages', 'prepare', 'profile_reasoning', 'prompt_progress', 'provider', 'provider_tool_in_flight', 'reasoning_callback', 'reasoning_config', 'recover_from_cache_point_rejection', 'recover_from_redacted_reasoning_rejection', 'redact_assistant_reply', 'region', 'relay_llm', 'repr', 'response', 'response_started', 'result', 'sanitize_anthropic_kwargs', 'sanitize_outbound_kwargs', 'session_id', 'set', 'set_client', 'shape_wire_carriers', 'should_skip_candidate', 'sleep', 'stale', 'stale_timeout', 'start', 'started_at', 'startswith', 'stop_details', 'stop_reason', 'str', 'stream', 'stream.close', 'stream_attempt_lock', 'stream_attempt_state', 'stream_converse_with_callbacks', 'stream_delta_callback', 'stream_writer_is_current', 'strip', 'strip_images_for_rejecting_model', 'strip_pattern_and_format', 'strip_slash_enum', 'substitute_api_content', 'suppress', 'sys.modules.get', 'thinking', 'threading.Timer', 'threading.get_ident', 'time', 'time.monotonic', 'time.time', 'type', 'uuid4', 'values', 'verbose_logging', 'wait', 'waits_like_local', 'warning', 'watchdog.cancel', 'watchdog.start', 'with_prompt_progress', 'worker']

OBJ_NAMES = set("self agent transport client chunk message tool_call raw_stream response final_response diag obj owner _event _stream fr final_kwargs writer_token tool_call_dict tool_call_obj request_client stream response_obj next_api_kwargs payload msg message_obj assistant_message".split())
SCALARS = [0, 1, "", "x", None]

cases = []

def seam(n): return {"__seam__": n}
def ref(n): return {"__ref__": n}

def objs_for(p):
    objs = {}
    for name in p["pos"] + p["kwonly"]:
        if name in OBJ_NAMES:
            objs[name] = {"__obj__": {
                "api_kwargs": {}, "stream_attempt_state": {"current": 1, "cancelled": []},
                "managed_stream_holder": {}, "result": {}, "agent": {},
                "api_mode": "", "provider": "", "model": "m", "max_tokens": 10,
                "provider_tool_in_flight": {}, "prompt_progress": {},
                "_interrupt_requested": False, "stale_timeout": 30.0,
                "client": None, "done": False, "cancelled": False, "stale": False,
                "last_chunk_time": {}, "clients": {}, "_writer_token": None,
                "_stale_counted_attempts": [], "delivered": False, "response_started": False,
                "first_delta_fired": False, "on_first_delta": None,
            }}
    return objs

def io_for(i):
    io = {}
    for s in SEAMS:
        io[s] = None
    # a few distinct non-null values to exercise branches
    io["_payload_chars"] = i
    io["is_headed_mode"] = (i % 2 == 0)
    io["sys.modules.get"] = None
    return io

def make_case(func, i):
    p = PARAMS[func]
    objs = objs_for(p)
    required = p["pos"][:len(p["pos"]) - p["n_defaults"]] if p["n_defaults"] else p["pos"]
    args = []
    for k, name in enumerate(required):
        if name in objs:
            args.append(ref(name))
        else:
            args.append(SCALARS[i % len(SCALARS)])
    kwargs = {}
    for name, hasdef in zip(p["kwonly"], p["kw_defaults"]):
        if not hasdef:
            kwargs[name] = SCALARS[i % len(SCALARS)]
    return {"func": func, "in": {
        "objs": objs, "args": args, "kwargs": kwargs, "globals": {}, "io": io_for(i), "trace_mask": TRACE_MASK,
    }}

for func in PARAMS:
    for i in range(5):
        cases.append(make_case(func, i))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
counts = Counter(c["func"] for c in cases)
print("wrote %d cases to %s" % (len(cases), OUT))
assert all(n >= 5 for n in counts.values())
assert len(counts) == len(PARAMS)
