/*
 * port_models_validate.h — public API for the model-validation port of
 * hermes_cli/models.py:validate_requested_model (+ ensure_lmstudio_model_loaded).
 *
 * All external surface is INJECTED (HTTP transport, MoA-preset resolver,
 * catalog resolver) so the orchestration logic is faithfully testable
 * offline.  No stubs.
 */

#ifndef PORT_MODELS_VALIDATE_H
#define PORT_MODELS_VALIDATE_H

#include <stddef.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "port_models_net.h"   /* http_fetch_fn */

#ifdef __cplusplus
extern "C" {
#endif

/* Injectable HTTP POST transport for LM Studio model loading.
 * Returns 0 on success, non-zero on error. */
typedef int (*http_post_fn)(const char *url,
                             const char *headers_json,
                             const char *body_json,
                             void *ctx);

/* MoA preset resolver: returns a malloc'd NULL-terminated array of preset
 * names (caller frees each + array), or NULL. Injected (NULL → no MoA). */
typedef char **(*moa_presets_fn)(void *ctx);

/* Catalog resolver: given a normalized provider slug, returns a malloc'd
 * JSON array-of-strings (caller frees) of that provider's model ids, or NULL.
 * Injected (NULL → no catalog). Used for codex/oauth/minimax/bedrock/unreachable. */
typedef char *(*catalog_resolver_fn)(void *ctx, const char *provider);

/* Result of validate_requested_model. Caller frees message/corrected_model
 * via models_validate_result_free(). */
typedef struct {
    int    accepted;
    int    persist;
    int    recognized;
    char  *message;          /* malloc'd or NULL */
    char  *corrected_model;  /* malloc'd or NULL (auto-correct suggestion) */
} models_validate_result_t;

void models_validate_result_free(models_validate_result_t *r);

/* Faithful port of models.py:ensure_lmstudio_model_loaded. Probes LM Studio,
 * (re)loads `model` with target context via injected POST, returns the
 * resolved loaded context length or -1 on failure. `fetch` is used for the
 * probe; `post` for the load. */
int models_ensure_lmstudio_model_loaded(http_fetch_fn fetch, void *fetch_ctx,
                                         http_post_fn post, void *post_ctx,
                                         const char *model, const char *base_url,
                                         const char *api_key,
                                         int target_context_length, double timeout);

/* Faithful port of models.py:validate_requested_model. Returns the verdict
 * (caller frees via models_validate_result_free). All network/config surface
 * is injected. */
models_validate_result_t models_validate_requested_model(
    http_fetch_fn fetch, void *ctx,
    const char *model_name, const char *provider,
    const char *api_key, const char *base_url, const char *api_mode,
    moa_presets_fn moa_resolve, void *moa_ctx,
    catalog_resolver_fn catalog_resolve, void *cat_ctx);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODELS_VALIDATE_H */
