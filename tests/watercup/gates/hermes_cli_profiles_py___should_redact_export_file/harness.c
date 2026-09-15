/* gate: hermes_cli_profiles_py___should_redact_export_file — traced copy of profiles_should_redact_export_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int profiles_should_redact_export_file(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("profiles_should_redact_export_file");
    int result = profiles_should_redact_export_file(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("profiles_should_redact_export_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profiles_should_redact_export_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
