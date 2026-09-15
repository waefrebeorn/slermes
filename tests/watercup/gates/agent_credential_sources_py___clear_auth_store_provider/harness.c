/* gate: agent_credential_sources_py___clear_auth_store_provider — traced copy of crs_clear_auth_store_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool crs_clear_auth_store_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("crs_clear_auth_store_provider");
    bool result = crs_clear_auth_store_provider(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("crs_clear_auth_store_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s crs_clear_auth_store_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
