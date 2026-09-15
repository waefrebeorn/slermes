/* gate: hermes_cli_container_boot_py___is_dashboard_container — traced copy of hcli_is_dashboard_container */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hcli_is_dashboard_container(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hcli_is_dashboard_container");
    bool result = hcli_is_dashboard_container(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hcli_is_dashboard_container");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_is_dashboard_container <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
