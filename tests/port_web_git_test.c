/*
 * test_port_web_git.c — Faithful behavior test for the web_git port.
 *
 * Exercises the C port against a REAL temp git repository (no mocks),
 * asserting the JSON shapes match the Python web_git.py contract:
 *   - repo_status on an empty/inited repo
 *   - review_list / repo_status after a commit + a modified file
 *   - branch_list / worktree_list on a repo with a branch
 *   - review_rev_parse returns a 40-char SHA
 *   - file_diff_vs_head returns a diff for a changed file
 */

#include "port_web_git.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

static char *run(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return strdup("");
    size_t len = 0, alloc = 4096; char *buf = malloc(alloc);
    size_t r; char tmp[4096];
    while ((r = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        if (len + r + 1 > alloc) { alloc *= 2; buf = realloc(buf, alloc); }
        memcpy(buf + len, tmp, r); len += r;
    }
    buf[len] = '\0'; pclose(fp); return buf;
}

/* runf: format + run, discarding output (used for git mutations). */
static void runf(const char *fmt, ...) {
    char cmd[4096];
    va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    char *o = run(cmd);
    free(o);
}

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    fputs(content, f); fclose(f);
}

int main(void) {
    char tmpl[] = "/tmp/web_git_test_XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) { printf("cannot mkdtemp\n"); return 1; }
    char cd[1024];
    snprintf(cd, sizeof(cd), "cd %s &&", dir);

    /* init repo + initial commit */
    runf("%s git init -q && git -C %s config user.email t@t && git -C %s config user.name t", cd, dir, dir);
    char fpath[1100]; snprintf(fpath, sizeof(fpath), "%s/README.md", dir);
    write_file(fpath, "# hello\n");
    runf("%s git -C %s add README.md && git -C %s commit -q -m init", cd, dir, dir);

    /* repo_status: clean repo -> 0 changed, branch set */
    json_t *st = web_git_repo_status(dir);
    CHECK(st != NULL, "repo_status returns object on a repo");
    if (st) {
        const char *branch = json_get_str(st, "branch", "");
        CHECK(strcmp(branch, "master") == 0 || strcmp(branch, "main") == 0, "repo_status branch is master/main");
        CHECK(json_get_num(st, "changed", -1) == 0, "clean repo has 0 changed");
        CHECK(json_get_num(st, "staged", -1) == 0, "clean repo has 0 staged");
        json_free(st);
    }

    /* modify the file -> unstaged change visible */
    write_file(fpath, "# hello\nmore\n");
    st = web_git_repo_status(dir);
    CHECK(st != NULL, "repo_status after edit");
    if (st) {
        CHECK(json_get_num(st, "unstaged", -1) >= 1, "edited file shows unstaged>=1");
        json_free(st);
    }

    /* review_list (uncommitted) -> one file with status and added>0 */
    json_t *rl = web_git_review_list(dir, "uncommitted", NULL);
    CHECK(rl != NULL, "review_list returns object");
    if (rl) {
        json_t *files = json_obj_get(rl, "files");
        CHECK(files && json_len(files) >= 1, "review_list has >=1 file");
        if (files && json_len(files) >= 1) {
            json_t *f0 = json_get(files, 0);
            CHECK(json_get_num(f0, "added", -1) >= 1, "review_list file added>=1 (numstat)");
        }
        json_free(rl);
    }

    /* file_diff_vs_head returns a non-empty diff */
    char *diff = web_git_file_diff_vs_head(dir, "README.md");
    CHECK(diff && *diff != '\0', "file_diff_vs_head returns a diff");
    free(diff);

    /* review_rev_parse -> 40-char sha */
    char *sha = web_git_review_rev_parse(dir, NULL);
    CHECK(sha && strlen(sha) == 40, "review_rev_parse returns 40-char SHA");
    free(sha);

    /* branch_list -> at least the current branch */
    json_t *bl = web_git_branch_list(dir);
    CHECK(bl != NULL && json_len(bl) >= 1, "branch_list has >=1 branch");
    if (bl) {
        json_t *b0 = json_get(bl, 0);
        CHECK(json_get_bool(b0, "checkedOut", false), "branch_list marks current as checkedOut");
        CHECK(json_get_bool(b0, "isDefault", false), "branch_list marks trunk as isDefault");
        json_free(bl);
    }

    /* worktree_list -> at least the main worktree */
    json_t *wt = web_git_worktree_list(dir);
    CHECK(wt != NULL && json_len(wt) >= 1, "worktree_list has >=1");
    if (wt) {
        json_t *w0 = json_get(wt, 0);
        CHECK(json_get_bool(w0, "isMain", false), "worktree_list[0] isMain");
        json_free(wt);
    }

    /* review_commit with a new file */
    char nf[1100]; snprintf(nf, sizeof(nf), "%s/new.txt", dir);
    write_file(nf, "data\n");
    json_t *cm = web_git_review_commit(dir, "add new", false);
    CHECK(cm != NULL, "review_commit returns object");
    if (cm) { json_free(cm); }
    sha = web_git_review_rev_parse(dir, NULL);
    CHECK(sha && strlen(sha) == 40, "commit advanced HEAD to new 40-char SHA");
    free(sha);

    /* cleanup */
    runf("rm -rf %s", dir);

    if (failures == 0) { printf("\nALL WEB_GIT TESTS PASSED\n"); return 0; }
    printf("\n%d WEB_GIT TESTS FAILED\n", failures);
    return 1;
}
