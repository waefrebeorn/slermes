/*
 * t_port_hosted_rooms_common.c -- oracle harness for
 * port_hosted_rooms_common.c
 *
 * `kind`: 0 = not a str, 2 = str.
 */
#include "port_hosted_rooms_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_ident(const char *label, int kind, const char *value,
                       int max_chars, int use_pattern)
{
    char *out = NULL, *err = NULL;
    int rc = hrc_identifier(kind, value, "name", max_chars, use_pattern, NULL, &out, &err);
    printf("{\"fn\":\"identifier\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"rc\":%d,\"out\":", rc);
    jstr(out);
    printf(",\"err\":");
    jstr(err);
    printf("}\n");
    free(out); free(err);
}

static void emit_fields(const char *label,
                        const char *const *keys, int nkeys,
                        const char *const *req, int nreq,
                        const char *const *opt, int nopt)
{
    char *err = NULL;
    int rc = hrc_exact_fields(keys, nkeys, req, nreq, opt, nopt, "room", NULL, &err);
    printf("{\"fn\":\"exact_fields\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"rc\":%d,\"err\":", rc);
    jstr(err);
    printf("}\n");
    free(err);
}

static void emit_text(const char *label, int kind, const char *value,
                      int max_bytes, int strip)
{
    char *out = NULL, *err = NULL;
    int rc = hrc_text(kind, value, "title", max_bytes, strip, &out, &err);
    printf("{\"fn\":\"text\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"rc\":%d,\"out\":", rc);
    jstr(out);
    printf(",\"err\":");
    jstr(err);
    printf("}\n");
    free(out); free(err);
}

static void emit_compact(const char *label, const char *json_text)
{
    json_t *v = json_text ? json_parse(json_text, NULL) : NULL;
    char *s = hrc_compact_json(v);
    printf("{\"fn\":\"compact_json\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
    if (v) json_free(v);
}

static void emit_utf8(const char *label, const char *const *parts, int n)
{
    size_t len = hrc_utf8_len(parts, n);
    printf("{\"fn\":\"utf8_len\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%zu}\n", len);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- identifier ---- */
    emit_ident("simple",     2, "abc", 128, 1);
    emit_ident("padded",     2, "  abc  ", 128, 1);
    emit_ident("empty",      2, "   ", 128, 1);
    emit_ident("too-long",   2, "abcdefghij", 5, 1);
    emit_ident("bad-shape",  2, "1abc", 128, 1);        /* starts with a digit -> ok */
    emit_ident("bad-start",  2, "-abc", 128, 1);
    emit_ident("space-mid",  2, "a b", 128, 1);
    emit_ident("colon",      2, "a:b", 128, 1);
    emit_ident("no-pattern", 2, "a b!", 128, 0);
    emit_ident("not-str",    0, NULL, 128, 1);
    emit_ident("unicode",    2, "café", 128, 1);

    /* ---- exact_fields ---- */
    static const char *k1[] = { "a", "b", NULL };
    static const char *r1[] = { "a", "b", NULL };
    static const char *o1[] = { NULL };
    emit_fields("exact", k1, 2, r1, 2, o1, 0);

    static const char *k2[] = { "a", NULL };
    static const char *r2[] = { "a", "b", "c", NULL };
    emit_fields("missing", k2, 1, r2, 3, o1, 0);

    static const char *k3[] = { "a", "b", "z", "y", NULL };
    static const char *r3[] = { "a", "b", NULL };
    emit_fields("unknown", k3, 4, r3, 2, o1, 0);

    static const char *k4[] = { "a", "b", "opt", NULL };
    static const char *r4[] = { "a", "b", NULL };
    static const char *o4[] = { "opt", NULL };
    emit_fields("optional", k4, 3, r4, 2, o4, 1);

    static const char *k5[] = { NULL };
    emit_fields("empty-obj", k5, 0, r1, 2, o1, 0);

    /* ---- text ---- */
    emit_text("simple",    2, "hello", 100, 1);
    emit_text("padded",    2, "  hi  ", 100, 1);
    emit_text("blank",     2, "   ", 100, 1);
    emit_text("too-large", 2, "abcdef", 3, 1);
    emit_text("unicode",   2, "café", 5, 1);
    emit_text("unicode-4", 2, "café", 4, 1);
    emit_text("no-strip",  2, "  hi  ", 10, 0);
    emit_text("not-str",   0, NULL, 100, 1);
    emit_text("exact-fit", 2, "abc", 3, 1);

    /* ---- compact_json ---- */
    emit_compact("object",   "{\"b\":1,\"a\":2}");
    emit_compact("nested",   "{\"z\":{\"d\":4,\"c\":3},\"a\":[1,2]}");
    emit_compact("string",   "\"hi\"");
    emit_compact("array",    "[3,1,2]");
    emit_compact("escapes",  "{\"s\":\"a\\nb\\t\\\"c\\\"\"}");
    emit_compact("numbers",  "{\"i\":42,\"f\":1.5}");
    emit_compact("empty",    "{}");
    emit_compact("null",     NULL);

    /* ---- utf8_len ---- */
    static const char *p1[] = { "abc", "de", NULL };
    static const char *p2[] = { "café", "!", NULL };
    static const char *p3[] = { NULL };
    emit_utf8("ascii", p1, 2);
    emit_utf8("unicode", p2, 2);
    emit_utf8("none", p3, 0);

    return 0;
}
