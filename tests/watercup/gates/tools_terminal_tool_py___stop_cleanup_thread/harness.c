/* gate: tools_terminal_tool_py___stop_cleanup_thread — traced copy of tt_u_stop_cleanup_thread */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tt_u_stop_cleanup_thread(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tt_u_stop_cleanup_thread");
    int result = tt_u_stop_cleanup_thread(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tt_u_stop_cleanup_thread");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tt_u_stop_cleanup_thread <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
