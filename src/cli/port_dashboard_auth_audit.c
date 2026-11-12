/*
 * port_dashboard_auth_audit.c — Port of
 * hermes_cli/dashboard_auth/audit.py::_bounded_value:
 * recursively bound strings in a JSON-shaped value (audit-log field cap).
 * Values that are not str/int/float/bool/None become repr() in Python; the C
 * side only ever sees parsed JSON, so that branch is unreachable here.
 */

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* PoP: dab_bounded_value @ hermes_cli/dashboard_auth/audit.py:_bounded_value */
json_t *dab_bounded_value(const json_t *value, int max_len, const char *marker)
{
    if (!value)
        return json_new_null();
    switch (value->type) {
    case JSON_OBJECT: {
        json_t *out = json_new_object();
        if (!out)
            return NULL;
        for (size_t i = 0; i < value->c.count; i++) {
            json_t *bounded = dab_bounded_value(value->c.items[i], max_len, marker);
            if (bounded)
                json_object_set(out, value->c.keys[i], bounded);
        }
        return out;
    }
    case JSON_ARRAY: {
        json_t *out = json_new_array();
        if (!out)
            return NULL;
        for (size_t i = 0; i < value->c.count; i++) {
            json_t *bounded = dab_bounded_value(value->c.items[i], max_len, marker);
            if (bounded)
                json_array_append(out, bounded);
        }
        return out;
    }
    case JSON_STRING: {
        const char *s = value->str_val ? value->str_val : "";
        size_t len = strlen(s);
        if (max_len > 0 && marker && (int)len > max_len) {
            size_t mlen = strlen(marker);
            size_t keep = ((int)len > max_len && max_len > (int)mlen)
                              ? (size_t)max_len - mlen : 0;
            char *buf = (char *)malloc(keep + mlen + 1);
            if (!buf)
                return NULL;
            memcpy(buf, s, keep);
            memcpy(buf + keep, marker, mlen + 1);
            json_t *out = json_new_string(buf);
            free(buf);
            return out;
        }
        return json_new_string(s);
    }
    default:
        return json_copy(value);
    }
}
