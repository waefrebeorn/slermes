/*
 * tool_progress.c — C home for upstream Hermes tui_gateway/tool_progress.py.
 *
 * PoP: toolprogr_cap_tui_verbose_text @ tui_gateway/tool_progress.py:_cap_tui_verbose_text
 * PoP: toolprogr_fmt_tool_duration @ tui_gateway/tool_progress.py:_fmt_tool_duration
 * PoP: toolprogr_count_list @ tui_gateway/tool_progress.py:_count_list
 * PoP: toolprogr_tool_summary @ tui_gateway/tool_progress.py:_tool_summary
 * PoP: toolprogr_normalize_todo_state @ tui_gateway/tool_progress.py:_normalize_todo_state
 * PoP: toolprogr_tool_result_needs_user @ tui_gateway/tool_progress.py:_tool_result_needs_user
 * PoP: toolprogr_int_or_skip @ tui_gateway/tool_progress.py:_int_or_skip
 * PoP: toolprogr_str_list @ tui_gateway/tool_progress.py:_str_list
 *
 * Faithful C11 ports of the deterministic helpers. Text is measured in bytes;
 * fixtures use ASCII so byte length == Python code-point length (documented caveat).
 */
#include "noc_w13_toolprogre_d54862.h"
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(char c) { return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v'; }
static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupn("", 0);
    if (v->type == JSON_STRING) return dupn(v->str_val ? v->str_val : "", strlen(v->str_val ? v->str_val : ""));
    if (v->type == JSON_BOOL) return dupn(v->bool_val ? "True" : "False", v->bool_val ? 5 : 6);
    if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val))
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupn(buf, strlen(buf));
    }
    return dupn("None", 4);
}

/* Python round() uses round-half-to-even. */
static long long py_round(double x) { return (long long)nearbyint(x); }

char *toolprogr_cap_tui_verbose_text(const char *text) {
    if (!text) text = "";
    size_t len = strlen(text);
    size_t nl = 0;
    for (size_t i = 0; i < len; ++i) if (text[i] == '\n') ++nl;
    if (len <= 1000 && nl < 16) return dupn(text, len);

    size_t line_start = 0;
    if (nl >= 16) {
        size_t to_skip = nl - 15;   /* pass this many newlines */
        size_t seen = 0;
        for (size_t i = 0; i < len; ++i) {
            if (text[i] == '\n') {
                ++seen;
                if (seen == to_skip) { line_start = i + 1; break; }
            }
        }
    }
    size_t start = line_start;
    if (len - (line_start > len ? len : line_start) > 1000 || len > 1000 + line_start) {
        size_t budget = len - 1000;
        if (budget > start) start = budget;
    } else {
        size_t budget = len > 1000 ? len - 1000 : 0;
        if (budget > start) start = budget;
    }
    if (start > line_start) {
        const char *p = memchr(text + start, '\n', len - start);
        if (p) {
            size_t nb = (size_t)(p - text);
            if (nb < len - 1) start = nb + 1;
        }
    }
    /* tail = text[start:].lstrip() */
    size_t ts = start;
    while (ts < len && is_ws(text[ts])) ++ts;
    size_t tlen = len - ts;
    size_t omitted_chars = len - tlen;
    size_t omitted_lines = 0;
    for (size_t i = 0; i < start; ++i) if (text[i] == '\n') ++omitted_lines;

    char head[160];
    if (omitted_lines)
        snprintf(head, sizeof(head), "[showing verbose tail; omitted %zu lines / %zu chars]\n",
                 omitted_lines, omitted_chars);
    else
        snprintf(head, sizeof(head), "[showing verbose tail; omitted %zu chars]\n", omitted_chars);
    size_t hl = strlen(head);
    char *out = (char *)malloc(hl + tlen + 1);
    if (out) { memcpy(out, head, hl); memcpy(out + hl, text + ts, tlen); out[hl + tlen] = '\0'; }
    return out;
}

char *toolprogr_fmt_tool_duration(const struct json_t *seconds) {
    char buf[64];
    if (!seconds || seconds->type == JSON_NULL) return dupn("", 0);
    double s = (seconds->type == JSON_NUMBER) ? seconds->num_val : 0.0;
    if (s < 10) {
        snprintf(buf, sizeof(buf), "%.1fs", s);
    } else if (s < 60) {
        snprintf(buf, sizeof(buf), "%llds", py_round(s));
    } else {
        long long q = py_round(s);
        long long mins = q / 60, secs = q % 60;
        if (secs) snprintf(buf, sizeof(buf), "%lldm %llds", mins, secs);
        else      snprintf(buf, sizeof(buf), "%lldm", mins);
    }
    return dupn(buf, strlen(buf));
}

struct json_t *toolprogr_count_list(const struct json_t *obj, const struct json_t *path) {
    const json_t *cur = obj;
    if (path && path->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(path); ++i) {
            const json_t *kj = json_get(path, i);
            if (!cur || cur->type != JSON_OBJECT) return json_null();
            const char *k = (kj && kj->type == JSON_STRING) ? kj->str_val : NULL;
            cur = k ? json_obj_get(cur, k) : NULL;
        }
    }
    if (cur && cur->type == JSON_ARRAY) return json_number((double)json_len(cur));
    return json_null();
}

