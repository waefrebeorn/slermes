/* gate: hermes_cli_security_audit_py___extract_mcp_component — traced copy of hermes_cli_security_audit_u_extract_mcp_component */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_security_audit_u_extract_mcp_component(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_security_audit_u_extract_mcp_component");
    int result = hermes_cli_security_audit_u_extract_mcp_component(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_security_audit_u_extract_mcp_component");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_security_audit_u_extract_mcp_component <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
