/* gate: agent_curator_py__save_state — traced copy of cur_save_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cur_save_state(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cur_save_state");
    int result = cur_save_state(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cur_save_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cur_save_state <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
