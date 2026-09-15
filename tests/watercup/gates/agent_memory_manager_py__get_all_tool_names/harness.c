/* gate: agent_memory_manager_py__get_all_tool_names — traced copy of mm_get_all_tool_names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mm_get_all_tool_names(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mm_get_all_tool_names");
    int result = mm_get_all_tool_names(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mm_get_all_tool_names");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mm_get_all_tool_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
