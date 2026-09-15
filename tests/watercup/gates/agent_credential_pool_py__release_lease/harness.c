/* gate: agent_credential_pool_py__release_lease — traced copy of cpl_release_lease */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cpl_release_lease(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cpl_release_lease");
    int result = cpl_release_lease(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cpl_release_lease");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_release_lease <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
