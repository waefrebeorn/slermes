/* gate: hermes_cli_session_export_md_py___segments — traced copy of sexmd_u_segments */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexmd_u_segments(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexmd_u_segments");
    int result = sexmd_u_segments(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexmd_u_segments");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexmd_u_segments <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
