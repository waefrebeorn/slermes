/* gate: hermes_cli_security_audit_startup_py__log_startup_security_warnings — traced copy of hermes_cli_security_audit_star_log_startup_security_warnings */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_security_audit_star_log_startup_security_warnings(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_security_audit_star_log_startup_security_warnings");
    int result = hermes_cli_security_audit_star_log_startup_security_warnings(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_security_audit_star_log_startup_security_warnings");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_security_audit_star_log_startup_security_warnings <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
