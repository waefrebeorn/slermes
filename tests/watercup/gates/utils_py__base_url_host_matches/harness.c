/* gate: utils_py__base_url_host_matches — traced copy of provider_base_url_host_matches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int provider_base_url_host_matches(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("provider_base_url_host_matches");
    int result = provider_base_url_host_matches(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("provider_base_url_host_matches");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s provider_base_url_host_matches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
