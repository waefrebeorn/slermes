/* gate: hermes_cli_backup_py___safe_restore_db — traced copy of _safe_restore_db */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _safe_restore_db(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_safe_restore_db");
    int result = _safe_restore_db(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_safe_restore_db");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _safe_restore_db <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
