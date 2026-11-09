/*
 * t_port_skill_bundles.c -- oracle harness for port_skill_bundles.c
 */
#include "port_skill_bundles.h"

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

static void emit_key(const char *label, const char *name)
{
    char *k = sb_bundle_key(name);
    printf("{\"fn\":\"get_bundle_key\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(k);
    printf("}\n");
    free(k);
}

static void emit_path(const char *label, const char *name, const char *dir)
{
    int err = 0;
    char *p = sb_bundle_path_for(name, dir, &err);
    printf("{\"fn\":\"bundle_path_for\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"err\":%d,\"out\":", err);
    jstr(p);
    printf("}\n");
    free(p);
}

static void emit_sig(const char *label, const char *const *paths, const char *const *contents)
{
    char **op = NULL, **od = NULL;
    int n = 0;
    sb_bundles_sig(paths, contents, &op, &od, &n);
    printf("{\"fn\":\"_bundles_sig\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        printf("[");
        jstr(op ? op[i] : NULL);
        putchar(',');
        jstr(od ? od[i] : NULL);
        printf("]");
    }
    printf("]}\n");
    if (op) for (int i = 0; i < n; i++) free(op[i]);
    if (od) for (int i = 0; i < n; i++) free(od[i]);
    free(op); free(od);
}

static void emit_order(const char *label, const char *const *slugs)
{
    int n = 0;
    while (slugs[n]) n++;
    int *order = sb_list_bundles_order(slugs, n);
    printf("{\"fn\":\"list_bundles\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) { if (i) putchar(','); printf("%d", order[i]); }
    printf("]}\n");
    free(order);
}

static void emit_resolve(const char *label, const char *cmd, const char *const *table)
{
    char *k = sb_resolve_command_key(cmd, table);
    printf("{\"fn\":\"resolve_bundle_command_key\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(k);
    printf("}\n");
    free(k);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- get_bundle_key ---- */
    emit_key("simple", "My Bundle");
    emit_key("under",  "my_bundle");
    emit_key("empty",  "");
    emit_key("null",   NULL);
    emit_key("punct",  "a+b/c");

    /* ---- bundle_path_for ---- */
    emit_path("basic",   "My Bundle", "/home/u/skill-bundles");
    emit_path("empty-slug", "+++",   "/home/u/skill-bundles");
    emit_path("null-name",  NULL,   "/home/u/skill-bundles");
    emit_path("no-dir",     "work",  NULL);
    emit_path("trailing",   "work",  "/home/u/skill-bundles/");

    /* ---- _bundles_sig ---- */
    static const char *p1[] = { "/b/beta.yaml", "/b/alpha.yaml", "/b/gamma.yaml", NULL };
    static const char *c1[] = { "beta body",    "alpha body",    "gamma body",    NULL };
    emit_sig("three", p1, c1);

    static const char *p2[] = { "/b/a.yaml", "/b/b.yaml", "/b/c.yaml", NULL };
    static const char *c2[] = { "aaa",       NULL,        "ccc",       NULL };
    emit_sig("one-missing", p2, c2);

    static const char *p3[] = { NULL };
    static const char *c3[] = { NULL };
    emit_sig("empty", p3, c3);

    /* ---- list_bundles order ---- */
    static const char *s1[] = { "zeta", "alpha", "mid", NULL };
    static const char *s2[] = { "only", NULL };
    static const char *s3[] = { "b", "a", "c", "a", NULL };
    static const char *s4[] = { NULL };
    emit_order("three", s1);
    emit_order("one", s2);
    emit_order("dupe", s3);
    emit_order("empty", s4);

    /* ---- resolve_bundle_command_key ---- */
    static const char *t1[] = { "/work", "/my-bundle", NULL };
    emit_resolve("exact", "work", t1);
    emit_resolve("under", "my_bundle", t1);
    emit_resolve("missing", "nope", t1);
    emit_resolve("empty", "", t1);

    return 0;
}
