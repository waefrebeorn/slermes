/*
 * t_port_update_cmd.c -- oracle harness for port_update_cmd.c
 */
#include "port_update_cmd.h"

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

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(v);
    printf("}\n");
}

static void emit_parts(const char *label, char **parts, int n)
{
    printf("{\"fn\":\"_called_process_error_cmd_parts\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        jstr(parts[i]);
    }
    printf("]}\n");
}

static void free_parts(char **parts, int n)
{
    for (int i = 0; i < n; i++) free(parts[i]);
    free(parts);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *s;
    int n = 0;
    char **parts;

    /* ---- _called_process_error_cmd_parts ---- */
    static const char *av1[] = { "git", "pull", NULL };
    parts = uc_cmd_parts(av1, NULL, 0, &n);
    emit_parts("argv", parts, n); free_parts(parts, n);

    parts = uc_cmd_parts(NULL, "git pull --rebase", 0, &n);
    emit_parts("cmdline", parts, n); free_parts(parts, n);

    parts = uc_cmd_parts(NULL, "  git   pull  ", 0, &n);
    emit_parts("padded", parts, n); free_parts(parts, n);

    parts = uc_cmd_parts(NULL, "\"C:\\Program Files\\git.exe\" pull", 0, &n);
    emit_parts("quoted", parts, n); free_parts(parts, n);

    parts = uc_cmd_parts(NULL, "", 0, &n);
    emit_parts("empty", parts, n); free_parts(parts, n);

    parts = uc_cmd_parts(NULL, NULL, 0, &n);
    emit_parts("null", parts, n); free_parts(parts, n);

    /* ---- _called_process_error_is_git ---- */
    static const char *g1[] = { "git", NULL };
    static const char *g2[] = { "C:\\Program Files\\Git\\cmd\\git.exe", NULL };
    static const char *g3[] = { "GIT", NULL };
    static const char *g4[] = { "python", NULL };
    static const char *g5[] = { NULL };
    emit_bool("_called_process_error_is_git", "git",     uc_is_git(g1));
    emit_bool("_called_process_error_is_git", "git-exe", uc_is_git(g2));
    emit_bool("_called_process_error_is_git", "upper",   uc_is_git(g3));
    emit_bool("_called_process_error_is_git", "python",  uc_is_git(g4));
    emit_bool("_called_process_error_is_git", "empty",   uc_is_git(g5));

    /* ---- _called_process_error_is_python_dep_install ---- */
    static const char *d1[] = { "pip", "install", "foo", NULL };
    static const char *d2[] = { "C:\\venv\\Scripts\\python.exe", "-m", "ensurepip", NULL };
    static const char *d3[] = { "uv", "pip", "install", "foo", NULL };
    static const char *d4[] = { "C:\\venv\\Scripts\\uv.exe", "pip", "install", NULL };
    static const char *d5[] = { "git", "pull", NULL };
    static const char *d6[] = { "pip", "uninstall", "foo", NULL };
    emit_bool("_called_process_error_is_python_dep_install", "pip",     uc_is_dep_install(d1));
    emit_bool("_called_process_error_is_python_dep_install", "ensurepip", uc_is_dep_install(d2));
    emit_bool("_called_process_error_is_python_dep_install", "uv",      uc_is_dep_install(d3));
    emit_bool("_called_process_error_is_python_dep_install", "uv-exe",  uc_is_dep_install(d4));
    emit_bool("_called_process_error_is_python_dep_install", "git",     uc_is_dep_install(d5));
    emit_bool("_called_process_error_is_python_dep_install", "uninstall", uc_is_dep_install(d6));

    /* ---- _format_update_failure_stage ---- */
    s = uc_failure_stage(d1); emit_str("_format_update_failure_stage", "dep", s); free(s);
    s = uc_failure_stage(g1); emit_str("_format_update_failure_stage", "git", s); free(s);
    s = uc_failure_stage(g4); emit_str("_format_update_failure_stage", "other", s); free(s);

    /* ---- _should_zip_fallback_on_update_error ---- */
    emit_bool("_should_zip_fallback_on_update_error", "all",   uc_zip_fallback(g1, 1, 1, 1));
    emit_bool("_should_zip_fallback_on_update_error", "posix", uc_zip_fallback(g1, 1, 0, 1));
    emit_bool("_should_zip_fallback_on_update_error", "dirty", uc_zip_fallback(g1, 1, 1, 0));
    emit_bool("_should_zip_fallback_on_update_error", "dep",   uc_zip_fallback(d1, 1, 1, 1));
    emit_bool("_should_zip_fallback_on_update_error", "other", uc_zip_fallback(g4, 1, 1, 1));
    emit_bool("_should_zip_fallback_on_update_error", "not-cpe", uc_zip_fallback(g1, 0, 1, 1));

    return 0;
}
