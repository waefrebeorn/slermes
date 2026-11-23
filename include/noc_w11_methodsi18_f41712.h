/* noc_w11_methodsi18_f41712.h — public C API for the faithful port of
 * upstream Hermes tui_gateway/methods_i18n.py (i18n.languages / i18n.catalog).
 *
 * PoP: w11_methodsi18_f41712_languages @ tui_gateway/methods_i18n.py:_
 * PoP: w11_methodsi18_f41712_catalog  @ tui_gateway/methods_i18n.py:_
 */
#ifndef NOC_W11_METHODSI18_F41712_H
#define NOC_W11_METHODSI18_F41712_H

#include "hermes_json.h"

/* i18n.languages body: _ok(rid, {"languages": language_options()}) or
 * _err(rid, 5400, str(e)). fail_message non-null/non-empty models the raise. */
json_t *w11_methodsi18_f41712_languages(const char *rid, json_t *languages,
                                         const char *fail_message);

/* i18n.catalog body: surface coercion/validation, lang fallback, _ok/_err.
 * surfaces: JSON array of allowed surface names; core_surface: fallback;
 * resolve_result: resolved language id (null/empty -> "en");
 * messages: surface_catalog(lang, surface) result;
 * fail_message: str(e) for the 5400 error path (null = no exception). */
json_t *w11_methodsi18_f41712_catalog(const char *rid, json_t *params,
                                       json_t *surfaces, const char *core_surface,
                                       const char *resolve_result, json_t *messages,
                                       const char *fail_message);

/* Module constant. */
const char *w11_methodsi18_f41712_i18n_err(void);

#endif /* NOC_W11_METHODSI18_F41712_H */
