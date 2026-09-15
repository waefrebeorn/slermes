/* gate: agent_verification_evidence_py___strip_command_prefix — traced copy of vev_u_strip_command_prefix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vev_u_strip_command_prefix(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vev_u_strip_command_prefix");
    int result = vev_u_strip_command_prefix(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vev_u_strip_command_prefix");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vev_u_strip_command_prefix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
