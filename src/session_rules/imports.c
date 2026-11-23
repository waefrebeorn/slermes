/*
 * imports.c — faithful C11 port of upstream Hermes session_rules/imports.py.
 * PoP: w8_imports_8a5933_text_or_none @ session_rules/imports.py:_import_text_or_none
 * PoP: w8_imports_8a5933_int_or_none @ session_rules/imports.py:_import_int_or_none
 * PoP: w8_imports_8a5933_non_finite_path @ session_rules/imports.py:_import_non_finite_path
 * PoP: w8_imports_8a5933_bounds @ session_rules/imports.py:IMPORT_INT_MIN/IMPORT_INT_MAX
 * PoP: w8_imports_8a5933_field_tables @ session_rules/imports.py:IMPORT_SESSION_TEXT_FIELDS/_MESSAGE_TEXT_FIELDS/_MESSAGE_JSON_FIELDS/IMPORT_INT_FIELDS/IMPORT_FLOAT_FIELDS
 */
#include "hermes_json.h"
#include "noc_w8_imports_8a5933.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct json_t *tagged_value(struct json_t *v) {
    struct json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", v ? v : json_null());
    return o;
}
static struct json_t *tagged_error(const char *msg) {
    struct json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(msg));
    return o;
}

/* int(value) for the JSON value types. Returns 1 on success and writes the
 * exact decimal string; 0 when Python would raise TypeError/ValueError. */
static int py_int_of_json(const struct json_t *v, char *buf, size_t cap) {
    if (!v) return 0;
    if (v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) {
        snprintf(buf, cap, "%d", v->bool_val ? 1 : 0);
        return 1;
    }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d) || d > 9.2233720368547758e18 || d < -9.2233720368547758e18) return 0;
        long long i = (long long)d; /* C truncates toward zero, same as Python int(float) */
        snprintf(buf, cap, "%lld", i);
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s && isspace((unsigned char)*s)) s++;
        char sign = 0;
        if (*s == '+' || *s == '-') { sign = *s; s++; }
        if (!*s) return 0;
        long long acc = 0;
        int any = 0;
        for (; *s; s++) {
            if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') {
                const char *t = s;
                while (*t && isspace((unsigned char)*t)) t++;
                if (*t) return 0;
                break;
            }
            if (*s == '_') continue; /* Python int() permits underscores between digits */
            if (*s < '0' || *s > '9') return 0;
            acc = acc * 10 + (*s - '0');
            any = 1;
        }
        if (!any) return 0;
        if (sign == '-') acc = -acc;
        snprintf(buf, cap, "%lld", acc);
        return 1;
    }
    return 0;
}

struct json_t *w8_imports_8a5933_text_or_none(const struct json_t *value, const char *field) {
    char msg[256];
    if (!value || value->type == JSON_NULL) return tagged_value(json_null());
    if (value->type == JSON_STRING) return tagged_value(json_string(value->str_val ? value->str_val : ""));
    snprintf(msg, sizeof(msg), "%s must be a string", field ? field : "");
    return tagged_error(msg);
}

struct json_t *w8_imports_8a5933_int_or_none(const struct json_t *value, const char *field) {
    char msg[256], num[64];
    if (!value || value->type == JSON_NULL) return tagged_value(json_null());
    if (py_int_of_json(value, num, sizeof(num))) return tagged_value(json_string(num));
    snprintf(msg, sizeof(msg), "%s must be an integer", field ? field : "");
    return tagged_error(msg);
}

/* Iterative walk mirroring Python's pending stack: children are pushed in
 * insertion order and popped from the end (LIFO). */
struct frame { struct json_t *val; char *path; int owned; };
static int is_marker(const struct json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return 0;
    return !strcmp(v->str_val, "NaN") || !strcmp(v->str_val, "Infinity") || !strcmp(v->str_val, "-Infinity");
}
char *w8_imports_8a5933_non_finite_path(const struct json_t *data, const char *path) {
    struct frame *st = NULL;
    size_t n = 0, cap = 0;
    const char *root = path ? path : "";
    struct frame f0 = { (struct json_t *)data, (char *)root, 0 };
    cap = 16;
    st = (struct frame *)malloc(cap * sizeof(*st));
    if (!st) return NULL;
    st[n++] = f0;
    char *result = NULL;
    while (n) {
        struct frame cur = st[--n];
        struct json_t *v = cur.val;
        if ((v && v->type == JSON_NUMBER && !isfinite(v->num_val)) || is_marker(v)) {
            result = strdup(cur.path);
            if (cur.owned) free(cur.path);
            break;
        }
        if (v && v->type == JSON_OBJECT) {
            size_t cnt = json_object_size(v);
            for (size_t i = 0; i < cnt; i++) {
                const char *k = json_object_get_key_at(v, i);
                struct json_t *child = json_object_get_at(v, i);
                char *p = NULL;
                if (k) { p = (char *)malloc(strlen(cur.path) + strlen(k) + 2); if (p) sprintf(p, "%s.%s", cur.path, k); }
                if (n == cap) { cap *= 2; st = (struct frame *)realloc(st, cap * sizeof(*st)); if (!st) break; }
                st[n].val = child; st[n].path = p ? p : (char *)""; st[n].owned = p ? 1 : 0; n++;
            }
        } else if (v && v->type == JSON_ARRAY) {
            size_t cnt = json_len(v);
            for (size_t i = 0; i < cnt; i++) {
                char *p = (char *)malloc(strlen(cur.path) + 32);
                if (p) sprintf(p, "%s[%zu]", cur.path, i);
                if (n == cap) { cap *= 2; st = (struct frame *)realloc(st, cap * sizeof(*st)); if (!st) break; }
                st[n].val = json_get(v, i); st[n].path = p ? p : (char *)""; st[n].owned = p ? 1 : 0; n++;
            }
        }
        if (cur.owned) free(cur.path);
    }
    free(st);
    return result;
}

struct json_t *w8_imports_8a5933_bounds(void) {
    struct json_t *o = json_object();
    json_set(o, "min", json_string("-9223372036854775808"));
    json_set(o, "max", json_string("9223372036854775807"));
    return o;
}

static void set_table(struct json_t *o, const char *name, const char *const *fields, size_t n) {
    struct json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(fields[i]));
    json_set(o, name, a);
}
struct json_t *w8_imports_8a5933_field_tables(void) {
    static const char *const sess[] = { "source","user_id","model","system_prompt","end_reason","cwd","git_branch",
        "git_repo_root","billing_provider","billing_base_url","billing_mode","cost_status","cost_source",
        "pricing_version","title" };
    static const char *const mtext[] = { "tool_call_id","tool_name","effect_disposition","finish_reason",
        "reasoning","reasoning_content","platform_message_id","message_id" };
    static const char *const mjson[] = { "reasoning_details","codex_reasoning_items","codex_message_items",
        "absorbed_message_uids","tool_call_uids" };
    static const char *const ints[] = { "input_tokens","output_tokens","cache_read_tokens","cache_write_tokens",
        "reasoning_tokens","api_call_count" };
    static const char *const floats[] = { "ended_at","estimated_cost_usd","actual_cost_usd" };
    struct json_t *o = json_object();
    set_table(o, "session_text", sess, sizeof(sess)/sizeof(sess[0]));
    set_table(o, "message_text", mtext, sizeof(mtext)/sizeof(mtext[0]));
    set_table(o, "message_json", mjson, sizeof(mjson)/sizeof(mjson[0]));
    set_table(o, "int", ints, sizeof(ints)/sizeof(ints[0]));
    set_table(o, "float", floats, sizeof(floats)/sizeof(floats[0]));
    return o;
}