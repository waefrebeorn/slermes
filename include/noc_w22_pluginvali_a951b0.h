#ifndef NOC_W22_PLUGINVALI_A951B0_H
#define NOC_W22_PLUGINVALI_A951B0_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/plugin_validate_locales.py (gate w22_pluginvali_a951b0).
 *
 * The document read (`_load_document`), the bundled English reference set
 * (`reference_keys`) and the report sink are host seams: the caller injects the
 * parsed document, the reference keys and the parse failure, and the module's
 * admission decisions and messages are built here. The scan of locales/ and the
 * `hermes plugins validate` driver stay upstream.
 */

/* PoP: w22_pluginvali_a951b0_locale_label @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 * "locale <id>" for the core surface, "locale <id>.<surface>" otherwise.
 * Caller frees. */
char *w22_pluginvali_a951b0_locale_label(const char *lang_id, const char *surface);

/* PoP: w22_pluginvali_a951b0_flatten_keys @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 * `flatten(document)`'s dotted keys, sorted. A non-object document flattens to {}. */
json_t *w22_pluginvali_a951b0_flatten_keys(const json_t *document);

/* PoP: w22_pluginvali_a951b0_non_text_leaves @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 * Dotted paths of leaves that are neither text nor a mapping, sorted; "<root>"
 * when the document itself is such a leaf. */
json_t *w22_pluginvali_a951b0_non_text_leaves(const json_t *document);

/* PoP: w22_pluginvali_a951b0_known_ids @ hermes_cli/plugin_validate_locales.py:check_language_packs
 * `parse_provides_locales(raw, key)[0]`: one id or a list of
 * "pl" / {"id": ...} entries, canonicalised (lower-case, '_' -> '-'),
 * invalid and duplicate ids dropped, order preserved. null/absent -> []. */
json_t *w22_pluginvali_a951b0_known_ids(const json_t *raw);

/* PoP: w22_pluginvali_a951b0_check_document @ hermes_cli/plugin_validate_locales.py:_check_locale_file
 * The per-file verdict: {"label", "ok", "detail", "warnings"}. `document` is the
 * parsed YAML (null = an empty document), `parse_error` non-null reproduces the
 * failure branch (its text is str(exc)), and `reference` null means the key
 * reference is unavailable so the key check is skipped, never failed. */
json_t *w22_pluginvali_a951b0_check_document(const json_t *document, const char *lang_id,
                                             const char *surface, const json_t *reference,
                                             const char *filename, const char *parse_error);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_PLUGINVALI_A951B0_H */
