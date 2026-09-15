/* gate: agent_skill_utils_py___detect_environment — traced copy of sku_u_detect_environment */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_u_detect_environment(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_u_detect_environment");
    int result = sku_u_detect_environment(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_u_detect_environment");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_u_detect_environment <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
