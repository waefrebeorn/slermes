/*
 * cli_chat_error_copy.c — C home for upstream hermes_cli/cli_chat_error_copy.py.
 *
 * PoP: w18_clichater_b6a2c6_short @ hermes_cli/cli_chat_error_copy.py:_short
 * PoP: w18_clichater_b6a2c6_refusal_text @ hermes_cli/cli_chat_error_copy.py:refusal_text
 *
 * Self-contained, libjson-only.  UTF-8 truncation walks whole code points so a
 * cut never lands in the middle of a multibyte sequence.
 */
#include "noc_w18_clichater_b6a2c6.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *substrdup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *py_strip(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    return substrdup(a, (size_t)(b - a));
}
static char *py_rstrip(const char *s) {
    const char *a = s ? s : "";
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    return substrdup(a, (size_t)(b - a));
}

typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n; b->p[b->n] = '\0';
}
static void bputs(buf_t *b, const char *s) { bput(b, s, strlen(s ? s : "")); }

/* UTF-8 code-point helpers: offsets always land on a lead byte. */
static size_t utf8_count(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    size_t n = 0;
    while (*p) {
        n++;
        if (*p < 0x80) p++;
        else if ((*p & 0xE0) == 0xC0) p += 2;
        else if ((*p & 0xF0) == 0xE0) p += 3;
        else p += 4;
    }
    return n;
}
static size_t utf8_offset(const char *s, size_t cps) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *start = p;
    size_t i = 0;
    while (*p && i < cps) {
        i++;
        if (*p < 0x80) p++;
        else if ((*p & 0xE0) == 0xC0) p += 2;
        else if ((*p & 0xF0) == 0xE0) p += 3;
        else p += 4;
    }
    return (size_t)(p - start);
}

char *w18_clichater_b6a2c6_short(const char *text, long long limit) {
    char *stripped = py_strip(text);
    /* splitlines()[0]: first \r or \n ends the first line. */
    char *nl = strpbrk(stripped, "\r\n");
    if (nl) *nl = '\0';
    if (limit < 0) return stripped;                 /* None: no truncation */
    size_t cps = utf8_count(stripped);
    if ((long long)cps <= limit) return stripped;
    size_t keep = (limit > 0) ? utf8_offset(stripped, (size_t)(limit - 1)) : 0;
    char *head = substrdup(stripped, keep);
    char *r = py_rstrip(head);
    free(head);
    buf_t b = {0};
    bputs(&b, r);
    bputs(&b, "\xe2\x80\xa6");                     /* … */
    free(r);
    free(stripped);
    return b.p ? b.p : dupstr("");
}

/* Python `str(v or "").strip()` for the string/null domain used by the gates. */
static char *field_or_detail(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return py_strip(v->str_val);
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char tmp[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%g", v->num_val);
        return dupstr(tmp);
    }
    return dupstr("");
}

char *w18_clichater_b6a2c6_refusal_text(const char *error_str, const struct json_t *data) {
    const struct json_t *fv = data ? json_obj_get(data, "field") : NULL;
    const struct json_t *dv = data ? json_obj_get(data, "detail") : NULL;
    char *field = field_or_detail(fv);
    char *detail = field_or_detail(dv);
    const char *err = error_str ? error_str : "";

    if (!*field && !*detail) { free(field); free(detail); return dupstr(err); }

    buf_t b = {0};
    bputs(&b, err);
    bputs(&b, " (");
    bputs(&b, *field ? field : "unnamed field");
    if (*detail) { bputs(&b, ": "); bputs(&b, detail); }
    bputs(&b, ")");
    free(field); free(detail);
    return b.p ? b.p : dupstr("");
}
