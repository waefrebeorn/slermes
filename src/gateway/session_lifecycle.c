/*
 * session_lifecycle.c — C home for upstream Hermes gateway/session_lifecycle.py.
 *
 * PoP: w12_sessionlif_36761a_is_delegate_child_row @ gateway/session_lifecycle.py:is_delegate_child_row
 * PoP: w12_sessionlif_36761a_auto_continue_freshness_window_ex @ gateway/session_lifecycle.py:auto_continue_freshness_window
 */
#include "noc_w12_sessionlif_36761a.h"

#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_FRESHNESS 3600.0

static const char ADOPTED_MARKER[] = "_chat_adopted";

/* Python truthiness of a decoded JSON value (None/False/0/""/[]/{} are falsy). */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 && !isnan(v->num_val);
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

static int is_blank(const char *s) {
    if (!s) return 1;
    for (; *s; ++s)
        if (!isspace((unsigned char)*s)) return 0;
    return 1;
}

/* Mirrors persistence.conversation.sessions.parse_model_config:
 * non-blank JSON text -> json.loads (object only counts), mapping -> itself, else {}.
 * Returns a node the CALLER must free (may be a fresh object, or json_copy of the mapping). */
static json_t *parse_model_config(const json_t *raw) {
    json_t *result = NULL;
    if (raw && raw->type == JSON_STRING && !is_blank(raw->str_val)) {
        char *err = NULL;
        json_t *parsed = json_parse(raw->str_val, &err);
        free(err);
        if (parsed && parsed->type == JSON_OBJECT)
            result = parsed;
        else
            json_free(parsed);
    } else if (raw && raw->type == JSON_OBJECT) {
        result = json_copy(raw);
    }
    if (!result) result = json_object();
    return result;
}

int w12_sessionlif_36761a_is_delegate_child_row(const struct json_t *row) {
    const json_t *raw = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "model_config") : NULL;
    json_t *cfg = parse_model_config(raw);

    /* Copy every value we need OUT of cfg before freeing it. */
    int has_delegate = py_truthy(json_obj_get(cfg, "_delegate_from"));
    int has_reset = json_has(cfg, "_reset_from");
    int has_adopted = py_truthy(json_obj_get(cfg, ADOPTED_MARKER));

    json_free(cfg);
    return (has_delegate && !has_reset && !has_adopted) ? 1 : 0;
}

double w12_sessionlif_36761a_auto_continue_freshness_window_ex(const char *env_value) {
    if (!env_value || !*env_value) return DEFAULT_FRESHNESS;
    const char *p = env_value;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (!*p) return DEFAULT_FRESHNESS;
    char *end = NULL;
    double value = strtod(p, &end);
    if (end == p) return DEFAULT_FRESHNESS;
    while (*end && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return DEFAULT_FRESHNESS;
    if (isnan(value) || isinf(value)) return DEFAULT_FRESHNESS;
    return value;
}
