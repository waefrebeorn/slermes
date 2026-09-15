/* gate: hermes_cli_kanban_db_py__repair_db — traced copy of kdbport_repair_db */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdbport_repair_db(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdbport_repair_db");
    int result = kdbport_repair_db(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdbport_repair_db");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdbport_repair_db <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
