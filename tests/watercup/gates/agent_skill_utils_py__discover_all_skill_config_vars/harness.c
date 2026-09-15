/* gate: agent_skill_utils_py__discover_all_skill_config_vars — traced copy of sku_discover_all_skill_config_vars */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_discover_all_skill_config_vars(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_discover_all_skill_config_vars");
    int result = sku_discover_all_skill_config_vars(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_discover_all_skill_config_vars");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_discover_all_skill_config_vars <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
