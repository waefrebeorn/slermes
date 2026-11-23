#ifndef NOC_W12_UIPREFEREN_26932E_H
#define NOC_W12_UIPREFEREN_26932E_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_uipreferen_26932e_sanitize @ hermes_cli/ui_preferences.py:_sanitize
 * Keep only entries whose key and value are both strings; non-dict -> {}. */
struct json_t *w12_uipreferen_26932e_sanitize(const struct json_t *values);

/* PoP: w12_uipreferen_26932e_merge @ hermes_cli/ui_preferences.py:_merge
 * Per-key merge: None deletes, seed only adds missing keys.  Returns a fresh
 * object with current's key order preserved. */
struct json_t *w12_uipreferen_26932e_merge(const struct json_t *current,
                                           const struct json_t *changes,
                                           int seed);

/* PoP: w12_uipreferen_26932e_validate @ hermes_cli/ui_preferences.py:_validate
 * 1 when all changes are acceptable, else 0 with the PreferencesError message
 * written to errbuf.  Mirrors KEY_PREFIXES / MAX_KEY_CHARS / MAX_VALUE_CHARS. */
int w12_uipreferen_26932e_validate(const struct json_t *changes,
                                   char *errbuf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_UIPREFEREN_26932E_H */
