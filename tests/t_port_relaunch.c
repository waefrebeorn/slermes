/*
 * t_port_relaunch.c -- oracle harness for port_relaunch.c
 */
#include "port_relaunch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_list(const char *fn, const char *label, const char *const *v, int n)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) { if (i) putchar(','); printf("\"%s\"", v[i]); }
    printf("]}\n");
}

static void emit_extract(const char *label, const char *const *argv)
{
    int n = 0;
    const char **out = rl_extract_inherited_flags(argv, NULL, NULL, 0, &n);
    emit_list("_extract_inherited_flags", label, out, n);
    free(out);
}

static void emit_build(const char *label, const char *bin, const char *py,
                       const char *const *orig, int preserve,
                       const char *const *extra)
{
    int n = 0;
    const char **out = rl_build_relaunch_argv(bin, py, orig, preserve, extra, &n);
    emit_list("build_relaunch_argv", label, out, n);
    free(out);
}

int main(void)
{
    /* ---- _extract_inherited_flags ---- */
    static const char *a1[] = { "--profile", "work", "sessions", "browse", NULL };
    static const char *a2[] = { "--profile=work", "sessions", NULL };
    static const char *a3[] = { "-p", "work", "--dev", NULL };
    static const char *a4[] = { "--profile", NULL };
    static const char *a5[] = { "--profile", "-tui", NULL };
    static const char *a6[] = { "sessions", "browse", NULL };
    static const char *a7[] = { "-p", "work", "-p", "home", NULL };
    static const char *a8[] = { "--dev", "--profile", "x", "--profile=y", NULL };
    static const char *a9[] = { NULL };

    emit_extract("basic", a1);
    emit_extract("equals", a2);
    emit_extract("short", a3);
    emit_extract("trailing-no-value", a4);
    emit_extract("next-starts-dash", a5);
    emit_extract("none", a6);
    emit_extract("repeated", a7);
    emit_extract("mixed", a8);
    emit_extract("empty", a9);

    /* ---- build_relaunch_argv ---- */
    static const char *e1[] = { "--yes", NULL };
    static const char *e2[] = { NULL };
    static const char *o1[] = { "--profile", "work", "--dev", NULL };

    emit_build("bin+inherited", "C:/hermes/hermes.exe", NULL, o1, 1, e1);
    emit_build("bin+no-inherit", "C:/hermes/hermes.exe", NULL, o1, 0, e1);
    emit_build("bin-no-extra", "C:/hermes/hermes.exe", NULL, o1, 1, e2);
    emit_build("python-fallback", NULL, "C:/py/python.exe", o1, 1, e1);
    emit_build("python-no-inherit", NULL, "C:/py/python.exe", o1, 0, e1);
    emit_build("bin-no-orig", "C:/hermes/hermes.exe", NULL, NULL, 1, e1);

    return 0;
}
