/*
 * tool_history_as_text.c — C home for upstream Hermes agent/tool_history_as_text.py.
 * PoP: w13_toolhistor_2d13fe_text_of @ agent/tool_history_as_text.py:_text_of
 * PoP: w13_toolhistor_2d13fe_history @ agent/tool_history_as_text.py:tool_history_for_request
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_toolhistor_2d13fe.h"

static char *th_dupn(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (n) memcpy(o, s, n);
    o[n] = 0;
    return o;
}
static char *th_dups(const char *s) { s = s ? s : ""; return th_dupn(s, strlen(s)); }
static int th_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count > 0;
}
static char *th_py_repr(json_t *v);
/* Python str() for the scalar types that reach these helpers. */
static char *th_py_str(json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return th_dups("None");
    if (v->type == JSON_BOOL) return th_dups(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return th_dups(v->str_val);
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%g", v->num_val);
        return th_dups(buf);
    }
    return th_py_repr(v);
}
/* Python repr() for containers (dict/list) used by str() on non-scalars. */
static char *th_stresc(const char *s, char quote) {
    size_t n = strlen(s);
    char *o = (char *)malloc(n * 4 + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\\' || c == quote) { o[k++] = '\\'; o[k++] = c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if ((unsigned char)c < 32) { char b[8]; snprintf(b, sizeof(b), "\\x%02x", (unsigned char)c); size_t L = strlen(b); memcpy(o + k, b, L); k += L; }
        else o[k++] = c;
    }
    o[k] = 0;
    return o;
}
static char *th_py_repr(json_t *v) {
    if (!v || v->type == JSON_NULL) return th_dups("None");
    if (v->type == JSON_BOOL) return th_dups(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return th_py_str(v);
    if (v->type == JSON_STRING) {
        char q = (strchr(v->str_val, '\'') && !strchr(v->str_val, '"')) ? '"' : '\'';
        char *e = th_stresc(v->str_val ? v->str_val : "", q);
        size_t L = strlen(e);
        char *o = (char *)malloc(L + 3);
        o[0] = q;
        memcpy(o + 1, e, L);
        o[L + 1] = q;
        o[L + 2] = 0;
        free(e);
        return o;
    }
    size_t n = v->c.count;
    char **items = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t total = 2; /* brackets/braces + possible nul accounted below */
    for (size_t i = 0; i < n; i++) {
        if (v->type == JSON_ARRAY) {
            items[i] = th_py_repr(v->c.items[i]);
        } else {
            const char *key = v->c.keys[i] ? v->c.keys[i] : "";
            char *kr = th_py_repr(json_string(key));
            char *vr = th_py_repr(v->c.items[i]);
            size_t L = strlen(kr) + 2 + strlen(vr);
            items[i] = (char *)malloc(L + 1);
            snprintf(items[i], L + 1, "%s: %s", kr, vr);
            free(kr);
            free(vr);
        }
        total += strlen(items[i]) + (i ? 2 : 0);
    }
    char open = v->type == JSON_ARRAY ? '[' : '{';
    char close = v->type == JSON_ARRAY ? ']' : '}';
    char *o = (char *)malloc(total + 1);
    size_t k = 0;
    o[k++] = open;
    for (size_t i = 0; i < n; i++) {
        if (i) { o[k++] = ','; o[k++] = ' '; }
        size_t L = strlen(items[i]);
        memcpy(o + k, items[i], L);
        k += L;
        free(items[i]);
    }
    free(items);
    o[k++] = close;
    o[k] = 0;
    return o;
}
/* F-string helper: name/arguments with `or` fallbacks, coerced with str(). */
static char *th_or_str(json_t *v, const char *fallback) {
    if (!th_truthy(v)) return th_dups(fallback);
    return th_py_str(v);
}

json_t *w13_toolhistor_2d13fe_text_of(json_t *content) {
    if (!content || content->type == JSON_NULL) return json_string("");
    if (content->type == JSON_STRING) return json_string(content->str_val ? content->str_val : "");
    if (content->type == JSON_ARRAY) {
        size_t n = content->c.count;
        char **parts = (char **)calloc(n ? n : 1, sizeof(char *));
        size_t m = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *p = content->c.items[i];
            if (p && p->type == JSON_STRING) {
                parts[m++] = th_dups(p->str_val);
                continue;
            }
            if (p && p->type == JSON_OBJECT) {
                json_t *t = json_obj_get(p, "type");
                int ok = (!t) || (t->type == JSON_NULL) ||
                         (t->type == JSON_STRING && t->str_val && !strcmp(t->str_val, "text"));
                if (ok) {
                    json_t *txt = json_obj_get(p, "text");
                    if (th_truthy(txt) && txt->type == JSON_STRING) parts[m++] = th_dups(txt->str_val);
                    else parts[m++] = th_dups("");
                    continue;
                }
            }
            parts[m++] = th_dups("[non-text content]");
        }
        size_t tot = 0;
        for (size_t i = 0; i < m; i++) tot += strlen(parts[i]) + (i ? 1 : 0);
        char *res = (char *)malloc(tot + 1);
        size_t k = 0;
        for (size_t i = 0; i < m; i++) {
            if (i) res[k++] = '\n';
            size_t L = strlen(parts[i]);
            memcpy(res + k, parts[i], L);
            k += L;
            free(parts[i]);
        }
        free(parts);
        res[k] = 0;
        json_t *out = json_string(res);
        free(res);
        return out;
    }
    char *s = th_py_str(content);
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w13_toolhistor_2d13fe_history(json_t *messages, json_t *tools, json_t *tool_choice) {
    int choice_none = tool_choice && tool_choice->type == JSON_STRING && tool_choice->str_val &&
                      !strcmp(tool_choice->str_val, "none");
    if (th_truthy(tools) && !choice_none)
        return json_copy(messages ? messages : (json_t *)json_null());

    size_t mn = (messages && messages->type == JSON_ARRAY) ? messages->c.count : 0;

    int any = 0;
    for (size_t i = 0; i < mn && !any; i++) {
        json_t *m = messages->c.items[i];
        json_t *role = json_obj_get(m, "role");
        if (role && role->type == JSON_STRING && role->str_val && !strcmp(role->str_val, "tool")) { any = 1; break; }
        if (th_truthy(json_obj_get(m, "tool_calls"))) { any = 1; break; }
    }
    if (!any) return json_copy(messages ? messages : (json_t *)json_null());

    json_t *out = json_array();
    for (size_t i = 0; i < mn; i++) {
        json_t *m = messages->c.items[i];
        json_t *role = json_obj_get(m, "role");
        int is_tool = role && role->type == JSON_STRING && role->str_val && !strcmp(role->str_val, "tool");
        json_t *tcalls = json_obj_get(m, "tool_calls");
        int is_assistant_calls = role && role->type == JSON_STRING && role->str_val &&
                                 !strcmp(role->str_val, "assistant") && th_truthy(tcalls);

        if (is_tool) {
            json_t *o = json_object();
            json_set(o, "role", json_string("assistant"));
            char *id = th_or_str(json_obj_get(m, "tool_call_id"), "");
            json_t *tv = w13_toolhistor_2d13fe_text_of(json_obj_get(m, "content"));
            const char *tt = (tv && tv->type == JSON_STRING && tv->str_val) ? tv->str_val : "";
            size_t need = strlen("[Tool result ") + strlen(id) + strlen("]: ") + strlen(tt) + 1;
            char *content = (char *)malloc(need);
            snprintf(content, need, "[Tool result %s]: %s", id, tt);
            json_set(o, "content", json_string(content));
            free(content);
            free(id);
            json_free(tv);
            json_append(out, o);
        } else if (is_assistant_calls) {
            size_t cn = (tcalls && tcalls->type == JSON_ARRAY) ? tcalls->c.count : 0;
            char **lines = (char **)calloc(cn ? cn : 1, sizeof(char *));
            size_t ln = 0;
            for (size_t j = 0; j < cn; j++) {
                json_t *tc = tcalls->c.items[j];
                if (!tc || tc->type != JSON_OBJECT) continue;
                json_t *fn = json_obj_get(tc, "function");
                if (!th_truthy(fn) || fn->type != JSON_OBJECT) fn = NULL;
                char *name = fn ? th_or_str(json_obj_get(fn, "name"), "tool") : th_dups("tool");
                char *args = fn ? th_or_str(json_obj_get(fn, "arguments"), "") : th_dups("");
                size_t need = 2 + strlen(name) + 1 + strlen(args) + 1 + 1;
                char *line = (char *)malloc(need);
                snprintf(line, need, "  %s(%s)", name, args);
                free(name);
                free(args);
                lines[ln++] = line;
            }
            size_t callslen = 0;
            for (size_t j = 0; j < ln; j++) callslen += strlen(lines[j]) + (j ? 1 : 0);
            char *calls = (char *)malloc(callslen + 1);
            size_t k = 0;
            for (size_t j = 0; j < ln; j++) {
                if (j) calls[k++] = '\n';
                size_t L = strlen(lines[j]);
                memcpy(calls + k, lines[j], L);
                k += L;
                free(lines[j]);
            }
            calls[k] = 0;
            free(lines);

            json_t *tv = w13_toolhistor_2d13fe_text_of(json_obj_get(m, "content"));
            const char *tt = (tv && tv->type == JSON_STRING && tv->str_val) ? tv->str_val : "";
            size_t need = strlen(tt) + (tt[0] ? 1 : 0) + strlen("[Tool calls:\n") + strlen(calls) + strlen("\n]") + 1;
            char *content = (char *)malloc(need);
            snprintf(content, need, "%s%s[Tool calls:\n%s\n]", tt[0] ? tt : "", tt[0] ? "\n" : "", calls);
            free(calls);
            json_free(tv);

            json_t *o = json_object();
            size_t msz = (m && m->type == JSON_OBJECT) ? json_object_size(m) : 0;
            for (size_t z = 0; z < msz; z++) {
                const char *key = json_object_get_key_at(m, z);
                if (!key || !strcmp(key, "tool_calls")) continue;
                json_set(o, key, json_copy(json_object_get_at(m, z)));
            }
            json_set(o, "content", json_string(content));
            free(content);
            json_append(out, o);
        } else {
            json_append(out, json_copy(m));
        }
    }
    return out;
}
