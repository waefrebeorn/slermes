#ifndef NOC_W6_I18NLayers_F42323_H
#define NOC_W6_I18NLayers_F42323_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_i18nlayers_f42323_normalize_language_id @ agent/i18n_layers.py:normalize_language_id */
char *w6_i18nlayers_f42323_normalize_language_id(const struct json_t *value);

/* PoP: w6_i18nlayers_f42323_is_language_id @ agent/i18n_layers.py:is_language_id */
int w6_i18nlayers_f42323_is_language_id(const struct json_t *value);

/* PoP: w6_i18nlayers_f42323_flatten @ agent/i18n_layers.py:flatten */
struct json_t *w6_i18nlayers_f42323_flatten(const struct json_t *node);

/* PoP: w6_i18nlayers_f42323_non_text_leaves @ agent/i18n_layers.py:non_text_leaves */
struct json_t *w6_i18nlayers_f42323_non_text_leaves(const struct json_t *node);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_I18NLayers_F42323_H */
