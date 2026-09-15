/* gate: agent_auxiliary_client_py___recoverable_pool_provider — traced copy of aux__recoverable_pool_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__recoverable_pool_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__recoverable_pool_provider");
    int result = aux__recoverable_pool_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__recoverable_pool_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__recoverable_pool_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
