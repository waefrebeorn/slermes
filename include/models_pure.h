#ifndef HERMES_MODELS_PURE_H
#define HERMES_MODELS_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic model-catalog helpers ported from hermes_cli/models.py.
 * All return freshly malloc'd JSON strings (free with free()) or NULL. */

char *models_xai_finalize_catalog(const char *json_ids);
int   models_is_nous_free_tier(const char *json_account_info);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MODELS_PURE_H */
