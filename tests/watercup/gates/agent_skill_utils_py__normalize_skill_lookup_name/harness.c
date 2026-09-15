/* gate: agent_skill_utils_py__normalize_skill_lookup_name — traced copy of sku_normalize_skill_lookup_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_normalize_skill_lookup_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_normalize_skill_lookup_name");
    int result = sku_normalize_skill_lookup_name(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_normalize_skill_lookup_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_normalize_skill_lookup_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
