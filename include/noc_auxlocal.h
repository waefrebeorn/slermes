#ifndef NOC_AUXLOCAL_H
#define NOC_AUXLOCAL_H
/*
 * Public C API for the port of agent/auxiliary_local_runtime.py (gate auxlocal).
 * All exported symbols are module-prefixed to avoid collisions.
 */
/*
 * Decision core of bare_llamacpp_endpoint.
 *
 * Live resolution of the managed server (state file, process detection, config)
 * cannot be performed from a pure C port. The caller supplies the ALREADY
 * RESOLVED facts: alias membership is decided here from the literal
 * LLAMACPP_ALIASES set; named_custom_exists models
 * _get_named_custom_provider(alias); endpoint_present/ep_base_url/ep_api_key
 * model resolve_llamacpp_endpoint(...) or {}.
 *
 * provider/base_url_in/api_key are strings or NULL (= Python None).
 * Returns json_null() when the request is not a bare managed alias, otherwise
 * a JSON array [base_url, api_key] (caller frees). No endpoint is fabricated.
 */
#include "hermes_json.h"

json_t *auxlocal_bare_llamacpp_endpoint(const char *provider,
        const char *base_url_in, const char *api_key,
        int named_custom_exists,
        int endpoint_present, const char *ep_base_url, const char *ep_api_key);

#endif /* NOC_AUXLOCAL_H */
