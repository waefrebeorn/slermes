/* gate: hermes_state_py___try_acquire_auto_maintenance_lock — traced copy of _try_acquire_auto_maintenance_lock */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _try_acquire_auto_maintenance_lock(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_try_acquire_auto_maintenance_lock");
    int result = _try_acquire_auto_maintenance_lock(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_try_acquire_auto_maintenance_lock");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _try_acquire_auto_maintenance_lock <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
