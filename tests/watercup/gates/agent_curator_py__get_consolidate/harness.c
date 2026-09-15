/* gate: agent_curator_py__get_consolidate — traced copy of cur_get_consolidate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cur_get_consolidate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cur_get_consolidate");
    bool result = cur_get_consolidate(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cur_get_consolidate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cur_get_consolidate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
