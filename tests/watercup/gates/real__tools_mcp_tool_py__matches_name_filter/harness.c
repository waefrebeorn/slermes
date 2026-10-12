/* gate: real__tools_mcp_tool_py__matches_name_filter — GAP sentinel for matches_name_filter */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "matches_name_filter";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s matches_name_filter <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("matches_name_filter");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("matches_name_filter");
    }
    return 0;
}
