/* gate: hermes_cli_update_cmd_py___restore_state_db_from_snapshot — traced copy of _restore_state_db_from_snapshot */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _restore_state_db_from_snapshot(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_restore_state_db_from_snapshot");
    int result = _restore_state_db_from_snapshot(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_restore_state_db_from_snapshot");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _restore_state_db_from_snapshot <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
