#ifndef NOC_W12_MODELS_PRIC_4EACB7_H
#define NOC_W12_MODELS_PRIC_4EACB7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_modelspric_4eacb7_strip_v1 @ hermes_cli/models_pricing.py:_strip_v1 */
char *w12_modelspric_4eacb7_strip_v1(const char *url);

/* PoP: w12_modelspric_4eacb7_format_price_per_mtok @ hermes_cli/models_pricing.py:_format_price_per_mtok */
char *w12_modelspric_4eacb7_format_price_per_mtok(const char *per_token_str);

/* PoP: w12_modelspric_4eacb7_price_float @ hermes_cli/models_pricing.py:_price_float */
struct json_t *w12_modelspric_4eacb7_price_float(const struct json_t *raw, int positive);

/* PoP: w12_modelspric_4eacb7_sale_pct @ hermes_cli/models_pricing.py:_sale_pct */
struct json_t *w12_modelspric_4eacb7_sale_pct(const struct json_t *current, const struct json_t *original);

/* PoP: w12_modelspric_4eacb7_compute_sale_discount @ hermes_cli/models_pricing.py:compute_sale_discount */
struct json_t *w12_modelspric_4eacb7_compute_sale_discount(const struct json_t *prompt,
                                                           const struct json_t *completion,
                                                           const struct json_t *original);

/* PoP: w12_modelspric_4eacb7_pricing_entry @ hermes_cli/models_pricing.py:_pricing_entry */
struct json_t *w12_modelspric_4eacb7_pricing_entry(const struct json_t *pricing,
                                                   const char *prompt_key,
                                                   const char *completion_key);

/* PoP: w12_modelspric_4eacb7_per_token @ hermes_cli/models_pricing.py:_per_token */
char *w12_modelspric_4eacb7_per_token(const struct json_t *per_mtok);

/* PoP: w12_modelspric_4eacb7_catalog_items @ hermes_cli/models_pricing.py:_catalog_items */
struct json_t *w12_modelspric_4eacb7_catalog_items(const struct json_t *payload);

/* PoP: w12_modelspric_4eacb7_restrict_to_nous_policy @ hermes_cli/models_pricing.py:restrict_to_nous_policy */
struct json_t *w12_modelspric_4eacb7_restrict_to_nous_policy(const struct json_t *model_ids,
                                                             const struct json_t *allowed,
                                                             int rescue_empty);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_MODELS_PRIC_4EACB7_H */
