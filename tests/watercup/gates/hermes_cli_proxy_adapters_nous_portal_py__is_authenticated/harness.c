/* gate: hermes_cli_proxy_adapters_nous_portal_py__is_authenticated — traced copy of cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated");
    int result = cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_proxy_adapters_nous_portal_is_authenticated <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
