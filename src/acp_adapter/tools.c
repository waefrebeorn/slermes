/*
 * tools.c — C home for upstream Hermes acp_adapter/tools.py.
 * PoP: w6_tools_9c1fca_tool_kind @ acp_adapter/tools.py:get_tool_kind
 * PoP: w6_tools_9c1fca_plural @ acp_adapter/tools.py:_plural
 * PoP: w6_tools_9c1fca_nonempty @ acp_adapter/tools.py:_nonempty
 * PoP: w6_tools_9c1fca_failure @ acp_adapter/tools.py:_failure
 * PoP: w6_tools_9c1fca_truncate_text @ acp_adapter/tools.py:_truncate_text
 * PoP: w6_tools_9c1fca_fenced_text @ acp_adapter/tools.py:_fenced_text
 * PoP: w6_tools_9c1fca_json_loads_maybe @ acp_adapter/tools.py:_json_loads_maybe
 * PoP: w6_tools_9c1fca_coerce_args @ acp_adapter/tools.py:coerce_tool_args
 * PoP: w6_tools_9c1fca_more @ acp_adapter/tools.py:_more
 * PoP: w6_tools_9c1fca_arg @ acp_adapter/tools.py:_arg
 */
#include "noc_w6_tools_9c1fca.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *py_strip(const char *s) {
    size_t n;
    char *out;
    if (!s) return strdup("");
    while (*s && is_space(*s)) s++;
    n = strlen(s);
    while (n && is_space(s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

/* Python str() for scalar JSON values (numbers are integral in these fixtures). */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    }
    return strdup("");
}

/* ---- tool kind map ------------------------------------------------------- */
static const char *const KINDS[] = {"read", "edit", "search", "execute", "fetch", "other", "think"};
static const char *const *const NAMES[] = {
    (const char *const[]){"read_file","skill_view","skills_list","browser_snapshot","browser_vision","browser_get_images","vision_analyze",NULL},
    (const char *const[]){"write_file","patch","skill_manage",NULL},
    (const char *const[]){"search_files",NULL},
    (const char *const[]){"terminal","process","execute_code","browser_click","browser_type","browser_scroll","browser_press","browser_back","delegate_task","image_generate","text_to_speech",NULL},
    (const char *const[]){"web_search","web_extract","browser_navigate",NULL},
    (const char *const[]){"todo",NULL},
    (const char *const[]){"_thinking",NULL}
};

const char *w6_tools_9c1fca_tool_kind(const char *tool_name) {
    if (!tool_name) return "other";
    for (size_t k = 0; k < sizeof(KINDS)/sizeof(KINDS[0]); k++)
        for (size_t i = 0; NAMES[k][i]; i++)
            if (!strcmp(tool_name, NAMES[k][i])) return KINDS[k];
    return "other";
}

struct json_t *w6_tools_9c1fca_plural(const struct json_t *count, const char *word, const char *suffix) {
    long long n = count ? (long long)count->num_val : 0;
    char buf[128];
    if (!suffix) suffix = "s";
    if (!word) word = "";
    snprintf(buf, sizeof(buf), "%lld %s%s", n, word, n != 1 ? suffix : "");
    return json_string(buf);
}

struct json_t *w6_tools_9c1fca_nonempty(const struct json_t *result) {
    if (result && result->type == JSON_STRING && result->str_val) {
        char *s = py_strip(result->str_val);
        int ok = *s ? 1 : 0;
        free(s);
        if (ok) return json_copy(result);
    }
    return json_null();
}

struct json_t *w6_tools_9c1fca_failure(const struct json_t *data, const char *prefix) {
    const json_t *err;
    int failed;
    char *etxt;
    char *out;
    size_t n;
    if (!prefix) prefix = "";
    failed = 0;
    if (data && data->type == JSON_OBJECT) {
        const json_t *succ = json_obj_get(data, "success");
        if (succ && succ->type == JSON_BOOL && !succ->bool_val) failed = 1;
        err = json_obj_get(data, "error");
        if (py_truthy(err)) failed = 1;
    } else err = NULL;
    if (!failed) return json_null();
    if (!err) etxt = strdup("unknown error");
    else etxt = py_str(err);
    n = strlen(prefix) + strlen(": ") + strlen(etxt) + 1;
    out = (char *)malloc(n);
    snprintf(out, n, "%s: %s", prefix, etxt);
    free(etxt);
    { json_t *r = json_string(out); free(out); return r; }
}

struct json_t *w6_tools_9c1fca_truncate_text(const char *text, long long limit) {
    size_t len;
    char *head, *out;
    json_t *r;
    if (!text) text = "";
    len = strlen(text);
    if ((long long)len <= limit) return json_string(text);
    {
        long long keep = limit - 100;
        if (keep < 0) keep = 0;
        head = (char *)malloc((size_t)keep + 1);
        memcpy(head, text, (size_t)keep);
        head[keep] = '\0';
    }
    out = (char *)malloc(strlen(head) + 128);
    snprintf(out, strlen(head) + 128, "%s\n... (%zu chars total, truncated)", head, len);
    free(head);
    r = json_string(out);
    free(out);
    return r;
}

struct json_t *w6_tools_9c1fca_fenced_text(const char *text, const char *language) {
    size_t cur = 0, seg = 0, longest = 0, fence_len, i, n, pos = 0;
    char *out;
    json_t *r;
    if (!text) text = "";
    if (!language) language = "";
    for (i = 0; text[i]; i++) {
        if (text[i] == '`') {
            if (seg % 2 == 1 && cur > longest) longest = cur;
            seg++;
            cur = 0;
        } else cur++;
    }
    if (seg % 2 == 1 && cur > longest) longest = cur;
    fence_len = longest + 1;
    if (fence_len < 3) fence_len = 3;
    n = fence_len + strlen(language) + 1 + strlen(text) + 1 + fence_len + 1;
    out = (char *)malloc(n);
    for (i = 0; i < fence_len; i++) out[pos++] = '`';
    pos += (size_t)snprintf(out + pos, n - pos, "%s\n%s\n", language, text);
    for (i = 0; i < fence_len; i++) out[pos++] = '`';
    out[pos] = '\0';
    r = json_string(out);
    free(out);
    return r;
}

