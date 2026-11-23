#ifndef NOC_JOBSPIN_H
#define NOC_JOBSPIN_H
#include "hermes_json.h"

/* C port of cron/jobs_model_pin.py:_main_model_pin's pure decision logic.
 * Returns the Python (provider, model) tuple as a 2-element JSON array whose
 * members are strings or null. `cfg` is the effective config object;
 * `provider_raw` is the (already resolved) provider value, NULL when that
 * resolution was unavailable/suppressed. Caller owns the returned json_t. */
json_t *jobspin_main_model_pin(const json_t *cfg, const char *provider_raw);
#endif
