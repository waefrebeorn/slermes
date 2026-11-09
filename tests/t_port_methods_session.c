/*
 * t_port_methods_session.c -- oracle harness for port_methods_session.c
 */
#include "port_methods_session.h"

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

static void emit_bool(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_int(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%d}\n", v);
}

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(v);
    printf("}\n");
}

static void emit_str_param(const char *label, const char *const *params,
                           const char *key, const char *dflt)
{
    char *out = ms_str_param(params, key, dflt);
    emit_str("_str_param", label, out);
    free(out);
}

static void emit_flag(const char *label, const char *const *params, const char *name)
{
    emit_bool("_flag", label, ms_flag(params, name));
}

static void emit_int_param(const char *label, const char *const *params,
                           const char *key, int dflt)
{
    emit_int("_int_param", label, ms_int_param(params, key, dflt));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- is_truthy_value ---- */
    emit_bool("is_truthy_value", "true",   ms_is_truthy(2, "true", 0, 0));
    emit_bool("is_truthy_value", "yes",    ms_is_truthy(2, "YES", 0, 0));
    emit_bool("is_truthy_value", "one",    ms_is_truthy(2, "1", 0, 0));
    emit_bool("is_truthy_value", "on",     ms_is_truthy(2, " on ", 0, 0));
    emit_bool("is_truthy_value", "false",  ms_is_truthy(2, "false", 0, 0));
    emit_bool("is_truthy_value", "empty",  ms_is_truthy(2, "", 0, 0));
    emit_bool("is_truthy_value", "null",   ms_is_truthy(0, NULL, 0, 0));
    emit_bool("is_truthy_value", "int1",   ms_is_truthy(3, NULL, 1, 0));
    emit_bool("is_truthy_value", "int0",   ms_is_truthy(3, NULL, 0, 0));

    /* ---- _str_param ---- */
    static const char *p1[] = { "name", "  hello  ", NULL };
    static const char *p2[] = { "name", "", NULL };
    static const char *p3[] = { "name", NULL, NULL };
    static const char *p4[] = { "other", "x", NULL };
    static const char *p5[] = { NULL };
    emit_str_param("plain",  p1, "name", "dflt");
    emit_str_param("empty",  p2, "name", "dflt");
    emit_str_param("null",   p3, "name", "dflt");
    emit_str_param("missing",p4, "name", "dflt");
    emit_str_param("no-map", p5, "name", "dflt");
    emit_str_param("nodflt", p4, "name", NULL);

    /* ---- _flag ---- */
    static const char *f1[] = { "verbose", "true", NULL };
    static const char *f2[] = { "verbose", "0", NULL };
    static const char *f3[] = { "other", "true", NULL };
    emit_flag("on",     f1, "verbose");
    emit_flag("off",    f2, "verbose");
    emit_flag("missing",f3, "verbose");

    /* ---- _int_param ---- */
    static const char *i1[] = { "limit", "42", NULL };
    static const char *i2[] = { "limit", "  7  ", NULL };
    static const char *i3[] = { "limit", "abc", NULL };
    static const char *i4[] = { "limit", "-5", NULL };
    static const char *i5[] = { "limit", "12x", NULL };
    static const char *i6[] = { "other", "9", NULL };
    emit_int_param("plain",  i1, "limit", 10);
    emit_int_param("padded", i2, "limit", 10);
    emit_int_param("bad",    i3, "limit", 10);
    emit_int_param("neg",    i4, "limit", 10);
    emit_int_param("trailing", i5, "limit", 10);
    emit_int_param("missing",  i6, "limit", 10);

    /* ---- _denied_source / _auto_resume_denied_source ---- */
    static const char *s1[] = { "source", "kanban", NULL };
    static const char *s2[] = { "source", "  TOOL ", NULL };
    static const char *s3[] = { "source", "oneshot", NULL };
    static const char *s4[] = { "source", "unknown", NULL };
    static const char *s5[] = { "source", "cli", NULL };
    static const char *s6[] = { "source", "", NULL };
    static const char *s7[] = { NULL };
    emit_bool("_denied_source", "kanban", ms_denied_source(s1));
    emit_bool("_denied_source", "tool",   ms_denied_source(s2));
    emit_bool("_denied_source", "oneshot",ms_denied_source(s3));
    emit_bool("_denied_source", "unknown",ms_denied_source(s4));
    emit_bool("_denied_source", "cli",    ms_denied_source(s5));
    emit_bool("_denied_source", "empty",  ms_denied_source(s6));
    emit_bool("_denied_source", "no-row", ms_denied_source(s7));

    emit_bool("_auto_resume_denied_source", "kanban", ms_auto_resume_denied(s1));
    emit_bool("_auto_resume_denied_source", "unknown",ms_auto_resume_denied(s4));
    emit_bool("_auto_resume_denied_source", "cli",    ms_auto_resume_denied(s5));
    emit_bool("_auto_resume_denied_source", "empty",  ms_auto_resume_denied(s6));

    return 0;
}
