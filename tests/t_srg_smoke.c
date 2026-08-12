/* t_srg_smoke.c — runtime smoke test for port_self_repo_guard.c (self_repo_guard.py).
 * Detects git commands that would rewrite the live source checkout. */
#include "self_repo_guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } \
    else { printf("ok: %s\n", m); } } while (0)

int main(void) {
    int failures = 0;
    const char *root = "/home/wubu/hermes-agent-dev"; /* SRG_REPO_ROOT */
    const char *cwd = "/home/wubu/hermes-agent-dev/slermes";

    /* _executable_name (faithful: .removesuffix('.exe') is case-sensitive) */
    char *n = srg_executable_name("/usr/bin/GIT.EXE");
    CHECK(n && strcmp(n, "git.exe") == 0, "executable_name /usr/bin/GIT.EXE -> git.exe (case-sensitive .exe)");
    free(n);
    n = srg_executable_name("git");
    CHECK(n && strcmp(n, "git") == 0, "executable_name git -> git");
    free(n);

    /* _is_within */
    CHECK(srg_is_within("/home/wubu/hermes-agent-dev/slermes", root), "slermes within root");
    CHECK(!srg_is_within("/home/wubu/other", root), "other not within root");

    /* _mutates_worktree */
    char *dummy[4] = {NULL};
    CHECK(srg_mutates_worktree("checkout", dummy), "checkout mutates");
    CHECK(!srg_mutates_worktree("log", dummy), "log does not mutate");
    char *reset_soft[] = {"--soft", NULL};
    CHECK(!srg_mutates_worktree("reset", reset_soft), "reset --soft safe");
    char *reset_hard[] = {"--hard", NULL};
    CHECK(srg_mutates_worktree("reset", reset_hard), "reset --hard mutates");
    char *stash_list[] = {"list", NULL};
    CHECK(!srg_mutates_worktree("stash", stash_list), "stash list safe");
    char *clean_n[] = {"-n", NULL};
    CHECK(!srg_mutates_worktree("clean", clean_n), "clean -n safe");

    /* detect_self_repo_git_mutation — blocking cases */
    char *op = NULL, *msg = NULL;
    CHECK(srg_detect_self_repo_git_mutation("git checkout main", cwd, root, &op, &msg),
          "git checkout main -> mutation detected");
    if (op) { printf("   op=%s\n", op); CHECK(strcmp(op, "git checkout") == 0, "op == 'git checkout'"); free(op); }
    free(msg);
    op = msg = NULL;

    CHECK(srg_detect_self_repo_git_mutation("git -C /home/wubu/hermes-agent-dev reset --hard HEAD", cwd, root, &op, &msg),
          "git -C root reset --hard -> mutation");
    if (op) { printf("   op=%s\n", op); CHECK(strcmp(op, "git reset") == 0, "op == 'git reset'"); free(op); }
    free(msg);
    op = msg = NULL;

    CHECK(srg_detect_self_repo_git_mutation("cd /home/wubu/hermes-agent-dev && git pull", cwd, root, &op, &msg),
          "cd root && git pull -> mutation");
    if (op) { printf("   op=%s\n", op); CHECK(strcmp(op, "git pull") == 0, "op == 'git pull'"); free(op); }
    free(msg);
    op = msg = NULL;

    CHECK(srg_detect_self_repo_git_mutation("bash -c 'git rebase origin/main'", cwd, root, &op, &msg),
          "bash -c git rebase -> mutation");
    if (op) { printf("   op=%s\n", op); CHECK(strcmp(op, "git rebase") == 0, "op == 'git rebase'"); free(op); }
    free(msg);
    op = msg = NULL;

    /* non-blocking cases */
    CHECK(!srg_detect_self_repo_git_mutation("git log --oneline", cwd, root, &op, &msg),
          "git log -> no mutation");
    CHECK(!srg_detect_self_repo_git_mutation("git status", cwd, root, &op, &msg),
          "git status -> no mutation");
    CHECK(!srg_detect_self_repo_git_mutation("git checkout main", "/tmp", root, &op, &msg),
          "git checkout in /tmp -> no mutation (outside root)");

    /* _block_message */
    char *bm = srg_block_message("git checkout", root);
    CHECK(bm && strstr(bm, "would rewrite") != NULL, "block_message mentions rewrite");
    free(bm);

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
