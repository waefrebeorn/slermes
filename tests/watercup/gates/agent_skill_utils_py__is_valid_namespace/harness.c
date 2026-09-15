/* gate: agent_skill_utils_py__is_valid_namespace — traced copy of sku_is_valid_namespace */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_is_valid_namespace(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_is_valid_namespace");
    int result = sku_is_valid_namespace(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_is_valid_namespace");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_is_valid_namespace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
