/*
 * slash_commands_status.c — C port of upstream Hermes gateway/slash_commands_status.py.
 *
 * PoP: w17_slashcomma_7fa9bc_clean_str @ gateway/slash_commands_status.py:_clean_str
 * PoP: w17_slashcomma_7fa9bc_int_value @ gateway/slash_commands_status.py:_int_value
 * PoP: w17_slashcomma_7fa9bc_fmt @ gateway/slash_commands_status.py:_fmt
 * PoP: w17_slashcomma_7fa9bc_pct @ gateway/slash_commands_status.py:_pct
 * PoP: w17_slashcomma_7fa9bc_clip @ gateway/slash_commands_status.py:_clip
 * PoP: w17_slashcomma_7fa9bc_routes_for_default @ gateway/slash_commands_status.py:_routes_for_default
 */
#include "noc_w17_slashcomma_7fa9bc.h"
#include "hermes_json.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* Python str.strip() default set (ASCII subset used by fixtures). */
static int is_py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

char *w17_slashcomma_7fa9bc_clean_str(const json_t *value) {
    if (!value || value->type != JSON_STRING || !value->str_val) return dupstr("");
    const char *s = value->str_val;
    size_t a = 0, b = strlen(s);
    while (a < b && is_py_ws((unsigned char)s[a])) a++;
    while (b > a && is_py_ws((unsigned char)s[b - 1])) b--;
    if (a == b) return dupstr("");
    char *out = (char *)malloc(b - a + 1);
    if (!out) return dupstr("");
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';           /* write NUL at the shortened length */
    return out;
}

/* int(value) with the (TypeError, ValueError) -> 0 contract. */
long long w17_slashcomma_7fa9bc_int_value(const json_t *value) {
    if (!value) return 0;
    if (value->type == JSON_BOOL) return value->bool_val ? 1 : 0;
    if (value->type == JSON_NUMBER) {
        double d = value->num_val;
        if (d >= 9.22e18 || d <= -9.22e18) return 0;  /* out of int64 fixture domain */
        return (long long)d;                            /* truncates toward zero */
    }
    if (value->type == JSON_STRING) {
        const char *s = value->str_val ? value->str_val : "";
        const char *p = s;
        while (*p && is_py_ws((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && is_py_ws((unsigned char)e[-1])) e--;
        if (e == p || e[-1] == '_') return 0;
        char *buf = (char *)malloc((size_t)(e - p) + 1);
        if (!buf) return 0;
        size_t k = 0;
        int prev_digit = 0;
        for (const char *q = p; q < e; q++) {
            if (*q == '_') {
                if (!prev_digit) { free(buf); return 0; }
                prev_digit = 0;
                continue;
            }
            buf[k++] = *q;
            prev_digit = (*q >= '0' && *q <= '9');
        }
        buf[k] = '\0';
        const char *d = buf;
        if (*d == '+' || *d == '-') d++;
        if (!*d) { free(buf); return 0; }
        for (const char *q = d; *q; q++)
            if (*q < '0' || *q > '9') { free(buf); return 0; }
        char *end = NULL;
        errno = 0;
        long long r = strtoll(buf, &end, 10);
        free(buf);
        if (errno == ERANGE) return 0;
        return r;
    }
    return 0;  /* None / list / dict -> TypeError -> 0 */
}

char *w17_slashcomma_7fa9bc_fmt(const json_t *n) {
    long long v = w17_slashcomma_7fa9bc_int_value(n);
    unsigned long long u;
    int neg = 0;
    if (v < 0) { neg = 1; u = (unsigned long long)(-(v + 1)) + 1ULL; }
    else u = (unsigned long long)v;
    char digits[32];
    int dl = 0;
    do { digits[dl++] = (char)('0' + (int)(u % 10)); u /= 10; } while (u);
    size_t len = (size_t)dl + (dl > 0 ? (size_t)((dl - 1) / 3) : 0) + (neg ? 1u : 0u) + 1u;
    char *out = (char *)malloc(len);
    if (!out) return dupstr("");
    size_t k = 0;
    if (neg) out[k++] = '-';
    for (int i = dl - 1; i >= 0; i--) {
        out[k++] = digits[i];
        if (i > 0 && i % 3 == 0) out[k++] = ',';
    }
    out[k] = '\0';
    return out;
}

double w17_slashcomma_7fa9bc_pct(double used, double total) {
    if (total == 0.0) return 0.0;
    double v = used / total * 100.0;
    return v < 100.0 ? v : 100.0;   /* min(100, ...) */
}

static size_t cp_count_n(const char *s, size_t n) {
    size_t c = 0;
    for (size_t i = 0; i < n; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) c++;  /* count lead bytes */
    return c;
}

/* Byte offset of the idx-th codepoint (a lead byte, or strlen at the end). */
static size_t cp_byte_off(const char *s, size_t idx) {
    size_t c = 0, i = 0;
    while (s[i]) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {  /* at a UTF-8 lead byte */
            if (c == idx) return i;
            c++;
        }
        i++;
    }
    return i;
}

char *w17_slashcomma_7fa9bc_clip(const char *text, int limit) {
    if (!text) return dupstr("");
    size_t nbytes = strlen(text);
    size_t ncp = cp_count_n(text, nbytes);
    if ((int)ncp <= limit) return dupstr(text);
    int prefix = limit - 3;
    if (prefix < 0) prefix = 0;
    size_t off = cp_byte_off(text, (size_t)prefix);  /* starts at a UTF-8 lead byte */
    char *out = (char *)malloc(off + 4);
    if (!out) return dupstr("");
    memcpy(out, text, off);
    memcpy(out + off, "...", 3);
    out[off + 3] = '\0';
    return out;
}

json_t *w17_slashcomma_7fa9bc_routes_for_default(const json_t *default_route,
        const json_t *persisted_route, const json_t *session_row) {
    json_t *res = json_array();
    json_t *p = (persisted_route && persisted_route->type == JSON_OBJECT)
                ? json_copy(persisted_route) : json_object();
    json_t *s = (session_row && session_row->type == JSON_OBJECT)
                ? json_copy(session_row) : json_object();

    char *model = w17_slashcomma_7fa9bc_clean_str(
        (default_route && default_route->type == JSON_ARRAY) ? json_get(default_route, 0) : NULL);
    char *provider = w17_slashcomma_7fa9bc_clean_str(
        (default_route && default_route->type == JSON_ARRAY) ? json_get(default_route, 1) : NULL);
    if (!model[0]) {
        free(model); free(provider);
        json_append(res, p); json_append(res, s);
        return res;
    }

    char *pm = w17_slashcomma_7fa9bc_clean_str(json_obj_get(p, "model"));
    char *pp = w17_slashcomma_7fa9bc_clean_str(json_obj_get(p, "billing_provider"));
    int pag = !strcmp(pm, model) && (!provider[0] || !strcmp(pp, provider));
    free(pm); free(pp);
    if (!pag) { json_free(p); p = json_object(); }

    char *sm = w17_slashcomma_7fa9bc_clean_str(json_obj_get(s, "model"));
    char *sp = w17_slashcomma_7fa9bc_clean_str(json_obj_get(s, "billing_provider"));
    int sag = !strcmp(sm, model) && (!provider[0] || !strcmp(sp, provider));
    free(sm); free(sp);
    if (!sag) {
        json_set(s, "model", json_string(model));
        json_set(s, "billing_provider", json_string(provider));
    }
    free(model); free(provider);
    json_append(res, p); json_append(res, s);
    return res;
}
