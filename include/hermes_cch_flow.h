/*
 * hermes_cch_flow.h -- streaming / tool-call FLOW cores ported from
 * agent/chat_completion_helpers.py (and the tool-call accumulator in
 * agent/chat_completion_helpers_tool_calls.py).
 *
 * MISSION: port the ORCHESTRATION logic (decision sequences, call order,
 * branch conditions, state transitions, error paths), not just pure leaves.
 * All live I/O (network stream, display callbacks, clock, tokenizer, retry
 * backoff, error classification) is injected through SEAMS: callback typedefs
 * and injected-result parameters backed by scripted json_t state. The decision
 * core never performs I/O itself.
 *
 * Each entry point takes the scripted input object `in` and returns a freshly
 * allocated json_t ARRAY capturing the ordered DECISION SEQUENCE (actions /
 * chunks / state transitions). The oracle drives the LIVE Python with the same
 * scripted I/O and compares the sequences element-by-element. Caller json_free()s
 * the returned array. `in` is borrowed (never freed / mutated by the entry).
 *
 * Link shape (reuses the canonical SSE/status ports):
 *   src/agent/chat_completion_helpers_flow.o
 *   src/agent/port_chat_completion_helpers.o lib/libjson/json.o
 *   lib/libregex/hermes_regex.o
 */
#ifndef HERMES_CCH_FLOW_H
#define HERMES_CCH_FLOW_H

#include "../lib/libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── tool-call delta assembly (chat_completion_helpers_tool_calls.py) ── */

/* _ToolCallAccumulator.feed + materialize driven over a scripted delta list.
 * in: {"deltas":[{index,id,function:{name,arguments},extra_content,...}],
 *      "materialize":bool}
 * out: sequence of {"a":"feed","ret":name|null} per delta, then (if materialize)
 *      {"a":"materialize","acc":{slot:entry}}. */
json_t *cch_flow_tc_acc(const json_t *in);

/* ── chat_completions chunk loop (_call_chat_completions) ── */

/* The per-chunk accumulation / routing loop. Relay open, watchdogs, carriers,
 * prompt-progress and the clock are seams fed from `in`.
 * in: {"chunks":[chunk...], "llamacpp":bool, "interrupt_at":[idx...],
 *      "attempt_active":bool, "watch":{"content":[bool...],"reasoning":[...],
 *      "detail":[...]}, "tool_calls_acc":bool, "deltas_were_sent":bool,
 *      "hold_stream":bool, "stream_delta_callback":bool, "carriers":{...}}
 * out: ordered actions (emit_text / emit_reasoning / emit_tool_started /
 *      route_suppressed / pending_flush / runaway / choiceless / finish ...). */
json_t *cch_flow_chat_chunk_loop(const json_t *in);

/* _StreamingCall._choiceless_chunk (usage accounting + lastOne + in-stream error).
 * in: {"chunk":{...}, "finish_reason":str|null}
 * out: [{"a":"usage",...},{"a":"finish","reason":...}] or
 *      [{"a":"raise","kind":"ProviderStreamError",...}]. */
json_t *cch_flow_choiceless_chunk(const json_t *in);

/* _StreamingCall._assemble_tool_calls (materialize + truncated/repair flags).
 * in: {"acc":{slot:entry}, "finish_reason":str|null,
 *      "repair_results":[str...], "repetition":[bool...]}
 * out: [{"a":"assemble","tool_calls":[...]|null,"has_truncated":bool,
 *        "actions":[...]}]. */
json_t *cch_flow_assemble_tool_calls(const json_t *in);

/* _StreamingCall._finish_chat_stream (finish-reason routing / stub / error).
 * in: {"content_parts":[...],"reasoning_parts":[...],"refusal_parts":[...],
 *      "acc":{...},"finish_reason":...,"model":...,"usage":...,"role":...,
 *      "response_id":...,"upstream_provider":...,"reasoning_details":[...],
 *      "extract_reasoning":str|null,"repair_results":[...],"repetition":[...]}
 * out: ordered decisions ending in {"a":"response",...} / {"a":"stub",...} /
 *      {"a":"raise",...}. */
json_t *cch_flow_finish_chat_stream(const json_t *in);

/* _provider_stream_error_from_text (SSE error-text -> exception decision).
 * in: {"text":str,"finish_reason":str|null}
 * out: [] when None, else [{"a":"error","status_code":...,"body":{...},
 *      "raw_text":...,"message":...}]. */
json_t *cch_flow_provider_stream_error_from_text(const json_t *in);

