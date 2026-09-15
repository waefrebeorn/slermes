/* gate: hermes_cli_kanban_db_py__from_row — traced copy of kdbport_from_row */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdbport_from_row(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdbport_from_row");
    int result = kdbport_from_row(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdbport_from_row");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdbport_from_row <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
