/* gate: web_git_parse_worktrees — traced copy of the `git worktree list
 * --porcelain` parser inside src/hermes_cli/web_git.c::web_git_worktree_list
 * (parsing rungs only; the git subprocess itself is I/O and untraceable).
 * LIVE twin: hermes_cli/web_git.py::_parse_worktrees.
 *
 * Input encoding: one porcelain blob per input.
 *
 * Rung ladder per line (identical both sides):
 *   B1 worktree line | B2 flush-previous | B3 branch line |
 *   B4 detached | B5 bare | B6 locked ;  final have-flush; V canonical result
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void wc_parse(const char *out) {
    WCUP_BEGIN("parse_worktrees");
    char result[4096];
    size_t ro = 0;
    result[0] = '\0';
    int ntree = 0;

    char *buf = strdup(out);
    char *cur_path = NULL, *cur_branch = NULL;
    int detached = 0, bare = 0, locked = 0;
    int have = 0;

    /* iterate ALL newline-separated segments (LIVE str.split("\n") keeps
     * empty segments — they flow through the ladder as unmatched lines) */
    char *pseg = buf;
    while (1) {
        char *nl = strchr(pseg, '\n');
        if (nl) *nl = '\0';
        char *line = pseg;

        WCUP_BRANCH(strncmp(line, "worktree ", 9) == 0);         /* B1 */
        if (strncmp(line, "worktree ", 9) == 0) {
            WCUP_BRANCH(have);                                   /* B2 */
            if (have && ro < sizeof result - 32) {
                ro += (size_t)snprintf(result + ro, sizeof result - ro,
                        "%s|%s|%d%d%d%d;", cur_path ? cur_path : "",
                        cur_branch ? cur_branch : "",
                        detached, bare, locked, ntree == 0);
                ntree++;
            }
            free(cur_path); free(cur_branch);
            cur_path = strdup(line + 9);
            cur_branch = NULL;
            detached = bare = locked = 0;
            have = 1;
        } else if (!have) {
            /* LIVE skips preamble lines when cur is None */
        } else {
            WCUP_BRANCH(strncmp(line, "branch ", 7) == 0);       /* B3 */
            if (strncmp(line, "branch ", 7) == 0) {
                char *b = line + 7;
                char *slash = strstr(b, "refs/heads/");
                free(cur_branch);
                cur_branch = strdup(slash ? slash + 11 : b);
            } else {
                WCUP_BRANCH(strcmp(line, "detached") == 0);      /* B4 */
                if (strcmp(line, "detached") == 0) detached = 1;
                else {
                    WCUP_BRANCH(strcmp(line, "bare") == 0);      /* B5 */
                    if (strcmp(line, "bare") == 0) bare = 1;
                    else {
                        WCUP_BRANCH(strncmp(line, "locked", 6) == 0); /* B6 */
                        if (strncmp(line, "locked", 6) == 0) locked = 1;
                    }
                }
            }
        }

        if (!nl) break;
        pseg = nl + 1;
    }
    WCUP_BRANCH(have);
    if (have && ro < sizeof result - 32) {
        ro += (size_t)snprintf(result + ro, sizeof result - ro,
                "%s|%s|%d%d%d%d;", cur_path ? cur_path : "",
                cur_branch ? cur_branch : "", detached, bare, locked,
                ntree == 0);
    }
    free(cur_path); free(cur_branch);
    free(buf);

    WCUP_VALSTR(result);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_worktrees");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s parse_worktrees <porcelain-blob>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) wc_parse(argv[i]);
    return 0;
}
