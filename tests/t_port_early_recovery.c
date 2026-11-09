/*
 * t_port_early_recovery.c -- oracle harness for port_early_recovery.c
 */
#include "port_early_recovery.h"

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

static void emit_git(const char *label, int is_file, const char *text)
{
    char *out = er_git_dir("/repo", is_file, text);
    printf("{\"fn\":\"_git_dir\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_closure(const char *label, const char *pre)
{
    char *err = NULL;
    char *out = er_recovery_closure_dir("/repo", pre, 0, NULL, &err);
    printf("{\"fn\":\"recovery_closure_dir\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf(",\"err\":");
    jstr(err);
    printf("}\n");
    free(out); free(err);
}

static void emit_marker(const char *label, int is_file, const char *text)
{
    char *out = er_interrupted_marker("/repo", is_file, text);
    printf("{\"fn\":\"interrupted_pull_marker\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- is_object_id ---- */
    emit_bool("is_object_id", "sha1",      er_is_object_id("0123456789abcdef0123456789abcdef01234567"));
    emit_bool("is_object_id", "sha256",    er_is_object_id("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    emit_bool("is_object_id", "short",     er_is_object_id("0123456789abcdef0123456789abcdef0123456"));
    emit_bool("is_object_id", "long",      er_is_object_id("0123456789abcdef0123456789abcdef012345678"));
    emit_bool("is_object_id", "upper",     er_is_object_id("0123456789ABCDEF0123456789abcdef01234567"));
    emit_bool("is_object_id", "non-hex",   er_is_object_id("0123456789abcdef0123456789abcdef0123456g"));
    emit_bool("is_object_id", "trailing",  er_is_object_id("0123456789abcdef0123456789abcdef01234567\n"));
    emit_bool("is_object_id", "head",      er_is_object_id("HEAD"));
    emit_bool("is_object_id", "path",      er_is_object_id("../../etc/passwd"));
    emit_bool("is_object_id", "empty",     er_is_object_id(""));
    emit_bool("is_object_id", "null",      er_is_object_id(NULL));

    /* ---- _git_dir ---- */
    emit_git("directory", 0, NULL);
    emit_git("worktree",  1, "gitdir: /repo/.git/worktrees/wt\n");
    emit_git("worktree-pad", 1, "  gitdir:/other/path  \n");
    emit_git("bom",        1, "\xef\xbb\xbfgitdir: /bom/path\n");
    emit_git("not-gitdir", 1, "something else\n");

    /* ---- recovery_closure_dir ---- */
    emit_closure("ok-sha1", "0123456789abcdef0123456789abcdef01234567");
    emit_closure("ok-sha256", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    emit_closure("head",    "HEAD");
    emit_closure("path",    "../x");
    emit_closure("short",   "abc");

    /* ---- interrupted_pull_marker ---- */
    emit_marker("plain",     0, NULL);
    emit_marker("worktree",  1, "gitdir: /repo/.git/worktrees/wt\n");

    return 0;
}
