/* gate: hermes_cli_kanban_db_py___worker_survived_termination — traced copy of kb_worker_survived_termination */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kb_worker_survived_termination(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kb_worker_survived_termination");
    int result = kb_worker_survived_termination(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kb_worker_survived_termination");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kb_worker_survived_termination <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
