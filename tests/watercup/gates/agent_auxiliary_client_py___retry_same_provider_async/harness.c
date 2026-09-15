/* gate: agent_auxiliary_client_py___retry_same_provider_async — traced copy of aux__retry_same_provider_async */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__retry_same_provider_async(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("aux__retry_same_provider_async");
    int result = aux__retry_same_provider_async(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__retry_same_provider_async");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__retry_same_provider_async <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
