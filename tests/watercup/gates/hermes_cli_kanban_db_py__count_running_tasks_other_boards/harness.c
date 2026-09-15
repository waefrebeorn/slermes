/* gate: hermes_cli_kanban_db_py__count_running_tasks_other_boards — traced copy of kdb_count_running_tasks_other_boards */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdb_count_running_tasks_other_boards(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdb_count_running_tasks_other_boards");
    int result = kdb_count_running_tasks_other_boards(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdb_count_running_tasks_other_boards");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_count_running_tasks_other_boards <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
