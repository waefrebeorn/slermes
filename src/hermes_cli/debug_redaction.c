/*
 * debug_redaction.c — C port of upstream Hermes hermes_cli/debug_redaction.py.
 *
 * PoP: w13_debugredac_c651e1_decoded_compact_name @ hermes_cli/debug_redaction.py:_decoded_compact_name
 *
 * Faithful port of the compact-name helper used by the strict support-text
 * redactor: `str(value or "")`, up to three stable unquote_plus rounds (the
 * helper behind agent.redact._canonical_url_param_name), ASCII casefold, then
 * keep only str.isalnum() characters. The full regex redaction surface is out
 * of this seam; fixtures exercise the pure canonicalization step.
 */
#include "noc_w13_debugredac_c651e1.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hexval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* One urllib.parse.unquote_plus round (ASCII percent bytes; malformed kept). */
static void unquote_plus_one(const char *in, char *out, size_t cap) {
    size_t k = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        if (*p == '+') {
            if (k + 1 < cap) out[k++] = ' ';
        } else if (*p == '%' && p[1] && p[2] && hexval(p[1]) >= 0 && hexval(p[2]) >= 0) {
            unsigned char byte = (unsigned char)((hexval(p[1]) << 4) | hexval(p[2]));
            if (k + 1 < cap) out[k++] = (char)byte;
            p += 2;
        } else {
            if (k + 1 < cap) out[k++] = (char)*p;
        }
    }
    out[k] = '\0';
}

/* str(value or "") for the scalar shapes used by the redactor. */
static void coerce_value(const json_t *v, char *out, size_t cap) {
    out[0] = '\0';
    if (!v || v->type == JSON_NULL) return;                 /* None -> "" */
    if (v->type == JSON_BOOL) {
        if (!v->bool_val) return;                            /* False is falsy */
        snprintf(out, cap, "%s", "True");
        return;
    }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == 0.0) return;                       /* 0 is falsy */
        snprintf(out, cap, "%.17g", v->num_val);
        return;
    }
    if (v->type == JSON_STRING && v->str_val) {
        size_t n = strlen(v->str_val);
        if (n >= cap) n = cap - 1;
        memcpy(out, v->str_val, n);
        out[n] = '\0';
    }
    /* non-empty arrays/objects are out of this seam's scope -> "" */
}

json_t *w13_debugredac_c651e1_decoded_compact_name(const struct json_t *value) {
    char cur[8192], next[8192];
    coerce_value(value, cur, sizeof cur);
    for (int round = 0; round < 3; round++) {
        unquote_plus_one(cur, next, sizeof next);
        if (!strcmp(next, cur)) break;
        memcpy(cur, next, sizeof cur);
    }
    char *out = (char *)malloc(strlen(cur) + 1);
    if (!out) return json_string("");
    size_t k = 0;
    for (const unsigned char *p = (const unsigned char *)cur; *p; p++) {
        unsigned char c = (unsigned char)tolower(*p);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[k++] = (char)c;
    }
    out[k] = '\0';
    json_t *res = json_string(out);
    free(out);
    return res;
}
