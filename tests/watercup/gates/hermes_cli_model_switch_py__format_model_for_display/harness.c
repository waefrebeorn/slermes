/* gate: hermes_cli_model_switch_py__format_model_for_display — traced copy of msw_format_model_for_display */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msw_format_model_for_display(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msw_format_model_for_display");
    int result = msw_format_model_for_display(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msw_format_model_for_display");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msw_format_model_for_display <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
