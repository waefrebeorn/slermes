/* gate: agent_agent_runtime_helpers_py__switch_model — traced copy of arh_switch_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int arh_switch_model(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("arh_switch_model");
    int result = arh_switch_model(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("arh_switch_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s arh_switch_model <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
