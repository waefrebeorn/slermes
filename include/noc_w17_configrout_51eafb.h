#ifndef NOC_W17_CONFIGROUT_51EAFB_H
#define NOC_W17_CONFIGROUT_51EAFB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/config_route_guard.py (gate w17_configrout_51eafb).
 * Set-time stale-route guard helpers. normalize_route_base_url and
 * same_base_url_origin are mirrored from route_identity.py / utils/urls.py;
 * credential clearing mirrors config.clear_model_endpoint_credentials. */
json_t *w17_configrout_51eafb_route_of(const char *provider, const char *base_url);
json_t *w17_configrout_51eafb_url(const char *value);
json_t *w17_configrout_51eafb_clear_aux_credentials(const json_t *entry,
                                                    const char *provider,
                                                    const char *base_url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_CONFIGROUT_51EAFB_H */
