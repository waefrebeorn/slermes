/* gate: tools_project_tools_py__project_create — traced copy of tools_project_tools_project_create */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_project_tools_project_create(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_project_tools_project_create");
    int result = tools_project_tools_project_create(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_project_tools_project_create");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_project_tools_project_create <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
