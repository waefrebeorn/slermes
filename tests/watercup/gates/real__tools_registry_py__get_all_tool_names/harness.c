/* gate: real__tools_registry_py__get_all_tool_names — GAP sentinel for get_all_tool_names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "get_all_tool_names";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_all_tool_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("get_all_tool_names");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("get_all_tool_names");
    }
    return 0;
}
