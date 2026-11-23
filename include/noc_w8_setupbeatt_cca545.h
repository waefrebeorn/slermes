#ifndef NOC_W8_SETUPBEATT_CCA545_H
#define NOC_W8_SETUPBEATT_CCA545_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_setupbeatt_cca545_suggest_from @ tools/setup_beat_tool.py:_suggest_from
 * The card's "only these slugs" suffix; "" when slugs is null/empty.
 * Caller frees. */
char *w8_setupbeatt_cca545_suggest_from(const struct json_t *slugs);

/* PoP: w8_setupbeatt_cca545_unlisted_suggestions @ tools/setup_beat_tool.py:_unlisted_suggestions
 * Refusal JSON string for connector suggestions the card cannot show, or NULL.
 * `slugs` is an explicit seam for _connector_slugs() (array of slugs or null).
 * Caller frees. */
char *w8_setupbeatt_cca545_unlisted_suggestions(const struct json_t *args,
        const struct json_t *slugs);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_SETUPBEATT_CCA545_H */
