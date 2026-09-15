/* gate: agent_credential_pool_py__has_credentials — traced copy of cpl_has_credentials */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cpl_has_credentials(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cpl_has_credentials");
    bool result = cpl_has_credentials(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cpl_has_credentials");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_has_credentials <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
