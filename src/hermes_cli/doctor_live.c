/* doctor_live.c — C home for upstream Hermes hermes_cli/doctor_live.py.
 *
 * PoP: w8_doctorlive_classify_http_ex @ hermes_cli/doctor_live.py:_classify_http
 *
 * Faithful port of _classify_http(name, resp, key_hint):
 *   code = getattr(resp, "status_code", None)
 *   if code is not None and 200 <= code < 300:
 *       return ProbeResult(name, "pass", f"(HTTP {code})")
 *   return ProbeResult(name, "fail",
 *       f"(HTTP {code} — check {key_hint})" if code in (401, 403)
 *       else f"(HTTP {code})")
 * `code` is injected as a decimal string (or NULL == Python None) so no live
 * HTTP response object has to cross the boundary.
 */
#include "noc_w8_doctorlive_4ef4c4.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *w8_doctorlive_classify_http_ex(const char *name, const char *code, const char *key_hint) {
    const char *nm = name ? name : "";
    const char *hint = key_hint ? key_hint : "";
    int has = (code != NULL);
    long long c = has ? strtoll(code, NULL, 10) : 0;
    const char *status;
    char detail[4096];

    if (has && c >= 200 && c < 300) {
        status = "pass";
        snprintf(detail, sizeof(detail), "(HTTP %lld)", c);
    } else {
        status = "fail";
        if (has && (c == 401 || c == 403))
            snprintf(detail, sizeof(detail), "(HTTP %lld \xE2\x80\x94 check %s)", c, hint);
        else if (has)
            snprintf(detail, sizeof(detail), "(HTTP %lld)", c);
        else
            snprintf(detail, sizeof(detail), "(HTTP None)");
    }

    json_t *out = json_array();
    json_append(out, json_string(nm));
    json_append(out, json_string(status));
    json_append(out, json_string(detail));
    return out;
}

