/* gate: port__cli_py___worktree_branch_pr_merged — traced copy of cli_worktree_branch_pr_merged (json_param) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);
extern json_t *json_parse(const char *, char **);


extern int cli_worktree_branch_pr_merged(const char *worktree_path, int timeout, json_t *cache);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_worktree_branch_pr_merged <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_worktree_branch_pr_merged");
    json_t *_j2 = json_parse(argv[2 + (i - 2 + 2) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_worktree_branch_pr_merged(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), _j2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j2) json_free(_j2);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_worktree_branch_pr_merged");
    }
    return 0;
}
