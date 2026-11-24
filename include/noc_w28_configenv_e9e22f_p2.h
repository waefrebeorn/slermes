/*
 * noc_w28_configenv_e9e22f_p2.h — C surface for upstream
 * hermes_cli/web_routers/config_env.py (gate w28_configenv_e9e22f_p2).
 *
 * The config-payload half of the dashboard config router: the GET
 * /api/config strip, the custom-endpoint listing payload, the entry
 * resolution / legacy-pop / main-slot detach edits the delete+save routes do
 * on the in-memory config document, the create/update validation preamble and
 * the "200 but no models" probe message.
 *
 * Seams: config.yaml read/write, the credential store, the pool, the HTTP
 * probe and (for the legacy `custom_providers:` list) the normalization in
 * config_providers.get_compatible_custom_providers; this gate ports what the
 * in-memory document decides.
 */
#ifndef NOC_W28_CONFIGENV_E9E22F_P2_H
#define NOC_W28_CONFIGENV_E9E22F_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GET /api/config: drop every internal top-level key the SPA must never see. */
json_t *w28_configenv_e9e22f_p2_strip_internal(const json_t *config);

/* args {id,name,base_url,model,models,context_length,discover_models,
 * key_entry,is_current,source} -> _endpoint_row(). */
json_t *w28_configenv_e9e22f_p2_endpoint_row(const json_t *args);

/* _custom_endpoint_response(): {"endpoints":[...], "current":{...}} from a
 * config document (the `providers:` map plus the bare-custom direct-config row). */
json_t *w28_configenv_e9e22f_p2_custom_endpoint_response(const json_t *cfg);

/* _pop_legacy_custom_provider() -> {"cfg": <after>, "entry": <popped>|null}. */
json_t *w28_configenv_e9e22f_p2_pop_legacy_provider(const json_t *cfg,
                                                    const json_t *provider_key);

/* _detach_main_model_from_provider() -> the config after the main-slot mirror
 * of a deleted provider is dropped. */
json_t *w28_configenv_e9e22f_p2_detach_main_model(const json_t *cfg,
                                                  const json_t *provider_key,
                                                  const json_t *entry);

/* _drop_stale_addresses(): base_url is the address; drop url/api/baseUrl. */
json_t *w28_configenv_e9e22f_p2_drop_stale_addresses(const json_t *entry);

/* _resolve_custom_endpoint_entry() -> [stored_key, entry] or [null, null]. */
json_t *w28_configenv_e9e22f_p2_resolve_entry(const json_t *providers,
                                              const json_t *endpoint_id);

/* The _write_custom_endpoint() preamble: name/base_url/model required, base_url
 * needs scheme + host.  Success -> {"ok":true,"value":{name,base_url,model}}
 * (stripped, trailing "/" gone); failure -> {"ok":false,"status":400,"detail":…}. */
json_t *w28_configenv_e9e22f_p2_validate_endpoint(const json_t *name,
                                                  const json_t *base_url,
                                                  const json_t *model);

/* _no_models_probe_message(): req {base_url, content_type, payload_ok, payload}. */
json_t *w28_configenv_e9e22f_p2_no_models_message(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
