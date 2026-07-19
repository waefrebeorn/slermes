/*
 * timeouts_helpers.h — public API for the pure hermes_cli/timeouts.py
 * helpers. Opaque, minimal includes (forward-declares json_t).
 */

#ifndef TIMEOUTS_HELPERS_H
#define TIMEOUTS_HELPERS_H

#include <stddef.h>

typedef struct json_t json_t;

/* PoP: _coerce_timeout @ hermes_cli/timeouts.py:_coerce_timeout
 * Coerce a raw timeout string to a positive double, else -1.0 (None). */
double timeouts_coerce_timeout(const char *raw);

/* PoP: _get_model_config @ hermes_cli/timeouts.py:_get_model_config
 * Given a provider_config JSON object + optional model name, return the model's
 * config object (borrowed from config), or NULL if absent. Does NOT free. */
const json_t *timeouts_get_model_config(const json_t *provider_config,
                                       const char *model);

/* PoP: get_provider_request_timeout @ hermes_cli/timeouts.py */
double timeouts_get_provider_request_timeout(const char *config_json,
                                            const char *provider_id,
                                            const char *model);

/* PoP: get_provider_stale_timeout @ hermes_cli/timeouts.py */
double timeouts_get_provider_stale_timeout(const char *config_json,
                                          const char *provider_id,
                                          const char *model);

#endif /* TIMEOUTS_HELPERS_H */
