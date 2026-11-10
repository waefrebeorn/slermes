/*
 * t_port_worktree_ops.c -- oracle harness for port_worktree_ops.c
 */
#include "port_worktree_ops.h"

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

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(v);
    printf("}\n");
}

static void emit_bool(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_entries(const char *label, char **e, int n)
{
    printf("{\"fn\":\"_worktreeinclude_entries\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        jstr(e[i]);
    }
    printf("]}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *s;
    int n = 0;
    char **e;

    /* ---- _normalize_git_bash_path ---- */
    s = wo_normalize_bash_path("/c/Users/x", 1);
    emit_str("_normalize_git_bash_path", "c", s); free(s);
    s = wo_normalize_bash_path("/cygdrive/c/Users/x", 1);
    emit_str("_normalize_git_bash_path", "cygdrive", s); free(s);
    s = wo_normalize_bash_path("/mnt/c/Users/x", 1);
    emit_str("_normalize_git_bash_path", "mnt", s); free(s);
    s = wo_normalize_bash_path("/d", 1);
    emit_str("_normalize_git_bash_path", "no-slash", s); free(s);
    s = wo_normalize_bash_path("/cd/x", 1);
    emit_str("_normalize_git_bash_path", "two-letter", s); free(s);
    s = wo_normalize_bash_path("C:\\already\\win", 1);
    emit_str("_normalize_git_bash_path", "windows", s); free(s);
    s = wo_normalize_bash_path("/c/x", 0);
    emit_str("_normalize_git_bash_path", "posix", s); free(s);
    s = wo_normalize_bash_path(NULL, 1);
    emit_str("_normalize_git_bash_path", "null", s); free(s);

    /* ---- _path_is_within_root ---- */
    emit_bool("_path_is_within_root", "inside",  wo_within_root("C:/a/b", "C:/a"));
    emit_bool("_path_is_within_root", "equal",   wo_within_root("C:/a", "C:/a"));
    emit_bool("_path_is_within_root", "outside", wo_within_root("C:/a/bc", "C:/a/b"));
    emit_bool("_path_is_within_root", "other",   wo_within_root("C:/x", "C:/a"));
    emit_bool("_path_is_within_root", "backsl",  wo_within_root("C:\\a\\b", "C:/a"));
    emit_bool("_path_is_within_root", "null",    wo_within_root(NULL, "C:/a"));

    /* ---- _worktreeinclude_entries ---- */
    static const char *l1[] = { "main", "  feature  ", "", "# comment", "  # indented", "dev", NULL };
    static const char *l2[] = { NULL };
    static const char *l3[] = { "\xEF\xBB\xBFmain", "dev", NULL };
    static const char *l4[] = { "# only comments", "", NULL };

    e = wo_include_entries(l1, 0, &n);
    emit_entries("plain", e, n);
    for (int i = 0; i < n; i++) free(e[i]);
    free(e);

    e = wo_include_entries(l2, 0, &n);
    emit_entries("empty", e, n);
    for (int i = 0; i < n; i++) free(e[i]);
    free(e);

    e = wo_include_entries(l3, 1, &n);
    emit_entries("bom", e, n);
    for (int i = 0; i < n; i++) free(e[i]);
    free(e);

    e = wo_include_entries(l4, 0, &n);
    emit_entries("comments", e, n);
    for (int i = 0; i < n; i++) free(e[i]);
    free(e);

    return 0;
}
