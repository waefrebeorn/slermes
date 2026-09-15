/* gate: hermes_cli_kanban_db_py__board_exists — traced copy of kdb_board_exists */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdb_board_exists(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdb_board_exists");
    int result = kdb_board_exists(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdb_board_exists");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_board_exists <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
