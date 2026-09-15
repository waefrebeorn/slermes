/* gate: tools_skill_usage_py__set_state — traced copy of sku_set_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sku_set_state(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("sku_set_state");
    int result = sku_set_state(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sku_set_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sku_set_state <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
