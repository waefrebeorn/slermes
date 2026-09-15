/* gate: tools_approval_py___command_matches_permanent_allowlist — traced copy of apr_command_matches_permanent_allowlist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apr_command_matches_permanent_allowlist(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apr_command_matches_permanent_allowlist");
    int result = apr_command_matches_permanent_allowlist(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr_command_matches_permanent_allowlist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr_command_matches_permanent_allowlist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
