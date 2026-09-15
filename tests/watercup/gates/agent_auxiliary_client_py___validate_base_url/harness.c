/* gate: agent_auxiliary_client_py___validate_base_url — traced copy of auxc_validate_base_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auxc_validate_base_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auxc_validate_base_url");
    int result = auxc_validate_base_url(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auxc_validate_base_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auxc_validate_base_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
