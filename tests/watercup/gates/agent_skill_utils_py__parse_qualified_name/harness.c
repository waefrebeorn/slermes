/* gate: agent_skill_utils_py__parse_qualified_name — traced copy of sku_parse_qualified_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_parse_qualified_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sku_parse_qualified_name");
    int result = sku_parse_qualified_name(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_parse_qualified_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_parse_qualified_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
