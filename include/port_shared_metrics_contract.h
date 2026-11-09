/*
 * port_shared_metrics_contract.h -- pure helpers ported from
 * hermes_cli/observability/shared_metrics_contract.py.
 */
#ifndef PORT_SHARED_METRICS_CONTRACT_H
#define PORT_SHARED_METRICS_CONTRACT_H

#include <stddef.h>

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

#endif /* PORT_SHARED_METRICS_CONTRACT_H */
