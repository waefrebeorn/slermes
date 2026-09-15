/* gate: hermes_cli_kanban_db_py__set_current_board — traced copy of set_current_board */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int set_current_board(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("set_current_board");
    int result = set_current_board(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("set_current_board");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s set_current_board <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
