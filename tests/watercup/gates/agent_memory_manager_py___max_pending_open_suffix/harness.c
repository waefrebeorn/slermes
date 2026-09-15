/* gate: agent_memory_manager_py___max_pending_open_suffix — traced copy of mm_u_max_pending_open_suffix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mm_u_max_pending_open_suffix(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mm_u_max_pending_open_suffix");
    int result = mm_u_max_pending_open_suffix(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mm_u_max_pending_open_suffix");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mm_u_max_pending_open_suffix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
