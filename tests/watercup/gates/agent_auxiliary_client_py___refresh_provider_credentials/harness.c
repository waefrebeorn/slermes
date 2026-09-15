/* gate: agent_auxiliary_client_py___refresh_provider_credentials — traced copy of aux__refresh_provider_credentials */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__refresh_provider_credentials(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__refresh_provider_credentials");
    int result = aux__refresh_provider_credentials(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__refresh_provider_credentials");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__refresh_provider_credentials <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
