/* gate: hermes_cli_session_export_py__render_sessions_export — traced copy of sexp_render_sessions_export */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexp_render_sessions_export(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexp_render_sessions_export");
    int result = sexp_render_sessions_export(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexp_render_sessions_export");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexp_render_sessions_export <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
