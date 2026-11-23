/*
 * run_common.c — C port of upstream Hermes gateway/run_common.py.
 * PoP: runcsv_csv_or_list_to_set @ gateway/run_common.py:_csv_or_list_to_set
 * Set members are emitted in sorted order for a deterministic differential gate.
 */
#include "noc_runcsv.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else snprintf(buf, cap, "%.17g", v->num_val);
            break;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); break;
        default: buf[0] = 0; break;
    }
}
static void trim(char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\f'||s[a]=='\v')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\f'||s[b-1]=='\v')) b--;
    memmove(s, s + a, b - a); s[b - a] = 0;
}
static void add_unique(json_t *out, const char *s) {
    for (size_t i = 0; i < json_len(out); i++) {
        const char *e = json_get(out, i)->str_val;
        if (e && !strcmp(e, s)) return;
    }
    json_append(out, json_string(s));
}
static int cmp_str(const void *a, const void *b) {
    const json_t *x = *(const json_t * const *)a;
    const json_t *y = *(const json_t * const *)b;
    return strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "");
}
json_t *runcsv_csv_or_list_to_set(const json_t *raw) {
    json_t *out = json_array();
    if (!raw || raw->type == JSON_NULL) return out;
    if (raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw); i++) {
            char buf[512]; py_str(json_get(raw, i), buf, sizeof buf);
            trim(buf);
            if (*buf) add_unique(out, buf);
        }
    } else {
        char buf[4096]; py_str(raw, buf, sizeof buf);
        char *save = NULL;
        for (char *p = strtok_r(buf, ",", &save); p; p = strtok_r(NULL, ",", &save)) {
            trim(p);
            if (*p) add_unique(out, p);
        }
    }
    if (json_len(out) > 1) qsort(out->c.items, json_len(out), sizeof(json_t *), cmp_str);
    return out;
}
