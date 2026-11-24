#ifndef NOC_W19_SESSIONMOD_09F8F1_H
#define NOC_W19_SESSIONMOD_09F8F1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_sessionmod_09f8f1_display_base_url @ gateway/platforms/session_model.py:_display_base_url
 * Endpoint path without userinfo, query or fragment credentials. Returns a malloc'd
 * string the caller frees; an unusable URL yields "" (never NULL). */
char *w19_sessionmod_09f8f1_display_base_url(const char *value);

/* PoP: w19_sessionmod_09f8f1_resolve_settings @ gateway/platforms/session_model.py:_resolve_session_model_settings
 * Raw presentation inputs only. `model_cfg` is cfg["model"] before the isinstance dict
 * coercions; `override`/`store_override` are the two override sources (either may be
 * NULL); `has_channel`/`channel_model`/`channel_provider` model the channel override.
 * Returns {"session_key","model","provider","base_url"}. */
json_t *w19_sessionmod_09f8f1_resolve_settings(json_t *model_cfg, const char *key,
                                               json_t *override, json_t *store_override,
                                               int has_channel, const char *channel_model,
                                               const char *channel_provider);

/* PoP: w19_sessionmod_09f8f1_settings_display @ gateway/platforms/session_model.py:_SessionModelSettingsMixin.get_session_model_settings
 * The settings record with base_url passed through _display_base_url. */
json_t *w19_sessionmod_09f8f1_settings_display(json_t *settings);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_SESSIONMOD_09F8F1_H */