/* Prefix of s holding the first complete JSON value (handles the "hint" suffix). */
static char *first_json_value(const char *s) {
    size_t i = 0, start, depth = 0;
    int in_str = 0, esc = 0, have = 0;
    char *out;
    while (s[i] && is_space(s[i])) i++;
    start = i;
    if (!s[i]) return NULL;
    if (s[i] == '{' || s[i] == '[') {
        for (; s[i]; i++) {
            char c = s[i];
            if (in_str) {
                if (esc) esc = 0;
                else if (c == '\\') esc = 1;
                else if (c == '"') in_str = 0;
            } else if (c == '"') in_str = 1;
            else if (c == '{' || c == '[') depth++;
            else if (c == '}' || c == ']') {
                if (depth) depth--;
                if (depth == 0) { i++; break; }
            }
        }
        if (depth) return NULL;
    } else if (s[i] == '"') {
        i++;
        while (s[i]) {
            if (esc) esc = 0;
            else if (s[i] == '\\') esc = 1;
            else if (s[i] == '"') { i++; break; }
            i++;
        }
    } else {
        while (s[i] && !is_space(s[i]) && s[i] != ',' && s[i] != '}' && s[i] != ']') i++;
    }
    have = i > start;
    if (!have) return NULL;
    out = (char *)malloc(i - start + 1);
    memcpy(out, s + start, i - start);
    out[i - start] = '\0';
    return out;
}

/* libjson's json_parse() only accepts object/array documents; mirror Python's
 * json.loads() for scalar documents by parsing inside a one-element array. */
static json_t *parse_any(const char *s) {
    json_t *p = json_parse(s, NULL);
    if (p) return p;
    {
        char *wrapped = (char *)malloc(strlen(s) + 3);
        json_t *arr, *item;
        snprintf(wrapped, strlen(s) + 3, "[%s]", s);
        arr = json_parse(wrapped, NULL);
        free(wrapped);
        if (arr && arr->type == JSON_ARRAY && json_len(arr) == 1) {
            item = json_copy(json_get(arr, 0));
            json_free(arr);
            return item;
        }
        if (arr) json_free(arr);
    }
    return NULL;
}

struct json_t *w6_tools_9c1fca_json_loads_maybe(const struct json_t *value) {
    if (!value || value->type != JSON_STRING) return value ? json_copy(value) : json_null();
    {
        json_t *p = parse_any(value->str_val ? value->str_val : "");
        if (p) return p;
    }
    {
        char *prefix = first_json_value(value->str_val ? value->str_val : "");
        json_t *p;
        if (!prefix) return json_null();
        p = parse_any(prefix);
        free(prefix);
        return p ? p : json_null();
    }
}

struct json_t *w6_tools_9c1fca_coerce_args(const struct json_t *raw) {
    json_t *obj;
    const char *s;
    if (!raw) return json_object();
    if (raw->type == JSON_OBJECT) return json_copy(raw);
    if (raw->type == JSON_STRING) {
        json_t *p, *w;
        char *wrapped;
        s = raw->str_val ? raw->str_val : "";
        if (!*s) { obj = json_object(); json_set(obj, "raw", json_copy(raw)); return obj; }
        p = json_parse(s, NULL);
        if (p) {
            if (p->type == JSON_OBJECT) return p;   /* decoded dict: use it */
            json_free(p);                          /* decoded non-dict scalar -> {} */
            return json_object();
        }
        /* libjson rejects bare top-level scalars; wrap to test if s is valid JSON. */
        wrapped = (char *)malloc(strlen(s) + 3);
        snprintf(wrapped, strlen(s) + 3, "[%s]", s);
        w = json_parse(wrapped, NULL);
        free(wrapped);
        if (w) { json_free(w); return json_object(); }
        obj = json_object();
        json_set(obj, "raw", json_copy(raw));
        return obj;
    }
    return json_object();
}

struct json_t *w6_tools_9c1fca_more(const struct json_t *items, long long shown, const char *unit) {
    json_t *arr = json_array();
    size_t n = (items && items->type == JSON_ARRAY) ? json_len(items) : 0;
    if (!unit) unit = "";
    if ((long long)n > shown) {
        char buf[128];
        snprintf(buf, sizeof(buf), "... %lld more%s", (long long)n - shown, unit);
        json_append(arr, json_string(buf));
    }
    return arr;
}

struct json_t *w6_tools_9c1fca_arg(const struct json_t *args, const struct json_t *keys,
                                   const char *default_value) {
    const json_t *first = NULL;
    char *s, *t;
    json_t *r;
    size_t n = (keys && keys->type == JSON_ARRAY) ? json_len(keys) : 0;
    if (!default_value) default_value = "";
    for (size_t i = 0; i < n; i++) {
        const json_t *k = json_get(keys, i);
        const json_t *v;
        if (!k || k->type != JSON_STRING) continue;
        v = (args && args->type == JSON_OBJECT) ? json_obj_get(args, k->str_val) : NULL;
        if (py_truthy(v)) { first = v; break; }
    }
    s = first ? py_str(first) : strdup(default_value);
    t = py_strip(s);
    free(s);
    r = json_string(*t ? t : default_value);
    free(t);
    return r;
}
