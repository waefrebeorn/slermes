/* gate: port__cli_py___worktree_commits_all_merged_upstream — traced copy of cli_worktree_commits_all_merged_upstream (json_param) */
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


extern int cli_worktree_commits_all_merged_upstream(const char *worktree_path, int timeout,
                                             int max_ahead, json_t *cache);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_worktree_commits_all_merged_upstream <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_worktree_commits_all_merged_upstream");
    json_t *_j3 = json_parse(argv[2 + (i - 2 + 3) % 5], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_cap = tmpfile();
    if (_cap) dup2(fileno(_cap), 1);
    int result = cli_worktree_commits_all_merged_upstream(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), _j3);
    fflush(stdout);
    if (_cap) {
        dup2(_sv, 1); close(_sv);
        rewind(_cap);
        char _ob[4096]; size_t _on = 0; int _ch;
        while ((_ch = fgetc(_cap)) != EOF && _on < sizeof(_ob) - 1)
            _ob[_on++] = (char)_ch;
        _ob[_on] = '\0';
        /* strip the trailing newline the port emits */
        while (_on && (_ob[_on-1] == '\n' || _ob[_on-1] == '\r'))
            _ob[--_on] = '\0';
        WCUP_VALSTR(_ob);
        fclose(_cap);
    }
    if (_j3) json_free(_j3);

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_worktree_commits_all_merged_upstream");
    }
    return 0;
}
