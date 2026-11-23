/*
 * kanban_db_notify.c — C port of upstream Hermes hermes_cli/kanban_db_notify.py.
 * Gate: w15_kanbandbn_8ba814
 *
 * PoP: w15_kanbandbn_8ba814_sub_key @ hermes_cli/kanban_db_notify.py:_sub_key
 * PoP: w15_kanbandbn_8ba814_encode_notify_delivery_metadata @ hermes_cli/kanban_db_notify.py:_encode_notify_delivery_metadata
 * PoP: w15_kanbandbn_8ba814_decode_notify_delivery_metadata @ hermes_cli/kanban_db_notify.py:_decode_notify_delivery_metadata
 * PoP: w15_kanbandbn_8ba814_notify_profile_filter @ hermes_cli/kanban_db_notify.py:_notify_profile_filter
 *
 * Only the pure helpers are ported; DB/network functions stay in Python.
 * Fixtures use integer (not float) metadata numbers so C JSON number emission
 * matches Python json.dumps exactly (integral doubles would lose the ".0").
 */
#include "noc_w15_kanbandbn_8ba814.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_ws(const char *s) {
    size_t a = 0, b = s ? strlen(s) : 0;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { if (b > a) memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

static int is_scalar(const json_t *v) {
    return v && (v->type == JSON_STRING || v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

static int obj_count(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_OBJECT) return (int)json_object_size(v);
    if (v->type == JSON_ARRAY) return (int)json_len(v);
    return 0;
}

/* Python `value or ""` -> a fresh json_t (either the value or ""). */
static json_t *truthy_or_empty(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_string("");
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? json_copy(v) : json_string("");
        case JSON_NUMBER: return v->num_val != 0.0 ? json_copy(v) : json_string("");
        case JSON_STRING: return (v->str_val && *v->str_val) ? json_copy(v) : json_string("");
        case JSON_ARRAY:
        case JSON_OBJECT: return obj_count(v) > 0 ? json_copy(v) : json_string("");
        default:          return json_string("");
    }
}

json_t *w15_kanbandbn_8ba814_sub_key(const struct json_t *task_id,
                                     const struct json_t *platform,
                                     const struct json_t *chat_id,
                                     const struct json_t *thread_id) {
    json_t *arr = json_array();
    json_append(arr, task_id ? json_copy(task_id) : json_null());
    json_append(arr, platform ? json_copy(platform) : json_null());
    json_append(arr, chat_id ? json_copy(chat_id) : json_null());
    json_append(arr, truthy_or_empty(thread_id));
    return arr;
}

/* --- tiny string builder --- */
typedef struct { char *p; size_t len, cap; } sb_t;
static void sb_init(sb_t *b) { b->cap = 64; b->len = 0; b->p = (char *)malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void sb_add(sb_t *b, const char *s) {
    if (!b->p || !s) return;
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        while (cap < b->len + n + 1) cap *= 2;
        char *q = (char *)realloc(b->p, cap);
        if (!q) return;
        b->p = q; b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

json_t *w15_kanbandbn_8ba814_encode_notify_delivery_metadata(const struct json_t *metadata) {
    if (!json_is_object(metadata)) return json_null();
    size_t n = json_object_size(metadata);
    const char **keys = (const char **)malloc((n ? n : 1) * sizeof(char *));
    size_t cnt = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(metadata, i);
        if (k && is_scalar(json_object_get_at(metadata, i))) keys[cnt++] = k;
    }
    if (cnt == 0) { free(keys); return json_null(); }
    /* sort keys like Python sort_keys=True (ASCII code-point order) */
    for (size_t i = 1; i < cnt; i++) {
        const char *tmp = keys[i];
        size_t j = i;
        while (j > 0 && strcmp(keys[j - 1], tmp) > 0) { keys[j] = keys[j - 1]; j--; }
        keys[j] = tmp;
    }
    sb_t out; sb_init(&out);
    sb_add(&out, "{");
    for (size_t i = 0; i < cnt; i++) {
        if (i) sb_add(&out, ",");
        char *ek = json_escape_string(keys[i]);
        sb_add(&out, ek ? ek : "\"\"");
        free(ek);
        sb_add(&out, ":");
        json_t *v = json_obj_get(metadata, keys[i]);
        if (v->type == JSON_STRING) {
            char *ev = json_escape_string(v->str_val ? v->str_val : "");
            sb_add(&out, ev ? ev : "\"\"");
            free(ev);
        } else if (v->type == JSON_BOOL) {
            sb_add(&out, v->bool_val ? "true" : "false");
        } else { /* JSON_NUMBER: integral -> plain integer, else shortest round-trip */
            char tmp[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
            else {
                snprintf(tmp, sizeof(tmp), "%.15g", v->num_val);
                if (strtod(tmp, NULL) != v->num_val) snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
            }
            sb_add(&out, tmp);
        }
    }
    sb_add(&out, "}");
    free(keys);
    json_t *res = json_string(out.p ? out.p : "{}");
    free(out.p);
    return res;
}

json_t *w15_kanbandbn_8ba814_decode_notify_delivery_metadata(const struct json_t *raw) {
    json_t *res = json_object();
    if (!raw) return res;
    if (raw->type == JSON_OBJECT) { json_free(res); return json_copy(raw); }
    int falsy = 0;
    switch (raw->type) {
        case JSON_NULL: falsy = 1; break;
        case JSON_BOOL: if (!raw->bool_val) falsy = 1; break;
        case JSON_NUMBER: if (raw->num_val == 0.0) falsy = 1; break;
        case JSON_STRING: if (!raw->str_val || !*raw->str_val) falsy = 1; break;
        case JSON_ARRAY: if (json_len(raw) == 0) falsy = 1; break;
        default: break;
    }
    if (falsy || raw->type != JSON_STRING) return res;
    json_t *parsed = json_parse(raw->str_val, NULL);
    if (!parsed || parsed->type != JSON_OBJECT) { json_free(parsed); return res; }
    for (size_t i = 0; i < json_object_size(parsed); i++) {
        const char *k = json_object_get_key_at(parsed, i);
        json_t *v = json_object_get_at(parsed, i);
        if (k && is_scalar(v)) json_set(res, k, json_copy(v));
    }
    json_free(parsed);
    return res;
}

static int cmp_strptr(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

json_t *w15_kanbandbn_8ba814_notify_profile_filter(const struct json_t *profiles,
                                                   int include_unowned) {
    json_t *out = json_array();
    if (!profiles || profiles->type != JSON_ARRAY) {
        json_append(out, json_string(""));
        json_append(out, json_array());
        return out;
    }
    size_t n = json_len(profiles);
    char **vals = (char **)malloc((n ? n : 1) * sizeof(char *));
    size_t cnt = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(profiles, i);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        char *t = strip_ws(s ? s : "");
        if (t && *t) vals[cnt++] = t; else free(t);
    }
    if (cnt > 1) qsort(vals, cnt, sizeof(char *), cmp_strptr);
    /* dedupe after sort */
    size_t m = 0;
    for (size_t i = 0; i < cnt; i++) {
        if (i == 0 || strcmp(vals[i], vals[m - 1]) != 0) vals[m++] = vals[i];
        else free(vals[i]);
    }
    char *clauses[2];
    int nc = 0;
    sb_t c0; sb_init(&c0);
    if (m > 0) {
        sb_add(&c0, "notifier_profile IN (");
        for (size_t i = 0; i < m; i++) { if (i) sb_add(&c0, ","); sb_add(&c0, "?"); }
        sb_add(&c0, ")");
        clauses[nc++] = c0.p;
    }
    char *unowned = dupstr("notifier_profile IS NULL OR notifier_profile = ''");
    if (include_unowned) clauses[nc++] = unowned;
    sb_t sql; sb_init(&sql);
    if (nc == 0) {
        sb_add(&sql, "0");
    } else {
        sb_add(&sql, "(");
        for (int i = 0; i < nc; i++) { if (i) sb_add(&sql, ") OR ("); sb_add(&sql, clauses[i]); }
        sb_add(&sql, ")");
    }
    json_append(out, json_string(sql.p ? sql.p : "0"));
    json_t *params = json_array();
    for (size_t i = 0; i < m; i++) json_append(params, json_string(vals[i]));
    json_append(out, params);
    for (size_t i = 0; i < m; i++) free(vals[i]);
    free(vals);
    free(c0.p);
    free(unowned);
    free(sql.p);
    return out;
}
