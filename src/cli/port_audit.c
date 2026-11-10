/*
 * port_audit.c — Port of hermes_cli/dashboard_auth/audit.py::_bounded_value:
 * recursively bound audit field values. The live Python keeps str/int/float/bool/
 * None as-is (recursing into dict/list), repr()s anything else (irrelevant here:
 * JSON only carries str/number/bool/null), and truncates strings longer than
 * _MAX_FIELD_LENGTH to (256 - len(marker)) chars + the marker.
 *
 *   _MAX_FIELD_LENGTH = 256
 *   _TRUNCATION_MARKER = "...[truncated]"   (14 chars -> keeps 242)
 *
 * Returns a NEW json_t tree (caller frees), mirroring the Python which builds a
 * fresh dict/list on recursion.
 */

#include "port_audit.h"

#include <stdlib.h>
#include <string.h>

#define MAX_FIELD_LENGTH 256
#define TRUNC_MARKER "...[truncated]"
#define TRUNC_MARKER_LEN ((size_t)14)              /* "...[truncated]" = 14 chars */
#define KEEP_LEN (MAX_FIELD_LENGTH - TRUNC_MARKER_LEN) /* 242 */

static char *bounded_str(const char *s)
{
    size_t n = strlen(s);
    if (n <= MAX_FIELD_LENGTH) {
        char *cp = malloc(n + 1);
        if (cp) memcpy(cp, s, n + 1);
        return cp;
    }
    /* keep 241 chars + "...[truncated]" -> total 256 */
    char *out = malloc(KEEP_LEN + TRUNC_MARKER_LEN + 1);
    if (!out) return NULL;
    memcpy(out, s, KEEP_LEN);
    memcpy(out + KEEP_LEN, TRUNC_MARKER, TRUNC_MARKER_LEN);
    out[KEEP_LEN + TRUNC_MARKER_LEN] = '\0';
    return out;
}

/* PoP: port_audit_bounded_value @ hermes_cli/dashboard_auth/audit.py:_bounded_value */
json_t *port_audit_bounded_value(const json_t *value)
{
    if (!value)
        return json_null();
    switch (value->type) {
    case JSON_STRING: {
        char *b = bounded_str(value->str_val);
        json_t *out = json_string(b);
        free(b);
        return out;
    }
    case JSON_ARRAY: {
        json_t *out = json_array();
        for (size_t i = 0; i < value->c.count; i++)
            json_append(out, port_audit_bounded_value(value->c.items[i]));
        return out;
    }
    case JSON_OBJECT: {
        json_t *out = json_object();
        for (size_t i = 0; i < value->c.count; i++) {
            json_t *bv = port_audit_bounded_value(value->c.items[i]);
            json_set(out, value->c.keys[i] ? value->c.keys[i] : "", bv);
        }
        return out;
    }
    default: /* JSON_NULL / BOOL / NUMBER: pass through unchanged (still new node) */
        switch (value->type) {
        case JSON_NULL:   return json_null();
        case JSON_BOOL:   return json_bool(value->bool_val);
        default:          return json_number(value->num_val);
        }
    }
}
