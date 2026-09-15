/* gate: tools_tool_search_py__is_deferrable_tool_name — traced copy of ts_is_deferrable_tool_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ts_is_deferrable_tool_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ts_is_deferrable_tool_name");
    int result = ts_is_deferrable_tool_name(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ts_is_deferrable_tool_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ts_is_deferrable_tool_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
