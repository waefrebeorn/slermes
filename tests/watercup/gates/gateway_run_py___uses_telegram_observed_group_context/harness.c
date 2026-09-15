/* gate: gateway_run_py___uses_telegram_observed_group_context — traced copy of gw_uses_observed_group_context */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_uses_observed_group_context(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_uses_observed_group_context");
    bool result = gw_uses_observed_group_context(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_uses_observed_group_context");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_uses_observed_group_context <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
