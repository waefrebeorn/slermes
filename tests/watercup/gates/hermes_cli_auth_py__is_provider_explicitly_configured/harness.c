/* gate: hermes_cli_auth_py__is_provider_explicitly_configured — traced copy of is_provider_explicitly_configured */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_provider_explicitly_configured(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_provider_explicitly_configured");
    bool result = is_provider_explicitly_configured(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_provider_explicitly_configured");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_provider_explicitly_configured <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
