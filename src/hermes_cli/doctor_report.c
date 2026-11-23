/*
 * doctor_report.c — C port of upstream Hermes hermes_cli/doctor_report.py.
 *
 * PoP: w10_doctorrepo_32657c_check_bool @ hermes_cli/doctor_report.py:check_bool
 * PoP: w10_doctorrepo_32657c_finding_merge @ hermes_cli/doctor_report.py:Finding.merge
 * PoP: w10_doctorrepo_32657c_ensure_dir @ hermes_cli/doctor_report.py:ensure_dir
 *
 * The doctor output primitives (check_ok/check_warn/check_fail) print coloured
 * text; this port emits the decision those primitives receive as JSON instead,
 * so it can be compared against the live module without a terminal.
 */
#include "noc_w10_doctorrepo_32657c.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static char *py_str(const json_t *v) {
    if (!v) return strdup("");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else snprintf(b, sizeof(b), "%g", v->num_val);
        return strdup(b);
    }
    return strdup("");
}

/* args = ok if cond else bad; a string is a single positional, a 2-list is (text, detail). */
static void fill_args(json_t *out, const json_t *args) {
    if (args && args->type == JSON_ARRAY) {
        json_t *t = json_len(args) > 0 ? json_get(args, 0) : NULL;
        json_t *d = json_len(args) > 1 ? json_get(args, 1) : NULL;
        json_set(out, "text", json_string(t && t->type == JSON_STRING ? t->str_val : ""));
        json_set(out, "detail", d && d->type == JSON_STRING ? json_string(d->str_val) : json_null());
    } else {
        char *s = py_str(args);
        json_set(out, "text", json_string(s)); free(s);
        json_set(out, "detail", json_null());
    }
}

json_t *w10_doctorrepo_32657c_check_bool(const json_t *cond, int fail,
                                         const json_t *ok, const json_t *bad) {
    int c = truthy(cond) ? 1 : 0;
    json_t *out = json_object();
    json_set(out, "ret", json_bool(c ? true : false));
    json_set(out, "kind", json_string(c ? "ok" : (fail ? "fail" : "warn")));
    fill_args(out, c ? ok : bad);
    return out;
}

json_t *w10_doctorrepo_32657c_finding_merge(const json_t *first, const json_t *second) {
    json_t *out = json_object();
    json_t *issues = json_array(), *manual = json_array();
    const char *keys[2] = {"issues", "manual_issues"};
    json_t *dst[2] = {issues, manual};
    for (int k = 0; k < 2; k++) {
        const json_t *f = json_obj_get(first, keys[k]);
        if (json_is_array(f)) for (size_t i = 0; i < json_len(f); i++)
            json_append(dst[k], json_copy(json_get(f, i)));
        const json_t *s = json_obj_get(second, keys[k]);
        if (json_is_array(s)) for (size_t i = 0; i < json_len(s); i++)
            json_append(dst[k], json_copy(json_get(s, i)));
    }
    json_set(out, "issues", issues);
    json_set(out, "manual_issues", manual);
    const json_t *f1 = json_obj_get(first, "fixed"), *f2 = json_obj_get(second, "fixed");
    long long n = (f1 && f1->type == JSON_NUMBER ? (long long)f1->num_val : 0)
                + (f2 && f2->type == JSON_NUMBER ? (long long)f2->num_val : 0);
    json_set(out, "fixed", json_number((double)n));
    return out;
}

json_t *w10_doctorrepo_32657c_ensure_dir(int exists, int should_fix,
        const char *exists_msg, const char *created_msg, const char *missing_msg) {
    json_t *out = json_object();
    if (exists) {
        json_set(out, "kind", json_string("ok"));
        json_set(out, "msg", json_string(exists_msg ? exists_msg : ""));
        json_set(out, "detail", json_null());
        json_set(out, "fixed", json_number(0));
    } else if (should_fix) {
        json_set(out, "kind", json_string("ok"));
        json_set(out, "msg", json_string(created_msg ? created_msg : ""));
        json_set(out, "detail", json_null());
        json_set(out, "fixed", json_number(1));
    } else {
        json_set(out, "kind", json_string("warn"));
        json_set(out, "msg", json_string(missing_msg ? missing_msg : ""));
        json_set(out, "detail", json_string("(will be created on first use)"));
        json_set(out, "fixed", json_number(0));
    }
    return out;
}
