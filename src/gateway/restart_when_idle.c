/*
 * restart_when_idle.c — C home for upstream Hermes gateway/restart_when_idle.py.
 *
 * PoP: w11_restartwhe_158280_log_view @ gateway/restart_when_idle.py:_log_view
 */
#include "noc_w11_restartwhe_158280.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *s; size_t n, cap; } sb_t;

static void sb_add(sb_t *b, const char *s) {
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + l + 1) nc *= 2;
        b->s = (char *)realloc(b->s, nc);
        b->cap = nc;
    }
    memcpy(b->s + b->n, s, l);
    b->n += l;
    b->s[b->n] = '\0';
}

/* Python truthiness of a JSON node. */
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* Python str() of a JSON scalar (arrays/objects are unused by the gate). */
static void py_str(const json_t *v, sb_t *b) {
    if (!v || v->type == JSON_NULL) { sb_add(b, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:   sb_add(b, v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER: {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%.15g", v->num_val);
            sb_add(b, tmp);
            break;
        }
        case JSON_STRING: sb_add(b, v->str_val ? v->str_val : ""); break;
        case JSON_ARRAY:
        case JSON_OBJECT: sb_add(b, ""); break;
        default: sb_add(b, "None"); break;
    }
}

char *w11_restartwhe_158280_log_view(const struct json_t *units) {
    static const char *KEYS[] = {"session", "delegation_id", "process_id", "id", "job_id", "pid"};
    static const size_t NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);

    sb_t result = {0};
    result.s = (char *)calloc(1, 1);
    int any = 0;

    if (units && units->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(units); ++i) {
            const json_t *u = json_get(units, i);
            if (!u || u->type != JSON_OBJECT) continue; /* only dicts */

            if (any) sb_add(&result, "; ");
            any = 1;

            const json_t *kind = json_obj_get(u, "kind");
            py_str(kind, &result); /* u.get('kind') -> "None" when absent */
            sb_add(&result, "(");
            int first = 1;
            for (size_t k = 0; k < NKEYS; ++k) {
                const json_t *val = json_obj_get(u, KEYS[k]);
                if (!truthy(val)) continue;
                if (!first) sb_add(&result, ", ");
                first = 0;
                sb_add(&result, KEYS[k]);
                sb_add(&result, "=");
                py_str(val, &result);
            }
            sb_add(&result, ")");
        }
    }
    if (!any) { free(result.s); return strdup("nothing"); }
    return result.s; /* malloc'd; caller frees */
}
