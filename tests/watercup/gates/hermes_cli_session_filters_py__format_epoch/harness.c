/* gate: hermes_cli_session_filters_py__format_epoch — traced copy of hermes_cli_session_filters_format_epoch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_session_filters_format_epoch(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_session_filters_format_epoch");
    int result = hermes_cli_session_filters_format_epoch(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_session_filters_format_epoch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_session_filters_format_epoch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
