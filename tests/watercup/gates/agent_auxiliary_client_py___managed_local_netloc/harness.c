/* gate: agent_auxiliary_client_py___managed_local_netloc — traced copy of _managed_local_netloc */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _managed_local_netloc(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_managed_local_netloc");
    int result = _managed_local_netloc(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_managed_local_netloc");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _managed_local_netloc <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
