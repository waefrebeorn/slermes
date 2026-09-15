/* gate: tools_terminal_tool_py___is_safe_workdir_char — traced copy of termtool_is_safe_workdir_char */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int termtool_is_safe_workdir_char(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("termtool_is_safe_workdir_char");
    int result = termtool_is_safe_workdir_char(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("termtool_is_safe_workdir_char");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s termtool_is_safe_workdir_char <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
