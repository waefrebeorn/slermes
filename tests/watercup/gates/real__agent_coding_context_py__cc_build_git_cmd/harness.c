/* gate: command line built for `git -C <cwd> <args>`. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
void cc_build_git_cmd(const char *cwd, const char *args, char *out, size_t outsz);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char work[1024]; snprintf(work, sizeof work, "%s", argv[i]);
        char *bar = strchr(work, '|');
        char *args = ""; if (bar) { *bar = 0; args = bar + 1; }
        char out[4096];
        WCUP_BEGIN("cc_build_git_cmd");
        cc_build_git_cmd(work, args, out, sizeof out);
        WCUP_VALSTR(out);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("cc_build_git_cmd");
    }
    return 0;
}
