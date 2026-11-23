#ifndef NOC_W11_MODELCATAL_563C9E_H
#define NOC_W11_MODELCATAL_563C9E_H

/* C API for the acp_adapter/model_catalog.py port (gate w11_modelcatal_563c9e). */
#include "hermes_json.h"

/* encode_model_choice(provider, model) -> malloc'd str */
char *w11_modelcatal_563c9e_encode_model_choice(const char *provider, const char *model);
/* _semantic_provider(provider_id, normalize_provider) -> malloc'd str.
 * normalize_provider is injected as its result for the lower/stripped raw id. */
char *w11_modelcatal_563c9e_semantic_provider(const char *provider_id, const char *normalized);
/* _empty_catalog_applies(provider_id, empty_authoritative, normalize_provider) -> bool.
 * norm_map maps raw strings to their normalize_provider() result (identity by default). */
bool w11_modelcatal_563c9e_empty_catalog_applies(const char *provider_id,
                                                  const json_t *candidates,
                                                  const json_t *norm_map);

#endif /* NOC_W11_MODELCATAL_563C9E_H */
