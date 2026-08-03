/*
 * t_port_file_ops_lint.c — oracle harness for v552 fixes to port_file_operations.
 * Exercises file_ops_looks_like_linter_unusable (LIVE-Python comparable) and
 * file_ops_delete_path (write-deny guard) and emits JSON lines for the oracle.
 *
 *   gcc -O2 -I include -I src/tools -I lib/libjson \
 *       tests/t_port_file_ops_lint.c src/tools/port_file_operations.o \
 *       src/agent/file_safety.o lib/libjson/json.o -o /tmp/t_lint
 */
/* ported in src/tools/port_file_operations.c (no standalone header) */
char *file_ops_delete_path(const char *path);
#include "file_ops_lint.h"
#include "hermes_file_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* minimal hermes_log stub so the test links without the full logger */
void hermes_log(int level, const char *fmt, ...) { (void)level; (void)fmt; }

static void emit_bool2(const char *fn, const char *a, const char *b, int v)
{
    printf("{\"fn\":\"%s\",\"base_cmd\":", fn);
    if (a) printf("\"%s\"", a); else printf("null");
    printf(",\"out\":");
    if (b) printf("\"%s\"", b); else printf("null");
    printf(",\"res\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    /* Self-contained: create the sandbox dirs the write-deny test needs
     * (the runner does not pre-create them). */
    mkdir("/tmp/hermes-lint-test-home", 0755);
    mkdir("/tmp/hermes-lint-test-root", 0755);
    file_safety_set_test_paths("/tmp/hermes-lint-test-home", "/tmp/hermes-lint-test-root");

    /* ---- looks_like_linter_unusable (mirrors test_install_and_lint_fixes.py) ---- */
    emit_bool2("looks_like_linter_unusable", "npx",
        "this is not the tsc command you are looking for",
        file_ops_looks_like_linter_unusable("npx",
            "this is not the tsc command you are looking for"));
    emit_bool2("looks_like_linter_unusable", "npx",
        "real lint error in file.ts:1:1",
        file_ops_looks_like_linter_unusable("npx", "real lint error in file.ts:1:1"));
    emit_bool2("looks_like_linter_unusable", "eslint", "any output",
        file_ops_looks_like_linter_unusable("eslint", "any output"));
    emit_bool2("looks_like_linter_unusable", "", "anything",
        file_ops_looks_like_linter_unusable("", "anything"));
    emit_bool2("looks_like_linter_unusable", "rustfmt", "error: not a workspace",
        file_ops_looks_like_linter_unusable("rustfmt", "error: not a workspace"));
    emit_bool2("looks_like_linter_unusable", "go", "go: cannot find main module",
        file_ops_looks_like_linter_unusable("go", "go: cannot find main module"));
    emit_bool2("looks_like_linter_unusable", "go", "real compile error",
        file_ops_looks_like_linter_unusable("go", "real compile error"));
    /* case-insensitive: npx + UPPERCASE registry string must still match */
    emit_bool2("looks_like_linter_unusable", "npx", "NOT FOUND IN NPM REGISTRY",
        file_ops_looks_like_linter_unusable("npx", "NOT FOUND IN NPM REGISTRY"));

    /* ---- delete_path write-deny guard ---- */
    emit_bool2("delete_path_denied", "/etc/passwd", "",
        file_ops_delete_path("/etc/passwd"));
    emit_bool2("delete_path_denied", "~/.ssh/id_rsa", "",
        file_ops_delete_path("~/.ssh/id_rsa"));
    /* a real temp file INSIDE the safe root should delete successfully */
    char tp[256];
    snprintf(tp, sizeof(tp), "/tmp/hermes-lint-test-home/del-%d.txt", (int)getpid());
    FILE *f = fopen(tp, "w"); if (f) { fputs("x", f); fclose(f); }
    emit_bool2("delete_path_ok", tp, "", file_ops_delete_path(tp));

    return 0;
}
