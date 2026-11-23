/*
 * config_language.c — C port of upstream Hermes hermes_cli/config_language.py.
 *
 * PoP: w13_configlang_cbd080_display_language_error @ hermes_cli/config_language.py:display_language_error
 *
 * display_language_error() is pure apart from resolve_display_language() and
 * supported_languages() (plugin discovery / i18n layers). Both are passed as
 * explicit JSON seams, matching the function's own formatting exactly.
 */
#include "noc_w13_configlang_cbd080.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DISPLAY_LANGUAGE_KEY = "display.language";

typedef struct { char *p; size_t n, cap; } buf_t;

static void bput(buf_t *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 128;
        while (b->n + len + 1 > nc) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}
static void bputs(buf_t *b, const char *s) { bput(b, s, strlen(s)); }

/* Python repr() of a JSON scalar (strings need quote selection + escaping). */
static char *py_repr(const json_t *v) {
    buf_t b = {0};
    if (!v || v->type == JSON_NULL) { bputs(&b, "None"); return b.p; }
    if (v->type == JSON_BOOL) { bputs(&b, v->bool_val ? "True" : "False"); return b.p; }
    if (v->type == JSON_NUMBER) {
        char num[64];
        snprintf(num, sizeof num, "%.17g", v->num_val);
        bputs(&b, num);
        return b.p;
    }
    const char *s = (v->type == JSON_STRING && v->str_val) ? v->str_val : "";
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    char qstr[2] = { q, '\0' };
    bputs(&b, qstr);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') bputs(&b, "\\\\");
        else if (c == (unsigned char)q) { char esc[2] = {'\\', (char)c}; bput(&b, esc, 2); }
        else if (c == '\n') bputs(&b, "\\n");
        else if (c == '\r') bputs(&b, "\\r");
        else if (c == '\t') bputs(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) {
            char esc[5];
            snprintf(esc, sizeof esc, "\\x%02x", c);
            bputs(&b, esc);
        } else {
            char one[2] = { (char)c, '\0' };
            bputs(&b, one);
        }
    }
    bputs(&b, qstr);
    return b.p;
}

/* str(value).strip() emptiness test for the shapes the function sees. */
static int value_blank(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;              /* str(None) == "None" */
    if (v->type != JSON_STRING) return 0;
    const char *s = v->str_val ? v->str_val : "";
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == '\0';
}

json_t *w13_configlang_cbd080_display_language_error(
        const struct json_t *value, const struct json_t *resolved,
        const struct json_t *supported) {
    if (value_blank(value)) return json_null();
    if (resolved && resolved->type != JSON_NULL) return json_null();
    char *repr = py_repr(value);
    buf_t b = {0};
    bputs(&b, "✗ Unknown language ");
    bputs(&b, repr ? repr : "None");
    bputs(&b, " for ");
    bputs(&b, DISPLAY_LANGUAGE_KEY);
    bputs(&b, ". Available: ");
    if (json_is_array(supported)) {
        size_t n = json_len(supported);
        for (size_t i = 0; i < n; i++) {
            if (i) bputs(&b, ", ");
            json_t *item = json_get(supported, i);
            if (item && item->type == JSON_STRING && item->str_val) bputs(&b, item->str_val);
        }
    }
    bputs(&b, ".\n  Install a language pack plugin (hermes plugins install <pack>) "
             "or drop <HERMES_HOME>/locales/<id>.yaml to add one.");
    json_t *out = json_string(b.p ? b.p : "");
    free(repr);
    free(b.p);
    return out;
}
