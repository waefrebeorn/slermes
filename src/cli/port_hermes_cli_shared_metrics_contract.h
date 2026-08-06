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

#ifdef __cplusplus
extern "C" {
#endif

/* ── String constants matching Python ──────────────────────────── */

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

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_SHARED_METRICS_CONTRACT_H */