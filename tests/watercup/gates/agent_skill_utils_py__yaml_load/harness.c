/* gate: agent_skill_utils_py__yaml_load — traced copy of sku_yaml_load */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_yaml_load(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_yaml_load");
    int result = sku_yaml_load(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_yaml_load");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_yaml_load <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
