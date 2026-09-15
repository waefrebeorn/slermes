/* gate: hermes_cli_gateway_enroll_py___default_gateway_id — traced copy of hermes_cli_gateway_enroll_u_default_gateway_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_gateway_enroll_u_default_gateway_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_gateway_enroll_u_default_gateway_id");
    int result = hermes_cli_gateway_enroll_u_default_gateway_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_gateway_enroll_u_default_gateway_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_gateway_enroll_u_default_gateway_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
