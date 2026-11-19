/* auxiliary_client_flow.h — FLOW-core port of agent/auxiliary_client.py.
 *
 * Mission: match the program's FLOW (decision sequences, call order, branch
 * conditions, state transitions, error paths) of the auxiliary-LLM-call
 * orchestration: route/model selection, request building, the recovery ladder
 * (parameter strips -> Nous heal/refresh -> credential refresh/pool rotation ->
 * provider fallback), and budget decisions. I/O is INJECTED through seams (a
 * generic callback dispatcher + json_t state), never performed inside the
 * ported decision core.
 *
 * Seam model
 * ----------
 * Every live module-level I/O call, provider-registry/config read, clock read,
 * and aux_errors verdict predicate the ported flow makes goes through
 * `acf_io_t.call(io, name, args)`. The dispatcher:
 *   - appends {"name": name, "args": args} to `io->trace` (ordered JSON array)
 *     so the decision SEQUENCE can be compared against live Python under
 *     scripted I/O;
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "...",
 *     "status_code": <int|null>, "exc_id": <str>, "classified": <failure|null>,
 *     "base_only": <bool>} ("classified" non-null = a live _ClassifiedAttempt
 *     wrapping that failure; "base_only" = BaseException but not Exception).
 *
 * Live counterpart of each seam name (the call it stands in for):
 *   perform                       the ladder driver's `perform(step)` callback
 *                                 (_drive_ladder / _drive_ladder_async)
 *   time.sleep                    time.sleep(step.args[0]) for "wait" steps
 *   time.monotonic                time.monotonic() in _elapsed_ms
 *   classify_attempt              aux_errors.classify_attempt (agent/auxiliary_error_policy.py)
 *   ladder_accepts                aux_errors.ladder_accepts
 *   fallback_label                aux_errors.fallback_label
 *   auth_refresh                  aux_errors.auth_refresh
 *   benches_provider              aux_errors.benches_provider
 *   same_key_retry                aux_errors.same_key_retry
 *   same_provider_retry           aux_errors.same_provider_retry
 *   resend_cannot_help            aux_errors.resend_cannot_help
 *   _is_unsupported_parameter_error  agent/auxiliary_client.py (claimed port)
 *   is_reasoning_field_rejection  agent/error_classifier.py
 *   is_reasoning_required_rejection  agent/error_classifier.py
 *   parse_reasoning_effort        hermes_constants.py
 *   _get_auxiliary_task_config    agent/auxiliary_task_config.py (config read)
 *   _get_task_extra_body          agent/auxiliary_client.py (config read)
 *   _effective_aux_timeout        agent/auxiliary_client.py (config read)
 *   _resolve_task_provider_model  agent/auxiliary_client.py (route resolution)
 *   _resolve_call_client          agent/auxiliary_client.py (client build)
 *   _build_call_kwargs            agent/auxiliary_client.py (request build)
 *   _set_relay_auxiliary_route    agent/auxiliary_client.py (contextvar write)
 *   _fallback_provider_from_label agent/auxiliary_client.py
 *   _normalize_aux_provider       agent/auxiliary_client.py
 *   _normalize_main_runtime       agent/auxiliary_client.py
 *   _effective_provider_for_client agent/auxiliary_client.py
 *   _recoverable_pool_provider    agent/auxiliary_client.py
 *   _fallback_destination         agent/auxiliary_client.py
 *   _fallback_entry_timeout       agent/auxiliary_client.py (config read)
 *   _fallback_chain_entry         agent/auxiliary_client.py (config read)
 *   _replan_synchronous_cache_sections agent/auxiliary_client.py
 *   _candidate_context_window     agent/auxiliary_client.py (catalog read)
 *   _fallback_entry_api_key       agent/auxiliary_client.py (env read)
 *   _resolve_moa_aggregator       agent/auxiliary_client.py (config read)
 *   _to_openai_base_url           agent/auxiliary_client.py (URL rewrite)
 *   base_url_path                 utils/urls.py
 *   base_url_host_matches         utils/urls.py
 *   profile_for_host              providers (registry read)
 *   get_provider                  hermes_cli.providers (catalog read)
 *   get_provider_profile          providers (registry read)
 *   _get_named_custom_provider    hermes_cli.runtime_provider (config read)
 *   nous_api_mode                 hermes_cli.providers (catalog read)
 *   is_native_gemini_base_url     agent/gemini_native_adapter.py
 *   _is_anthropic_compat_endpoint agent/auxiliary_client.py
 *   _is_managed_local_endpoint    agent/auxiliary_managed_local.py
 *   profile_forwards_max_tokens   agent/auxiliary_profile_gates.py
 *   resolve_compression_fast_lane agent/auxiliary_client.py (lane certification)
 *   generic_nested_reasoning      agent/reasoning_effort.py
 *   clamp_reasoning_config        agent/reasoning_effort.py
 *   _nous_portal_tags             agent/auxiliary_client.py (portal tags)
 *   get_conversation_context      agent/portal_tags.py
 *   load_config_readonly          hermes_cli.config (config read)
 *   get_compatible_custom_providers hermes_cli.config
 *   _custom_provider_extra_body_for_agent agent/agent_init.py
 *   with_prompt_progress          agent/local_prompt_progress.py
 *   _aux_stream_total_ceiling     agent/auxiliary_client.py
 *   _refresh_provider_credentials agent/auxiliary_client.py (OAuth refresh)
 *   _refresh_nous_auxiliary_client agent/auxiliary_client.py
 *   _refresh_nous_recommended_model agent/auxiliary_client.py (Portal fetch)
 *   _nous_portal_account_has_fresh_paid_access agent/auxiliary_client.py
 *   _auth_refresh_provider_for_route agent/auxiliary_client.py
 *   _evict_cached_clients         agent/auxiliary_client.py (cache write)
 *   _evict_cached_client_instance agent/auxiliary_client.py (cache write)
 *   _recover_provider_pool        agent/auxiliary_client.py (pool rotation)
 *   _mark_provider_unhealthy      agent/auxiliary_client.py (health write)
 *   fallback_candidate_quarantine_ttl agent/auxiliary_health.py
 *   _custom_health_base_url       agent/auxiliary_health.py
 *   _try_configured_fallback_chain agent/auxiliary_client.py (chain select)
 *   _try_main_fallback_chain      agent/auxiliary_client.py (chain select)
 *   _try_payment_fallback         agent/auxiliary_client.py (chain select)
 *   _try_main_agent_model_fallback agent/auxiliary_client.py (chain select)
 *   _get_cached_client            agent/auxiliary_client.py (cache read)
 *   rate_limit_retry_rungs        agent/auxiliary_rate_limit_backoff.py
 *                                 (yield-from: its wait/call sub-steps flow
 *                                 through perform/time.sleep on both sides)
 *   reapply_repairs               agent/auxiliary_fallback_recovery.py
 *   remember_temperature_rejection agent/auxiliary_client.py (route memory)
 *   remember_structured_output_rejection agent/auxiliary_client.py
 *   remember_reasoning_floor      agent/auxiliary_reasoning_floor.py
 *   _without_structured_output_format agent/auxiliary_client.py (strip)
 *   with_reasoning_floor          agent/auxiliary_reasoning_floor.py (strip)
 *   model_validate                type(response).model_validate(data)
 *   rebuild                       _plan_fallback_candidate's _rebuild closure
 *   <thunk name>                  _outcome/_aoutcome's injected callable
 *
 * Data model (json_t mirrors of the live shapes)
 * ----------------------------------------------
 *   client    {"id": <str>, "base_url"?: <str>, "api_key"?: <str>,
 *              "_hermes_aux_backend_provider"?: <str>,
 *              "_hermes_aux_effective_provider"?: <str>, "kind"?: <str>}
 *             Identity (`is`) compares "id". "kind" in {"codex","async_codex"}
 *             models isinstance(client, CodexAuxiliaryClient/Async...).
 *             An ABSENT "base_url" key models a missing attribute (getattr
 *             default); a present null models attribute=None.
 *   exception {"__raise__": <type>, "message": <str(exc)>, "status_code"?: int,
 *              "exc_id"?: <str>, "classified"?: <failure>, "base_only"?: bool}
 *   failure   {"__failure__": true, "error": <exception>, "verdict": <verdict>,
 *              "rejects_parameter": bool, "client": <client id|null>}
 *   verdict   {"reason": <FailoverReason name|null>, "error_context": {...},
 *              "retryable": bool, "should_rotate_credential": bool,
 *              "should_fallback": bool, "model": <str|null>, ...}
 *   route     _LadderRoute fields as an object (client = id, route_info = obj).
 *   step      {"kind": "call"|"retry_same_provider"|"fallback"|"attempt"|"wait",
 *              "args": [...]}  (clients projected to ids, repairs to names)
 *   object    {"__obj__": true, ...attrs} models a non-dict SDK/SimpleNamespace
 *             response where dict-vs-object dispatch is observable.
 *
 * Ownership: every acf_* function returns a NEW json_t* (caller frees).
 * `io->trace` is owned by the harness. Args passed to acf_call are borrowed
 * (the dispatcher copies what it records). Inputs are borrowed.
 */
