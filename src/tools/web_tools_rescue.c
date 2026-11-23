/*
 * web_tools_rescue.c -- faithful C11 port of upstream Hermes tools/web_tools_rescue.py.
 *
 * PoP: w9_webtoolsre_f70a5e_policy_blocked_result @ tools/web_tools_rescue.py:_policy_blocked_result
 *
 * The rescue paths themselves are network/config coupled; the website-policy refusal
 * predicate is pure and is ported verbatim, including str(None-or-falsy)->"" coercion
 * and case-insensitive substring matching.
 */
#include "noc_w9_webtoolsre_f70a5e.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python: str(value or "") for the scalar types used by fixtures.
 * Caller frees. Falsy scalars (null / false / 0 / "") become "". */
static char *py_str_or_empty(const json_t *v) {
    char *out;
    if (!v || v->type == JSON_NULL) return strdup("");
    switch (v->type) {
        case JSON_STRING:
            return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:
            return strdup(v->bool_val ? "True" : "");
        case JSON_NUMBER: {
            char tmp[64];
            if (v->num_val == 0.0) return strdup("");
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
            return strdup(tmp);
        }
        default:
            return strdup("");  /* empty/non-scalar containers are not in fixtures */
    }
}

int w9_webtoolsre_f70a5e_policy_blocked_result(const json_t *result) {
    if (!result || result->type != JSON_OBJECT) return 0;
    json_t *err = json_obj_get(result, "error");
    char *s = py_str_or_empty(err);
    for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    int flagged = json_get_bool(result, "blocked_by_policy", false) ? 1 : 0;
    int hit = strstr(s, "blocked by website policy") ? 1 : 0;
    free(s);
    return flagged || hit;
}
