/* gate: gateway_status_py___scoped_lock_owner_state — traced copy of gstat_u_scoped_lock_owner_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gstat_u_scoped_lock_owner_state(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gstat_u_scoped_lock_owner_state");
    int result = gstat_u_scoped_lock_owner_state(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gstat_u_scoped_lock_owner_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gstat_u_scoped_lock_owner_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
