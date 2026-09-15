/* gate: hermes_cli_session_export_py___heading_text — traced copy of sexp_u_heading_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexp_u_heading_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexp_u_heading_text");
    int result = sexp_u_heading_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexp_u_heading_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexp_u_heading_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
