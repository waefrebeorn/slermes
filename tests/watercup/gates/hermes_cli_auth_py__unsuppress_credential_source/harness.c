/* gate: hermes_cli_auth_py__unsuppress_credential_source — traced copy of unsuppress_credential_source */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int unsuppress_credential_source(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("unsuppress_credential_source");
    int result = unsuppress_credential_source(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("unsuppress_credential_source");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s unsuppress_credential_source <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
