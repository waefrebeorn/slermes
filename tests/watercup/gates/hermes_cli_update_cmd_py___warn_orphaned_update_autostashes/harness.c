/* gate: hermes_cli_update_cmd_py___warn_orphaned_update_autostashes — traced copy of _warn_orphaned_update_autostashes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _warn_orphaned_update_autostashes(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_warn_orphaned_update_autostashes");
    int result = _warn_orphaned_update_autostashes(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_warn_orphaned_update_autostashes");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _warn_orphaned_update_autostashes <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
