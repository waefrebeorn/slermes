/* gate: hermes_cli_model_switch_py__get_authenticated_provider_slugs — traced copy of msw_get_authenticated_provider_slugs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msw_get_authenticated_provider_slugs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msw_get_authenticated_provider_slugs");
    int result = msw_get_authenticated_provider_slugs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msw_get_authenticated_provider_slugs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msw_get_authenticated_provider_slugs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
