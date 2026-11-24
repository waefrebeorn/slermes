#ifndef NOC_W16_METHODSPRO_1E5967_H
#define NOC_W16_METHODSPRO_1E5967_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_methodspro_1e5967_model_provider_params @ tui_gateway/methods_profiles.py:_model_provider_params */
struct json_t *w16_methodspro_1e5967_model_provider_params(const struct json_t *params);
/* PoP: w16_methodspro_1e5967_clean_revisions @ tui_gateway/methods_profiles.py:_clean_revisions */
struct json_t *w16_methodspro_1e5967_clean_revisions(const struct json_t *raw);
/* PoP: w16_methodspro_1e5967_clean_names @ tui_gateway/methods_profiles.py:_clean_names */
struct json_t *w16_methodspro_1e5967_clean_names(const struct json_t *values);
/* PoP: w16_methodspro_1e5967_bots_title @ tui_gateway/methods_profiles.py:_bots_title */
struct json_t *w16_methodspro_1e5967_bots_title(const struct json_t *ui_meta);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_METHODSPRO_1E5967_H */
