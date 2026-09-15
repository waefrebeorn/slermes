/* gate: hermes_cli_runtime_provider_py___config_base_url_trustworthy_for_bare_custom — traced copy of _config_base_url_trustworthy_for_bare_custom */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _config_base_url_trustworthy_for_bare_custom(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_config_base_url_trustworthy_for_bare_custom");
    int result = _config_base_url_trustworthy_for_bare_custom(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_config_base_url_trustworthy_for_bare_custom");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _config_base_url_trustworthy_for_bare_custom <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
