/* gate: real__cron_lifecycle_guard_py___executable_name — GAP sentinel for _executable_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_executable_name";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _executable_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_executable_name");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_executable_name");
    }
    return 0;
}
