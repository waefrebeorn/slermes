/* gate: hermes_cli_model_switch_py__is_nous_hermes_non_agentic — traced copy of msw_is_nous_hermes_non_agentic */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msw_is_nous_hermes_non_agentic(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msw_is_nous_hermes_non_agentic");
    int result = msw_is_nous_hermes_non_agentic(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msw_is_nous_hermes_non_agentic");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msw_is_nous_hermes_non_agentic <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
