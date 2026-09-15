/* gate: hermes_cli_session_export_py__normalize_export_only — traced copy of sexp_normalize_export_only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexp_normalize_export_only(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexp_normalize_export_only");
    int result = sexp_normalize_export_only(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexp_normalize_export_only");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexp_normalize_export_only <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
