/*
 * server.c — C home for upstream Hermes acp_adapter/server.py.
 *
 * PoP: w6_server_e95893_flatten_history_text @ acp_adapter/server.py:_flatten_history_text
 * PoP: w6_server_e95893_history_reasoning_text @ acp_adapter/server.py:_history_reasoning_text
 * PoP: w6_server_e95893_content_refused_notice @ acp_adapter/server.py:_content_refused_notice
 * PoP: w6_server_e95893_attach_interrupted_prompt @ acp_adapter/server.py:_attach_interrupted_prompt
 * PoP: w6_server_e95893_history_tool_call_name_args @ acp_adapter/server.py:_history_tool_call_name_args
 *
 * The ACP agent / session manager / event loop plumbing is runtime concern and
 * is not reimplemented. The tested behaviour is the pure history projection
 * helpers, driven by injected persisted-message data so the gate is
 * deterministic (no connection or event loop needed).
 */
#include "noc_w6_server_e95893.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W6_SERVER_REFUSED_KIND  "content_refused"
#define W6_SERVER_REFUSED_KEY   "content_refused"
#define W6_SERVER_REFUSED_NOTICE "This message was refused by the model's safety filter \xE2\x80\x94 try a different query."

static char *dup_str(const char *s)
{
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

static char *strip_ws(const char *s)
{
    const unsigned char *a, *b;
    char *o;
    size_t n;
    if (!s) return dup_str("");
    n = strlen(s);
    a = (const unsigned char *)s;
    b = a + n;
    while (a < b && isspace(*a)) a++;
    while (b > a && isspace(*(b - 1))) b--;
    o = (char *)malloc((size_t)(b - a) + 1);
    if (!o) return NULL;
    memcpy(o, a, (size_t)(b - a));
    o[b - a] = '\0';
    return o;
}

static int py_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

static char *py_str(const json_t *v)
{
    char buf[64];
    if (!v || v->type == JSON_NULL) return dup_str("");
    switch (v->type) {
        case JSON_STRING: return dup_str(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dup_str(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%g", v->num_val);
            return dup_str(buf);
        default:          return dup_str("");
    }
}

/* _flatten_history_text -> malloc'd stripped string. */
static char *flatten_str(const json_t *value)
{
    char **parts;
    size_t n, i, cnt = 0, total = 0, pos = 0;
    char *joined, *res;

    if (!value || value->type == JSON_NULL) return dup_str("");
    if (value->type == JSON_STRING) return strip_ws(value->str_val ? value->str_val : "");
    if (value->type != JSON_ARRAY) return dup_str("");

    n = json_len(value);
    parts = (char **)calloc(n ? n : 1, sizeof(char *));
    if (!parts) return dup_str("");
    for (i = 0; i < n; i++) {
        json_t *item = json_get(value, i);
        char *part = NULL;
        if (item && item->type == JSON_STRING) {
            part = strip_ws(item->str_val ? item->str_val : "");
        } else if (item && item->type == JSON_OBJECT) {
            json_t *t = json_obj_get(item, "text");
            if (t && t->type == JSON_STRING) {
                part = dup_str(t->str_val ? t->str_val : "");
            } else {
                json_t *ty = json_obj_get(item, "type");
                json_t *c = json_obj_get(item, "content");
                if (ty && ty->type == JSON_STRING && ty->str_val &&
                    !strcmp(ty->str_val, "text") && c && c->type == JSON_STRING)
                    part = dup_str(c->str_val ? c->str_val : "");
            }
            if (part) {
                char *s = strip_ws(part);
                free(part);
                part = s;
            }
        }
        if (part && part[0]) {
            parts[cnt++] = part;
            total += strlen(part);
        } else {
            free(part);
        }
    }
    joined = (char *)malloc(total + cnt + 1);
    if (!joined) {
        for (i = 0; i < cnt; i++) free(parts[i]);
        free(parts);
        return dup_str("");
    }
    for (i = 0; i < cnt; i++) {
        if (i) joined[pos++] = '\n';
        memcpy(joined + pos, parts[i], strlen(parts[i]));
        pos += strlen(parts[i]);
        free(parts[i]);
    }
    joined[pos] = '\0';
    free(parts);
    res = strip_ws(joined);
    free(joined);
    return res;
}

struct json_t *w6_server_e95893_flatten_history_text(const struct json_t *value)
{
    char *s = flatten_str(value);
    json_t *res = json_string(s ? s : "");
    free(s);
    return res;
}

struct json_t *w6_server_e95893_history_reasoning_text(const struct json_t *message)
{
    static const char *keys[2] = {"reasoning_content", "reasoning"};
    int i;
    for (i = 0; i < 2; i++) {
        char *t = flatten_str(message ? json_obj_get(message, keys[i]) : NULL);
        if (t && t[0]) {
            json_t *res = json_string(t);
            free(t);
            return res;
        }
        free(t);
    }
    return json_string("");
}

struct json_t *w6_server_e95893_content_refused_notice(const struct json_t *message)
{
    json_t *dk = message ? json_obj_get(message, "display_kind") : NULL;
    json_t *meta, *owned = NULL, *entry, *notice;
    json_t *res;

    if (!dk || dk->type != JSON_STRING || !dk->str_val ||
        strcmp(dk->str_val, W6_SERVER_REFUSED_KIND))
        return json_null();

    meta = json_obj_get(message, "display_metadata");
    if (meta && meta->type == JSON_STRING && meta->str_val) {
        json_t *parsed = json_parse(meta->str_val, NULL);
        if (parsed && parsed->type == JSON_OBJECT) {
            owned = parsed;
            meta = parsed;
        } else {
            json_free(parsed);
            meta = NULL;
        }
    }
    entry = (meta && meta->type == JSON_OBJECT) ? json_obj_get(meta, W6_SERVER_REFUSED_KEY) : NULL;
    notice = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "notice") : NULL;
    if (notice && notice->type == JSON_STRING && notice->str_val) {
        char *st = strip_ws(notice->str_val);
        int ok = st && st[0];
        free(st);
        res = json_string(ok ? notice->str_val : W6_SERVER_REFUSED_NOTICE);
    } else {
        res = json_string(W6_SERVER_REFUSED_NOTICE);
    }
    json_free(owned);
    return res;
}

struct json_t *w6_server_e95893_attach_interrupted_prompt(const char *interrupted,
                                                          const char *guidance)
{
    const char *a = interrupted ? interrupted : "";
    const char *b = guidance ? guidance : "";
    static const char mid[] = "\n\nUser correction/guidance after interrupt: ";
    size_t n = strlen(a) + sizeof(mid) - 1 + strlen(b) + 1;
    char *s = (char *)malloc(n);
    json_t *res;
    if (!s) return json_null();
    snprintf(s, n, "%s%s%s", a, mid, b);
    res = json_string(s);
    free(s);
    return res;
}

/* coerce_tool_args(): JSON string -> dict, else {"raw": s}; non-dict -> {}. */
static json_t *coerce_args(const json_t *raw)
{
    json_t *o;
    if (!raw) return json_object();
    if (raw->type == JSON_STRING) {
        json_t *parsed = json_parse(raw->str_val ? raw->str_val : "", NULL);
        if (parsed && parsed->type == JSON_OBJECT) return parsed;
        json_free(parsed);
        o = json_object();
        json_set(o, "raw", json_copy(raw));
        return o;
    }
    if (raw->type == JSON_OBJECT) return json_copy(raw);
    return json_object();
}

struct json_t *w6_server_e95893_history_tool_call_name_args(const struct json_t *tool_call)
{
    json_t *fn = tool_call ? json_obj_get(tool_call, "function") : NULL;
    json_t *nv, *raw;
    char *name;
    json_t *out;

    if (!fn || fn->type != JSON_OBJECT) fn = NULL;
    nv = fn ? json_obj_get(fn, "name") : NULL;
    if (!py_truthy(nv)) nv = tool_call ? json_obj_get(tool_call, "name") : NULL;
    if (!py_truthy(nv)) nv = NULL;
    name = nv ? py_str(nv) : dup_str("unknown_tool");

    raw = fn ? json_obj_get(fn, "arguments") : NULL;
    if (!py_truthy(raw)) raw = tool_call ? json_obj_get(tool_call, "arguments") : NULL;
    if (!py_truthy(raw)) raw = tool_call ? json_obj_get(tool_call, "args") : NULL;
    if (!py_truthy(raw)) raw = NULL;

    out = json_array();
    json_append(out, json_string(name));
    json_append(out, coerce_args(raw));
    free(name);
    return out;
}
