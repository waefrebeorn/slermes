/*
 * port_hermes_cli_shared_metrics_contract.h — Bounded product contract
 * for the first Hermes shared-metrics slice.
 *
 * C11 port of hermes_cli/observability/shared_metrics_contract.py.
 * This is the contract layer: allowlisted dimensions, families, buckets,
 * and validators. No I/O, no SQLite, no relay dependency — all pure
 * deterministic functions.
 *
 * PoP annotation: port_hermes_cli_shared_metrics_contract.c
 * Python module: hermes_cli/observability/shared_metrics_contract.py
 */

#ifndef PORT_HERMES_CLI_SHARED_METRICS_CONTRACT_H
#define PORT_HERMES_CLI_SHARED_METRICS_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hermes_json.h"   /* json_t, json_obj_get, json_set, ... */

/* ── Runtime/schema constants (shared with relay callers) ─────── */
#define SMC_RUNTIME_INSTANCE_KEY "hermes.relay.runtime_instance"
#define SMC_RUNTIME_SCHEMA_KEY   "hermes.relay.schema_version"
#define SMC_RUNTIME_SCHEMA_VERSION "hermes.relay.runtime.v1"
#define SMC_TOOL_CALL_SCOPE "hermes.tool_call"
#define SMC_TOOL_APPROVAL_MARK "hermes.tool_approval"
#define SMC_SKILL_LIFECYCLE_MARK "hermes.skill.lifecycle"
#define SMC_SKILL_LOAD_MARK "hermes.skill.load"
#define SMC_CLIENT_ACTIVE_MARK "hermes.client.active"
#define SMC_MODEL_CALL_SCOPE_PREFIX "hermes.model_call"
#define SMC_MODEL_CALL_PROFILE_MODEL "unknown"

#define SMC_SCHEMA_KEY       "hermes.metrics.schema_version"
#define SMC_SCHEMA_VERSION   "hermes.metrics.event.v1"
#define SMC_MODEL_CALL_SCOPE "hermes.model_call"
#define SMC_TASK_SCOPE       "hermes.task_run"
#define SMC_SUBSCRIBER_NAME  "hermes.nemo_relay.shared_metrics"
#define SMC_PRIMARY_MODEL_CALL_ROLE "primary"
#define SMC_MODEL_CALL_METRIC  "hermes.model_call.count"
#define SMC_TASK_STARTED_METRIC  "hermes.task_run.started"
#define SMC_TASK_FINISHED_METRIC "hermes.task_run.finished"
#define SMC_RUNTIME_INSTANCE_KEY "hermes.runtime.instance_id"

/* ── Allowlisted value sets (frozensets) ───────────────────────── */

/* Execution surfaces */
extern const char *smc_execution_surfaces[];
extern const size_t smc_execution_surfaces_count;

/* Provider families */
extern const char *smc_provider_families[];
extern const size_t smc_provider_families_count;

/* Model localities */
extern const char *smc_model_localities[];
extern const size_t smc_model_localities_count;

/* Model outcomes */
extern const char *smc_model_outcomes[];
extern const size_t smc_model_outcomes_count;

/* Task outcomes */
extern const char *smc_task_outcomes[];
extern const size_t smc_task_outcomes_count;

/* Task end reasons */
extern const char *smc_task_end_reasons[];
extern const size_t smc_task_end_reasons_count;

/* Task terminations */
extern const char *smc_task_terminations[];
extern const size_t smc_task_terminations_count;

/* Task entrypoints */
extern const char *smc_task_entrypoints[];
extern const size_t smc_task_entrypoints_count;

/* Duration buckets */
extern const char *smc_duration_buckets[];
extern const size_t smc_duration_buckets_count;

/* Count buckets */
extern const char *smc_count_buckets[];
extern const size_t smc_count_buckets_count;

/* Model families (allowlist) */
extern const char *smc_model_families[];
extern const size_t smc_model_families_count;

/* ── Counter metrics (frozenset of metric names) ───────────────── */
extern const char *smc_counter_metrics[];
extern const size_t smc_counter_metrics_count;

