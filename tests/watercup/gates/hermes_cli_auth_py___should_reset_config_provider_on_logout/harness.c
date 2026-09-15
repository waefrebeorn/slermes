/* gate: hermes_cli_auth_py___should_reset_config_provider_on_logout — traced copy of auth_should_reset_config_provider_on_logout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool auth_should_reset_config_provider_on_logout(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_should_reset_config_provider_on_logout");
    bool result = auth_should_reset_config_provider_on_logout(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_should_reset_config_provider_on_logout");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_should_reset_config_provider_on_logout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
