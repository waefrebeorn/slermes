/* gate: hermes_cli_runtime_provider_py___provider_supports_explicit_api_mode — traced copy of provider_supports_explicit_api_mode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int provider_supports_explicit_api_mode(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("provider_supports_explicit_api_mode");
    int result = provider_supports_explicit_api_mode(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("provider_supports_explicit_api_mode");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s provider_supports_explicit_api_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
