/* gate: hermes_cli_kanban_db_py___fire_kanban_lifecycle_hook — traced copy of kdb_fire_kanban_lifecycle_hook */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdb_fire_kanban_lifecycle_hook(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("kdb_fire_kanban_lifecycle_hook");
    int result = kdb_fire_kanban_lifecycle_hook(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdb_fire_kanban_lifecycle_hook");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_fire_kanban_lifecycle_hook <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
