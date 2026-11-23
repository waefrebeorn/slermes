#ifndef NOC_W13_CONFIGLANG_CBD080_H
#define NOC_W13_CONFIGLANG_CBD080_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_configlang_cbd080_display_language_error @ hermes_cli/config_language.py:display_language_error
 * Exact refusal copy of the live function. The two import/env-driven dependencies are
 * injected as seams: `resolved` = resolve_display_language(value), `supported` = the
 * supported_languages() array. Returns JSON NULL when the value is acceptable. */
struct json_t *w13_configlang_cbd080_display_language_error(
        const struct json_t *value, const struct json_t *resolved,
        const struct json_t *supported);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_CONFIGLANG_CBD080_H */
