#ifndef NOC_W11_MODELMETAD_06AB9B_H
#define NOC_W11_MODELMETAD_06AB9B_H
#include "hermes_json.h"

/* PoP port of agent/model_metadata_http.py (gate w11_modelmetad_06ab9b). */
/* Injected seams: settings = get_custom_provider_tls_settings(base_url) result;
 * settings_raise models that lookup raising (swallowed by Python). */
json_t *w11_modelmetad_06ab9b_resolve_verify(const char *base_url, json_t *settings, int settings_raise);
double  w11_modelmetad_06ab9b_total_deadline_s(void);
/* Credential scoping from stream(): headers left after an optional cross-origin redirect. */
json_t *w11_modelmetad_06ab9b_scope_headers(const char *initial_url, const char *request_url, json_t *headers);
#endif /* NOC_W11_MODELMETAD_06AB9B_H */
