/* gate: hermes_cli_personality_py__persist_personality — traced copy of per_persist_personality */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int per_persist_personality(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("per_persist_personality");
    int result = per_persist_personality(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("per_persist_personality");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s per_persist_personality <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
