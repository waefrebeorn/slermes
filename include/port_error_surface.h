#ifndef PORT_ERROR_SURFACE_H
#define PORT_ERROR_SURFACE_H

#include "hermes_json.h"

/* Port of Python: agent/error_surface.py
 *
 * Maps the internal failure taxonomy onto the stable wire descriptor
 * {"layer","code","retryable"} (+ provider/model identity) that UI clients
 * consume. Pure classification; never raises.
 *
 * The two advisory lookups `_provider_label` and `auth_kind`/`_api_key_env`
 * read hermes_cli.models / hermes_cli.provider_catalog, which have no
 * comparable C registry yet, so this port takes their results as parameters:
 * pass NULL for provider/model-only descriptors, or the resolved strings when
 * the caller can supply them.
 */

#define ESC_LAYER_PROVIDER  "provider"
#define ESC_LAYER_ENDPOINT  "endpoint"
#define ESC_LAYER_STREAMING "streaming"
#define ESC_LAYER_AUTH      "auth"
#define ESC_LAYER_BILLING   "billing"
#define ESC_LAYER_GATEWAY   "gateway"
#define ESC_LAYER_DISK      "disk"

/* failure_reason -> layer; unlisted reasons fall back to provider unless the
 * text looks like a stream drop. */
const char *esc_result_layer(const char *reason, const char *error_text,
                            const char *provider);

int esc_is_custom_endpoint(const char *provider);
int esc_looks_like_stream_drop(const char *message);
int esc_is_non_retryable_reason(const char *reason);

/* Mirrors hermes_state_errors.is_disk_full_error for string input. */
int esc_disk_full(const char *text);

/* Build one descriptor. layer/code are required; provider/model/label/
 * auth_kind/api_key_env may be NULL and are then omitted. Caller owns the
 * returned object. */
json_t *esc_surface(const char *layer, const char *code, int retryable,
                    const char *provider, const char *model,
                    const char *provider_label, const char *auth_kind,
                    const char *api_key_env);

/* Descriptor for a returned-error turn result (a JSON object), or NULL when
 * the result carries no failure signal. `result_json` may be any JSON value;
 * non-objects yield NULL. */
json_t *esc_build_from_result(const char *result_json, const char *provider,
                              const char *model, const char *provider_label,
                              const char *auth_kind, const char *api_key_env);

/* build_error_surface_from_exception is NOT ported: it runs
 * classify_api_error and re-keys its result into the descriptor. The C
 * classifier (lib/liberrorclassifier) speaks failover_reason_t, and
 * mapping that enum back onto the Python reason strings is its own bite.
 * Inventing an "unknown" descriptor here would report the wrong layer. */

#endif /* PORT_ERROR_SURFACE_H */
