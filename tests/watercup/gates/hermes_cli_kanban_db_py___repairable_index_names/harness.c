/* gate: hermes_cli_kanban_db_py___repairable_index_names — traced copy of kdbport_u_repairable_index_names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdbport_u_repairable_index_names(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdbport_u_repairable_index_names");
    int result = kdbport_u_repairable_index_names(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdbport_u_repairable_index_names");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdbport_u_repairable_index_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
