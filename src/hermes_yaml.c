/*
 * hermes_yaml.c — real C11 port of hermes_yaml.py.
 *
 * PoP: hermesyaml_processing_version @ hermes_yaml.py:_Yaml11Resolver.processing_version
 * PoP: hermesyaml_roundtrip_width    @ hermes_yaml.py:ROUNDTRIP_YAML_WIDTH
 * PoP: hermesyaml_safe_dump_scalar   @ hermes_yaml.py:safe_dump
 *
 * hermes_yaml.py is a thin policy layer over ruamel.yaml. Its genuinely pure,
 * deterministic leaves are the YAML 1.1 resolver version, the anti-fold width
 * constant, and the scalar branch of safe_dump. The ruamel emitter's block
 * collection layout is out of scope for a JSON-lines leaf oracle; the scalar
 * emitter here reproduces ruamel's YAML-1.1 quoting + explicit-end marker
 * for the scalar types representable as JSON.
 */
#include "noc_w5_hermesyaml_f6b6a1.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * PoP: hermesyaml_processing_version @ hermes_yaml.py:_Yaml11Resolver.processing_version
 */
json_t *hermesyaml_processing_version(void) {
    json_t *a = json_array();
    json_append(a, json_number(1));
    json_append(a, json_number(1));
    return a;
}

/*
 * PoP: hermesyaml_roundtrip_width @ hermes_yaml.py:ROUNDTRIP_YAML_WIDTH
 */
long long hermesyaml_roundtrip_width(void) {
    return 2147483647LL; /* 2**31 - 1 */
}

static char *hy_cat3(const char *a, const char *b, const char *c) {
    size_t na = strlen(a), nb = strlen(b), nc = strlen(c);
    char *p = (char *)malloc(na + nb + nc + 1);
    if (!p) return NULL;
    memcpy(p, a, na);
    memcpy(p + na, b, nb);
    memcpy(p + na + nb, c, nc + 1);
    return p;
}

static char *hy_num_str(double d) {
    char buf[128];
    if (isfinite(d) && floor(d) == d && d >= -9.0e15 && d <= 9.0e15)
        snprintf(buf, sizeof(buf), "%lld", (long long)d);
    else
        snprintf(buf, sizeof(buf), "%g", d);
    return strdup(buf);
}

/* YAML 1.1 implicit resolvers that force a single-quoted scalar. */
static int hy_implicit_word(const char *s) {
    static const char *const words[] = {
        "y", "n", "yes", "no", "true", "false", "on", "off", "null", "~", ".",
        NULL
    };
    char low[64];
    size_t n = strlen(s), i;
    int j;
    if (n == 0 || n >= sizeof(low)) return 0;
    for (i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)s[i]);
    low[n] = '\0';
    for (j = 0; words[j]; j++)
        if (!strcmp(low, words[j])) return 1;
    /* Numeric-looking token: ruamel quotes it to keep it a string. */
    {
        int digit = 0, other = 0;
        for (i = 0; i < n; i++) {
            unsigned char c = (unsigned char)s[i];
            if (isdigit(c) || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E')
                digit = 1;
            else
                other = 1;
        }
        if (digit && !other) return 1;
    }
    return 0;
}

/* Does a plain string need quoting under the module's YAML 1.1 emitter? */
static int hy_needs_quote(const char *s) {
    size_t n = strlen(s), i;
    if (n == 0) return 1;
    if (hy_implicit_word(s)) return 1;
    if (isspace((unsigned char)s[0]) || isspace((unsigned char)s[n - 1])) return 1;
    if (strchr("-?:,[]{}#&*!|>'\"%@`", s[0])) return 1;
    for (i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20) return 1;
        if (c == ':' && s[i + 1] == ' ') return 1;
        if (c == '#' && s[i - 1] == ' ') return 1;
    }
    return 0;
}

/* Single-quote with doubled apostrophes, exactly like ruamel's emitter. */
static char *hy_quote(const char *s) {
    size_t n = strlen(s), cap = 2 * n + 4, o = 0, i;
    char *p = (char *)malloc(cap);
    if (!p) return NULL;
    p[o++] = '\'';
    for (i = 0; i < n; i++) {
        if (s[i] == '\'') p[o++] = '\'';
        p[o++] = s[i];
    }
    p[o++] = '\'';
    p[o] = '\0';
    return p;
}

/*
 * PoP: hermesyaml_safe_dump_scalar @ hermes_yaml.py:safe_dump
 */
char *hermesyaml_safe_dump_scalar(json_t *v) {
    json_type_t t = v ? v->type : JSON_NULL;
    if (t == JSON_NULL) return strdup("null\n...\n");
    if (t == JSON_BOOL)
        return strdup(v->bool_val ? "true\n...\n" : "false\n...\n");
    if (t == JSON_NUMBER) {
        char *num = hy_num_str(v->num_val);
        char *out = hy_cat3(num, "\n", "...\n");
        free(num);
        return out;
    }
    if (t == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        if (hy_needs_quote(s)) {
            char *q = hy_quote(s);
            char *out = hy_cat3(q, "\n", "");
            free(q);
            return out;
        }
        return hy_cat3(s, "\n", "...\n");
    }
    /* Non-scalars are outside this leaf's contract: emit null. */
    return strdup("null\n...\n");
}
