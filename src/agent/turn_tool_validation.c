/*
 * turn_tool_validation.c — C home for upstream Hermes agent/turn_tool_validation.py.
 * PoP: w13_turntoolv_de0ba5_preview_name @ agent/turn_tool_validation.py:_preview_name
 * PoP: w13_turntoolv_de0ba5_validate_facts @ agent/turn_tool_validation.py:validate_tool_calls
 *
 * The verdict-level decisions of validate_tool_calls: mixed-batch detection,
 * the 3-strike invalid-name exit, and the invalid-JSON retry/truncated-args
 * exit.  Repairs, deferred-call routing, message mutation and hook side
 * effects are dependencies and are stubbed off by the oracle.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_turntoolv_de0ba5.h"

static char *tv_dup(const char *s) { size_t n = strlen(s); char *o = (char *)malloc(n + 1); memcpy(o, s, n + 1); return o; }
static char *tv_dupn(const char *s, size_t n);
static char *tv_py_str(json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return tv_dup("None");
    if (v->type == JSON_BOOL) return tv_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return tv_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%g", v->num_val);
        return tv_dup(buf);
    }
    return tv_dup("");
}
static const char *tv_str(json_t *v) { return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL; }
static size_t tv_utf8_count(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static char *tv_utf8_cut(const char *s, size_t nchars) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && n < nchars) { if ((*p & 0xC0) != 0x80) n++; p++; }
    while (*p && (*p & 0xC0) == 0x80) p++; /* finish the last counted codepoint */
    return tv_dupn(s, (size_t)(p - (const unsigned char *)s));
}
static char *tv_dupn(const char *s, size_t n) { char *o = (char *)malloc(n + 1); if (n) memcpy(o, s, n); o[n] = 0; return o; }
static char *tv_rstrip(const char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    return tv_dupn(s, n);
}
static int tv_json_valid(const char *s) {
    json_t *v = json_parse(s, NULL);
    if (!v) return 0;
    json_free(v);
    return 1;
}
static int tv_name_in(json_t *names, const char *name) {
    if (!name || !names || names->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < names->c.count; i++) {
        const char *s = tv_str(names->c.items[i]);
        if (s && !strcmp(s, name)) return 1;
    }
    return 0;
}
static json_t *tv_partial(const char *final_response) {
    json_t *o = json_object();
    json_set(o, "final_response", json_string(final_response));
    json_set(o, "partial", json_bool(1));
    return o;
}

json_t *w13_turntoolv_de0ba5_preview_name(json_t *name) {
    const char *s = tv_str(name);
    if (!s) s = "";
    if (tv_utf8_count(s) > 80) {
        char *head = tv_utf8_cut(s, 80);
        size_t n = strlen(head) + 3 + 1;
        char *o = (char *)malloc(n);
        snprintf(o, n, "%s...", head);
        free(head);
        json_t *out = json_string(o);
        free(o);
        return out;
    }
    return json_string(s);
}

json_t *w13_turntoolv_de0ba5_validate_facts(json_t *tool_calls, json_t *valid_names,
        json_t *invalid_tool_retries, json_t *invalid_json_retries, json_t *finish_reason) {
    size_t n = (tool_calls && tool_calls->type == JSON_ARRAY) ? tool_calls->c.count : 0;
    int itr = (invalid_tool_retries && invalid_tool_retries->type == JSON_NUMBER)
        ? (int)invalid_tool_retries->num_val : 0;
    int ijr = (invalid_json_retries && invalid_json_retries->type == JSON_NUMBER)
        ? (int)invalid_json_retries->num_val : 0;

    const char *first_invalid = NULL;
    int n_invalid = 0, n_valid = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *fn = json_obj_get(tool_calls->c.items[i], "function");
        const char *name = tv_str(json_obj_get(fn, "name"));
        if (name && tv_name_in(valid_names, name)) n_valid++;
        else { n_invalid++; if (!first_invalid) first_invalid = name ? name : ""; }
    }
    int mixed = n_invalid > 0 && n_valid > 0;

    json_t *out = json_object();
    json_set(out, "mixed", json_bool(mixed ? 1 : 0));

    /* A mixed batch does NOT exit: it resets the strike counter, errors only
     * the invalid calls, and still runs the valid ones (falls through below). */
    if (mixed) itr = 0;
    if (n_invalid > 0 && !mixed) {
        itr++;
        if (itr >= 3) {
            char *prev = tv_dupn(first_invalid ? first_invalid : "", tv_utf8_count(first_invalid ? first_invalid : "") > 80 ? 80 : strlen(first_invalid ? first_invalid : ""));
            if (tv_utf8_count(first_invalid ? first_invalid : "") > 80) {
                char *h = tv_utf8_cut(first_invalid, 80);
                free(prev);
                size_t nn = strlen(h) + 4 + strlen("Model generated invalid tool call: ");
                prev = (char *)malloc(nn);
                snprintf(prev, nn, "Model generated invalid tool call: %s...", h);
                free(h);
            } else {
                char *p2 = (char *)malloc(strlen("Model generated invalid tool call: ") + strlen(prev) + 1);
                snprintf(p2, strlen("Model generated invalid tool call: ") + strlen(prev) + 1,
                         "Model generated invalid tool call: %s", prev);
                free(prev);
                prev = p2;
            }
            json_set(out, "action", json_string("return"));
            json_set(out, "result", tv_partial(prev));
            free(prev);
        } else {
            json_set(out, "action", json_string("continue"));
            json_set(out, "result", (json_t *)json_null());
        }
        return out;
    }

    /* all names valid — argument JSON validation */
    int bad = 0, truncated = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *fn = json_obj_get(tool_calls->c.items[i], "function");
        const char *cname = tv_str(json_obj_get(fn, "name"));
        /* a mixed-batch invalid-name call never executes; its broken args are ignored */
        if (mixed && !(cname && tv_name_in(valid_names, cname))) continue;
        json_t *args = json_obj_get(fn, "arguments");
        if (args && (args->type == JSON_OBJECT || args->type == JSON_ARRAY)) continue; /* json.dumps -> ends in }/] */
        char *s;
        if (args && args->type == JSON_STRING) s = tv_dup(args->str_val ? args->str_val : "");
        else if (!args || args->type == JSON_NULL) s = tv_dup("");
        else s = tv_py_str(args);
        const char *t = s;
        while (*t && isspace((unsigned char)*t)) t++;
        if (!*t) { free(s); s = tv_dup("{}"); t = s; }
        (void)t;
        if (!tv_json_valid(s)) {
            bad = 1;
            char *r = tv_rstrip(s);
            size_t L = strlen(r);
            if (!(L && (r[L - 1] == '}' || r[L - 1] == ']'))) truncated = 1;
            free(r);
        }
        free(s);
    }
    if (bad) {
        if (truncated) {
            const char *key = (finish_reason && finish_reason->type == JSON_STRING && finish_reason->str_val &&
                               !strcmp(finish_reason->str_val, "length")) ? "truncated" : "truncated_unreported";
            json_set(out, "action", json_string("return"));
            json_set(out, "result", tv_partial(key));
        } else {
            ijr++;
            if (ijr >= 3) ijr = 0;
            json_set(out, "action", json_string("continue"));
            json_set(out, "result", (json_t *)json_null());
        }
    } else {
        json_set(out, "action", json_string("ok"));
        json_set(out, "result", (json_t *)json_null());
    }
    return out;
}
