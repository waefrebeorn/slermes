/* gate: real__tools_process_registry_py___host_pid_is_ours — GAP sentinel for _host_pid_is_ours */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_host_pid_is_ours";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _host_pid_is_ours <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_host_pid_is_ours");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_host_pid_is_ours");
    }
    return 0;
}
