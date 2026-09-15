/* gate: gateway_platforms_base_py___session_task_is_stale — traced copy of gw_base__session_task_is_stale */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_base__session_task_is_stale(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_base__session_task_is_stale");
    int result = gw_base__session_task_is_stale(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_base__session_task_is_stale");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_base__session_task_is_stale <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
