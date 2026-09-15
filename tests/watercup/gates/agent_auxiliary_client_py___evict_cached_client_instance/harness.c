/* gate: agent_auxiliary_client_py___evict_cached_client_instance — traced copy of aux__evict_cached_client_instance */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__evict_cached_client_instance(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("aux__evict_cached_client_instance");
    int result = aux__evict_cached_client_instance(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__evict_cached_client_instance");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__evict_cached_client_instance <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
