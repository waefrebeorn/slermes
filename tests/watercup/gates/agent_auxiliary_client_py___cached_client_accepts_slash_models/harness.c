/* gate: agent_auxiliary_client_py___cached_client_accepts_slash_models — traced copy of aux__cached_client_accepts_slash_models */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__cached_client_accepts_slash_models(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__cached_client_accepts_slash_models");
    int result = aux__cached_client_accepts_slash_models(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__cached_client_accepts_slash_models");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__cached_client_accepts_slash_models <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