#ifndef AUXILIARY_CLIENT_FLOW_H
#define AUXILIARY_CLIENT_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Generic seam dispatcher + trace ───────────────────────────────────── */
typedef struct acf_io_t {
    /* Dispatch a live call by name. Returns a NEW json_t* (caller frees).
     * A result object carrying "__raise__" models a raised exception. */
    json_t *(*call)(struct acf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    json_t *clients; /* JSON object id -> client object (registry; borrowed).
                      * Routes/steps/failures carry client IDS; the core resolves
                      * them here for getattr reads. Mirrors the oracle's client
                      * instance registry (identity = same id = same instance). */
    json_t *relay_context; /* _RELAY_AUX_CALL_CONTEXT.get() mirror: JSON object
                      * {provider,model,attempt_count,attempt} or NULL. Mutated
                      * in place by _note_relay_auxiliary_error. Borrowed. */
    void   *ctx;     /* harness backend (scripted results) */
} acf_io_t;

/* Dispatch a seam call: records {"name","args"} on io->trace, then calls
 * io->call (NULL call => json_null()). args may be NULL (recorded as {}). */
json_t *acf_call(acf_io_t *io, const char *name, json_t *args);
/* True when a seam result models a raised exception. */
bool acf_raised(const json_t *result);

/* ── Uniform port contract: json in -> json out ────────────────────────── */
/* Pure helpers */
json_t *acf_field(acf_io_t *io, json_t *in);
json_t *acf_message_field(acf_io_t *io, json_t *in);
json_t *acf_contains_any(acf_io_t *io, json_t *in);
json_t *acf_creds_pair(acf_io_t *io, json_t *in);
json_t *acf_creds_have_api_key(acf_io_t *io, json_t *in);
json_t *acf_coerce_positive_timeout(acf_io_t *io, json_t *in);
json_t *acf_without_temperature(acf_io_t *io, json_t *in);
json_t *acf_without_max_tokens(acf_io_t *io, json_t *in);
json_t *acf_without_reasoning_fields(acf_io_t *io, json_t *in);
json_t *acf_affordable_max_tokens_from_error(acf_io_t *io, json_t *in);
json_t *acf_elapsed_ms(acf_io_t *io, json_t *in);
json_t *acf_stamp_latency_once(acf_io_t *io, json_t *in);
json_t *acf_record_route_info(acf_io_t *io, json_t *in);
json_t *acf_fallback_lane(acf_io_t *io, json_t *in);
json_t *acf_coerce_llm_message(acf_io_t *io, json_t *in);
json_t *acf_dedupe_tool_names(acf_io_t *io, json_t *in);
json_t *acf_tag_backend(acf_io_t *io, json_t *in);
json_t *acf_provider_outcome(acf_io_t *io, json_t *in);
json_t *acf_ladder_step_call(acf_io_t *io, json_t *in);
json_t *acf_failure_sender(acf_io_t *io, json_t *in);
/* Rejection detectors / rung acceptance */
json_t *acf_is_structured_output_rejection(acf_io_t *io, json_t *in);
json_t *acf_is_reasoning_field_rejection(acf_io_t *io, json_t *in);
json_t *acf_is_reasoning_required_rejection(acf_io_t *io, json_t *in);
json_t *acf_is_max_tokens_rejection(acf_io_t *io, json_t *in);
json_t *acf_param_rung_accepts(acf_io_t *io, json_t *in);
/* Selection / config / budget */
json_t *acf_task_prefers_fast_model(acf_io_t *io, json_t *in);
json_t *acf_fast_lane_config_fields(acf_io_t *io, json_t *in);
json_t *acf_compression_config_claims_fast_lane(acf_io_t *io, json_t *in);
json_t *acf_compression_fast_lane_controls(acf_io_t *io, json_t *in);
json_t *acf_get_task_no_progress_timeout(acf_io_t *io, json_t *in);
json_t *acf_unwrap_moa_provider(acf_io_t *io, json_t *in);
json_t *acf_preserve_provider_with_base_url(acf_io_t *io, json_t *in);
json_t *acf_builtin_provider_present(acf_io_t *io, json_t *in);
json_t *acf_profile_credentials_bypass_custom(acf_io_t *io, json_t *in);
json_t *acf_named_custom_provider_present(acf_io_t *io, json_t *in);
json_t *acf_routes_to_custom_endpoint(acf_io_t *io, json_t *in);
json_t *acf_classification_provider(acf_io_t *io, json_t *in);
json_t *acf_provider_for_host(acf_io_t *io, json_t *in);
json_t *acf_relay_path(acf_io_t *io, json_t *in);
json_t *acf_context_too_small(acf_io_t *io, json_t *in);
json_t *acf_nous_on_messages_wire(acf_io_t *io, json_t *in);
json_t *acf_is_gemini_native_route(acf_io_t *io, json_t *in);
json_t *acf_forwards_max_tokens(acf_io_t *io, json_t *in);
json_t *acf_stream_request_plan(acf_io_t *io, json_t *in);
json_t *acf_with_custom_endpoint_extra_body(acf_io_t *io, json_t *in);
json_t *acf_merge_aux_extra_body(acf_io_t *io, json_t *in);
json_t *acf_unwrap_data_envelope(acf_io_t *io, json_t *in);
json_t *acf_note_relay_auxiliary_error(acf_io_t *io, json_t *in);
json_t *acf_should_skip_same_provider_retry(acf_io_t *io, json_t *in);
json_t *acf_should_retry_same_provider(acf_io_t *io, json_t *in);
/* Outcome wrappers */
json_t *acf_outcome(acf_io_t *io, json_t *in);
json_t *acf_aoutcome(acf_io_t *io, json_t *in);
/* Recovery ladder */
json_t *acf_parameter_rungs(acf_io_t *io, json_t *in);
json_t *acf_classify_failure(acf_io_t *io, json_t *in);
json_t *acf_failure(acf_io_t *io, json_t *in);
json_t *acf_rung(acf_io_t *io, json_t *in);
json_t *acf_ladder_parameter_rungs(acf_io_t *io, json_t *in);
json_t *acf_refreshed_nous_step(acf_io_t *io, json_t *in);
json_t *acf_ladder_nous_rungs(acf_io_t *io, json_t *in);
json_t *acf_failure_pool(acf_io_t *io, json_t *in);
json_t *acf_recover_failure_pool(acf_io_t *io, json_t *in);
json_t *acf_ladder_credential_rungs(acf_io_t *io, json_t *in);
json_t *acf_next_fallback_after_quarantine(acf_io_t *io, json_t *in);
json_t *acf_ladder_provider_fallback(acf_io_t *io, json_t *in);
json_t *acf_aux_recovery_ladder(acf_io_t *io, json_t *in);
json_t *acf_drive_ladder(acf_io_t *io, json_t *in);
json_t *acf_primary_failure(acf_io_t *io, json_t *in);
json_t *acf_start_recovery_ladder(acf_io_t *io, json_t *in);
json_t *acf_prepare_aux_request(acf_io_t *io, json_t *in);
json_t *acf_plan_aux_call(acf_io_t *io, json_t *in);
/* Fallback candidates */
json_t *acf_fallback_request_kwargs(acf_io_t *io, json_t *in);
json_t *acf_plan_fallback_candidate(acf_io_t *io, json_t *in);
json_t *acf_quarantine_fallback_candidate(acf_io_t *io, json_t *in);
json_t *acf_plan_fallback_auth_retry(acf_io_t *io, json_t *in);
json_t *acf_fallback_candidate_ladder(acf_io_t *io, json_t *in);
json_t *acf_candidate_classification_provider(acf_io_t *io, json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* AUXILIARY_CLIENT_FLOW_H */