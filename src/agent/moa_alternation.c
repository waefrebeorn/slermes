/*
 * moa_alternation.c — C port of upstream Hermes agent/moa_alternation.py.
 *
 * PoP: moaalt_destination_key @ agent/moa_alternation.py:destination_key
 * PoP: moaalt_merge_same_role_messages @ agent/moa_alternation.py:merge_same_role_messages
 * PoP: moaalt_is_role_alternation_rejection @ agent/moa_alternation.py:is_role_alternation_rejection
 *
 * The reactive same-role merge for the MoA aggregator request. merge uses the
 * faithful _merge_user_content semantics from agent_runtime_helpers.py.
 */
#include "noc_w7_moaalternat_e7a67f.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
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

/* str() coercion for the scalar JSON values a runtime dict carries. */
static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { buf[0] = '\0'; return; }  /* the `or ""` fallback */
    switch (v->type) {
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, cap, "%lld", (long long)v->num_val);
            else
                snprintf(buf, cap, "%.17g", v->num_val);
            return;
        default: buf[0] = '\0'; return;
    }
}

static void strip_rstrip_slash(char *s) {
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    while (b > a && s[b - 1] == '/') b--;
    memmove(s, s + a, b - a);
    s[b - a] = '\0';
}

json_t *moaalt_destination_key(const struct json_t *runtime) {
    const json_t *route_v = json_obj_get(runtime, "base_url");
    if (!truthy(route_v)) route_v = json_obj_get(runtime, "provider");
    if (!truthy(route_v)) route_v = NULL;
    const json_t *model_v = json_obj_get(runtime, "model");
    if (!truthy(model_v)) model_v = NULL;

    char route[8192], model[8192];
    py_str(route_v, route, sizeof(route));
    py_str(model_v, model, sizeof(model));
    strip_rstrip_slash(route);
    /* model: str(...).strip() only */
    size_t n = strlen(model), a = 0, b = n;
    while (a < b && isspace((unsigned char)model[a])) a++;
    while (b > a && isspace((unsigned char)model[b - 1])) b--;
    memmove(model, model + a, b - a);
    model[b - a] = '\0';

    json_t *out = json_array();
    json_append(out, json_string(route));
    json_append(out, json_string(model));
    return out;
}

/* Faithful port of _merge_user_content; NULL return == _UNMERGEABLE. */
static json_t *merge_contents(const json_t *a, const json_t *b) {
    int as = a && a->type == JSON_STRING, bs = b && b->type == JSON_STRING;
    int aa = a && a->type == JSON_ARRAY, ba = b && b->type == JSON_ARRAY;
    const char *av = (as && a->str_val) ? a->str_val : "";
    const char *bv = (bs && b->str_val) ? b->str_val : "";
    if (as && bs) {
        char *buf = (char *)malloc(strlen(av) + strlen(bv) + 4);
        if (!buf) return NULL;
        buf[0] = '\0';
        strcat(buf, av);
        if (av[0] && bv[0]) strcat(buf, "\n\n");
        strcat(buf, bv);
        json_t *r = json_string(buf);
        free(buf);
        return r;
    }
    if (aa && ba) {
        json_t *r = json_array();
        for (size_t i = 0; i < json_len(a); i++) json_append(r, json_copy(json_get(a, i)));
        for (size_t i = 0; i < json_len(b); i++) json_append(r, json_copy(json_get(b, i)));
        return r;
    }
    if (aa && bs) {
        json_t *r = json_array();
        for (size_t i = 0; i < json_len(a); i++) json_append(r, json_copy(json_get(a, i)));
        if (bv[0]) {
            json_t *part = json_object();
            json_set(part, "type", json_string("text"));
            json_set(part, "text", json_string(bv));
            json_append(r, part);
        }
        return r;
    }
    if (as && ba) {
        json_t *r = json_array();
        if (av[0]) {
            json_t *part = json_object();
            json_set(part, "type", json_string("text"));
            json_set(part, "text", json_string(av));
            json_append(r, part);
        }
        for (size_t i = 0; i < json_len(b); i++) json_append(r, json_copy(json_get(b, i)));
        return r;
    }
    return NULL; /* unmergeable (dict content, explicit null, mixed unknown shapes) */
}

json_t *moaalt_merge_same_role_messages(const struct json_t *messages) {
    json_t *merged = json_array();
    size_t n = json_is_array(messages) ? json_len(messages) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *msg = json_get(messages, i);
        if (!json_is_object(msg)) { json_append(merged, json_copy(msg)); continue; }
        const json_t *prev = json_array_size(merged) ? json_get(merged, json_len(merged) - 1) : NULL;
        const char *prole = json_get_str(prev, "role", "");
        const char *mrole = json_get_str(msg, "role", "");
        json_t *content = NULL;
        if (prev && !strcmp(prole, "user") && !strcmp(mrole, "user")) {
            const json_t *ca = json_obj_get(prev, "content");
            const json_t *cb = json_obj_get(msg, "content");
            json_t *ta = NULL, *tb = NULL;
            if (!ca) { ta = json_string(""); ca = ta; }
            if (!cb) { tb = json_string(""); cb = tb; }
            content = merge_contents(ca, cb);
            json_free(ta); json_free(tb);
        }
        if (!content) {
            json_append(merged, json_copy(msg));
        } else {
            json_t *replacement = json_copy(prev);
            json_set(replacement, "content", content);
            json_array_set(merged, json_len(merged) - 1, replacement);
        }
    }
    return merged;
}

int moaalt_is_role_alternation_rejection(int classified, const struct json_t *runtime) {
    (void)runtime;
    return classified ? 1 : 0;
}
