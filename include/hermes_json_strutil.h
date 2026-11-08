/*
 * hermes_json_strutil.h — shared JSON-to-string helpers.
 *
 * node_str (json_t -> malloc'd string:  %.17g number, True/False bool,
 * strdup string, "" null, json_serialize default) was copy-pasted
 * byte-identical into agent/port_error_surface.c and
 * gateway/port_hosted_room_execution_policy.c. Single source of truth.
 */
#ifndef HERMES_JSON_STRUTIL_H
#define HERMES_JSON_STRUTIL_H

#include <stdio.h>
#include <string.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render a json node as a malloc'd string. Caller free()s.
 * Mirrors the two identical port-local helpers exactly. */
static inline char *json_node_to_str(const json_t *n) {
    if (!n) return strdup("");
    char buf[64];
    switch (n->type) {
    case JSON_STRING: return strdup(n->str_val ? n->str_val : "");
    case JSON_NUMBER: snprintf(buf, sizeof(buf), "%.17g", n->num_val); return strdup(buf);
    case JSON_BOOL:   return strdup(n->bool_val ? "True" : "False");
    case JSON_NULL:   return strdup("");
    default:          return json_serialize(n);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* HERMES_JSON_STRUTIL_H */
