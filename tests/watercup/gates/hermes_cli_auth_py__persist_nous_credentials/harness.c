/* gate: hermes_cli_auth_py__persist_nous_credentials — traced copy of auth_persist_nous_credentials */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_persist_nous_credentials(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_persist_nous_credentials");
    int result = auth_persist_nous_credentials(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_persist_nous_credentials");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_persist_nous_credentials <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
