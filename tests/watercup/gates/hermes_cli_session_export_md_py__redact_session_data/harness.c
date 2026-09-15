/* gate: hermes_cli_session_export_md_py__redact_session_data — traced copy of sexmd_redact_session_data */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexmd_redact_session_data(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexmd_redact_session_data");
    int result = sexmd_redact_session_data(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexmd_redact_session_data");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexmd_redact_session_data <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
