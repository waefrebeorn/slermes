/*
 * error_surface.h — C11 port of agent/error_surface.py.
 *
 * Structured error-surface descriptors for UI clients (Desktop/TUI).
 * Dependency-light and NEVER raises: an absent/partial descriptor falls back
 * to the caller's string-sniffing behavior.
 */
#ifndef SLERMES_ERROR_SURFACE_H
#define SLERMES_ERROR_SURFACE_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UI layers (wire values — stable contract with desktop/TUI clients). */
#define ES_LAYER_PROVIDER  "provider"
#define ES_LAYER_ENDPOINT  "endpoint"
#define ES_LAYER_STREAMING "streaming"
#define ES_LAYER_AUTH      "auth"
#define ES_LAYER_BILLING   "billing"
#define ES_LAYER_GATEWAY   "gateway"
#define ES_LAYER_RUNTIME   "runtime"
#define ES_LAYER_DISK      "disk"

/*
 * build_error_surface_from_result @ agent/error_surface.py:
 * build_error_surface_from_result
 *
 * Descriptor {"layer","code","retryable"[,"provider","model"]} for a
 * returned-error turn result (``failed=True`` dict). Returns a NEW json
 * object (caller frees) or NULL when the result carries no failure signal.
 */
json_t *es_build_error_surface_from_result(const json_t *result,
                                           const char *provider,
                                           const char *model);

/*
 * build_error_surface_from_exception @ agent/error_surface.py:
 * build_error_surface_from_exception
 *
 * C adaptation of the exception path: the escaped exception is described by
 * its module ("openai", "requests", ...), type name, str() message, whether
 * it carries a status_code attribute, and that code. API/transport-shaped
 * exceptions are classified through the real classify_api_error pipeline;
 * anything else is a gateway-layer failure. Returns NEW json object (caller
 * frees) or NULL.
 */
json_t *es_build_error_surface_from_exception(const char *exc_module,
                                              const char *exc_type,
                                              const char *message,
                                              int has_status_code,
                                              int status_code,
                                              const char *provider,
                                              const char *model);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_ERROR_SURFACE_H */
