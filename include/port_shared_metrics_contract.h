/*
 * port_shared_metrics_contract.h -- pure helpers ported from
 * hermes_cli/observability/shared_metrics_contract.py.
 */
#ifndef PORT_SHARED_METRICS_CONTRACT_H
#define PORT_SHARED_METRICS_CONTRACT_H

#include <stddef.h>

#include "libjson/json.h"

typedef struct {
    double upper;
    const char *label;
} smc_threshold_t;

/* _bucket -- the first threshold whose upper bound exceeds `value` wins. */
const char *smc_bucket(double value, const smc_threshold_t *thresholds, size_t n,
                       const char *last);

/* _non_negative_number -- `kind` carries the Python type:
 * 0 = not a number, 1 = int, 2 = float, 3 = bool (rejected outright).
 * Returns 1 and sets *out when the value is a finite non-negative number. */
int smc_non_negative_number(int kind, long long ival, double dval, double *out);

const char *smc_long_size_bucket(long long count);
const char *smc_update_duration_bucket(int kind, long long ival, double dval);
const char *smc_version_age_bucket(int kind, long long ival, double dval);
const char *smc_desktop_active_minutes_bucket(int kind, long long ival, double dval);
const char *smc_duration_bucket(long long duration_ms);
const char *smc_count_bucket(long long count);
const char *smc_size_bucket(long long count);
const char *smc_tool_retry_bucket(int kind, long long ival);
const char *smc_tool_latency_bucket(int kind, long long ival, double dval,
                                    int fb_kind, long long fb_ival, double fb_dval);

/* _norm -- str(value or "").strip().lower(). Caller frees. */
char *smc_norm(const char *value);
const char *smc_client_os_family(const char *value);
const char *smc_client_architecture(const char *value);

/* ---- normalisation half --------------------------------------------- */

/* _allowlisted -- the matching STATIC entry from `allowed`, else "unknown".
 * The result never points at `normalized`, so it outlives the caller's free. */
const char *smc_allowlisted(const char *normalized, const char *const *allowed, size_t n);

const char *smc_client_install_method(const char *value);
const char *smc_skill_provenance(const char *value);

/* client_resource -- caller owns the returned json_t (json_free). */
json_t *smc_client_resource(const char *hermes_version, const char *os_name,
                            const char *architecture, const char *install_method);
int smc_client_resource_is_valid(json_t *resource);

/* _metric_identifier -- caller frees. `value` NULL reads as "unknown". */
char *smc_metric_identifier(const char *value, size_t max_length);

/* kwargs-driven helpers: `kwargs` is a JSON object (as the emitters build it). */
const char *smc_tool_category(json_t *kwargs);
const char *smc_tool_outcome(json_t *kwargs);
const char *smc_model_error_class(json_t *kwargs);

#endif /* PORT_SHARED_METRICS_CONTRACT_H */
