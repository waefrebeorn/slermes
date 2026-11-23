/*
 * main_tui_launch.c — C port of upstream Hermes hermes_cli/main_tui_launch.py.
 *
 * PoP: w13_maintuila_9fecf1_bypass @ hermes_cli/main_tui_launch.py:_bypass_chat_launch
 * PoP: w13_maintuila_9fecf1_split_comma @ hermes_cli/main_tui_launch.py:_split_comma_items
 * PoP: w13_maintuila_9fecf1_unforwarded_flags @ hermes_cli/main_tui_launch.py:_refuse_unforwarded_tui_flags
 * PoP: w13_maintuila_9fecf1_heap_mb @ hermes_cli/main_tui_launch.py:_resolve_tui_heap_mb
 * PoP: w13_maintuila_9fecf1_normalize_toolsets @ hermes_cli/main_tui_launch.py:_normalize_tui_toolsets
 *
 * Pure argv/env shaping seams; no process spawning or filesystem access.
 */
#include "noc_w13_maintuila_9fecf1.h"
#include "hermes_json.h"
#include <ctype.h>
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

static void py_scalar(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_STRING: snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, n, "%lld", (long long)v->num_val);
            else
                snprintf(buf, n, "%g", v->num_val);
            break;
        default: if (n) buf[0] = '\0'; break;
    }
}

static char *strip_dup(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

json_t *w13_maintuila_9fecf1_bypass(const struct json_t *args) {
    int gate = truthy(json_obj_get((json_t *)args, "safe_mode")) ||
               truthy(json_obj_get((json_t *)args, "ignore_user_config"));
    int explicit_tui = truthy(json_obj_get((json_t *)args, "tui"));
    return json_bool(gate && !explicit_tui ? 1 : 0);
}

static json_t *split_comma_impl(const struct json_t *items, int split_non_str) {
    json_t *out = json_array();
    json_t *raw;
    if (items && items->type == JSON_ARRAY) {
        raw = (json_t *)items;
    } else {
        raw = json_array();
        json_append(raw, items ? json_copy(items) : json_null());
    }
    for (size_t i = 0; i < raw->c.count; i++) {
        json_t *item = json_get(raw, i);
        if (item && item->type != JSON_STRING && !split_non_str) {
            char v[256];
            py_scalar(item, v, sizeof(v));
            char *t = strip_dup(v);
            if (t && *t) json_append(out, json_string(t));
            free(t);
        } else {
            char v[2048];
            py_scalar(item, v, sizeof(v));
            char *p = v;
            while (1) {
                char *c = strchr(p, ',');
                if (c) *c = '\0';
                char *t = strip_dup(p);
                if (t && *t) json_append(out, json_string(t));
                free(t);
                if (!c) break;
                p = c + 1;
            }
        }
    }
    if (raw != items) json_free(raw);
    return out;
}

json_t *w13_maintuila_9fecf1_split_comma(const struct json_t *items, int split_non_str) {
    return split_comma_impl(items, split_non_str);
}

static const char *const UNFWD[][2] = {
    {"api_key", "--api-key"},
    {"base_url", "--base-url"},
    {"reasoning", "--reasoning"},
    {NULL, NULL}
};

json_t *w13_maintuila_9fecf1_unforwarded_flags(const struct json_t *args) {
    json_t *out = json_array();
    for (size_t i = 0; UNFWD[i][0]; i++)
        if (truthy(json_obj_get((json_t *)args, UNFWD[i][0])))
            json_append(out, json_string(UNFWD[i][1]));
    return out;
}

json_t *w13_maintuila_9fecf1_heap_mb(const struct json_t *limit, const struct json_t *default_mb) {
    long long d = default_mb ? (long long)default_mb->num_val : 8192;
    long long lim = limit ? (long long)limit->num_val : 0;
    if (!lim) return json_number((double)d);
    long long limit_mb = lim / (1024LL * 1024LL);
    long long sized = (long long)(limit_mb * 0.75);
    if (sized >= d) return json_number((double)d);
    long long result = (limit_mb > 2048) ? (sized > 1536 ? sized : 1536) : sized;
    return json_number((double)result);
}

json_t *w13_maintuila_9fecf1_normalize_toolsets(const struct json_t *toolsets) {
    if (!truthy(toolsets)) return json_array();
    return split_comma_impl(toolsets, 0);
}
