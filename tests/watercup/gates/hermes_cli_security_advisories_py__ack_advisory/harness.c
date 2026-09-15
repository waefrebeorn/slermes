/* gate: hermes_cli_security_advisories_py__ack_advisory — traced copy of cli_security_advisories_ack_advisory */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cli_security_advisories_ack_advisory(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_security_advisories_ack_advisory");
    bool result = cli_security_advisories_ack_advisory(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_security_advisories_ack_advisory");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_security_advisories_ack_advisory <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
