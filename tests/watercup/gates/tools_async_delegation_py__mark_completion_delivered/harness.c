/* gate: tools_async_delegation_py__mark_completion_delivered — traced copy of adel_mark_completion_delivered */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int adel_mark_completion_delivered(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("adel_mark_completion_delivered");
    int result = adel_mark_completion_delivered(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("adel_mark_completion_delivered");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s adel_mark_completion_delivered <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