/* _build_partial_stream_stub (chat + anthropic stub shapes).
 * in: {"role","full_content","full_reasoning","model_name","usage",
 *      "dropped_tool_names","overflow_terminal","api_mode","clean_eof"}
 * out: [{"a":"stub","shape":{...}}]. */
json_t *cch_flow_build_partial_stream_stub(const json_t *in);

/* _StreamingCall._partial_stream_stub (partial-delivery fallback decision).
 * in: {"streamed_text":str,"partial_tool_names":[...],"error":{...},
 *      "classify":{"reason":str|null},"warning_presentation":bool,
 *      "model":...,"api_mode":...}
 * out: ordered actions ending in {"a":"stub",...}. */
json_t *cch_flow_partial_stream_stub(const json_t *in);

/* _StreamingCall._maybe_disable_streaming (channel-switch decision).
 * in: {"error":{...},"llamacpp":bool,"empty_frame":bool,
 *      "stream_unavailable":bool,"api_mode":...,"provider":...}
 * out: ordered actions (disable_streaming / emit_warning / safe_print / none). */
json_t *cch_flow_maybe_disable_streaming(const json_t *in);

/* _StreamingCall._retry_after_drop (teardown + backoff sequence).
 * in: {"error":{...},"attempt":int,"max_retries":int,"mid_tool_call":bool,
 *      "reason":str,"is_parse_error":bool,"backoff_delay":num}
 * out: ordered actions (emit_stream_drop / buffer_anthropic / cancel_attempt /
 *      close_once / wait_backoff / stamp_clock). */
json_t *cch_flow_retry_after_drop(const json_t *in);

/* _StreamingCall._handle_stream_error (retry / fallback classification).
 * in: {"error":{...},"attempt","max_retries","request_cancelled":bool,
 *      "deltas_were_sent":bool,"stream_options_unsupported":bool,
 *      "rejects_stream_options":bool,"is_timeout":bool,"is_conn_err":bool,
 *      "is_sse_conn_err":bool,"is_parse_err":bool,"is_empty_stream":bool,
 *      "partial_tool_in_flight":bool,"warning_presentation":bool,
 *      "unmask_handles":bool,"backoff_delay":num,"lifetime_error":{...}|null}
 * out: ordered actions ending in {"a":"return","retry":bool}. */
json_t *cch_flow_handle_stream_error(const json_t *in);

/* _StreamingCall._unmask_server_error_with_nonstreaming (5xx probe decision).
 * in: {"error":{...},"interrupt_requested":bool,"llamacpp":bool,
 *      "status":int|null,"deltas_were_sent":bool,"api_mode":...,
 *      "last_probe_ts":num|null,"now":num,"probe":{"outcome":"success"|
 *      "error","status":int|null,"response":{...}},"replay_ok":bool}
 * out: ordered actions ending in {"a":"return","handled":bool}. */
json_t *cch_flow_unmask_probe(const json_t *in);

/* _StreamingCall._call (retry-loop orchestration).
 * in: {"max_stream_retries":int,"interrupt_before":[attempt_idx...],
 *      "wire":[{"outcome":"response","response":{...}}|
 *             {"outcome":"error","error":{...}}...],
 *      "handle_retry":[bool...],"route_switched":[bool...],
 *      "provider":...,"model":...}
 * out: ordered actions (start_attempt / interrupt / wire / handle_error /
 *      route_switch / close_once / return). */
json_t *cch_flow_call_retry_loop(const json_t *in);

/* _StreamingCall.run outcome translation (post-monitor decision tail).
 * in: {"monitor_interrupted":bool,"interrupt_requested":bool,
 *      "result_error":{...}|null,"result_response":{...}|null,
 *      "abandon_error":{...}|null,"deltas_were_sent":bool,
 *      "diag_first_chunk_at":num|null,"partial_stub":{...}}
 * out: ordered actions ending in {"a":"return",...} / {"a":"raise",...}. */
json_t *cch_flow_run_outcome(const json_t *in);

/* _reasoning_config_for_wire (+ _consume_ephemeral_reasoning_off).
 * in: {"reasoning_config":{...}|null,"ephemeral_reasoning_off":bool,
 *      "effort_rejected":bool,"disable_rejected":bool,"floor_required":bool,
 *      "unset_default":{...}|null,"floor_effort":str}
 * out: ordered actions ending in {"a":"wire","config":{...}|null,
 *      "wire_reasoning_config":{...}|null,"ephemeral_consumed":bool}. */
json_t *cch_flow_reasoning_config_for_wire(const json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CCH_FLOW_H */