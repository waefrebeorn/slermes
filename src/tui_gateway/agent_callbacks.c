/*
 * agent_callbacks.c — faithful C11 port of the pure helpers in
 * tui_gateway/agent_callbacks.py.
 * PoP: w15_agentcallb_f310a6_parse_tui_skills_env        @ tui_gateway/agent_callbacks.py:_parse_tui_skills_env
 * PoP: w15_agentcallb_f310a6_preview_tool_result_preview @ tui_gateway/agent_callbacks.py:_preview_tool_result_preview
 * PoP: w15_agentcallb_f310a6_preview_restart_history     @ tui_gateway/agent_callbacks.py:_preview_restart_history
 *
 * The live functions are data transforms over an env string / JSON tool result
 * / message history. The env read is injected as a resolved fact; all parsing,
 * de-duplication, truncation and role filtering is reproduced exactly.
 */
#include "noc_w15_agentcallb_f310a6.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static char *py_strip(const char *s) {
    if (!s) return dupstr("");
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (p) { memcpy(p, s + a, b - a); p[b - a] = '\0'; }
    return p;
}

static int array_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

json_t *w15_agentcallb_f310a6_parse_tui_skills_env(const char *raw) {
    json_t *out = json_array();
    const char *src = raw ? raw : "";
    size_t n = strlen(src);
    size_t i = 0;
    while (i <= n) {
        size_t j = i;
        while (j < n && src[j] != ',' && src[j] != '\n') j++;
        char *seg = (char *)malloc(j - i + 1);
        if (seg) {
            memcpy(seg, src + i, j - i);
            seg[j - i] = '\0';
            char *part = py_strip(seg);
            if (*part && !array_has_str(out, part)) json_append(out, json_string(part));
            free(part);
            free(seg);
        }
        if (j == n) break;
        i = j + 1;
    }
    return out;
}

/* Python truthiness of a JSON value (for `x or ""`). */
static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:  return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && *v->str_val;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* str() coercion for the scalar JSON values fixtures use. */
static char *py_str_coerce(const json_t *v) {
    if (!v) return dupstr("");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL: return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        }
        default: return dupstr("");
    }
}

static char *stripped_coerce(const json_t *v) {
    char *s = py_str_coerce(v);
    char *out = py_strip(s);
    free(s);
    return out;
}

static char *substr(const char *s, size_t start, size_t end) {
    if (end > start) {
        char *p = (char *)malloc(end - start + 1);
        if (p) { memcpy(p, s + start, end - start); p[end - start] = '\0'; }
        return p;
    }
    return dupstr("");
}

char *w15_agentcallb_f310a6_preview_tool_result_preview(const char *name, const char *result) {
    json_t *data = result ? json_parse(result, NULL) : NULL;
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return dupstr("");
    }
    char *out = NULL;
    if (name && !strcmp(name, "terminal")) {
        json_t *o = json_obj_get(data, "output");
        if (jtruthy(o)) {
            char *s = stripped_coerce(o);
            size_t n = strlen(s);
            if (n > 1200) {
                out = substr(s, n - 1200, n);
            } else {
                out = dupstr(s);
            }
            free(s);
            goto done;
        }
        json_t *sid = json_obj_get(data, "session_id");
        if (jtruthy(sid)) {
            char *s = py_str_coerce(sid);
            size_t n = strlen(s) + 64;
            out = (char *)malloc(n);
            if (out) snprintf(out, n, "Background process started: %s", s);
            free(s);
            goto done;
        }
        json_t *ec = json_obj_get(data, "exit_code");
        if (ec && ec->type != JSON_NULL) {
            char *s = py_str_coerce(ec);
            size_t n = strlen(s) + 64;
            out = (char *)malloc(n);
            if (out) snprintf(out, n, "terminal exited with code %s", s);
            free(s);
            goto done;
        }
    }
    {
        char *e = stripped_coerce(json_obj_get(data, "error"));
        size_t n = strlen(e);
        if (n > 1200) { out = substr(e, 0, 1200); free(e); }
        else out = e;
    }
done:
    json_free(data);
    return out;
}

json_t *w15_agentcallb_f310a6_preview_restart_history(const json_t *messages,
        int max_messages, int max_tool_chars) {
    json_t *out = json_array();
    if (!messages || messages->type != JSON_ARRAY || messages->c.count == 0) return out;
    size_t len = messages->c.count;
    long last_user = -1;
    for (size_t i = len; i > 0; i--) {
        json_t *m = messages->c.items[i - 1];
        if (m && m->type == JSON_OBJECT) {
            const char *role = json_get_str(m, "role", "");
            if (!strcmp(role, "user")) { last_user = (long)(i - 1); break; }
        }
    }
    long start = (long)len - (long)max_messages;
    if (start < 0) start = 0;
    if (last_user >= 0 && last_user < start) start = last_user;
    for (long idx = start; idx < (long)len; idx++) {
        json_t *m = messages->c.items[idx];
        if (!m || m->type != JSON_OBJECT) continue;
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "user") && strcmp(role, "assistant") &&
            strcmp(role, "tool") && strcmp(role, "system")) continue;
        json_t *copy = json_object();
        for (size_t k = 0; k < json_object_size(m); k++) {
            const char *key = json_object_get_key_at(m, k);
            if (!key || !strcmp(key, "reasoning")) continue;
            json_set(copy, key, json_copy(json_object_get_at(m, k)));
        }
        if (!strcmp(role, "tool")) {
            json_t *content = json_obj_get(m, "content");
            if (content && content->type == JSON_STRING && content->str_val &&
                strlen(content->str_val) > (size_t)(max_tool_chars > 0 ? max_tool_chars : 0)) {
                size_t keep = (size_t)(max_tool_chars > 0 ? max_tool_chars : 0);
                size_t n = strlen(content->str_val);
                size_t cap = keep + n + 96;
                char *buf = (char *)malloc(cap);
                if (buf) {
                    memcpy(buf, content->str_val, keep);
                    snprintf(buf + keep, cap - keep, "\n... (truncated, original %zu chars)", n);
                    json_set(copy, "content", json_string(buf));
                    free(buf);
                }
            }
        }
        json_append(out, copy);
    }
    return out;
}
