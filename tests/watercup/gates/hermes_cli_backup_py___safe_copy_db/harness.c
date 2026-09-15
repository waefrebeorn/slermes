/* gate: hermes_cli_backup_py___safe_copy_db — traced copy of backup_safe_copy_db */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int backup_safe_copy_db(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("backup_safe_copy_db");
    int result = backup_safe_copy_db(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("backup_safe_copy_db");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s backup_safe_copy_db <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
