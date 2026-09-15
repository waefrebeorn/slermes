/* gate: tools_skill_manager_tool_py___validate_file_path — traced copy of _validate_file_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _validate_file_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_validate_file_path");
    int result = _validate_file_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_validate_file_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _validate_file_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
