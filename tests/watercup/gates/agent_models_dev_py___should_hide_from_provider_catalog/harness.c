/* gate: agent_models_dev_py___should_hide_from_provider_catalog — traced copy of md_should_hide_from_provider_catalog */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int md_should_hide_from_provider_catalog(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("md_should_hide_from_provider_catalog");
    int result = md_should_hide_from_provider_catalog(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("md_should_hide_from_provider_catalog");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s md_should_hide_from_provider_catalog <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
