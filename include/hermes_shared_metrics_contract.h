#ifndef HERMES_SHARED_METRICS_CONTRACT_H
#define HERMES_SHARED_METRICS_CONTRACT_H

/*
 * hermes_shared_metrics_contract.h — Bounded product contract for
 * Hermes shared-metrics. Port of hermes_cli/observability/shared_metrics_contract.py.
 * C11, no C++.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ────────────────────────────────────────────── */

#define SMC_SCHEMA_KEY          "hermes.metrics.schema_version"
#define SMC_SCHEMA_VERSION      "hermes.metrics.event.v1"
#define SMC_MODEL_CALL_SCOPE    "hermes.model_call"
#define SMC_TASK_SCOPE          "hermes.task_run"
#define SMC_SUBSCRIBER_NAME     "hermes.nemo_relay.shared_metrics"
#define SMC_PRIMARY_CALL_ROLE   "primary"
#define SMC_MODEL_CALL_METRIC   "hermes.model_call.count"
#define SMC_TASK_STARTED_METRIC "hermes.task_run.started"
#define SMC_TASK_FINISHED_METRIC "hermes.task_run.finished"

/* ── Frozen sets — checked via membership functions ───────── */

/* PoP: execution_surface @ agent/shared_metrics_contract.py:execution_surface */
bool smc_is_valid_execution_surface(const char *s);

/* PoP: provider_family @ agent/shared_metrics_contract.py:provider_family */
const char *smc_provider_family(const char *raw_provider);

/* PoP: model_family @ agent/shared_metrics_contract.py:model_family */
const char *smc_model_family_from_model(const char *model);

/* PoP: model_call_outcome @ agent/shared_metrics_contract.py:model_call_outcome */
const char *smc_model_call_outcome(const char *outcome);

/* PoP: counter_dimensions_are_valid @ agent/shared_metrics_contract.py:counter_dimensions_are_valid */
bool smc_counter_dimensions_are_valid(const char *metric_name, const json_t *dimensions);

/* PoP: model_call_dimensions @ agent/shared_metrics_contract.py:model_call_dimensions */
const json_t *smc_model_call_dimensions(const json_t *event);

/* PoP: task_counter @ agent/shared_metrics_contract.py:task_counter */
const json_t *smc_task_counter(const json_t *event);

/* PoP: task_start_fields @ agent/shared_metrics_contract.py:task_start_fields */
const json_t *smc_task_start_fields(const json_t *kwargs);

/* PoP: task_entrypoint @ agent/shared_metrics_contract.py:task_entrypoint */
const char *smc_task_entrypoint(const json_t *kwargs, const char *surface);

/* PoP: task_terminal_fields @ agent/shared_metrics_contract.py:task_terminal_fields */
const json_t *smc_task_terminal_fields(const json_t *kwargs,
                                        int64_t duration_ms,
                                        int64_t model_call_count,
                                        int64_t tool_call_count,
                                        int64_t retry_count);

/* PoP: task_terminal_state @ agent/shared_metrics_contract.py:task_terminal_state */
const char *smc_task_terminal_state(const json_t *kwargs,
                                     const char **out_end_reason,
                                     const char **out_termination);

/* PoP: duration_bucket @ agent/shared_metrics_contract.py:duration_bucket */
const char *smc_duration_bucket(int64_t duration_ms);

/* PoP: count_bucket @ agent/shared_metrics_contract.py:count_bucket */
const char *smc_count_bucket(int64_t count);

/* ── Model call fields builder ───────────────────────────── */

/* PoP: model_call_fields @ agent/shared_metrics_contract.py:model_call_fields */
const json_t *smc_model_call_fields(const json_t *kwargs);

/* ── Provider metadata helpers ────────────────────────────── */

/* PoP: _provider_metadata @ agent/shared_metrics_contract.py:_provider_metadata */
bool smc_provider_metadata(const char *provider,
                            const char **out_canonical,
                            bool *out_is_aggregator,
                            bool *out_is_known);

/* PoP: model_locality @ agent/shared_metrics_contract.py:model_locality */
const char *smc_model_locality(const json_t *kwargs);

/* PoP: _model_locality @ agent/shared_metrics_contract.py:_model_locality */
const char *smc_model_locality_with_family(const json_t *kwargs, const char *provider_category);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SHARED_METRICS_CONTRACT_H */