/* gate: agent_auxiliary_client_py___recover_provider_pool — traced copy of aux__recover_provider_pool */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__recover_provider_pool(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__recover_provider_pool");
    int result = aux__recover_provider_pool(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__recover_provider_pool");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__recover_provider_pool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
