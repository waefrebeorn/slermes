/* gate: hermes_cli_backup_py___import_db_member — traced copy of _import_db_member */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _import_db_member(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_import_db_member");
    int result = _import_db_member(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_import_db_member");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _import_db_member <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
