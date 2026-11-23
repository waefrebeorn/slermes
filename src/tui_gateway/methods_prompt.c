/*
 * methods_prompt.c — C port of upstream Hermes tui_gateway/methods_prompt.py.
 *
 * PoP: w14_methodspro_1624be_message_row_id @ tui_gateway/methods_prompt.py:_message_row_id
 * PoP: w14_methodspro_1624be_row_ids_of @ tui_gateway/methods_prompt.py:_row_ids_of
 * PoP: w14_methodspro_1624be_pdf_page_range @ tui_gateway/methods_prompt.py:_pdf_page_range
 * PoP: w14_methodspro_1624be_preview_restart_prompt @ tui_gateway/methods_prompt.py:_preview_restart_prompt
 *
 * _err is the injected {code,error} envelope; _PDF_ATTACH_MAX_PAGES and the
 * preview-restart constants are explicit fixture seams.
 */
#include "noc_w14_methodspro_1624be.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupn_local(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static int truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}
/* Python int(v) for JSON scalars. Returns 0 on TypeError/ValueError. */
static int py_int(const struct json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        while (*end && isspace((unsigned char)*end)) end++;
        if (end && *end == '\0') { *out = x; return 1; }
    }
    return 0;
}
static const char *jstr(const struct json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static char *strip_ws(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    return dupn_local(s + a, b - a);
}
static struct json_t *err_obj(int code, const char *msg) {
    struct json_t *o = json_object();
    json_set(o, "code", json_number(code));
    json_set_hdr(o, "error", msg);
    return o;
}
static struct json_t *range_result(const struct json_t *first, const struct json_t *last,
                                   struct json_t *e) {
    struct json_t *o = json_object();
    json_set(o, "first", first ? json_copy(first) : json_null());
    json_set(o, "last", last ? json_copy(last) : json_null());
    json_set(o, "err", e ? e : json_null());
    return o;
}

struct json_t *w14_methodspro_1624be_message_row_id(const struct json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return json_null();
    const struct json_t *raw = json_obj_get(msg, "_row_id");
    if (!raw || raw->type == JSON_NULL) raw = json_obj_get(msg, "row_id");
    long long x = 0;
    if (!py_int(raw, &x)) return json_null();
    return json_number((double)x);
}

struct json_t *w14_methodspro_1624be_row_ids_of(const struct json_t *messages) {
    struct json_t *out = json_array();
    if (!messages || messages->type != JSON_ARRAY) return out;
    size_t n = 0;
    long long vals[4096];
    for (size_t i = 0; i < json_len(messages) && n < 4096; ++i) {
        const struct json_t *m = json_get(messages, i);
        if (!m || m->type != JSON_OBJECT) continue;
        const struct json_t *rid = json_obj_get(m, "_row_id");
        if (!rid || rid->type == JSON_NULL) rid = json_obj_get(m, "row_id");
        long long x = 0;
        if (py_int(rid, &x)) vals[n++] = x;
    }
    for (size_t i = 1; i < n; ++i) {
        long long k = vals[i]; size_t j = i;
        while (j > 0 && vals[j-1] > k) { vals[j] = vals[j-1]; j--; }
        vals[j] = k;
    }
    long long prev = 0;
    for (size_t i = 0; i < n; ++i) {
        if (i && vals[i] == prev) continue;          /* Python set semantics */
        prev = vals[i];
        json_append(out, json_number((double)vals[i]));
    }
    return out;
}

struct json_t *w14_methodspro_1624be_pdf_page_range(const struct json_t *params, const struct json_t *max_pages) {
    long long mx = 20;
    py_int(max_pages, &mx);
    const struct json_t *p = (params && params->type == JSON_OBJECT) ? params : NULL;
    long long first = 0, last = 0;
    const struct json_t *fv = p ? json_obj_get(p, "first_page") : NULL;
    struct json_t *v = NULL;
    if (!fv || !truthy(fv)) first = 1;                       /* params.get(...) or 1 */
    else if (!py_int(fv, &first)) v = err_obj(4015, "first_page/last_page must be integers");
    const struct json_t *lv = p ? json_obj_get(p, "last_page") : NULL;
    int has_last = lv && lv->type != JSON_NULL;
    if (!v && has_last && !py_int(lv, &last)) v = err_obj(4015, "first_page/last_page must be integers");
    if (!v && first < 1) v = err_obj(4015, "first_page must be >= 1");
    if (!v && !has_last) last = first + mx - 1;
    if (!v && last < first) v = err_obj(4015, "last_page must be >= first_page");
    if (!v && (last - first + 1) > mx) {
        char msg[160];
        snprintf(msg, sizeof(msg), "page range exceeds cap of %lld pages per attach call", mx);
        v = err_obj(4019, msg);
    }
    if (v) return range_result(NULL, NULL, v);
    struct json_t *a = json_number((double)first), *b = json_number((double)last);
    struct json_t *res = range_result(a, b, NULL);
    json_free(a); json_free(b);
    return res;
}

/* "\n".join(line for line in [...] if line) */
struct json_t *w14_methodspro_1624be_preview_restart_prompt(const struct json_t *url, const struct json_t *cwd,
                                                            const struct json_t *context, const struct json_t *has_history,
                                                            const struct json_t *note, const struct json_t *rules) {
    size_t cap = 2048;
    char *out = (char *)malloc(cap);
    if (!out) return json_string("");
    size_t len = 0;
    #define ADD_LINE(S) do { \
        const char *_s = (S); \
        size_t _n = strlen(_s); \
        if (_n) { \
            if (len + _n + 2 > cap) { cap = (len + _n + 2) * 2; char *_t = (char *)realloc(out, cap); if (!_t) break; out = _t; } \
            if (len) out[len++] = '\n'; \
            memcpy(out + len, _s, _n); len += _n; \
        } \
    } while (0)
    ADD_LINE("The desktop preview pane cannot load a local server URL.");
    char *u = strip_ws(jstr(url));
    char line[1024];
    snprintf(line, sizeof(line), "Preview URL: %s", u); ADD_LINE(line);
    free(u);
    snprintf(line, sizeof(line), "Current working directory: %s", truthy(cwd) ? jstr(cwd) : "(unknown)");
    ADD_LINE(line);
    const char *ctx = jstr(context);
    if (ctx[0]) {
        size_t need = strlen(ctx) + 32;
        char *cl = (char *)malloc(need);
        if (cl) { snprintf(cl, need, "Preview console:\n%s", ctx); ADD_LINE(cl); free(cl); }
    }
    if (truthy(has_history)) ADD_LINE(jstr(note));
    if (rules && rules->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rules); ++i) ADD_LINE(jstr(json_get(rules, i)));
    }
    #undef ADD_LINE
    out[len] = '\0';
    struct json_t *res = json_string(out);
    free(out);
    return res;
}

