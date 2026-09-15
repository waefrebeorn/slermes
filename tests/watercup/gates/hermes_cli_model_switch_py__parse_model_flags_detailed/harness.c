/* gate: hermes_cli_model_switch_py__parse_model_flags_detailed — traced copy of msw_parse_model_flags_detailed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msw_parse_model_flags_detailed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msw_parse_model_flags_detailed");
    int result = msw_parse_model_flags_detailed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msw_parse_model_flags_detailed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msw_parse_model_flags_detailed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
