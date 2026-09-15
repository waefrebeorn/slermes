/* gate: tools_budget_config_py__resolve_threshold — traced copy of tools_budget_config_resolve_threshold */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_budget_config_resolve_threshold(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_budget_config_resolve_threshold");
    int result = tools_budget_config_resolve_threshold(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_budget_config_resolve_threshold");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_budget_config_resolve_threshold <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
