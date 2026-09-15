/* gate: tools_mcp_oauth_py__poison_client_registration — traced copy of mcpo_poison_client_registration */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_poison_client_registration(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_poison_client_registration");
    int result = mcpo_poison_client_registration(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_poison_client_registration");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_poison_client_registration <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
