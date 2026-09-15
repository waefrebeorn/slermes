/* gate: hermes_cli_model_switch_py__prewarm_picker_cache_async — traced copy of msw_prewarm_picker_cache_async */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msw_prewarm_picker_cache_async(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msw_prewarm_picker_cache_async");
    int result = msw_prewarm_picker_cache_async(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msw_prewarm_picker_cache_async");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msw_prewarm_picker_cache_async <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
