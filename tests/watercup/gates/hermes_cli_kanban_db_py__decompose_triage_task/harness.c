/* gate: hermes_cli_kanban_db_py__decompose_triage_task — traced copy of kdbport_decompose_triage_task */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdbport_decompose_triage_task(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdbport_decompose_triage_task");
    int result = kdbport_decompose_triage_task(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdbport_decompose_triage_task");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdbport_decompose_triage_task <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
