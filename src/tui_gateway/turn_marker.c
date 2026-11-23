/*
 * turn_marker.c — faithful C11 port of tui_gateway/turn_marker.py.
 * Gate w10_turnmarker_94f189.
 *
 * PoP: w10_turnmarker_94f189_started_at @ tui_gateway/turn_marker.py:_started_at
 * PoP: w10_turnmarker_94f189_marker_writer_state @ tui_gateway/turn_marker.py:marker_writer_state
 * PoP: w10_turnmarker_94f189_prune @ tui_gateway/turn_marker.py:_prune
 * PoP: w10_turnmarker_94f189_read_turn_marker @ tui_gateway/turn_marker.py:read_turn_marker
 */
#include "hermes_json.h"
#include "noc_w10_turnmarker_94f189.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_AGE_SECS 86400.0
#define MAX_ENTRIES 32

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n && s) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupn("", 0);
    switch (v->type) {
        case JSON_NULL:   return dupn("None", 4);
        case JSON_STRING: return dupn(v->str_val ? v->str_val : "", v->str_val ? strlen(v->str_val) : 0);
        case JSON_BOOL:   return dupn(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupn(buf, strlen(buf));
        default: return dupn("", 0);
    }
}

static char *py_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\v'||s[a]=='\f')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\v'||s[b-1]=='\f')) b--;
    return dupn(s + a, b - a);
}

static double to_double(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) return strtod(v->str_val ? v->str_val : "", NULL);
    return 0.0;
}

double w10_turnmarker_94f189_started_at(const json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return 0.0;
    const json_t *v = json_obj_get(entry, "started_at");
    return py_truthy(v) ? to_double(v) : 0.0;
}

const char *w10_turnmarker_94f189_marker_writer_state(const json_t *entry, int liveness) {
    if (!entry || entry->type != JSON_OBJECT) return "unknown";
    const json_t *pid = json_obj_get(entry, "writer_pid");
    if (!pid || pid->type != JSON_NUMBER) return "unknown";
    if (pid->num_val <= 0 || pid->num_val != (double)(long long)pid->num_val) return "unknown";
    if (liveness == 0 || liveness == 3) return "unknown";
    return liveness == 1 ? "alive" : "dead";
}

typedef struct { const char *key; json_t *val; double at; size_t idx; } prune_ent_t;

static int cmp_newer(const void *a, const void *b) {
    const prune_ent_t *x = (const prune_ent_t *)a, *y = (const prune_ent_t *)b;
    if (x->at > y->at) return -1;
    if (x->at < y->at) return 1;
    return x->idx < y->idx ? -1 : (x->idx > y->idx ? 1 : 0);
}

json_t *w10_turnmarker_94f189_prune(const json_t *entries, double now) {
    json_t *out = json_object();
    if (!entries || entries->type != JSON_OBJECT) return out;
    size_t n = json_object_size(entries);
    prune_ent_t *arr = (prune_ent_t *)calloc(n ? n : 1, sizeof(prune_ent_t));
    size_t cnt = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(entries, i);
        json_t *v = json_object_get_at(entries, i);
        if (!k || !v || v->type != JSON_OBJECT) continue;
        double at = w10_turnmarker_94f189_started_at(v);
        if (now - at <= MAX_AGE_SECS) {
            arr[cnt].key = k; arr[cnt].val = v; arr[cnt].at = at; arr[cnt].idx = i; cnt++;
        }
    }
    if (cnt > MAX_ENTRIES) { qsort(arr, cnt, sizeof(prune_ent_t), cmp_newer); cnt = MAX_ENTRIES; }
    for (size_t i = 0; i < cnt; i++) json_set(out, arr[i].key, json_copy(arr[i].val));
    free(arr);
    return out;
}

static long py_int(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_NUMBER) return (long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtol(v->str_val ? v->str_val : "", NULL, 10);
    return 0;
}

json_t *w10_turnmarker_94f189_read_turn_marker(const char *session_key, const json_t *stored) {
    if (!session_key || !*session_key) return NULL;
    const json_t *entry = (stored && stored->type == JSON_OBJECT)
                          ? json_obj_get(stored, session_key) : NULL;
    if (!entry || entry->type != JSON_OBJECT) return NULL;
    char *prompt = py_str(json_obj_get(entry, "prompt"));
    char *check = py_strip(prompt); /* only the emptiness check is stripped, not the value */
    int blank = !*check;
    free(check);
    if (blank) { free(prompt); return NULL; }
    json_t *out = json_object();
    long attempts = py_int(json_obj_get(entry, "attempts"));
    if (attempts < 0) attempts = 0;
    json_set(out, "attempts", json_number((double)attempts));
    json_set_hdr(out, "prompt", prompt);
    json_set(out, "started_at", json_number(w10_turnmarker_94f189_started_at(entry)));
    const json_t *ac = json_obj_get(entry, "auto_continue");
    json_set(out, "auto_continue", json_bool(ac ? (py_truthy(ac) ? true : false) : true));
    static const char *const wkeys[] = {"writer_pid", "writer_start_time"};
    for (size_t i = 0; i < 2; i++) {
        const json_t *v = json_obj_get(entry, wkeys[i]);
        if (v && v->type != JSON_NULL) json_set(out, wkeys[i], json_copy(v));
    }
    const json_t *nc = json_obj_get(entry, "notification_category");
    if (nc && nc->type == JSON_STRING && nc->str_val && !strcmp(nc->str_val, "diagnostic"))
        json_set_hdr(out, "notification_category", "diagnostic");
    free(prompt);
    return out;
}
