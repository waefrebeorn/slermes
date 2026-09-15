/* gate: cli_py___should_emit_cleanup_session_finalize — traced copy of _should_emit_cleanup_session_finalize */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _should_emit_cleanup_session_finalize(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_should_emit_cleanup_session_finalize");
    bool result = _should_emit_cleanup_session_finalize(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_should_emit_cleanup_session_finalize");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _should_emit_cleanup_session_finalize <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
