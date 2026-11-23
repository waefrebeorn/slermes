/*
 * PoP: w10_kanbanoutp_46c6b6_fmt_counts  @ hermes_cli/kanban_output.py:_fmt_counts
 * PoP: w10_kanbanoutp_46c6b6_status_icon @ hermes_cli/kanban_output.py:_STATUS_ICONS
 */
#include "hermes_json.h"
#include "noc_w10_kanbanoutp_46c6b6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, const char *s) {
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + l + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, l); b->n += l; b->p[b->n] = 0;
}

/* Python str(value) when used directly in an f-string (strings are unquoted). */
static void py_str(buf_t *b, json_t *v) {
    if (!v || v->type == JSON_NULL) { bput(b, "None"); return; }
    switch (v->type) {
        case JSON_STRING: bput(b, v->str_val ? v->str_val : ""); break;
        case JSON_BOOL:   bput(b, v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER: {
            char tmp[64];
            double d = v->num_val;
            snprintf(tmp, sizeof(tmp), "%.0f", d);
            bput(b, tmp);
            break;
        }
        case JSON_ARRAY: {
            bput(b, "[");
            for (size_t i = 0; i < json_array_size(v); i++) {
                if (i) bput(b, ", ");
                py_str(b, json_array_get(v, i));
            }
            bput(b, "]");
            break;
        }
        case JSON_OBJECT: {
            bput(b, "{");
            for (size_t i = 0; i < json_object_size(v); i++) {
                if (i) bput(b, ", ");
                char tmp[512];
                snprintf(tmp, sizeof(tmp), "'%s': ", json_object_get_key_at(v, i) ? json_object_get_key_at(v, i) : "");
                bput(b, tmp);
                py_str(b, json_object_get_at(v, i));
            }
            bput(b, "}");
            break;
        }
        default: bput(b, "None"); break;
    }
}

static int cmp_keys(const void *a, const void *b) {
    const char *ka = *(const char *const *)a;
    const char *kb = *(const char *const *)b;
    return strcmp(ka, kb);
}

json_t *w10_kanbanoutp_46c6b6_fmt_counts(json_t *counts, json_t *empty) {
    size_t n = (counts && counts->type == JSON_OBJECT) ? json_object_size(counts) : 0;
    if (n == 0) {
        if (empty) return json_copy(empty);
        return json_string("");
    }
    const char **keys = (const char **)malloc(n * sizeof(const char *));
    if (!keys) return json_string("");
    for (size_t i = 0; i < n; i++) keys[i] = json_object_get_key_at(counts, i);
    qsort(keys, n, sizeof(const char *), cmp_keys);
    buf_t out = {0};
    for (size_t i = 0; i < n; i++) {
        if (i) bput(&out, ", ");
        bput(&out, keys[i]);
        bput(&out, "=");
        /* json_obj_get uses the key name; look up the value that matches this key. */
        json_t *val = NULL;
        for (size_t j = 0; j < n; j++) {
            if (!strcmp(keys[i], json_object_get_key_at(counts, j))) {
                val = json_object_get_at(counts, j);
                break;
            }
        }
        py_str(&out, val);
    }
    free(keys);
    json_t *res = json_string(out.p ? out.p : "");
    free(out.p);
    return res;
}

const char *w10_kanbanoutp_46c6b6_status_icon(const char *status) {
    if (!status) return "?";
    if (!strcmp(status, "triage")) return "\xe2\x97\x87";      /* diamond */
    if (!strcmp(status, "todo")) return "\xe2\x97\xbb";        /* square */
    if (!strcmp(status, "scheduled")) return "\xe2\x8f\xb1";   /* stopwatch */
    if (!strcmp(status, "ready")) return "\xe2\x96\xb6";       /* triangle */
    if (!strcmp(status, "running")) return "\xe2\x97\x8f";     /* circle */
    if (!strcmp(status, "blocked")) return "\xe2\x8a\x98";     /* circled slash */
    if (!strcmp(status, "review")) return "\xe2\x97\x8e";      /* double circle */
    if (!strcmp(status, "done")) return "\xe2\x9c\x93";        /* check */
    if (!strcmp(status, "archived")) return "\xe2\x80\x94";    /* em dash */
    return "?";
}
