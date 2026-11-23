#ifndef NOC_W8_DOCTORLIVE_4EF4C4_H
#define NOC_W8_DOCTORLIVE_4EF4C4_H
#include "hermes_json.h"

/* C home for upstream Hermes hermes_cli/doctor_live.py (gate w8_doctorlive_4ef4c4).
 *
 * PoP: w8_doctorlive_classify_http_ex @ hermes_cli/doctor_live.py:_classify_http
 *
 * Faithful port of _classify_http(name, resp, key_hint).  `code` is the
 * response status code as a decimal string (or NULL for a response with no
 * status_code, i.e. Python None).  Returns a JSON array:
 *   [name, "pass"|"fail", detail]
 * where detail is "(HTTP <code>)" for 2xx, "(HTTP <code> — check <hint>)"
 * for 401/403, "(HTTP <code>)" for other codes, and "(HTTP None)" for NULL.
 */
json_t *w8_doctorlive_classify_http_ex(const char *name, const char *code, const char *key_hint);

#endif /* NOC_W8_DOCTORLIVE_4EF4C4_H */
