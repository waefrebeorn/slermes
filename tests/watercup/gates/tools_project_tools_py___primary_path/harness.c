/* gate: tools_project_tools_py___primary_path — traced copy of tools_project_tools_u_primary_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_project_tools_u_primary_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_project_tools_u_primary_path");
    int result = tools_project_tools_u_primary_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_project_tools_u_primary_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_project_tools_u_primary_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
