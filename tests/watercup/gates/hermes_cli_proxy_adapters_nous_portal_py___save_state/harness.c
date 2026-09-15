/* gate: hermes_cli_proxy_adapters_nous_portal_py___save_state — traced copy of cli_hermes_cli_proxy_adapters_nous_portal__save_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_hermes_cli_proxy_adapters_nous_portal__save_state(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_hermes_cli_proxy_adapters_nous_portal__save_state");
    int result = cli_hermes_cli_proxy_adapters_nous_portal__save_state(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_hermes_cli_proxy_adapters_nous_portal__save_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_proxy_adapters_nous_portal__save_state <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
