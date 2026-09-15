/* gate: hermes_cli_kanban_db_py__list_runs — traced copy of kdbport_list_runs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdbport_list_runs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdbport_list_runs");
    int result = kdbport_list_runs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdbport_list_runs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdbport_list_runs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
