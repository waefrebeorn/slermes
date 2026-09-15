/* gate: hermes_cli_session_export_py___session_metadata_lines — traced copy of sexp_u_session_metadata_lines */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexp_u_session_metadata_lines(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexp_u_session_metadata_lines");
    int result = sexp_u_session_metadata_lines(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexp_u_session_metadata_lines");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexp_u_session_metadata_lines <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
