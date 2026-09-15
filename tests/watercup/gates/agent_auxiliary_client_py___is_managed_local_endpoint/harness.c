/* gate: agent_auxiliary_client_py___is_managed_local_endpoint — traced copy of _is_managed_local_endpoint */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _is_managed_local_endpoint(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_managed_local_endpoint");
    int result = _is_managed_local_endpoint(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_managed_local_endpoint");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_managed_local_endpoint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