/* ── Pure helper functions ─────────────────────────────────────── */

/**
 * Return whether dimensions match one closed shared-metric contract.
 * Python: counter_dimensions_are_valid(metric_name, dimensions)
 */
bool smc_counter_dimensions_are_valid(const char *metric_name,
                                       const char * const *dim_keys,
                                       const char * const *dim_vals,
                                       size_t dim_count);

/**
 * Normalize a session surface string.
 * Python: execution_surface(kwargs)
 */
const char *smc_execution_surface(const char *platform_value);

/**
 * Build the bounded fields recorded on a task scope start event.
 * Returns a JSON string: {"entrypoint":"...","execution_surface":"..."}
 * Caller must free the returned string.
 * Python: task_start_fields(kwargs)
 */
char *smc_task_start_fields(const char *entrypoint_val,
                            const char *platform_val);

/**
 * Normalize the task dispatch owner.
 * Python: task_entrypoint(kwargs, surface)
 */
const char *smc_task_entrypoint(const char *entrypoint_val,
                                 const char *surface,
                                 bool has_parent_task,
                                 bool has_parent_session);

/**
 * Build bounded terminal payload for one task scope.
 * Returns a JSON string; caller must free.
 * Python: task_terminal_fields(kwargs, duration_ms, model_call_count,
 *                               tool_call_count, retry_count)
 */
char *smc_task_terminal_fields(const char *entrypoint_val,
                                const char *platform_val,
                                int duration_ms,
                                int model_call_count,
                                int tool_call_count,
                                int retry_count);

/**
 * Map Hermes terminal state to bounded task outcome dimensions.
 * Returns a JSON string: ["outcome","end_reason","termination"]
 * Caller must free.
 * Python: task_terminal_state(kwargs)
 */
char *smc_task_terminal_state(const char *turn_exit_reason,
                               bool interrupted,
                               bool completed,
                               bool failed);

/**
 * Bucket a non-negative task duration.
 * Python: duration_bucket(duration_ms)
 */
const char *smc_duration_bucket(int duration_ms);

/**
 * Bucket a non-negative per-task count.
 * Python: count_bucket(count)
 */
const char *smc_count_bucket(int count);

/**
 * Map a raw model identifier to an allowlisted family.
 * Python: model_family(kwargs)
 */
const char *smc_model_family(const char *declared_family,
                              const char *model_name,
                              const char *response_model);

/**
 * Return the outcome if recognized, otherwise "failed".
 * Python: model_call_outcome(kwargs)
 */
const char *smc_model_call_outcome(const char *outcome);

/**
 * Return true if the provider name is a telemetry aggregator override.
 * Python: internal set check against _TELEMETRY_AGGREGATOR_OVERRIDES
 */
bool smc_is_aggregator_override(const char *provider);

/**
 * Return true if the provider name is a local custom alias.
 * Python: internal set check against _LOCAL_CUSTOM_PROVIDER_ALIASES
 */
bool smc_is_local_custom_alias(const char *provider);

/**
 * Classify a provider string into a bounded category.
 * Python: provider_family(kwargs)
 */
const char *smc_provider_family(const char *provider);

/* ── Event-extraction functions (per-event extractors) ─────────── */

/* PoP: _event_metadata_is_valid @ hermes_cli/observability/shared_metrics_contract.py:_event_metadata_is_valid
 * Returns true if the event's metadata carries a valid v1 relay schema envelope. */
bool smc_event_metadata_is_valid(const json_t *event);

/* PoP: _auxiliary_model_call_dimensions @ hermes_cli/observability/shared_metrics_contract.py:_auxiliary_model_call_dimensions */
char *smc_auxiliary_model_call_dimensions(const char *event_json);

/* PoP: model_call_dimensions @ hermes_cli/observability/shared_metrics_contract.py:model_call_dimensions */
char *smc_model_call_dimensions(const char *event_json);

/* PoP: task_counter @ hermes_cli/observability/shared_metrics_contract.py:task_counter */
char *smc_task_counter(const char *event_json);

