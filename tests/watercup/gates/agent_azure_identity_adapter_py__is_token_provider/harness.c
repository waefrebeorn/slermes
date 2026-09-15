/* gate: agent_azure_identity_adapter_py__is_token_provider — traced copy of azu_is_token_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool azu_is_token_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("azu_is_token_provider");
    bool result = azu_is_token_provider(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("azu_is_token_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s azu_is_token_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
