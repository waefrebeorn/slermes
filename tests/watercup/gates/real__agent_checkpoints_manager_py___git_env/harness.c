/* gate: _git_env(store, worktree, index) values, one key per fixture.
 * Fixture form: "<KEY>|<store>|<worktree>|<index>". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
const char *git_env_value(const char *key, const char *store, const char *working_dir,
                          const char *index_file);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char work[8192]; snprintf(work, sizeof work, "%s", argv[i]);
        char *f[4] = {work, "", "", ""};
        int n = 0;
        for (char *p = work; *p && n < 3; p++) if (*p == '|') { *p = '\0'; f[++n] = p + 1; }
        WCUP_BEGIN("_git_env");
        const char *v = git_env_value(f[0], f[1], f[2], f[3]);
        WCUP_VALSTR(v ? v : "None");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("_git_env");
    }
    return 0;
}
