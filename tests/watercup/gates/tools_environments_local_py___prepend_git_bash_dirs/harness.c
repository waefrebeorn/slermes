/* gate: tools_environments_local_py___prepend_git_bash_dirs — traced copy of envl_u_prepend_git_bash_dirs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envl_u_prepend_git_bash_dirs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envl_u_prepend_git_bash_dirs");
    int result = envl_u_prepend_git_bash_dirs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envl_u_prepend_git_bash_dirs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envl_u_prepend_git_bash_dirs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
