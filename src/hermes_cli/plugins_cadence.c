/*
 * plugins_cadence.c — C home for upstream Hermes hermes_cli/plugins_cadence.py.
 *
 * PoP: w13_pluginscad_fd67f1_install_fingerprint @ hermes_cli/plugins_cadence.py:_install_fingerprint
 * PoP: w13_pluginscad_fd67f1_check_interval_hours @ hermes_cli/plugins_cadence.py:check_interval_hours
 * PoP: w13_pluginscad_fd67f1_auto_apply_enabled @ hermes_cli/plugins_cadence.py:auto_apply_enabled
 * PoP: w13_pluginscad_fd67f1_cached_git_updates @ hermes_cli/plugins_cadence.py:cached_git_updates
 */
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginscad_fd67f1.h"

static char *dups(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *trimdup(const char *s) {
    size_t a = 0, b;
    if (!s) return dups("");
    b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    {
        char *p = (char *)malloc(b - a + 1);
        if (!p) return dups("");
        memcpy(p, s + a, b - a); p[b - a] = '\0';
        return p;
    }
}
static bool is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}
static bool truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count != 0;
}
/* Python str() for JSON scalars. */
static char *py_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dups("None");
    if (v->type == JSON_STRING) return dups(v->str_val);
    if (v->type == JSON_BOOL) return dups(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return dups(buf);
    }
    return dups(v->type == JSON_ARRAY ? "[]" : "{}");
}
static json_t *fingerprint(const json_t *record) {
    json_t *out;
    const json_t *v;
    if (!record || record->type != JSON_OBJECT) return json_null();
    out = json_object();
    v = json_obj_get(record, "source");
    json_set(out, "source", v ? json_copy(v) : json_null());
    v = json_obj_get(record, "revision");
    json_set(out, "revision", v ? json_copy(v) : json_null());
    json_set(out, "pinned", json_bool(is_true(json_obj_get(record, "pinned"))));
    return out;
}
static bool value_eq(const json_t *a, const json_t *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->type == JSON_NULL || b->type == JSON_NULL) return a->type == b->type;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING) return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    return false;
}
/* Dict equality: current has exactly source/revision/pinned. */
static bool fingerprint_eq(const json_t *cur, const json_t *checked) {
    if (!checked || checked->type != JSON_OBJECT || json_object_size(checked) != 3) return false;
    return value_eq(json_obj_get(cur, "source"), json_obj_get(checked, "source")) &&
           value_eq(json_obj_get(cur, "revision"), json_obj_get(checked, "revision")) &&
           value_eq(json_obj_get(cur, "pinned"), json_obj_get(checked, "pinned"));
}

json_t *w13_pluginscad_fd67f1_install_fingerprint(const struct json_t *in) {
    return fingerprint(json_obj_get(in, "record"));
}

json_t *w13_pluginscad_fd67f1_check_interval_hours(const struct json_t *in) {
    const json_t *value;
    double hours;
    if (json_get_bool(in, "raises", false)) return json_number(24.0);
    value = json_obj_get(in, "value");
    if (!value || value->type == JSON_NULL) return json_number(24.0);
    if (value->type == JSON_BOOL) hours = value->bool_val ? 1.0 : 0.0;
    else if (value->type == JSON_NUMBER) hours = value->num_val;
    else if (value->type == JSON_STRING) {
        char *t = trimdup(value->str_val), *end = NULL;
        if (!t[0]) { free(t); return json_number(24.0); }
        hours = strtod(t, &end);
        if (!end || *end) { free(t); return json_number(24.0); }
        free(t);
    } else return json_number(24.0);
    if (isnan(hours) || isinf(hours)) return json_number(24.0);
    return json_number(hours > 0.0 ? hours : 0.0);
}

json_t *w13_pluginscad_fd67f1_auto_apply_enabled(const struct json_t *in) {
    return json_bool(truthy(json_obj_get(in, "value")));
}

json_t *w13_pluginscad_fd67f1_cached_git_updates(const struct json_t *in) {
    const json_t *rows = json_obj_get(in, "rows");
    const json_t *meta = json_obj_get(in, "install_metadata");
    json_t *out = json_object();
    size_t i;
    if (!rows || rows->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        json_t *rec, *cur;
        const json_t *ua, *latest;
        const char *name;
        char *str;
        if (!row || row->type != JSON_OBJECT) continue;
        if (strcmp(json_get_str(row, "class", ""), "git")) continue;
        ua = json_obj_get(row, "update_available");
        if (!is_true(ua)) continue;
        name = json_get_str(row, "name", "");
        rec = (meta && meta->type == JSON_OBJECT) ? json_obj_get(meta, name) : NULL;
        cur = fingerprint(rec);
        if (cur->type == JSON_OBJECT && !is_true(json_obj_get(cur, "pinned")) &&
            fingerprint_eq(cur, json_obj_get(row, "checked_install"))) {
            latest = json_obj_get(row, "latest");
            if (truthy(latest)) {
                str = py_scalar(latest);
                json_set(out, name, json_string(str));
                free(str);
            }
        }
        json_free(cur);
    }
    return out;
}