/* PoP: task_terminal_fields @ hermes_cli/observability/shared_metrics_contract.py:task_terminal_fields
 * Build the bounded terminal payload for a task from its start fields + end data.
 * (smc_task_start_fields / smc_task_entrypoint / smc_duration_bucket /
 * smc_count_bucket are the existing validators reused here.) */
char *smc_task_terminal_fields_json(const json_t *data);

/* PoP: tool_call_dimensions @ hermes_cli/observability/shared_metrics_contract.py:tool_call_dimensions */
char *smc_tool_call_dimensions(const char *event_json);

/* PoP: tool_approval_counter @ hermes_cli/observability/shared_metrics_contract.py:tool_approval_counter */
char *smc_tool_approval_counter(const char *event_json);

/* PoP: skill_counter @ hermes_cli/observability/shared_metrics_contract.py:skill_counter */
char *smc_skill_counter(const char *event_json);

/* PoP: client_active_counter @ hermes_cli/observability/shared_metrics_contract.py:client_active_counter */
char *smc_client_active_counter(const char *event_json);

/* PoP: _non_negative_number @ hermes_cli/observability/shared_metrics_contract.py:_non_negative_number */
double smc_non_negative_number(const json_t *value);

/* PoP: _metric_identifier @ hermes_cli/observability/shared_metrics_contract.py:_metric_identifier
 * Caller frees the returned string. */
char *smc_metric_identifier(const char *value, int max_length);

/* PoP: skill_provenance @ hermes_cli/observability/shared_metrics_contract.py:skill_provenance */
const char *smc_skill_provenance(const char *value);

/* PoP: tool_category @ hermes_cli/observability/shared_metrics_contract.py:tool_category */
const char *smc_tool_category(const char *toolset);
/* PoP: tool_outcome @ hermes_cli/observability/shared_metrics_contract.py:tool_outcome */
const char *smc_tool_outcome(const char *status);
/* PoP: tool_approval_outcome @ hermes_cli/observability/shared_metrics_contract.py:tool_approval_outcome */
const char *smc_tool_approval_outcome(const char *choice);

/* PoP: tool_terminal_fields @ hermes_cli/observability/shared_metrics_contract.py:tool_terminal_fields */
char *smc_tool_terminal_fields_json(const json_t *data, const char *category,
                                    const char *approval_outcome,
                                    const json_t *fallback_duration_ms);

/* PoP: tool_latency_bucket @ hermes_cli/observability/shared_metrics_contract.py:tool_latency_bucket */
const char *smc_tool_latency_bucket(const json_t *value, const json_t *fallback);
/* PoP: tool_retry_bucket @ hermes_cli/observability/shared_metrics_contract.py:tool_retry_bucket */
const char *smc_tool_retry_bucket(const json_t *value);

/* client resource taxonomies */
/* PoP: client_os_family @ hermes_cli/observability/shared_metrics_contract.py:client_os_family */
const char *smc_client_os_family(const char *value);
/* PoP: client_architecture @ hermes_cli/observability/shared_metrics_contract.py:client_architecture */
const char *smc_client_architecture(const char *value);
/* PoP: client_install_method @ hermes_cli/observability/shared_metrics_contract.py:client_install_method */
const char *smc_client_install_method(const char *value);
/* PoP: client_resource @ hermes_cli/observability/shared_metrics_contract.py:client_resource */
char *smc_client_resource(const char *hermes_version, const char *os_name,
                          const char *architecture, const char *install_method);
/* PoP: client_resource_is_valid @ hermes_cli/observability/shared_metrics_contract.py:client_resource_is_valid */
bool smc_client_resource_is_valid(const char *resource_json);

/* PoP: skill_lifecycle_fields @ hermes_cli/observability/shared_metrics_contract.py:skill_lifecycle_fields */
/* PoP: skill_load_fields @ hermes_cli/observability/shared_metrics_contract.py:skill_load_fields */
char *smc_skill_lifecycle_fields(const char *action_val, const char *provenance_val);
char *smc_skill_load_fields(bool reused, bool reuse_after_patch,
                            int use_count, const char *provenance_val);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_SHARED_METRICS_CONTRACT_H */