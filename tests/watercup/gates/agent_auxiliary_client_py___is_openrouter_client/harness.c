/* gate: agent_auxiliary_client_py___is_openrouter_client — traced copy of aux__is_openrouter_client */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__is_openrouter_client(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__is_openrouter_client");
    int result = aux__is_openrouter_client(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__is_openrouter_client");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__is_openrouter_client <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
