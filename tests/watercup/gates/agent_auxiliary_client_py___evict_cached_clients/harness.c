/* gate: agent_auxiliary_client_py___evict_cached_clients — traced copy of aux__evict_cached_clients */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__evict_cached_clients(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__evict_cached_clients");
    int result = aux__evict_cached_clients(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__evict_cached_clients");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__evict_cached_clients <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
