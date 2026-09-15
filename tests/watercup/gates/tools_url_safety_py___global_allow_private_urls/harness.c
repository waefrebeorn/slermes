/* gate: tools_url_safety_py___global_allow_private_urls — traced copy of yaml_config_allow_private */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yaml_config_allow_private(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yaml_config_allow_private");
    int result = yaml_config_allow_private(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yaml_config_allow_private");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yaml_config_allow_private <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
