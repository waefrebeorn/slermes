/*
 * t_port_skill_commands.c -- oracle harness for port_skill_commands.c
 */
#include "port_skill_commands.h"

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

static void emit_slug(const char *in)
{
    char *s = sc_slugify_skill_name(in);
    printf("{\"fn\":\"slugify_skill_name\",\"in\":");
    jstr(in);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_resolve(const char *label, const char *cmd, const char *const *table)
{
    char *k = sc_resolve_slash_key(cmd, table);
    printf("{\"fn\":\"resolve_slash_key\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(k);
    printf("}\n");
    free(k);
}

static void emit_setup(const char *label, int skipped, const char *hint, int needed, const char *note)
{
    char *s = sc_setup_note(skipped, hint, needed, note);
    printf("{\"fn\":\"_setup_note\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_missing(const char *label, const char *const *missing)
{
    char *s = sc_format_missing_skills(missing);
    printf("{\"fn\":\"format_missing_skills\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

static void emit_diff(const char *label, const char *const *before, const char *const *after)
{
    char *s = sc_diff_command_snapshots(before, after);
    printf("{\"fn\":\"diff_command_snapshots\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- slugify_skill_name ---- */
    emit_slug("Foo Bar");
    emit_slug("foo_bar");
    emit_slug("  multiple   spaces  ");
    emit_slug("UPPER");
    emit_slug("a--b---c");
    emit_slug("-leading-trailing-");
    emit_slug("with+plus/slash");
    emit_slug("café");
    emit_slug("");
    emit_slug(NULL);
    emit_slug("dots.and,commas");

    /* ---- resolve_slash_key ---- */
    static const char *t1[] = { "/work", "/status", "/model", NULL };
    static const char *t2[] = { "/foo-bar", NULL };
    emit_resolve("exact", "work", t1);
    emit_resolve("lower", "WORK", t1);
    emit_resolve("underscore", "foo_bar", t2);
    emit_resolve("missing", "nope", t1);
    emit_resolve("empty", "", t1);
    emit_resolve("null", NULL, t1);

    /* ---- _setup_note ---- */
    emit_setup("skipped",      1, "hint", 1, "note");
    emit_setup("hint",         0, "gateway says", 0, NULL);
    emit_setup("hint-empty",   0, "", 1, "setup needed");
    emit_setup("needed-note",  0, NULL, 1, "run setup");
    emit_setup("needed-empty", 0, NULL, 1, "");
    emit_setup("none",         0, NULL, 0, "unused");

    /* ---- format_missing_skills ---- */
    static const char *m1[] = { "alpha", "beta", NULL };
    static const char *m2[] = { "Ambiguous skill name X: use one of a, b", NULL };
    static const char *m3[] = { "Ambiguous skill name X: use one of a, b", "alpha", "beta", NULL };
    static const char *m4[] = { NULL };
    static const char *m5[] = { "Ambiguous skill name Y: pick one", "Ambiguous skill name Z: pick one", "gamma", NULL };
    emit_missing("all-unknown", m1);
    emit_missing("one-ambiguous", m2);
    emit_missing("mixed", m3);
    emit_missing("empty", m4);
    emit_missing("two-ambiguous", m5);

    /* ---- diff_command_snapshots ---- */
    static const char *b1[] = { "alpha", "A", "beta", "B", "gone", "G", NULL };
    static const char *a1[] = { "alpha", "A2", "beta", "B", "new", "N", NULL };
    emit_diff("mixed", b1, a1);

    static const char *b2[] = { "x", "1", NULL };
    static const char *a2[] = { "x", "1", NULL };
    emit_diff("identical", b2, a2);

    static const char *b3[] = { NULL };
    static const char *a3[] = { "only", "O", NULL };
    emit_diff("all-added", b3, a3);

    static const char *b4[] = { "only", "O", NULL };
    static const char *a4[] = { NULL };
    emit_diff("all-removed", b4, a4);

    return 0;
}
