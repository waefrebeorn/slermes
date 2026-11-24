/*
 * noc_w28_configenv_e9e22f_p1.h — C surface for upstream
 * hermes_cli/web_routers/config_env.py (gate w28_configenv_e9e22f_p1).
 *
 * The env-var / Keys-tab half of the dashboard config router: the redaction
 * sentinel the API hands the SPA, the predicate that refuses a masked preview
 * as a write, the whole GET /api/env row build (hand prose vs catalog
 * metadata, channel-managed hide, custom-key pass) and the small pure value
 * helpers the custom-endpoint routes share (id slug, model list, key display,
 * transport pin, provider-name match).
 *
 * Seams: .env on disk (load_env), the profile scope lock, the provider
 * catalog scan and the live provider-catalog descriptors; everything here is
 * what the values decide once those have answered.
 */
#ifndef NOC_W28_CONFIGENV_E9E22F_P1_H
#define NOC_W28_CONFIGENV_E9E22F_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* value -> "«redacted:<masked>»" (mask_secret head4/tail4/floor12), null when
 * the value is falsy.  redacted_credential_preview() in web_routers/_common. */
json_t *w28_configenv_e9e22f_p1_env_preview(const json_t *value);

/* submitted -> is_redacted_credential_preview(): "«redacted…", "***" or the
 * legacy 4+"..."+4 mask shape. */
json_t *w28_configenv_e9e22f_p1_is_redacted_preview(const json_t *submitted);

/* (OPTIONAL_ENV_VARS, catalog metadata, .env values, channel-managed keys) ->
 * the GET /api/env map: hand entries, catalog-only entries, custom keys. */
json_t *w28_configenv_e9e22f_p1_env_vars(const json_t *optional_env_vars,
                                         const json_t *catalog_meta,
                                         const json_t *env_on_disk,
                                         const json_t *channel_keys);

/* _env_write_errors(): kind "value_error" -> 400 + message, "http" -> the
 * exception's own status/detail, anything else -> 500 "Internal server error".
 * Returns {"ok":false,"status":<int>,"detail":<string>}. */
json_t *w28_configenv_e9e22f_p1_env_error(const json_t *kind, const json_t *message,
                                          const json_t *status, const json_t *detail);

/* _custom_endpoint_id(): slug a provider id ([^A-Za-z0-9_-]+ -> "-", strip "-_",
 * lower), fallback when the slug is empty. */
json_t *w28_configenv_e9e22f_p1_custom_endpoint_id(const json_t *raw, const char *fallback);

/* _models_from_custom_endpoint_entry(): model/default_model first, then the
 * declared models (array items or mapping keys), stripped and de-duplicated. */
json_t *w28_configenv_e9e22f_p1_models_from_entry(const json_t *entry);

/* _api_key_display(): [has_api_key, preview] from api_key then key_env. */
json_t *w28_configenv_e9e22f_p1_api_key_display(const json_t *entry);

/* _endpoint_api_mode(): a pinned desktop transport, or "" for auto-detect. */
json_t *w28_configenv_e9e22f_p1_endpoint_api_mode(const json_t *entry);

/* _model_names_provider(): does model.provider name this providers entry? */
json_t *w28_configenv_e9e22f_p1_model_names_provider(const json_t *model_cfg,
                                                     const json_t *provider_key,
                                                     const json_t *entry);

#ifdef __cplusplus
}
#endif
#endif