/* int(v) with TypeError/ValueError -> None; also powers `or 0`. */
static int py_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) ++s;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end && isspace((unsigned char)*end)) ++end;
        if (*end) return 0;
        *out = x; return 1;
    }
    return 0;
}
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}
static char *strip_str(const char *s, size_t n) {
    size_t a = 0, b = n;
    while (a < b && is_ws(s[a])) ++a;
    while (b > a && is_ws(s[b - 1])) --b;
    return dupn(s + a, b - a);
}

struct json_t *toolprogr_tool_summary(const char *name, const char *result,
                                      const struct json_t *duration_s) {
    json_t *data = json_parse(result ? result : "", NULL);
    if (!data || data->type != JSON_OBJECT) { if (data) json_free(data); return json_null(); }
    char *dur = toolprogr_fmt_tool_duration(duration_s);
    char suffix[64];
    if (dur[0]) snprintf(suffix, sizeof(suffix), " in %s", dur);
    else suffix[0] = '\0';
    free(dur);

    json_t *wv = json_obj_get(data, "fallback_warning");
    char *wrn = NULL;
    if (wv && wv->type == JSON_STRING) wrn = strip_str(wv->str_val, strlen(wv->str_val));
    if (wrn && wrn[0]) {
        char *out = (char *)malloc(strlen(wrn) + strlen(suffix) + 1);
        if (out) sprintf(out, "%s%s", wrn, suffix);
        free(wrn); json_free(data);
        if (!out) return json_null();
        json_t *r = json_string(out); free(out); return r;
    }
    free(wrn);

    long long n = 0;
    int have = 0;
    const char *verb = NULL, *one = NULL, *many = NULL;
    if (name && !strcmp(name, "web_search")) {
        json_t *c = toolprogr_count_list(data, NULL); /* placeholder, replaced below */
        json_free(c);
        /* _count_list(data,"data","web") */
        json_t *d = json_obj_get(data, "data");
        json_t *w = d && d->type == JSON_OBJECT ? json_obj_get(d, "web") : NULL;
        if (w && w->type == JSON_ARRAY) { n = (long long)json_len(w); have = 1; }
        verb = "Did"; one = "search"; many = "searches";
    } else if (name && !strcmp(name, "web_extract")) {
        json_t *r1 = json_obj_get(data, "results");
        if (r1 && r1->type == JSON_ARRAY && json_len(r1) > 0) { n = (long long)json_len(r1); have = 1; }
        if (!have) {
            json_t *d = json_obj_get(data, "data");
            json_t *r2 = d && d->type == JSON_OBJECT ? json_obj_get(d, "results") : NULL;
            if (r2 && r2->type == JSON_ARRAY) { n = (long long)json_len(r2); have = 1; }
        }
        verb = "Extracted"; one = "page"; many = "pages";
    }
    json_free(data);
    if (!have) return json_null();
    char out[256];
    snprintf(out, sizeof(out), "%s %lld %s%s", verb, n, (n == 1) ? one : many, suffix);
    return json_string(out);
}

struct json_t *toolprogr_normalize_todo_state(const struct json_t *value) {
    if (!value || value->type != JSON_OBJECT) return json_null();
    json_t *todos = json_obj_get(value, "todos");
    if (!todos || todos->type != JSON_ARRAY) return json_null();
    json_t *rv = json_obj_get(value, "revision");
    long long revision = 0;
    if (rv && rv->type != JSON_NULL && rv->type != JSON_BOOL) {
        if (rv->type == JSON_NUMBER) {
            if (rv->num_val != 0.0) revision = (long long)rv->num_val;
        } else if (rv->type == JSON_STRING) {
            if (rv->str_val && rv->str_val[0]) { if (!py_int(rv, &revision)) return json_null(); }
        } else return json_null();
    } else if (rv && rv->type == JSON_BOOL && rv->bool_val) revision = 1;
    if (revision < 0) revision = 0;
    if (json_len(todos) == 0 && revision == 0) return json_null();
    json_t *out = json_object();
    json_set(out, "todos", json_copy(todos));
    json_set(out, "revision", json_number((double)revision));
    return out;
}

struct json_t *toolprogr_tool_result_needs_user(const char *result) {
    json_t *data = json_parse(result ? result : "", NULL);
    if (!data || data->type != JSON_OBJECT) { if (data) json_free(data); return json_bool(false); }
    int need = 0;
    json_t *v = json_obj_get(data, "success");
    if (v && v->type == JSON_BOOL && !v->bool_val) need = 1;
    v = json_obj_get(data, "ok");
    if (v && v->type == JSON_BOOL && !v->bool_val) need = 1;
    if (!need) {
        v = json_obj_get(data, "error");
        if (v && v->type == JSON_STRING) {
            char *s = strip_str(v->str_val, strlen(v->str_val));
            if (s[0]) need = 1;
            free(s);
        }
    }
    if (!need) {
        v = json_obj_get(data, "exit_code");
        if (v && v->type == JSON_NUMBER && v->num_val != 0.0 &&
            v->num_val == floor(v->num_val)) need = 1;
    }
    json_free(data);
    return json_bool(need ? true : false);
}

struct json_t *toolprogr_int_or_skip(const struct json_t *v) {
    long long x = 0;
    if (!py_int(v, &x)) return json_null();
    return json_number((double)x);
}

struct json_t *toolprogr_str_list(const struct json_t *v) {
    json_t *out = json_array();
    if (!v || v->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(v); ++i) {
        char *s = py_str(json_get(v, i));
        json_append(out, json_string(s ? s : ""));
        free(s);
    }
    return out;
}
